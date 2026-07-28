#include "codegen.h"
#include "lexer.h"
#include "mir.h"
#include "parser.h"
#include "sema.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

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
  std::string source;
  if (!readFile(path, source)) {
    result.diagnostics.error({path.string(), 1, 1}, "could not read source file");
    return result;
  }
  rocket::Lexer lexer(path.string(), std::move(source), result.diagnostics);
  auto tokens = lexer.lex();
  rocket::Parser parser(tokens, result.diagnostics);
  result.module = parser.parseModule();
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

int invoke(const std::string& command) {
  std::cerr << "+ " << command << '\n';
  return std::system(command.c_str());
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
#ifdef ROCKETC_LLVM_DISCOVERED
    std::cerr << "rocketc: LLVM was discovered, but IR lowering is scheduled for Phase 4\n";
#else
    std::cerr << "rocketc: LLVM is not installed; emit-ir requires the Phase 4 LLVM backend\n";
#endif
    return 2;
  }
  if (command != "build" && command != "run" && command != "emit-asm") {
    usage(); return 2;
  }

  rocket::BootstrapCodeGenerator generator(*compilation.mir);
  fs::path generatedPath;
  if (!writeGenerated(sourcePath, generator.generate(), generatedPath)) {
    std::cerr << "rocketc: could not write bootstrap backend output\n";
    return 1;
  }
  const fs::path artifactDirectory = sourcePath.parent_path() / ".rocketc";
  if (command == "emit-asm") {
    const fs::path assemblyPath = artifactDirectory / (sourcePath.stem().string() + ".s");
    const std::string compile = "g++ -std=c++20 -O2 -S -masm=intel " + quote(generatedPath) + " -o " + quote(assemblyPath);
    if (invoke(compile) != 0) return 1;
    std::string assembly;
    if (!readFile(assemblyPath, assembly)) return 1;
    std::cout << assembly;
    return 0;
  }

  const fs::path executablePath = artifactDirectory / (sourcePath.stem().string() + ".exe");
  const std::string compile = "g++ -std=c++20 -O2 " + quote(generatedPath) + " -o " + quote(executablePath);
  if (invoke(compile) != 0) return 1;
  std::cout << "built " << executablePath.string() << '\n';
  if (command == "build") return 0;

  std::string runCommand = quote(executablePath);
  bool forwarding = false;
  for (int i = 3; i < argc; ++i) {
    if (!forwarding && std::string(argv[i]) == "--") { forwarding = true; continue; }
    if (forwarding) runCommand += " \"" + std::string(argv[i]) + "\"";
  }
  return invoke(runCommand);
}
