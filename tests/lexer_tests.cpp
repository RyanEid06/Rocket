#include "lexer.h"
#include "test_support.h"

int main() {
  int failures = 0;
  rocket::Diagnostics diagnostics;
  rocket::Lexer lexer("test.rocket", "fn main() -> Int:\n    return 0\n", diagnostics);
  const auto tokens = lexer.lex();

  rocket::test::expect(!diagnostics.hasErrors(), "valid source lexes", failures);
  rocket::test::expect(tokens.size() == 14, "lexer emits expected token count", failures);
  rocket::test::expect(tokens[0].kind == rocket::TokenKind::KwFn, "function keyword is recognized", failures);
  rocket::test::expect(tokens[8].kind == rocket::TokenKind::Indent, "indentation emits an indent token", failures);

  rocket::Diagnostics scalarDiagnostics;
  rocket::Lexer scalarLexer("test.rocket", "for index in 0..2:\n    print(1.5)\n    print('x')\n", scalarDiagnostics);
  const auto scalarTokens = scalarLexer.lex();
  rocket::test::expect(!scalarDiagnostics.hasErrors(), "ranges and scalar literals lex", failures);
  rocket::test::expect(scalarTokens[0].kind == rocket::TokenKind::KwFor, "for keyword is recognized", failures);
  rocket::test::expect(scalarTokens[4].kind == rocket::TokenKind::DotDot, "range delimiter is recognized", failures);
  rocket::test::expect(scalarTokens[11].kind == rocket::TokenKind::Float, "float literals are recognized", failures);
  rocket::test::expect(scalarTokens[16].kind == rocket::TokenKind::Character, "character literals are recognized", failures);

  rocket::Diagnostics logicalDiagnostics;
  rocket::Lexer logicalLexer("test.rocket", "true and not false or true\n", logicalDiagnostics);
  const auto logicalTokens = logicalLexer.lex();
  rocket::test::expect(!logicalDiagnostics.hasErrors(), "logical expressions lex", failures);
  rocket::test::expect(logicalTokens[1].kind == rocket::TokenKind::KwAnd, "and keyword is recognized", failures);
  rocket::test::expect(logicalTokens[2].kind == rocket::TokenKind::KwNot, "not keyword is recognized", failures);
  rocket::test::expect(logicalTokens[4].kind == rocket::TokenKind::KwOr, "or keyword is recognized", failures);

  rocket::Diagnostics badIndentation;
  rocket::Lexer badLexer("test.rocket", "fn main() -> Int:\n   return 0\n", badIndentation);
  badLexer.lex();
  rocket::test::expect(badIndentation.hasErrors(), "non-four-space indentation is rejected", failures);
  return rocket::test::finish(failures, "lexer");
}
