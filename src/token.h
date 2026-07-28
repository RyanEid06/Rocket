#pragma once

#include <string>

namespace rocket {

struct Location {
  std::string file;
  int line = 1;
  int column = 1;
};

enum class TokenKind {
  End, Newline, Indent, Dedent,
  Identifier, Integer, String,
  KwFn, KwLet, KwVar, KwIf, KwElse, KwWhile, KwReturn, KwTrue, KwFalse,
  LParen, RParen, Colon, Comma, Arrow,
  Plus, Minus, Star, Slash,
  Equal, EqualEqual, BangEqual,
  Less, LessEqual, Greater, GreaterEqual
};

struct Token {
  TokenKind kind = TokenKind::End;
  std::string text;
  Location location;
};

const char* tokenName(TokenKind kind);

} // namespace rocket
