#include "sema.h"
#include "test_support.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

std::string readFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
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
  return rocket::test::finish(failures, "diagnostics");
}
