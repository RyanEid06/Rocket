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


  rocket::Diagnostics collectionDiagnostics;
  auto collections = rocket::test::lowerToHir(
      "fn head(values: Slice[Int]) -> Int:\n"
      "    return values[0]\n"
      "fn main() -> Int:\n"
      "    let values = [10, 20, 30]\n"
      "    let tail = values[1..3]\n"
      "    return head(tail)\n",
      collectionDiagnostics);
  rocket::test::expect(collections.has_value(),
                       "collection program lowers to typed HIR", failures);
  if (collections.has_value()) {
    const auto& arrayBinding = static_cast<const rocket::HirBindingStmt&>(
        *collections->functions[1].body[0]);
    const auto& sliceBinding = static_cast<const rocket::HirBindingStmt&>(
        *collections->functions[1].body[1]);
    rocket::test::expect(arrayBinding.initializer->type == rocket::Type::ArrayInt &&
                             sliceBinding.initializer->type == rocket::Type::SliceInt,
                         "HIR records concrete built-in collection element types", failures);
    const auto& headReturn = static_cast<const rocket::HirReturnStmt&>(
        *collections->functions[0].body[0]);
    rocket::test::expect(headReturn.value->kind == rocket::HirExprKind::Index &&
                             headReturn.value->type == rocket::Type::Int,
                         "HIR index expressions carry their element type", failures);
  }

  rocket::Diagnostics phase6Diagnostics;
  auto phase6 = rocket::test::lowerToHir(
      "struct Box[T]:\n"
      "    value: T\n"
      "fn identity[T](value: T) -> T:\n"
      "    return value\n"
      "fn main() -> Int:\n"
      "    let boxed = Box(identity(7))\n"
      "    let maybe: Option[Int] = Some(boxed.value)\n"
      "    match maybe:\n"
      "        case Some(value):\n"
      "            return value\n"
      "        case None:\n"
      "            return 0\n",
      phase6Diagnostics);
  rocket::test::expect(phase6.has_value(), "Phase 6 program lowers to HIR", failures);
  if (phase6.has_value()) {
    rocket::test::expect(phase6->typeDeclarations.size() == 3,
                         "HIR records Option, Result, and user type declarations", failures);
    rocket::test::expect(phase6->functions.size() == 2 &&
                             phase6->symbols[phase6->functions[1].symbol].name ==
                                 "identity[Int]",
                         "generic calls create deterministic concrete specializations", failures);
    const auto& binding = static_cast<const rocket::HirBindingStmt&>(
        *phase6->functions[0].body[0]);
    rocket::test::expect(binding.initializer->kind == rocket::HirExprKind::Aggregate &&
                             binding.initializer->type ==
                                 rocket::Type{rocket::TypeKind::Struct, "Box", {rocket::Type::Int}},
                         "aggregate construction carries a structural generic type", failures);
  }
  return rocket::test::finish(failures, "hir");
}
