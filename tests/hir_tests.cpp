#include "test_support.h"

int main() {
  int failures = 0;
  rocket::Diagnostics diagnostics;
  auto hir = rocket::test::lowerToHir(
      "fn twice(value: Int) -> Int:\n"
      "    return value * 2\n"
      "fn main() -> Int:\n"
      "    var value = 21\n"
      "    if true:\n"
      "        let value = 1\n"
      "        print(value)\n"
      "    return twice(value)\n",
      diagnostics);

  rocket::test::expect(hir.has_value(), "valid source lowers to HIR", failures);
  if (hir.has_value()) {
    rocket::test::expect(hir->symbols.front().kind == rocket::SymbolKind::BuiltinFunction &&
                             hir->symbols.front().name == "print",
                         "built-ins receive the first stable symbol IDs", failures);
    rocket::test::expect(hir->functions.size() == 2, "HIR retains resolved functions", failures);

    const auto& twiceReturn = static_cast<const rocket::HirReturnStmt&>(
        *hir->functions[0].body[0]);
    const auto& product = static_cast<const rocket::HirBinaryExpr&>(*twiceReturn.value);
    rocket::test::expect(product.type == rocket::Type::Int,
                         "HIR expressions carry checked types", failures);
    const auto& parameter = static_cast<const rocket::HirNameExpr&>(*product.left);
    rocket::test::expect(parameter.symbol == hir->functions[0].parameters[0].symbol,
                         "parameter uses resolve to parameter declarations", failures);

    const auto& outer = static_cast<const rocket::HirBindingStmt&>(
        *hir->functions[1].body[0]);
    const auto& branch = static_cast<const rocket::HirIfStmt&>(
        *hir->functions[1].body[1]);
    const auto& inner = static_cast<const rocket::HirBindingStmt&>(*branch.thenBody[0]);
    const auto& printStatement = static_cast<const rocket::HirExprStmt&>(*branch.thenBody[1]);
    const auto& printCall = static_cast<const rocket::HirCallExpr&>(*printStatement.expression);
    const auto& printedName = static_cast<const rocket::HirNameExpr&>(*printCall.arguments[0]);
    rocket::test::expect(inner.symbol != outer.symbol && printedName.symbol == inner.symbol,
                         "shadowed names resolve to the nearest declaration", failures);

    const auto& returned = static_cast<const rocket::HirReturnStmt&>(
        *hir->functions[1].body[2]);
    const auto& call = static_cast<const rocket::HirCallExpr&>(*returned.value);
    const auto& argument = static_cast<const rocket::HirNameExpr&>(*call.arguments[0]);
    rocket::test::expect(call.callee == hir->functions[0].symbol,
                         "calls resolve to function declaration IDs", failures);
    rocket::test::expect(argument.symbol == outer.symbol,
                         "outer binding resolution resumes after nested scope", failures);
  }

  rocket::Diagnostics unresolvedDiagnostics;
  auto unresolved = rocket::test::lowerToHir(
      "fn main() -> Int:\n    return missing\n", unresolvedDiagnostics);
  rocket::test::expect(!unresolved.has_value() && unresolvedDiagnostics.hasErrors(),
                       "HIR is withheld when resolution fails", failures);

  rocket::Diagnostics forwardDiagnostics;
  auto forward = rocket::test::lowerToHir(
      "fn main() -> Int:\n"
      "    return later()\n"
      "fn later() -> Int:\n"
      "    return 0\n",
      forwardDiagnostics);
  rocket::test::expect(forward.has_value(), "forward calls lower to HIR", failures);
  if (forward.has_value()) {
    const auto& returned = static_cast<const rocket::HirReturnStmt&>(
        *forward->functions[0].body[0]);
    const auto& call = static_cast<const rocket::HirCallExpr&>(*returned.value);
    rocket::test::expect(call.callee == forward->functions[1].symbol,
                         "function signatures resolve before body traversal", failures);
  }
  return rocket::test::finish(failures, "hir");
}
