#pragma once

#include "lexer.h"
#include "parser.h"

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

inline int finish(int failures, const char* suite) {
  if (failures == 0) std::cout << suite << " tests passed\n";
  return failures == 0 ? 0 : 1;
}

} // namespace rocket::test
