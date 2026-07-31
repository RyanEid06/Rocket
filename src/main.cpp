#include "codegen.h"
#include "formatter.h"
#include "lexer.h"
#include "mir.h"
#include "module_loader.h"
#include "native.h"
#include "package.h"
#include "package_docs.h"
#include "package_registry.h"
#include "platform_credentials.h"
#include "parser.h"
#include "sema.h"
#ifdef ROCKETC_HAS_LLVM
#include "llvm_codegen.h"
#endif

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {

fs::path compilerDirectory;

std::string quote(const fs::path& path) { return "\"" + path.string() + "\""; }

void cliDiagnostic(rocket::DiagnosticCode code, const std::string& message) {
  std::cerr << "rocketc: error[" << rocket::diagnosticCodeName(code) << "]: "
            << message << '\n';
}

bool readFile(const fs::path& path, std::string& result) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  std::ostringstream buffer;
  buffer << input.rdbuf();
  result = buffer.str();
  return true;
}

struct Compilation {
  rocket::Diagnostics diagnostics;
  rocket::Module module;
  std::optional<rocket::HirModule> hir;
  std::optional<rocket::MirModule> mir;
};

Compilation compileFrontend(const fs::path& path, const fs::path& packageRoot,
                            bool library = false,
                            const std::vector<rocket::PackageDependencyRoot>&
                                dependencyRoots = {}) {
  Compilation result;
  auto loaded = rocket::loadModuleGraph(path, packageRoot, dependencyRoots,
                                        result.diagnostics);
  if (loaded.has_value()) {
    result.module = std::move(*loaded);
    result.module.library = library;
  }
  if (!result.diagnostics.hasErrors()) {
    rocket::SemanticAnalyzer analyzer(result.module, result.diagnostics);
    result.hir = analyzer.analyzeToHir();
  }
  if (result.hir.has_value()) {
    rocket::MirLowerer lowerer(*result.hir);
    result.mir = lowerer.lower();
    std::string verifierError;
    if (!rocket::verifyMir(*result.mir, verifierError)) {
      result.diagnostics.error({path.string(), 1, 1},
                               "internal MIR verification failed: " + verifierError,
                               rocket::DiagnosticCode::Internal);
      result.mir.reset();
    }
  }
  return result;
}

bool writeGenerated(const fs::path& sourcePath, const fs::path& directory,
                    const std::string& code,
                    fs::path& generatedPath) {
  generatedPath = directory / (sourcePath.stem().string() + ".bootstrap.cpp");
  std::ofstream output(generatedPath, std::ios::binary);
  output << code;
  return static_cast<bool>(output);
}

bool ensureArtifactDirectory(const fs::path& root, fs::path& directory) {
  directory = root / ".rocketc";
  std::error_code error;
  fs::create_directories(directory, error);
  return !error;
}

#ifdef _WIN32
std::wstring quoteWindowsArgument(const std::wstring& argument) {
  if (argument.empty()) return L"\"\"";
  if (argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) return argument;
  std::wstring result = L"\"";
  std::size_t backslashes = 0;
  for (const wchar_t character : argument) {
    if (character == L'\\') {
      ++backslashes;
    } else if (character == L'\"') {
      result.append(backslashes * 2 + 1, L'\\');
      result.push_back(L'\"');
      backslashes = 0;
    } else {
      result.append(backslashes, L'\\');
      backslashes = 0;
      result.push_back(character);
    }
  }
  result.append(backslashes * 2, L'\\');
  result.push_back(L'\"');
  return result;
}
#endif

int invokeExecutable(const fs::path& executable, const std::vector<std::string>& arguments) {
  std::cerr << "+ " << quote(executable);
  for (const auto& argument : arguments) std::cerr << " \"" << argument << '"';
  std::cerr << '\n';
#ifdef _WIN32
  const std::wstring nativeExecutable = executable.wstring();
  std::wstring commandLine = quoteWindowsArgument(nativeExecutable);
  for (const auto& argument : arguments)
    commandLine += L" " + quoteWindowsArgument(fs::path(argument).wstring());
  std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
  mutableCommandLine.push_back(L'\0');

  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(nativeExecutable.c_str(), mutableCommandLine.data(), nullptr, nullptr,
                      FALSE, 0, nullptr, nullptr, &startup, &process)) {
    std::cerr << "rocketc: could not start " << executable.string()
              << " (Windows error " << GetLastError() << ")\n";
    return 1;
  }
  WaitForSingleObject(process.hProcess, INFINITE);
  DWORD exitCode = 1;
  GetExitCodeProcess(process.hProcess, &exitCode);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return static_cast<int>(exitCode);
#else
  std::string command = quote(executable);
  for (const auto& argument : arguments) command += " \"" + argument + "\"";
  return std::system(command.c_str());
#endif
}

int compileBootstrap(const fs::path& source, const fs::path& output,
                     bool assembly,
                     rocket::PackageOutputKind outputKind = rocket::PackageOutputKind::Executable,
                     const std::vector<fs::path>& librarySearch = {},
                     const std::vector<std::string>& libraries = {}) {
  std::vector<std::string> arguments;
#ifdef _MSC_VER
  arguments = {"/nologo", "/std:c++20", "/EHsc", "/utf-8",
               "/O2", "/MT", "/Brepro",
               "/I" + fs::path(ROCKETC_SOURCE_INCLUDE_PATH).string()};
  if (assembly || outputKind == rocket::PackageOutputKind::StaticLibrary) {
    fs::path objectPath = output;
    objectPath.replace_extension(".obj");
    if (assembly) {
      arguments.push_back("/FA");
      arguments.push_back("/Fa" + output.string());
    }
    arguments.push_back("/Fo" + objectPath.string());
    arguments.push_back("/c");
  } else {
    if (outputKind == rocket::PackageOutputKind::DynamicLibrary) arguments.push_back("/LD");
    arguments.push_back("/Fe" + output.string());
  }
#else
  arguments = {"-std=c++20", "-O2", "-I", ROCKETC_SOURCE_INCLUDE_PATH};
  if (assembly) {
    arguments.push_back("-S");
    arguments.push_back("-masm=intel");
  }
  arguments.push_back("-o");
  arguments.push_back(output.string());
#endif
  arguments.push_back(source.string());
  if (!assembly && outputKind != rocket::PackageOutputKind::StaticLibrary) {
    arguments.push_back("/link");
    arguments.push_back("/Brepro");
    if (outputKind == rocket::PackageOutputKind::Executable)
      arguments.push_back("/STACK:8388608");
    for (const auto& search : librarySearch)
      arguments.push_back("/LIBPATH:" + search.string());
    for (const auto& library : libraries) arguments.push_back(library);
  }
  const int compiled = invokeExecutable(fs::path(ROCKETC_STAGE0_CXX_PATH), arguments);
  if (compiled != 0 || assembly ||
      outputKind != rocket::PackageOutputKind::StaticLibrary)
    return compiled;
  fs::path objectPath = output;
  objectPath.replace_extension(".obj");
  return invokeExecutable(fs::path(ROCKETC_STAGE0_AR_PATH),
                          {"/nologo", "/Brepro", "/OUT:" + output.string(),
                           objectPath.string()});
}

#ifdef ROCKETC_HAS_LLVM
fs::path clangDriverPath() {
  const fs::path packaged = compilerDirectory / "toolchain/clang.exe";
  return fs::is_regular_file(packaged) ? packaged : fs::path(ROCKETC_CLANG_PATH);
}

fs::path runtimeLibraryPath() {
  const fs::path packaged = compilerDirectory / "rocket_runtime.lib";
  return fs::is_regular_file(packaged) ? packaged : fs::path(ROCKETC_RUNTIME_LIBRARY_PATH);
}

fs::path llvmLibrarianPath() {
  const fs::path packaged = compilerDirectory / "toolchain/llvm-lib.exe";
  if (fs::is_regular_file(packaged)) return packaged;
  fs::path path = clangDriverPath();
  path.replace_filename("llvm-lib.exe");
  return path;
}
#endif

struct CommandTarget {
  fs::path source;
  fs::path packageRoot;
  fs::path artifactRoot;
  rocket::PackageOutputKind outputKind = rocket::PackageOutputKind::Executable;
  std::string outputName;
  std::string packageName;
  std::vector<std::string> nativeLibraries;
  std::vector<fs::path> nativeLibrarySearch;
  std::vector<rocket::PackageDependencyRoot> dependencyRoots;
};

bool writeFile(const fs::path& path, const std::string& contents);

std::optional<CommandTarget> resolveTarget(const fs::path& supplied, std::string& error) {
  const fs::path absolute = fs::absolute(supplied).lexically_normal();
  if (fs::is_directory(absolute) || absolute.filename() == "rocket.toml") {
    auto package = rocket::loadPackage(absolute, error);
    if (!package) return {};
    std::vector<rocket::PackageDependencyRoot> dependencyRoots;
    rocket::PackageLock lock;
    if (!rocket::prepareLockedPackageDependencies(
            *package, false, dependencyRoots, lock, error))
      return {};
    auto nativeLibraries = package->nativeLibraries;
    auto nativeSearch = package->nativeLibrarySearch;
    for (const auto& dependency : dependencyRoots) {
      nativeLibraries.insert(nativeLibraries.end(),
                             dependency.nativeLibraries.begin(),
                             dependency.nativeLibraries.end());
      nativeSearch.insert(nativeSearch.end(),
                          dependency.nativeLibrarySearch.begin(),
                          dependency.nativeLibrarySearch.end());
    }
    return CommandTarget{package->entry, package->root, package->root,
                         package->outputKind, package->outputName, package->name,
                         std::move(nativeLibraries), std::move(nativeSearch),
                         std::move(dependencyRoots)};
  }
  if (!fs::is_regular_file(absolute)) {
    error = "source path does not exist: '" + absolute.string() + "'";
    return {};
  }
  if (absolute.extension() != ".rocket") {
    error = "source files must use the .rocket extension";
    return {};
  }
  return CommandTarget{absolute, absolute.parent_path(), absolute.parent_path(),
                       rocket::PackageOutputKind::Executable,
                       absolute.stem().string(), absolute.stem().string(), {}, {}, {}};
}

int executeCompiler(const std::string& command, const CommandTarget& target,
                    const std::vector<std::string>& programArguments,
                    bool announceBuild = true,
                    const fs::path& headerOutput = {}) {
  const bool library = target.outputKind != rocket::PackageOutputKind::Executable;
  Compilation compilation = compileFrontend(target.source, target.packageRoot,
                                            library, target.dependencyRoots);
  if (compilation.diagnostics.hasErrors()) {
    compilation.diagnostics.print();
    return 1;
  }
  if (command == "check") {
    if (announceBuild) std::cout << target.source.string() << ": check succeeded\n";
    return 0;
  }
  if (command == "emit-ir") {
#ifdef ROCKETC_HAS_LLVM
    std::string ir;
    std::string error;
    if (!rocket::generateLlvmIr(*compilation.mir, false, ir, error)) {
      std::cerr << "rocketc: " << error << '\n';
      return 1;
    }
    std::cout << ir;
    return 0;
#else
    std::cerr << "rocketc: this build has no LLVM backend; rebuild with LLVM or use the "
                 "stage0 build/run fallback\n";
    return 2;
#endif
  }
  if (command == "emit-header") {
    std::string header;
    std::string error;
    if (!rocket::generateNativeHeader(compilation.module, target.packageName, header, error)) {
      cliDiagnostic(rocket::DiagnosticCode::Tooling, error);
      return 1;
    }
    if (headerOutput.empty()) std::cout << header;
    else if (!writeFile(headerOutput, header)) {
      cliDiagnostic(rocket::DiagnosticCode::Tooling,
                    "could not write generated native header");
      return 1;
    }
    return 0;
  }

  fs::path artifactDirectory;
  if (!ensureArtifactDirectory(target.artifactRoot, artifactDirectory)) {
    std::cerr << "rocketc: could not create artifact directory\n";
    return 1;
  }

#ifdef ROCKETC_HAS_LLVM
  if (command == "emit-asm") {
    const fs::path assemblyPath = artifactDirectory / (target.source.stem().string() + ".s");
    std::string error;
    if (!rocket::emitLlvmFile(*compilation.mir, true, rocket::LlvmFileType::Assembly,
                              assemblyPath, error)) {
      std::cerr << "rocketc: " << error << '\n';
      return 1;
    }
    std::string assembly;
    if (!readFile(assemblyPath, assembly)) return 1;
    std::cout << assembly;
    return 0;
  }

  const fs::path objectPath = artifactDirectory / (target.outputName + ".obj");
  const char* extension = target.outputKind == rocket::PackageOutputKind::Executable
                              ? ".exe"
                          : target.outputKind == rocket::PackageOutputKind::StaticLibrary
                              ? ".lib"
                              : ".dll";
  const fs::path executablePath = artifactDirectory / (target.outputName + extension);
  std::string error;
  if (!rocket::emitLlvmFile(*compilation.mir, true, rocket::LlvmFileType::Object, objectPath,
                            error)) {
    std::cerr << "rocketc: " << error << '\n';
    return 1;
  }
  if (target.outputKind == rocket::PackageOutputKind::StaticLibrary) {
    if (invokeExecutable(llvmLibrarianPath(),
                         {"/OUT:" + executablePath.string(), objectPath.string()}) != 0)
      return 1;
  } else {
    std::vector<std::string> linkArguments{objectPath.string(),
                                           runtimeLibraryPath().string(),
                                           "-fuse-ld=lld",
                                           "-Wl,/Brepro"};
    if (target.outputKind == rocket::PackageOutputKind::DynamicLibrary)
      linkArguments.push_back("-shared");
    for (const auto& search : target.nativeLibrarySearch) {
      linkArguments.push_back("-L");
      linkArguments.push_back(search.string());
    }
    for (const auto& library : target.nativeLibraries) {
      const fs::path libraryPath(library);
      bool resolved = false;
      if (!libraryPath.has_parent_path()) {
        for (const auto& search : target.nativeLibrarySearch) {
          const fs::path candidate = search / libraryPath;
          if (fs::is_regular_file(candidate)) {
            linkArguments.push_back(candidate.string());
            resolved = true;
            break;
          }
        }
      }
      if (!resolved) {
        if (!libraryPath.has_parent_path() && libraryPath.extension() == ".lib")
          linkArguments.push_back("-l" + libraryPath.stem().string());
        else
          linkArguments.push_back(library);
      }
    }
    linkArguments.push_back("-o");
    linkArguments.push_back(executablePath.string());
    if (invokeExecutable(clangDriverPath(), linkArguments) != 0) return 1;
  }
#else
  rocket::BootstrapCodeGenerator generator(*compilation.mir);
  fs::path generatedPath;
  if (!writeGenerated(target.source, artifactDirectory, generator.generate(), generatedPath)) {
    std::cerr << "rocketc: could not write bootstrap backend output\n";
    return 1;
  }
  if (command == "emit-asm") {
    const fs::path assemblyPath = artifactDirectory / (target.source.stem().string() + ".s");
    if (compileBootstrap(generatedPath, assemblyPath, true) != 0) return 1;
    std::string assembly;
    if (!readFile(assemblyPath, assembly)) return 1;
    std::cout << assembly;
    return 0;
  }
  const char* extension = target.outputKind == rocket::PackageOutputKind::Executable
                              ? ".exe"
                          : target.outputKind == rocket::PackageOutputKind::StaticLibrary
                              ? ".lib"
                              : ".dll";
  const fs::path executablePath = artifactDirectory / (target.outputName + extension);
  if (compileBootstrap(generatedPath, executablePath, false, target.outputKind,
                       target.nativeLibrarySearch, target.nativeLibraries) != 0)
    return 1;
#endif
  if (library) {
    std::string header;
    std::string headerError;
    if (!rocket::generateNativeHeader(compilation.module, target.packageName,
                                      header, headerError) ||
        !writeFile(artifactDirectory / (target.outputName + ".h"), header)) {
      cliDiagnostic(rocket::DiagnosticCode::Tooling,
                    headerError.empty() ? "could not write generated native header"
                                        : headerError);
      return 1;
    }
  }
  if (announceBuild) std::cout << "built " << executablePath.string() << '\n';
  if (command == "build") return 0;
  if (library) {
    cliDiagnostic(rocket::DiagnosticCode::Tooling,
                  "run requires an executable package");
    return 2;
  }
  return invokeExecutable(executablePath, programArguments);
}

bool writeFile(const fs::path& path, const std::string& contents) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
  return static_cast<bool>(output);
}

int formatCommand(const fs::path& path, bool checkOnly) {
  std::string error;
  const auto sources = rocket::rocketSources(path, error);
  if (!error.empty()) { cliDiagnostic(rocket::DiagnosticCode::Tooling, error); return 2; }
  if (sources.empty()) {
    cliDiagnostic(rocket::DiagnosticCode::Tooling, "no .rocket files found"); return 2;
  }
  int changed = 0;
  for (const auto& sourcePath : sources) {
    std::string source;
    if (!readFile(sourcePath, source)) {
      cliDiagnostic(rocket::DiagnosticCode::Tooling,
                    "could not read " + sourcePath.string());
      return 1;
    }
    rocket::Diagnostics diagnostics;
    const auto formatted = rocket::formatSource(sourcePath.string(), source, diagnostics);
    if (!formatted) { diagnostics.print(); return 1; }
    if (*formatted == source) continue;
    ++changed;
    if (checkOnly) {
      std::cout << "would reformat " << sourcePath.string() << '\n';
    } else {
      if (!writeFile(sourcePath, *formatted)) {
        cliDiagnostic(rocket::DiagnosticCode::Tooling,
                      "could not write " + sourcePath.string());
        return 1;
      }
      std::cout << "formatted " << sourcePath.string() << '\n';
    }
  }
  if (checkOnly && changed) {
    std::cout << changed << " file(s) need formatting\n";
    return 1;
  }
  std::cout << (checkOnly ? "format check passed" : "formatting complete") << '\n';
  return 0;
}

int testCommand(const fs::path& path, const std::string& filter) {
  std::string error;
  std::vector<fs::path> tests;
  fs::path packageRoot;
  fs::path artifactRoot;
  std::vector<std::string> nativeLibraries;
  std::vector<fs::path> nativeLibrarySearch;
  std::vector<rocket::PackageDependencyRoot> dependencyRoots;
  const fs::path absolute = fs::absolute(path).lexically_normal();
  if (fs::is_regular_file(absolute) && absolute.extension() == ".rocket") {
    tests.push_back(absolute);
    packageRoot = absolute.parent_path();
    artifactRoot = absolute.parent_path();
  } else {
    auto package = rocket::loadPackage(absolute, error);
    if (!package) { cliDiagnostic(rocket::DiagnosticCode::Manifest, error); return 2; }
    tests = rocket::packageTests(*package, error);
    if (!error.empty()) { cliDiagnostic(rocket::DiagnosticCode::Manifest, error); return 2; }
    packageRoot = package->root;
    artifactRoot = package->root;
    nativeLibraries = package->nativeLibraries;
    nativeLibrarySearch = package->nativeLibrarySearch;
    rocket::PackageLock lock;
    if (!rocket::prepareLockedPackageDependencies(
            *package, false, dependencyRoots, lock, error)) {
      cliDiagnostic(rocket::DiagnosticCode::Manifest, error);
      return 2;
    }
  }

  int passed = 0;
  int failed = 0;
  int expectedFailures = 0;
  int selected = 0;
  for (const auto& test : tests) {
    std::error_code relativeError;
    fs::path display = fs::relative(test, packageRoot, relativeError);
    if (relativeError) display = test.filename();
    if (!filter.empty() && display.generic_string().find(filter) == std::string::npos)
      continue;
    ++selected;
    const bool expectedFailure = test.stem().string().ends_with(".xfail");
    std::cout << "test " << display.generic_string() << '\n';
    const CommandTarget target{test, packageRoot, artifactRoot,
                               rocket::PackageOutputKind::Executable,
                               test.stem().string(), test.stem().string(),
                               nativeLibraries, nativeLibrarySearch,
                               dependencyRoots};
    const int buildStatus = executeCompiler("build", target, {}, false);
    if (buildStatus != 0) {
      ++failed;
      std::cout << "FAIL " << display.generic_string()
                << " (build failed with exit " << buildStatus << ")\n";
      continue;
    }
    const int status = invokeExecutable(
        artifactRoot / ".rocketc" / (test.stem().string() + ".exe"), {});
    if (expectedFailure && status != 0) {
      ++expectedFailures;
      std::cout << "XFAIL " << display.generic_string() << " (exit " << status << ")\n";
    } else if (expectedFailure) {
      ++failed;
      std::cout << "XPASS " << display.generic_string() << '\n';
    } else if (status == 0) {
      ++passed;
      std::cout << "PASS " << display.generic_string() << '\n';
    } else {
      ++failed;
      std::cout << "FAIL " << display.generic_string() << " (exit " << status << ")\n";
    }
  }
  if (selected == 0) {
    cliDiagnostic(rocket::DiagnosticCode::Tooling,
                  "test filter selected no .rocket files");
    return 2;
  }
  std::cout << passed << " passed; " << failed << " failed; "
            << expectedFailures << " expected failure(s)\n";
  return failed == 0 ? 0 : 1;
}

rocket::DiagnosticCode packageFailureCode(const std::string& error);

int dependencyCommand(const std::string& command, const fs::path& path,
                      const rocket::ResolveOptions& options = {}) {
  std::string error;
  auto package = rocket::loadPackage(path, error);
  if (!package) {
    cliDiagnostic(rocket::DiagnosticCode::Manifest, error);
    return 2;
  }
  rocket::PackageLock lock;
  if (command == "resolve") {
    if (!rocket::resolvePackageDependencies(*package, options, lock, error)) {
      cliDiagnostic(packageFailureCode(error), error);
      return 1;
    }
    if (options.offline)
      std::cout << "offline resolution verified " << lock.packages.size()
                << " cached package(s)\n";
    else if (options.locked)
      std::cout << "locked resolution verified " << lock.packages.size()
                << " package(s)\n";
    else
      std::cout << "resolved " << lock.packages.size() << " package(s); wrote "
                << (package->root / "rocket.lock").string() << '\n';
    return 0;
  }
  if (!rocket::readPackageLock(package->root / "rocket.lock", lock, error)) {
    cliDiagnostic(packageFailureCode(error), error);
    return 1;
  }
  if (command == "tree") {
    std::cout << rocket::packageDependencyTree(lock);
    return 0;
  }
  std::string report;
  if (!rocket::auditPackageDependencies(*package, lock, report, error)) {
    cliDiagnostic(packageFailureCode(error), error);
    return 1;
  }
  std::cout << report;
  return 0;
}

rocket::DiagnosticCode packageFailureCode(const std::string& error) {
  if (error.find("compromised") != std::string::npos ||
      error.find("yanked") != std::string::npos ||
      error.find("license policy") != std::string::npos ||
      error.find("SPDX") != std::string::npos ||
      error.find("advisory") != std::string::npos)
    return rocket::DiagnosticCode::DependencyAudit;
  if (error.find("signature") != std::string::npos ||
      error.find("checksum") != std::string::npos ||
      error.find("archive") != std::string::npos ||
      error.find("signing-key") != std::string::npos)
    return rocket::DiagnosticCode::PackageIntegrity;
  if (error.find("credential") != std::string::npos ||
      error.find("owner") != std::string::npos ||
      error.find("namespace") != std::string::npos ||
      error.find("reserved") != std::string::npos ||
      error.find("immutable") != std::string::npos ||
      error.find("typosquatting") != std::string::npos)
    return rocket::DiagnosticCode::RegistryAuthorization;
  if (error.find("HTTPS") != std::string::npos ||
      error.find("Git") != std::string::npos ||
      error.find("transport") != std::string::npos ||
      error.find("redirect") != std::string::npos ||
      error.find("timed out") != std::string::npos)
    return rocket::DiagnosticCode::PackageTransport;
  if (error.find("build script") != std::string::npos ||
      error.find("native inputs") != std::string::npos)
    return rocket::DiagnosticCode::PackageCapability;
  return rocket::DiagnosticCode::Manifest;
}

int registryCommand(int argc, char** argv) {
  if (argc < 4) return 2;
  const std::string action = argv[2];
  std::string error;
  if (action == "init") {
    if (argc != 9 || std::string(argv[4]) != "--id" ||
        std::string(argv[6]) != "--owner" ||
        std::string(argv[8]) != "--token-stdin")
      return 2;
    std::string token;
    if (!rocket::platform_credentials::readSecretLine(token, error)) {
      cliDiagnostic(rocket::DiagnosticCode::RegistryAuthorization, error);
      return 1;
    }
    std::string fingerprint;
    const bool initialized = rocket::initializeReferenceRegistry(
        argv[3], argv[5], argv[7], token, fingerprint, error);
    std::fill(token.begin(), token.end(), '\0');
    if (!initialized) {
      cliDiagnostic(packageFailureCode(error), error);
      return 1;
    }
    std::cout << "initialized signed Rocket registry "
              << fs::absolute(argv[3]).lexically_normal().string()
              << "\nregistry-key = \"" << fingerprint << "\"\n";
    return 0;
  }
  if (action == "transfer" && argc == 6) {
    if (!rocket::transferRegistryNamespace(argv[3], argv[4], argv[5], error)) {
      cliDiagnostic(packageFailureCode(error), error); return 1;
    }
    std::cout << "transferred namespace " << argv[4] << " to " << argv[5] << '\n';
    return 0;
  }
  if (action == "yank" && argc == 7 && std::string(argv[5]) == "--reason") {
    if (!rocket::yankRegistryPackage(argv[3], argv[4], argv[6], error)) {
      cliDiagnostic(packageFailureCode(error), error); return 1;
    }
    std::cout << "yanked " << argv[4] << " without deleting its archive\n";
    return 0;
  }
  if (action == "revoke" && argc == 5) {
    if (!rocket::revokeRegistryCredential(argv[3], argv[4], error)) {
      cliDiagnostic(packageFailureCode(error), error); return 1;
    }
    std::cout << "revoked registry credential " << argv[4] << '\n';
    return 0;
  }
  if (action == "advisory" && argc == 5) {
    if (!rocket::publishRegistryAdvisory(argv[3], argv[4], error)) {
      cliDiagnostic(packageFailureCode(error), error); return 1;
    }
    std::cout << "published signed registry advisory\n";
    return 0;
  }
  return 2;
}

void usage() {
  std::cerr
      << "Rocket compiler " ROCKETC_VERSION "\n"
         "usage:\n"
         "  rocketc <check|build|run|emit-ir|emit-asm|emit-header> [file.rocket|package] [-- arguments]\n"
         "  rocketc bind <header.h> [--output bindings.rocket]\n"
         "  rocketc fmt [file.rocket|directory] [--check]\n"
         "  rocketc test [file.rocket|package] [--filter text]\n"
         "  rocketc resolve [package] [--locked|--offline]\n"
         "  rocketc tree [package]\n"
         "  rocketc audit [package]\n"
         "  rocketc doc [package] [--output directory]\n"
         "  rocketc login <registry> --token-stdin\n"
         "  rocketc logout <registry>\n"
         "  rocketc publish [package]\n"
         "  rocketc registry <init|transfer|yank|revoke|advisory> ...\n"
         "  rocketc new <directory> [--name package-name]\n"
         "  rocketc --version\n";
}

} // namespace

int main(int argc, char** argv) {
  compilerDirectory = fs::absolute(argv[0]).parent_path().lexically_normal();
  const fs::path installedStandardLibrary =
      (compilerDirectory.parent_path() / "stdlib").lexically_normal();
  if (fs::is_directory(installedStandardLibrary))
    rocket::setStandardLibraryRoot(installedStandardLibrary);
  if (argc < 2) { usage(); return 2; }
  const std::string command = argv[1];
  if (command == "--help" || command == "-h" || command == "help") {
    usage(); return 0;
  }
  if (command == "--version" || command == "version") {
    std::cout << "rocketc " ROCKETC_VERSION "\n";
    return 0;
  }
  if (command == "new") {
    if (argc < 3) { usage(); return 2; }
    std::string name;
    for (int index = 3; index < argc; ++index) {
      if (std::string(argv[index]) == "--name" && index + 1 < argc) name = argv[++index];
      else {
        cliDiagnostic(rocket::DiagnosticCode::Tooling,
                      "unknown new option '" + std::string(argv[index]) + "'");
        return 2;
      }
    }
    std::string error;
    if (!rocket::createPackage(argv[2], name, error)) {
      cliDiagnostic(rocket::DiagnosticCode::Manifest, error); return 1;
    }
    std::cout << "created Rocket package " << fs::absolute(argv[2]).string() << '\n';
    return 0;
  }
  if (command == "login") {
    if (argc != 4 || std::string(argv[3]) != "--token-stdin") {
      usage(); return 2;
    }
    std::string token;
    std::string error;
    if (!rocket::platform_credentials::readSecretLine(token, error)) {
      cliDiagnostic(rocket::DiagnosticCode::RegistryAuthorization, error);
      return 1;
    }
    const bool loggedIn = rocket::loginRegistry(argv[2], token, error);
    std::fill(token.begin(), token.end(), '\0');
    if (!loggedIn) {
      cliDiagnostic(packageFailureCode(error), error);
      return 1;
    }
    std::cout << "stored scoped registry credential in Windows Credential Manager\n";
    return 0;
  }
  if (command == "--verify-package-lock") {
    if (argc != 3) return 2;
    std::string error;
    auto package = rocket::loadPackage(argv[2], error);
    if (!package) {
      cliDiagnostic(rocket::DiagnosticCode::Manifest, error); return 1;
    }
    rocket::PackageLock lock;
    std::vector<rocket::PackageDependencyRoot> roots;
    if (!rocket::prepareLockedPackageDependencies(*package, true, roots, lock,
                                                  error)) {
      cliDiagnostic(packageFailureCode(error), error); return 1;
    }
    return 0;
  }
  if (command == "logout") {
    if (argc != 3) { usage(); return 2; }
    std::string error;
    if (!rocket::logoutRegistry(argv[2], error)) {
      cliDiagnostic(packageFailureCode(error), error); return 1;
    }
    std::cout << "removed stored registry credential\n";
    return 0;
  }
  if (command == "publish") {
    if (argc > 3) { usage(); return 2; }
    std::string error;
    auto package = rocket::loadPackage(argc == 3 ? fs::path(argv[2]) : fs::path("."),
                                       error);
    if (!package) {
      cliDiagnostic(rocket::DiagnosticCode::Manifest, error); return 1;
    }
    std::string report;
    if (!rocket::publishPackage(*package, report, error)) {
      cliDiagnostic(packageFailureCode(error), error); return 1;
    }
    std::cout << report;
    return 0;
  }
  if (command == "registry") {
    const int status = registryCommand(argc, argv);
    if (status == 2) usage();
    return status;
  }
  if (command == "fmt") {
    fs::path path = ".";
    bool pathSet = false;
    bool checkOnly = false;
    for (int index = 2; index < argc; ++index) {
      if (std::string(argv[index]) == "--check") checkOnly = true;
      else if (!pathSet) { path = argv[index]; pathSet = true; }
      else {
        cliDiagnostic(rocket::DiagnosticCode::Tooling,
                      "unexpected fmt argument '" + std::string(argv[index]) + "'");
        return 2;
      }
    }
    return formatCommand(path, checkOnly);
  }
  if (command == "test") {
    fs::path path = ".";
    bool pathSet = false;
    std::string filter;
    for (int index = 2; index < argc; ++index) {
      const std::string argument = argv[index];
      if (argument == "--filter" && index + 1 < argc) filter = argv[++index];
      else if (!pathSet) { path = argument; pathSet = true; }
      else {
        cliDiagnostic(rocket::DiagnosticCode::Tooling,
                      "unexpected test argument '" + argument + "'");
        return 2;
      }
    }
    return testCommand(path, filter);
  }
  if (command == "resolve") {
    fs::path path = ".";
    bool pathSet = false;
    rocket::ResolveOptions options;
    for (int index = 2; index < argc; ++index) {
      const std::string argument = argv[index];
      if (argument == "--offline") options.offline = true;
      else if (argument == "--locked") options.locked = true;
      else if (!pathSet) { path = argument; pathSet = true; }
      else {
        cliDiagnostic(rocket::DiagnosticCode::Tooling,
                      "unexpected resolve argument '" + argument + "'");
        return 2;
      }
    }
    if (options.offline && options.locked) {
      cliDiagnostic(rocket::DiagnosticCode::Tooling,
                    "resolve accepts either --offline or --locked, not both");
      return 2;
    }
    return dependencyCommand(command, path, options);
  }
  if (command == "tree" || command == "audit") {
    if (argc > 3) {
      cliDiagnostic(rocket::DiagnosticCode::Tooling,
                    command + " accepts only one package path");
      return 2;
    }
    return dependencyCommand(command, argc == 3 ? fs::path(argv[2])
                                                 : fs::path("."));
  }
  if (command == "doc") {
    fs::path packagePath = ".";
    fs::path outputPath;
    bool pathSet = false;
    for (int index = 2; index < argc; ++index) {
      const std::string argument = argv[index];
      if (argument == "--output" && index + 1 < argc)
        outputPath = argv[++index];
      else if (!pathSet) { packagePath = argument; pathSet = true; }
      else { usage(); return 2; }
    }
    std::string error;
    auto package = rocket::loadPackage(packagePath, error);
    if (!package) {
      cliDiagnostic(rocket::DiagnosticCode::Manifest, error); return 1;
    }
    if (outputPath.empty()) outputPath = package->root / ".rocketc/docs";
    std::string report;
    if (!rocket::generatePackageDocumentation(*package, outputPath, report,
                                               error)) {
      cliDiagnostic(packageFailureCode(error), error); return 1;
    }
    std::cout << report;
    return 0;
  }
  if (command == "bind") {
    if (argc != 3 && argc != 5) { usage(); return 2; }
    fs::path outputPath;
    if (argc == 5) {
      if (std::string(argv[3]) != "--output") {
        cliDiagnostic(rocket::DiagnosticCode::Tooling,
                      "bind accepts only --output <bindings.rocket>");
        return 2;
      }
      outputPath = argv[4];
    }
    std::string header;
    if (!readFile(argv[2], header)) {
      cliDiagnostic(rocket::DiagnosticCode::Tooling,
                    "could not read native header '" + std::string(argv[2]) + "'");
      return 1;
    }
    std::string bindings;
    std::string bindError;
    if (!rocket::generateRocketBindings(header, bindings, bindError)) {
      cliDiagnostic(rocket::DiagnosticCode::Tooling, bindError);
      return 1;
    }
    if (outputPath.empty()) std::cout << bindings;
    else if (!writeFile(outputPath, bindings)) {
      cliDiagnostic(rocket::DiagnosticCode::Tooling,
                    "could not write generated bindings");
      return 1;
    }
    return 0;
  }
  if (command != "check" && command != "build" && command != "run" &&
      command != "emit-ir" && command != "emit-asm" &&
      command != "emit-header") {
    usage(); return 2;
  }

  const fs::path input = argc >= 3 ? fs::path(argv[2]) : fs::path(".");
  std::string error;
  auto target = resolveTarget(input, error);
  if (!target) {
    const fs::path absolute = fs::absolute(input).lexically_normal();
    const auto classified = packageFailureCode(error);
    const auto code = classified != rocket::DiagnosticCode::Manifest
                          ? classified
                          : fs::is_directory(absolute) ||
                                    absolute.filename() == "rocket.toml"
                                ? rocket::DiagnosticCode::Manifest
                                : rocket::DiagnosticCode::Tooling;
    cliDiagnostic(code, error); return 2;
  }
  std::vector<std::string> programArguments;
  fs::path headerOutput;
  bool forwarding = false;
  for (int index = 3; index < argc; ++index) {
    if (command == "emit-header" && index == 3 &&
        std::string(argv[index]) == "--output" && index + 1 < argc) {
      headerOutput = argv[++index];
      continue;
    }
    if (!forwarding && std::string(argv[index]) == "--") { forwarding = true; continue; }
    if (forwarding) programArguments.emplace_back(argv[index]);
    else {
      cliDiagnostic(rocket::DiagnosticCode::Tooling,
                    "unexpected argument '" + std::string(argv[index]) + "'");
      return 2;
    }
  }
  return executeCompiler(command, *target, programArguments, true, headerOutput);
}
