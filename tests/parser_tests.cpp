#include "test_support.h"

int main() {
  int failures = 0;
  rocket::Diagnostics diagnostics;
  auto module = rocket::test::parse(
      "fn main() -> Int:\n"
      "    if true:\n"
      "        print(42)\n"
      "    return 0\n", diagnostics);

  rocket::test::expect(!diagnostics.hasErrors(), "valid indented program parses", failures);
  rocket::test::expect(module.functions.size() == 1, "one function is parsed", failures);
  rocket::test::expect(module.functions[0].body.size() == 2,
                       "nested block returns to function indentation", failures);

  rocket::Diagnostics loopDiagnostics;
  auto loopModule = rocket::test::parse(
      "fn main() -> Int:\n"
      "    var total = 0\n"
      "    for index in 0..3:\n"
      "        total = total + index\n"
      "    return total\n", loopDiagnostics);
  rocket::test::expect(!loopDiagnostics.hasErrors(), "assignment and for-range parse", failures);
  rocket::test::expect(loopModule.functions[0].body[1]->kind == rocket::StmtKind::For,
                       "for statement is represented in the AST", failures);
  rocket::test::expect(loopModule.functions[0].body[2]->kind == rocket::StmtKind::Return,
                       "parser returns to the enclosing block after a for loop", failures);

  rocket::Diagnostics recoveryDiagnostics;
  auto recoveryModule = rocket::test::parse(
      "fn broken() -> Int:\n"
      "    let value =\n"
      "    return 0\n"
      "fn main() -> Int:\n"
      "    return 0\n", recoveryDiagnostics);
  rocket::test::expect(recoveryDiagnostics.hasErrors(), "malformed statements report errors", failures);
  rocket::test::expect(recoveryModule.functions.size() == 2,
                       "parser recovers to the next top-level function", failures);

  rocket::Diagnostics logicalDiagnostics;
  auto logicalModule = rocket::test::parse(
      "fn main() -> Int:\n"
      "    if true or false and not false:\n"
      "        return 0\n"
      "    else:\n"
      "        return 1\n", logicalDiagnostics);
  rocket::test::expect(!logicalDiagnostics.hasErrors(), "logical operators parse with precedence", failures);
  const auto& condition = static_cast<const rocket::IfStmt&>(*logicalModule.functions[0].body[0]).condition;
  rocket::test::expect(condition->kind == rocket::ExprKind::Binary,
                       "logical expression produces a binary AST", failures);
  const auto& logicalRoot = static_cast<const rocket::BinaryExpr&>(*condition);
  rocket::test::expect(logicalRoot.op == rocket::TokenKind::KwOr,
                       "or has lower precedence than and", failures);
  return rocket::test::finish(failures, "parser");
}
