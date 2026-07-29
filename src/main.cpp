#include "codegen.h"
#include "formatter.h"
#include "lexer.h"
#include "mir.h"
#include "module_loader.h"
#include "package.h"
#include "parser.h"
#include "sema.h"
#ifdef ROCKETC_HAS_LLVM
#include "llvm_codegen.h"
#endif

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

Compilation compileFrontend(const fs::path& path, const fs::path& packageRoot) {
  Compilation result;
  auto loaded = rocket::loadModuleGraph(path, packageRoot, result.diagnostics);
  if (loaded.has_value()) result.module = std::move(*loaded);
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
                     bool assembly) {
  std::vector<std::string> arguments;
#ifdef _MSC_VER
  arguments = {"/nologo", "/std:c++20", "/O2", "/EHsc", "/utf-8",
               "/I" + fs::path(ROCKETC_SOURCE_INCLUDE_PATH).string()};
  if (assembly) {
    fs::path objectPath = output;
    objectPath.replace_extension(".obj");
    arguments.push_back("/FA");
    arguments.push_back("/Fa" + output.string());
    arguments.push_back("/Fo" + objectPath.string());
    arguments.push_back("/c");
  } else {
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
  return invokeExecutable(fs::path(ROCKETC_STAGE0_CXX_PATH), arguments);
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
#endif

struct CommandTarget {
  fs::path source;
  fs::path packageRoot;
  fs::path artifactRoot;
};

std::optional<CommandTarget> resolveTarget(const fs::path& supplied, std::string& error) {
  const fs::path absolute = fs::absolute(supplied).lexically_normal();
  if (fs::is_directory(absolute) || absolute.filename() == "rocket.toml") {
    auto package = rocket::loadPackage(absolute, error);
    if (!package) return {};
    return CommandTarget{package->entry, package->root, package->root};
  }
  if (!fs::is_regular_file(absolute)) {
    error = "source path does not exist: '" + absolute.string() + "'";
    return {};
  }
  if (absolute.extension() != ".rocket") {
    error = "source files must use the .rocket extension";
    return {};
  }
  return CommandTarget{absolute, absolute.parent_path(), absolute.parent_path()};
}

int executeCompiler(const std::string& command, const CommandTarget& target,
                    const std::vector<std::string>& programArguments,
                    bool announceBuild = true) {
  Compilation compilation = compileFrontend(target.source, target.packageRoot);
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

  const fs::path objectPath = artifactDirectory / (target.source.stem().string() + ".obj");
  const fs::path executablePath = artifactDirectory / (target.source.stem().string() + ".exe");
  std::string error;
  if (!rocket::emitLlvmFile(*compilation.mir, true, rocket::LlvmFileType::Object, objectPath,
                            error)) {
    std::cerr << "rocketc: " << error << '\n';
    return 1;
  }
  if (invokeExecutable(clangDriverPath(),
                       {objectPath.string(), runtimeLibraryPath().string(),
                        "-fuse-ld=lld", "-o", executablePath.string()}) != 0)
    return 1;
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
  const fs::path executablePath = artifactDirectory / (target.source.stem().string() + ".exe");
  if (compileBootstrap(generatedPath, executablePath, false) != 0) return 1;
#endif
  if (announceBuild) std::cout << "built " << executablePath.string() << '\n';
  if (command == "build") return 0;
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

int testCommand(const fs::path& path) {
  std::string error;
  std::vector<fs::path> tests;
  fs::path packageRoot;
  fs::path artifactRoot;
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
  }

  int passed = 0;
  int failed = 0;
  for (const auto& test : tests) {
    std::error_code relativeError;
    fs::path display = fs::relative(test, packageRoot, relativeError);
    if (relativeError) display = test.filename();
    std::cout << "test " << display.generic_string() << '\n';
    const int status = executeCompiler("run", {test, packageRoot, artifactRoot}, {}, false);
    if (status == 0) { ++passed; std::cout << "PASS " << display.generic_string() << '\n'; }
    else { ++failed; std::cout << "FAIL " << display.generic_string() << " (exit " << status << ")\n"; }
  }
  std::cout << passed << " passed; " << failed << " failed\n";
  return failed == 0 ? 0 : 1;
}

void usage() {
  std::cerr
      << "Rocket compiler " ROCKETC_VERSION "\n"
         "usage:\n"
         "  rocketc <check|build|run|emit-ir|emit-asm> [file.rocket|package] [-- arguments]\n"
         "  rocketc fmt [file.rocket|directory] [--check]\n"
         "  rocketc test [file.rocket|package]\n"
         "  rocketc new <directory> [--name package-name]\n"
         "  rocketc --version\n";
}

} // namespace

int main(int argc, char** argv) {
  compilerDirectory = fs::absolute(argv[0]).parent_path().lexically_normal();
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
    if (argc > 3) {
      cliDiagnostic(rocket::DiagnosticCode::Tooling,
                    "test accepts only one file or package path");
      return 2;
    }
    return testCommand(argc >= 3 ? fs::path(argv[2]) : fs::path("."));
  }
  if (command != "check" && command != "build" && command != "run" &&
      command != "emit-ir" && command != "emit-asm") {
    usage(); return 2;
  }

  const fs::path input = argc >= 3 ? fs::path(argv[2]) : fs::path(".");
  std::string error;
  auto target = resolveTarget(input, error);
  if (!target) {
    const fs::path absolute = fs::absolute(input).lexically_normal();
    const auto code = fs::is_directory(absolute) || absolute.filename() == "rocket.toml"
                          ? rocket::DiagnosticCode::Manifest
                          : rocket::DiagnosticCode::Tooling;
    cliDiagnostic(code, error); return 2;
  }
  std::vector<std::string> programArguments;
  bool forwarding = false;
  for (int index = 3; index < argc; ++index) {
    if (!forwarding && std::string(argv[index]) == "--") { forwarding = true; continue; }
    if (forwarding) programArguments.emplace_back(argv[index]);
    else {
      cliDiagnostic(rocket::DiagnosticCode::Tooling,
                    "unexpected argument '" + std::string(argv[index]) + "'");
      return 2;
    }
  }
  return executeCompiler(command, *target, programArguments);
}
