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

  rocket::Diagnostics collectionDiagnostics;
  auto collectionModule = rocket::test::parse(
      "fn head(values: Slice[Int]) -> Int:\n"
      "    return values[0]\n"
      "fn main() -> Int:\n"
      "    let values = [10, 20, 30]\n"
      "    let middle = values[1..3]\n"
      "    return head(middle)\n",
      collectionDiagnostics);
  rocket::test::expect(!collectionDiagnostics.hasErrors(),
                       "collection types, literals, indexing, and slicing parse", failures);
  rocket::test::expect(collectionModule.functions[0].parameters[0].typeName == "Slice[Int]",
                       "built-in collection type arguments are preserved", failures);
  const auto& arrayBinding = static_cast<const rocket::BindingStmt&>(
      *collectionModule.functions[1].body[0]);
  const auto& sliceBinding = static_cast<const rocket::BindingStmt&>(
      *collectionModule.functions[1].body[1]);
  rocket::test::expect(arrayBinding.initializer->kind == rocket::ExprKind::Array &&
                           sliceBinding.initializer->kind == rocket::ExprKind::Slice,
                        "aggregate postfix syntax has explicit AST nodes", failures);

  rocket::Diagnostics mutationDiagnostics;
  auto mutationModule = rocket::test::parse(
      "fn main() -> Int:\n"
      "    var values = [1, 2]\n"
      "    values[0] = 3\n"
      "    return values[0]\n",
      mutationDiagnostics);
  rocket::test::expect(!mutationDiagnostics.hasErrors(),
                       "Array element assignment parses", failures);
  rocket::test::expect(
      mutationModule.functions[0].body[1]->kind == rocket::StmtKind::IndexAssignment,
      "Array element assignment has an explicit AST statement", failures);
  const auto& mutation = static_cast<const rocket::IndexAssignmentStmt&>(
      *mutationModule.functions[0].body[1]);
  rocket::test::expect(mutation.name == "values" &&
                           mutation.index->kind == rocket::ExprKind::Integer,
                       "Array assignment preserves its binding and index", failures);

  rocket::Diagnostics phase6Diagnostics;
  auto phase6 = rocket::test::parse(
      "struct Box[T]:\n"
      "    value: T\n"
      "enum Maybe[T]:\n"
      "    Present(T)\n"
      "    Missing\n"
      "fn unwrap[T](value: T) -> T:\n"
      "    return value\n"
      "fn main() -> Int:\n"
      "    let boxed: Box[Int] = Box(7)\n"
      "    let result: Result[Int, String] = Ok(boxed.value)\n"
      "    match result:\n"
      "        case Ok(value):\n"
      "            return value\n"
      "        case Err(message):\n"
      "            return 0\n",
      phase6Diagnostics);
  rocket::test::expect(!phase6Diagnostics.hasErrors() && phase6.structs.size() == 1 &&
                           phase6.enums.size() == 1 && phase6.functions.size() == 2,
                       "Phase 6 type declarations and generic functions parse", failures);
  rocket::test::expect(phase6.functions[1].body[2]->kind == rocket::StmtKind::Match,
                       "match cases have an explicit AST statement", failures);

  rocket::Diagnostics methodDiagnostics;
  auto methods = rocket::test::parse(
      "struct Counter:\n"
      "    value: Int\n"
      "impl Counter:\n"
      "    fn make(value: Int) -> Counter:\n"
      "        return Counter(value)\n"
      "    fn read(self: Counter) -> Int:\n"
      "        return self.value\n"
      "fn main() -> Int:\n"
      "    return Counter.make(42).read()\n",
      methodDiagnostics);
  rocket::test::expect(!methodDiagnostics.hasErrors() && methods.functions.size() == 3,
                       "impl methods and associated functions parse", failures);
  rocket::test::expect(methods.functions[0].methodOwner == "Counter" &&
                           methods.functions[1].parameters[0].name == "self",
                       "method AST records its impl owner and explicit receiver", failures);
  return rocket::test::finish(failures, "parser");
}
