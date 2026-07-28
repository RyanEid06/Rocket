#pragma once

#include "diagnostic.h"
#include "token.h"

#include <string>
#include <vector>

namespace rocket {

class Lexer {
public:
  Lexer(std::string file, std::string source, Diagnostics& diagnostics)
      : file_(std::move(file)), source_(std::move(source)), diagnostics_(diagnostics) {}

  std::vector<Token> lex();

private:
  void emit(TokenKind kind, std::string text, int line, int column);
  void scanLine(const std::string& lineText, int lineNumber, std::size_t start);

  std::string file_;
  std::string source_;
  Diagnostics& diagnostics_;
  std::vector<Token> tokens_;
  std::vector<int> indentStack_{0};
};

} // namespace rocket
