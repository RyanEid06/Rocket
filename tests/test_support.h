#pragma once

#include "lexer.h"
#include "mir.h"
#include "parser.h"
#include "sema.h"

#include <iostream>
#include <string>

namespace rocket::test {

inline void expect(bool condition, const std::string& message, int& failures) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

inline Module parse(const std::string& source, Diagnostics& diagnostics) {
  Lexer lexer("test.rocket", source, diagnostics);
  auto tokens = lexer.lex();
  Parser parser(tokens, diagnostics);
  return parser.parseModule();
}

inline std::optional<HirModule> lowerToHir(const std::string& source,
                                           Diagnostics& diagnostics) {
  Module module = parse(source, diagnostics);
  if (diagnostics.hasErrors()) return std::nullopt;
  SemanticAnalyzer analyzer(module, diagnostics);
  return analyzer.analyzeToHir();
}

inline std::optional<MirModule> lowerToMir(const std::string& source,
                                           Diagnostics& diagnostics) {
  auto hir = lowerToHir(source, diagnostics);
  if (!hir.has_value()) return std::nullopt;
  return MirLowerer(*hir).lower();
}

inline int finish(int failures, const char* suite) {
  if (failures == 0) std::cout << suite << " tests passed\n";
  return failures == 0 ? 0 : 1;
}

} // namespace rocket::test
