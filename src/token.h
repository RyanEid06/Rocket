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
  Identifier, Integer, Float, Character, String,
  KwFn, KwLet, KwVar, KwIf, KwElse, KwWhile, KwFor, KwIn, KwBreak, KwContinue,
  KwReturn, KwTrue, KwFalse, KwAnd, KwOr, KwNot,
  LParen, RParen, LBracket, RBracket, Colon, Comma, Arrow,
  Plus, Minus, Star, Slash, DotDot,
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
