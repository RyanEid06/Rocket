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

  rocket::Diagnostics mutationDiagnostics;
  auto mutation = rocket::test::lowerToHir(
      "fn main() -> Int:\n"
      "    var values = [10, 20]\n"
      "    values[0] = 30\n"
      "    return values[0]\n",
      mutationDiagnostics);
  rocket::test::expect(mutation.has_value(),
                       "Array mutation lowers to typed HIR", failures);
  if (mutation.has_value()) {
    const auto& assignment = static_cast<const rocket::HirIndexAssignmentStmt&>(
        *mutation->functions[0].body[1]);
    rocket::test::expect(
        assignment.target ==
                static_cast<const rocket::HirBindingStmt&>(
                    *mutation->functions[0].body[0]).symbol &&
            assignment.index->type == rocket::Type::Int &&
            assignment.value->type == rocket::Type::Int,
        "HIR resolves and types the mutable Array target, index, and value", failures);
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
    rocket::test::expect(phase6->typeDeclarations.size() == 17,
                         "HIR records standard built-ins and user type declarations", failures);
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

  rocket::Diagnostics methodDiagnostics;
  auto methods = rocket::test::lowerToHir(
      "struct Box[T]:\n"
      "    value: T\n"
      "impl[T] Box[T]:\n"
      "    fn make(value: T) -> Box[T]:\n"
      "        return Box(value)\n"
      "    fn get(self: Box[T]) -> T:\n"
      "        return self.value\n"
      "fn main() -> Int:\n"
      "    let boxed = Box.make(42)\n"
      "    return boxed.get()\n",
      methodDiagnostics);
  rocket::test::expect(methods.has_value(),
                       "generic associated and instance methods lower to HIR", failures);
  if (methods.has_value()) {
    const auto& returned = static_cast<const rocket::HirReturnStmt&>(
        *methods->functions[0].body[1]);
    const auto& call = static_cast<const rocket::HirCallExpr&>(*returned.value);
    rocket::test::expect(call.type == rocket::Type::Int && call.arguments.size() == 1,
                         "dot calls insert one explicit typed receiver", failures);
  }

  rocket::Diagnostics invalidMethodDiagnostics;
  auto invalidMethod = rocket::test::lowerToHir(
      "struct Counter:\n"
      "    value: Int\n"
      "impl Counter:\n"
      "    fn broken(self: String) -> Int:\n"
      "        return 0\n"
      "fn main() -> Int:\n"
      "    return 0\n",
      invalidMethodDiagnostics);
  rocket::test::expect(!invalidMethod.has_value() && invalidMethodDiagnostics.hasErrors(),
                       "method receivers must exactly match their impl owner", failures);

  rocket::Diagnostics namedDiagnostics;
  auto named = rocket::test::lowerToHir(
      "struct Pair:\n"
      "    first: Int\n"
      "    second: Int\n"
      "fn combine(left: Int, right: Int) -> Int:\n"
      "    return left * 10 + right\n"
      "fn main() -> Int:\n"
      "    let pair = Pair(second: 2, first: 1)\n"
      "    return combine(right: 4, left: 3)\n",
      namedDiagnostics);
  rocket::test::expect(named.has_value(),
                       "reordered function and struct named arguments lower to HIR",
                       failures);
  if (named.has_value()) {
    const auto& aggregateBinding = static_cast<const rocket::HirBindingStmt&>(
        *named->functions[1].body[0]);
    const auto& aggregate = static_cast<const rocket::HirAggregateExpr&>(
        *aggregateBinding.initializer);
    const auto& returned = static_cast<const rocket::HirReturnStmt&>(
        *named->functions[1].body[1]);
    const auto& call = static_cast<const rocket::HirCallExpr&>(*returned.value);
    rocket::test::expect(
        aggregate.argumentOrder == std::vector<std::size_t>{1, 0} &&
            call.argumentOrder == std::vector<std::size_t>{1, 0} &&
            named->symbols[call.callee].parameterNames ==
                std::vector<std::string>{"left", "right"},
        "HIR preserves written evaluation order and public parameter-name metadata",
        failures);
  }

  rocket::Diagnostics callableDiagnostics;
  auto callables = rocket::test::lowerToHir(
      "struct Counter:\n"
      "    value: Int\n"
      "impl Counter:\n"
      "    fn adjusted(self: Counter, amount: Int, factor: Int) -> Int:\n"
      "        return self.value + amount * factor\n"
      "extern fn native_add(left: Int, right: Int) -> Int\n"
      "fn main() -> Int:\n"
      "    let counter = Counter(value: 10)\n"
      "    let adjusted = counter.adjusted(factor: 2, amount: 3)\n"
      "    unsafe:\n"
      "        return native_add(right: adjusted, left: 1)\n",
      callableDiagnostics);
  rocket::test::expect(callables.has_value(),
                       "methods and extern functions accept named arguments",
                       failures);

  auto hasDiagnostic = [](const rocket::Diagnostics& diagnostics,
                          rocket::DiagnosticCode code,
                          const std::string& fragment) {
    for (const auto& diagnostic : diagnostics.all())
      if (diagnostic.code == code &&
          diagnostic.message.find(fragment) != std::string::npos)
        return true;
    return false;
  };
  auto expectNamedFailure = [&](const std::string& call,
                                rocket::DiagnosticCode code,
                                const std::string& fragment,
                                const std::string& label) {
    rocket::Diagnostics localDiagnostics;
    const std::string source =
        "fn combine(left: Int, right: Int) -> Int:\n"
        "    return left + right\n"
        "fn main() -> Int:\n"
        "    return " + call + "\n";
    auto result = rocket::test::lowerToHir(source, localDiagnostics);
    rocket::test::expect(!result.has_value() &&
                             hasDiagnostic(localDiagnostics, code, fragment),
                         label, failures);
  };
  expectNamedFailure("combine(rght: 2, left: 1)",
                     rocket::DiagnosticCode::Name,
                     "unknown named argument 'rght'; did you mean 'right'?",
                     "unknown named arguments use deterministic typo suggestions");
  expectNamedFailure("combine(left: 1, left: 2)",
                     rocket::DiagnosticCode::Arity,
                     "duplicate named argument 'left'",
                     "duplicate named arguments are rejected");
  expectNamedFailure("combine(left: 1)", rocket::DiagnosticCode::Arity,
                     "missing required argument 'right'",
                     "missing named arguments identify the required parameter");
  expectNamedFailure("combine(1, left: 2)", rocket::DiagnosticCode::Arity,
                     "argument 'left' is already supplied positionally",
                     "positional and named arguments cannot bind the same parameter");
  expectNamedFailure("combine(right: 2, left: \"wrong\")",
                     rocket::DiagnosticCode::Type,
                     "argument 'left' has type String, expected Int",
                     "wrong-typed named arguments identify their declared parameter");

  rocket::Diagnostics defaultDiagnostics;
  auto defaulted = rocket::test::lowerToHir(
      "fn offset(value: Int) -> Int:\n"
      "    return value + 1\n"
      "fn combine(first: Int, second: Int = offset(first), third: Int = second + 1) -> Int:\n"
      "    return first * 100 + second * 10 + third\n"
      "struct Counter:\n"
      "    value: Int\n"
      "impl Counter:\n"
      "    fn adjusted(self: Counter, amount: Int = 2, factor: Int = amount + 1) -> Int:\n"
      "        return self.value + amount * factor\n"
      "fn select[T](first: T, second: T = first) -> T:\n"
      "    return second\n"
      "fn main() -> Int:\n"
      "    let counter = Counter(10)\n"
      "    let combined = combine(third: 9, first: 1)\n"
      "    let adjusted = counter.adjusted()\n"
      "    return combined + adjusted + select(3)\n",
      defaultDiagnostics);
  rocket::test::expect(
      defaulted.has_value(),
      "function, method, generic, positional, and named calls expand omitted defaults",
      failures);
  if (defaulted.has_value()) {
    const rocket::HirFunction* mainFunction = nullptr;
    for (const auto& function : defaulted->functions)
      if (defaulted->symbol(function.symbol).name == "main")
        mainFunction = &function;
    rocket::test::expect(mainFunction != nullptr,
                         "default argument fixture retains main HIR", failures);
    if (mainFunction) {
    const auto& combinedBinding = static_cast<const rocket::HirBindingStmt&>(
        *mainFunction->body[1]);
    const auto& combinedCall = static_cast<const rocket::HirCallExpr&>(
        *combinedBinding.initializer);
    rocket::test::expect(
        combinedCall.arguments.size() == 3 &&
            combinedCall.argumentOrder == std::vector<std::size_t>{2, 0, 1},
        "HIR evaluates written arguments first and omitted defaults in parameter order",
        failures);
    }
  }

  auto expectDefaultFailure = [&](const std::string& source,
                                  rocket::DiagnosticCode code,
                                  const std::string& fragment,
                                  const std::string& label) {
    rocket::Diagnostics localDiagnostics;
    auto result = rocket::test::lowerToHir(source, localDiagnostics);
    rocket::test::expect(!result.has_value() &&
                             hasDiagnostic(localDiagnostics, code, fragment),
                         label, failures);
  };
  expectDefaultFailure(
      "fn invalid(first: Int = later, later: Int = 2) -> Int:\n"
      "    return first\n"
      "fn main() -> Int:\n"
      "    return 0\n",
      rocket::DiagnosticCode::Name, "undefined name 'later'",
      "defaults cannot reference later parameters");
  expectDefaultFailure(
      "fn invalid(value: Int = \"wrong\") -> Int:\n"
      "    return value\n"
      "fn main() -> Int:\n"
      "    return 0\n",
      rocket::DiagnosticCode::Type,
      "default argument for 'value' has type String, expected Int",
      "defaults are type-checked in declaration context");
  expectDefaultFailure(
      "fn combine(left: Int, right: Int = 2) -> Int:\n"
      "    return left + right\n"
      "fn main() -> Int:\n"
      "    return combine()\n",
      rocket::DiagnosticCode::Arity, "missing required argument 'left'",
      "omitted required arguments remain errors");

  auto expectExcluded = [&](const std::string& source,
                            const std::string& category) {
    rocket::Diagnostics localDiagnostics;
    auto result = rocket::test::lowerToHir(source, localDiagnostics);
    rocket::test::expect(
        !result.has_value() &&
            hasDiagnostic(localDiagnostics, rocket::DiagnosticCode::Arity,
                          "named arguments are not supported for " + category),
        "named arguments reject excluded " + category + " callables", failures);
  };
  expectExcluded(
      "fn main() -> Int:\n"
      "    let identity = fn(value: Int) -> Int => value\n"
      "    return identity(value: 1)\n",
      "closure values");
  expectExcluded(
      "fn main() -> Int:\n"
      "    return std.string.byte_length(value: \"rocket\")\n",
      "standard-library intrinsics");
  expectExcluded(
      "enum Choice:\n"
      "    Value(Int)\n"
      "fn main() -> Int:\n"
      "    let choice = Value(value: 1)\n"
      "    return 0\n",
      "enum constructors");
  expectExcluded(
      "fn main() -> Int:\n"
      "    print(value: 1)\n"
      "    return 0\n",
      "built-in functions");

  rocket::Diagnostics genericLambdaDiagnostics;
  auto genericLambdas = rocket::test::lowerToHir(
      "fn through_lambda[T](value: T) -> T:\n"
      "    let identity = fn(item: T) -> T => item\n"
      "    return identity(value)\n"
      "fn through_capture[T](value: T) -> T:\n"
      "    let captured = fn() -> T => value\n"
      "    return captured()\n"
      "fn main() -> Int:\n"
      "    let direct = through_lambda(42)\n"
      "    return through_capture(direct)\n",
      genericLambdaDiagnostics);
  rocket::test::expect(genericLambdas.has_value(),
                       "generic specializations substitute lambda signatures", failures);
  if (genericLambdas.has_value()) {
    bool concreteLambdaSignature = false;
    for (const auto& symbol : genericLambdas->symbols) {
      if (symbol.name.rfind("$closure.", 0) == 0 &&
          symbol.name.ends_with(".call") && symbol.type == rocket::Type::Int) {
        concreteLambdaSignature = true;
        for (const auto& parameter : symbol.parameterTypes)
          concreteLambdaSignature = concreteLambdaSignature &&
              parameter.kind != rocket::TypeKind::TypeParameter;
      }
    }
    rocket::test::expect(concreteLambdaSignature,
                         "generated lambda call functions have concrete types", failures);
  }

  rocket::Diagnostics invalidGenericLambdaDiagnostics;
  auto invalidGenericLambda = rocket::test::lowerToHir(
      "fn broken[T](value: T) -> Int:\n"
      "    let wrong = fn(item: T) -> Int => item\n"
      "    return wrong(value)\n"
      "fn main() -> Int:\n"
      "    return broken(\"not-an-int\")\n",
      invalidGenericLambdaDiagnostics);
  rocket::test::expect(!invalidGenericLambda.has_value() &&
                           invalidGenericLambdaDiagnostics.hasErrors(),
                       "generic lambda result mismatches remain diagnosed", failures);

  const std::string nativeSource =
      "extern callback Unary(value: Int) -> Int\n"
      "extern fn apply(action: Unary, value: Int) -> Int\n"
      "fn twice(value: Int) -> Int:\n"
      "    return value * 2\n"
      "fn main() -> Int:\n"
      "    unsafe:\n"
      "        return apply(twice, 21)\n";
  rocket::Diagnostics nativeDiagnostics;
  auto native = rocket::test::lowerToHir(nativeSource, nativeDiagnostics);
  rocket::test::expect(native.has_value(),
                       "native calls inside unsafe lower to HIR", failures);
  if (native.has_value()) {
    bool importFound = false;
    bool callbackFound = false;
    for (const auto& symbol : native->symbols)
      importFound = importFound || (symbol.name == "apply" && symbol.nativeImport);
    const auto& unsafe = static_cast<const rocket::HirUnsafeStmt&>(
        *native->functions.back().body[0]);
    const auto& returned = static_cast<const rocket::HirReturnStmt&>(*unsafe.body[0]);
    const auto& call = static_cast<const rocket::HirCallExpr&>(*returned.value);
    callbackFound = call.arguments[0]->kind == rocket::HirExprKind::FunctionRef;
    rocket::test::expect(importFound && callbackFound,
                         "HIR marks C imports and typed callback function references", failures);
  }

  rocket::Diagnostics unsafeDiagnostics;
  auto unsafeFailure = rocket::test::lowerToHir(
      "extern fn native_add(left: Int, right: Int) -> Int\n"
      "fn main() -> Int:\n"
      "    return native_add(1, 2)\n",
      unsafeDiagnostics);
  rocket::test::expect(!unsafeFailure.has_value() && unsafeDiagnostics.hasErrors(),
                       "extern calls are rejected outside explicit unsafe blocks", failures);

  rocket::Diagnostics asyncDiagnostics;
  auto asyncHir = rocket::test::lowerToHir(
      "async fn leaf(value: Int) -> Result[Int, String]:\n"
      "    return Ok(value)\n"
      "async fn parent() -> Result[Int, String]:\n"
      "    let result = await leaf(42)\n"
      "    return result\n"
      "fn main() -> Int:\n"
      "    return 0\n",
      asyncDiagnostics);
  rocket::test::expect(asyncHir.has_value(),
                       "async calls and await lower to typed HIR", failures);
  if (asyncHir.has_value()) {
    const auto& binding = static_cast<const rocket::HirBindingStmt&>(
        *asyncHir->functions[1].body[0]);
    const auto& awaited = static_cast<const rocket::HirAwaitExpr&>(*binding.initializer);
    rocket::test::expect(awaited.task->kind == rocket::HirExprKind::AsyncCall &&
                             awaited.type.declaration == "Result" &&
                             awaited.type.arguments[0] == rocket::Type::Int,
                         "HIR records Task[Int] scheduling and Result[Int, String] await",
                         failures);
  }

  return rocket::test::finish(failures, "hir");
}
