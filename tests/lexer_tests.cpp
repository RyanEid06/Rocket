#include "lexer.h"
#include "test_support.h"

#include <string>

int main() {
  int failures = 0;
  rocket::Diagnostics diagnostics;
  rocket::Lexer lexer("test.rocket", "fn main() -> Int:\n    return 0\n", diagnostics);
  const auto tokens = lexer.lex();

  rocket::test::expect(!diagnostics.hasErrors(), "valid source lexes", failures);
  rocket::test::expect(tokens.size() == 14, "lexer emits expected token count", failures);
  rocket::test::expect(tokens[0].kind == rocket::TokenKind::KwFn, "function keyword is recognized", failures);
  rocket::test::expect(tokens[8].kind == rocket::TokenKind::Indent, "indentation emits an indent token", failures);

  const std::string lfSource = "fn main() -> Int:\n\n    return 0\n";
  const std::string crlfSource = "fn main() -> Int:\r\n\r\n    return 0\r\n";
  rocket::Diagnostics lfDiagnostics;
  rocket::Diagnostics crlfDiagnostics;
  const auto lfTokens = rocket::Lexer("lf.rocket", lfSource, lfDiagnostics).lex();
  const auto crlfTokens =
      rocket::Lexer("crlf.rocket", crlfSource, crlfDiagnostics).lex();
  bool equivalentNewlines = !lfDiagnostics.hasErrors() &&
                            !crlfDiagnostics.hasErrors() &&
                            lfTokens.size() == crlfTokens.size();
  if (equivalentNewlines) {
    for (std::size_t index = 0; index < lfTokens.size(); ++index) {
      equivalentNewlines = equivalentNewlines &&
                           lfTokens[index].kind == crlfTokens[index].kind &&
                           lfTokens[index].text == crlfTokens[index].text &&
                           lfTokens[index].location.line == crlfTokens[index].location.line &&
                           lfTokens[index].location.column ==
                               crlfTokens[index].location.column;
    }
  }
  rocket::test::expect(equivalentNewlines,
                       "CRLF source, including blank lines, lexes like LF source",
                       failures);

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

  rocket::Diagnostics carriageReturnDiagnostics;
  rocket::Lexer carriageReturnLexer("test.rocket", "let c = '\\r'\nlet s = \"line\\r\"\n",
                                     carriageReturnDiagnostics);
  const auto carriageReturnTokens = carriageReturnLexer.lex();
  rocket::test::expect(!carriageReturnDiagnostics.hasErrors() &&
                           carriageReturnTokens[3].kind == rocket::TokenKind::Character &&
                           carriageReturnTokens[3].text == "\r" &&
                           carriageReturnTokens[8].kind == rocket::TokenKind::String &&
                           carriageReturnTokens[8].text == "line\r",
                       "carriage-return escapes decode in Char and String literals", failures);

  rocket::Diagnostics collectionDiagnostics;
  rocket::Lexer collectionLexer("test.rocket", "let values = [1, 2]\n",
                                 collectionDiagnostics);
  const auto collectionTokens = collectionLexer.lex();
  rocket::test::expect(!collectionDiagnostics.hasErrors(),
                       "Array delimiters lex", failures);
  rocket::test::expect(collectionTokens[3].kind == rocket::TokenKind::LBracket &&
                           collectionTokens[7].kind == rocket::TokenKind::RBracket,
                       "square brackets have dedicated tokens", failures);

  rocket::Diagnostics methodDiagnostics;
  const auto methodTokens = rocket::Lexer(
      "test.rocket", "impl Counter:\n    fn read(self: Counter) -> Int:\n        return 0\n",
      methodDiagnostics).lex();
  rocket::test::expect(!methodDiagnostics.hasErrors() &&
                           methodTokens[0].kind == rocket::TokenKind::KwImpl,
                       "impl is a dedicated Phase 12 keyword", failures);

  rocket::Diagnostics contextualDiagnostics;
  const auto contextualTokens = rocket::Lexer(
      "test.rocket", "fn use(callback: Int, unsafe: Int) -> Int:\n    return callback + unsafe\n",
      contextualDiagnostics).lex();
  rocket::test::expect(!contextualDiagnostics.hasErrors() &&
                           contextualTokens[3].kind == rocket::TokenKind::Identifier &&
                           contextualTokens[7].kind == rocket::TokenKind::Identifier,
                       "Phase 13 spellings remain contextual and valid identifiers", failures);

  rocket::Diagnostics badIndentation;
  rocket::Lexer badLexer("test.rocket", "fn main() -> Int:\n   return 0\n", badIndentation);
  badLexer.lex();
  rocket::test::expect(badIndentation.hasErrors(), "non-four-space indentation is rejected", failures);
  return rocket::test::finish(failures, "lexer");
}
