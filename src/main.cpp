#include "codegen.h"
#include "lexer.h"
#include "mir.h"
#include "module_loader.h"
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

std::string quote(const fs::path& path) { return "\"" + path.string() + "\""; }

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

Compilation compileFrontend(const fs::path& path) {
  Compilation result;
  auto loaded = rocket::loadModuleGraph(path, result.diagnostics);
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
                               "internal MIR verification failed: " + verifierError);
      result.mir.reset();
    }
  }
  return result;
}

bool writeGenerated(const fs::path& sourcePath, const std::string& code,
                    fs::path& generatedPath) {
  const fs::path directory = sourcePath.parent_path() / ".rocketc";
  std::error_code error;
  fs::create_directories(directory, error);
  if (error) return false;
  generatedPath = directory / (sourcePath.stem().string() + ".bootstrap.cpp");
  std::ofstream output(generatedPath, std::ios::binary);
  output << code;
  return static_cast<bool>(output);
}

bool ensureArtifactDirectory(const fs::path& sourcePath, fs::path& directory) {
  directory = sourcePath.parent_path() / ".rocketc";
  std::error_code error;
  fs::create_directories(directory, error);
  return !error;
}

int invokeShell(const std::string& command) {
  std::cerr << "+ " << command << '\n';
  return std::system(command.c_str());
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

void usage() {
  std::cerr << "usage: rocketc <check|build|run|emit-ir|emit-asm> <file.rocket> [-- program arguments]\n";
}

} // namespace

int main(int argc, char** argv) {
  if (argc < 3) { usage(); return 2; }
  const std::string command = argv[1];
  const fs::path sourcePath = fs::absolute(argv[2]);
  if (sourcePath.extension() != ".rocket") {
    std::cerr << "rocketc: source files must use the .rocket extension\n";
    return 2;
  }

  Compilation compilation = compileFrontend(sourcePath);
  if (compilation.diagnostics.hasErrors()) {
    compilation.diagnostics.print();
    return 1;
  }
  if (command == "check") {
    std::cout << sourcePath.string() << ": check succeeded\n";
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
  if (command != "build" && command != "run" && command != "emit-asm") {
    usage(); return 2;
  }

  fs::path artifactDirectory;
  if (!ensureArtifactDirectory(sourcePath, artifactDirectory)) {
    std::cerr << "rocketc: could not create artifact directory\n";
    return 1;
  }

#ifdef ROCKETC_HAS_LLVM
  if (command == "emit-asm") {
    const fs::path assemblyPath = artifactDirectory / (sourcePath.stem().string() + ".s");
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

  const fs::path objectPath = artifactDirectory / (sourcePath.stem().string() + ".obj");
  const fs::path executablePath = artifactDirectory / (sourcePath.stem().string() + ".exe");
  std::string error;
  if (!rocket::emitLlvmFile(*compilation.mir, true, rocket::LlvmFileType::Object, objectPath,
                            error)) {
    std::cerr << "rocketc: " << error << '\n';
    return 1;
  }
  if (invokeExecutable(fs::path(ROCKETC_CLANG_PATH),
                       {objectPath.string(), ROCKETC_RUNTIME_LIBRARY_PATH,
                        "-o", executablePath.string()}) != 0)
    return 1;
#else
  rocket::BootstrapCodeGenerator generator(*compilation.mir);
  fs::path generatedPath;
  if (!writeGenerated(sourcePath, generator.generate(), generatedPath)) {
    std::cerr << "rocketc: could not write bootstrap backend output\n";
    return 1;
  }
  if (command == "emit-asm") {
    const fs::path assemblyPath = artifactDirectory / (sourcePath.stem().string() + ".s");
    const std::string compile = "g++ -std=c++20 -O2 -S -masm=intel " +
                                quote(generatedPath) + " -o " + quote(assemblyPath);
    if (invokeShell(compile) != 0) return 1;
    std::string assembly;
    if (!readFile(assemblyPath, assembly)) return 1;
    std::cout << assembly;
    return 0;
  }
  const fs::path executablePath = artifactDirectory / (sourcePath.stem().string() + ".exe");
  const std::string compile = "g++ -std=c++20 -O2 " + quote(generatedPath) + " -o " +
                              quote(executablePath);
  if (invokeShell(compile) != 0) return 1;
#endif
  std::cout << "built " << executablePath.string() << '\n';
  if (command == "build") return 0;

  std::vector<std::string> programArguments;
  bool forwarding = false;
  for (int i = 3; i < argc; ++i) {
    if (!forwarding && std::string(argv[i]) == "--") { forwarding = true; continue; }
    if (forwarding) programArguments.emplace_back(argv[i]);
  }
  return invokeExecutable(executablePath, programArguments);
}
