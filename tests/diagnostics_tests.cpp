#include "sema.h"
#include "test_support.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

std::string readFile(const std::filesystem::path& path) {
  std::ifstream input(path);
  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

} // namespace

int main() {
  int failures = 0;
  const auto fixtureDirectory = std::filesystem::path(__FILE__).parent_path() / "fixtures";
  const auto source = readFile(fixtureDirectory / "immutable_assignment.rocket");
  const auto expected = readFile(fixtureDirectory / "immutable_assignment.expected");

  rocket::Diagnostics diagnostics;
  rocket::Lexer lexer("immutable_assignment.rocket", source, diagnostics);
  const auto tokens = lexer.lex();
  rocket::Parser parser(tokens, diagnostics);
  auto module = parser.parseModule();
  rocket::SemanticAnalyzer analyzer(module, diagnostics);
  analyzer.analyze();

  std::ostringstream actual;
  diagnostics.print(actual);
  rocket::test::expect(actual.str() == expected,
                       "immutable assignment diagnostic matches the golden file", failures);
  rocket::test::expect(!diagnostics.all().empty() &&
                           diagnostics.all().front().code == rocket::DiagnosticCode::Type,
                       "type diagnostics use the stable R4001 category", failures);

  rocket::Diagnostics lexicalDiagnostics;
  rocket::Lexer lexicalLexer("lexical.rocket", "fn main() -> Int:\n\treturn 0\n",
                             lexicalDiagnostics);
  lexicalLexer.lex();
  rocket::test::expect(lexicalDiagnostics.hasErrors() &&
                           lexicalDiagnostics.all().front().code ==
                               rocket::DiagnosticCode::Indentation,
                       "indentation diagnostics use R1002", failures);

  rocket::Diagnostics syntaxDiagnostics;
  rocket::test::parse("fn main() -> Int\n    return 0\n", syntaxDiagnostics);
  rocket::test::expect(syntaxDiagnostics.hasErrors() &&
                           syntaxDiagnostics.all().front().code ==
                               rocket::DiagnosticCode::Syntax,
                       "parser diagnostics use R2001", failures);

  rocket::Diagnostics nameDiagnostics;
  rocket::test::lowerToHir("fn main() -> Int:\n    return missing\n", nameDiagnostics);
  rocket::test::expect(nameDiagnostics.hasErrors() &&
                           nameDiagnostics.all().front().code ==
                               rocket::DiagnosticCode::Name,
                       "undefined-name diagnostics use R4002", failures);

  rocket::Diagnostics controlDiagnostics;
  rocket::test::lowerToHir("fn main() -> Int:\n    break\n    return 0\n",
                           controlDiagnostics);
  rocket::test::expect(controlDiagnostics.hasErrors() &&
                           controlDiagnostics.all().front().code ==
                               rocket::DiagnosticCode::ControlFlow,
                       "control-flow diagnostics use R4003", failures);

  rocket::Diagnostics matchDiagnostics;
  rocket::test::lowerToHir(
      "enum Choice:\n"
      "    First\n"
      "    Second\n"
      "fn main() -> Int:\n"
      "    let choice = First()\n"
      "    match choice:\n"
      "        case First:\n"
      "            return 0\n",
      matchDiagnostics);
  bool foundMatchCode = false;
  for (const auto& diagnostic : matchDiagnostics.all())
    foundMatchCode = foundMatchCode || diagnostic.code == rocket::DiagnosticCode::PatternMatch;
  rocket::test::expect(foundMatchCode, "pattern diagnostics use R4004", failures);

  rocket::Diagnostics arityDiagnostics;
  rocket::test::lowerToHir(
      "fn take(value: Int) -> Int:\n"
      "    return value\n"
      "fn main() -> Int:\n"
      "    return take()\n",
      arityDiagnostics);
  bool foundArityCode = false;
  for (const auto& diagnostic : arityDiagnostics.all())
    foundArityCode = foundArityCode || diagnostic.code == rocket::DiagnosticCode::Arity;
  rocket::test::expect(foundArityCode, "call-arity diagnostics use R4005", failures);

  rocket::test::expect(
      rocket::diagnosticCodeName(rocket::DiagnosticCode::Lexical) == "R1001" &&
          rocket::diagnosticCodeName(rocket::DiagnosticCode::ModuleNotFound) == "R3001" &&
          rocket::diagnosticCodeName(rocket::DiagnosticCode::Manifest) == "R5001" &&
          rocket::diagnosticCodeName(rocket::DiagnosticCode::Internal) == "R9001",
      "diagnostic catalog names remain four-digit stable identifiers", failures);
  return rocket::test::finish(failures, "diagnostics");
}
