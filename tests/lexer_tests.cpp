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

  rocket::Diagnostics badIndentation;
  rocket::Lexer badLexer("test.rocket", "fn main() -> Int:\n   return 0\n", badIndentation);
  badLexer.lex();
  rocket::test::expect(badIndentation.hasErrors(), "non-four-space indentation is rejected", failures);
  return rocket::test::finish(failures, "lexer");
}
