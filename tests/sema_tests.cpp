#include "sema.h"
#include "test_support.h"

int main() {
  int failures = 0;

  rocket::Diagnostics validDiagnostics;
  auto validModule = rocket::test::parse(
      "fn twice(value: Int) -> Int:\n"
      "    return value * 2\n"
      "fn main() -> Int:\n"
      "    let answer = twice(21)\n"
      "    print(answer)\n"
      "    return 0\n", validDiagnostics);
  rocket::SemanticAnalyzer validAnalyzer(validModule, validDiagnostics);
  validAnalyzer.analyze();
  rocket::test::expect(!validDiagnostics.hasErrors(), "well-typed function call passes analysis", failures);

  rocket::Diagnostics undefinedDiagnostics;
  auto undefinedModule = rocket::test::parse(
      "fn main() -> Int:\n    print(missing)\n    return 0\n", undefinedDiagnostics);
  rocket::SemanticAnalyzer undefinedAnalyzer(undefinedModule, undefinedDiagnostics);
  undefinedAnalyzer.analyze();
  rocket::test::expect(undefinedDiagnostics.hasErrors(), "undefined names are rejected", failures);

  rocket::Diagnostics mismatchDiagnostics;
  auto mismatchModule = rocket::test::parse(
      "fn main() -> Int:\n    if 1:\n        print(1)\n    return 0\n", mismatchDiagnostics);
  rocket::SemanticAnalyzer mismatchAnalyzer(mismatchModule, mismatchDiagnostics);
  mismatchAnalyzer.analyze();
  rocket::test::expect(mismatchDiagnostics.hasErrors(), "non-Bool conditions are rejected", failures);

  rocket::Diagnostics scalarDiagnostics;
  auto scalarModule = rocket::test::parse(
      "fn main() -> Int:\n"
      "    var total = 0\n"
      "    for index in 0..5:\n"
      "        if index == 3:\n"
      "            continue\n"
      "        total = total + index\n"
      "    let ratio = 1.5 / 0.5\n"
      "    let marker = 'x'\n"
      "    print(ratio)\n"
      "    print(marker)\n"
      "    return total\n", scalarDiagnostics);
  rocket::SemanticAnalyzer scalarAnalyzer(scalarModule, scalarDiagnostics);
  scalarAnalyzer.analyze();
  rocket::test::expect(!scalarDiagnostics.hasErrors(), "assignment, ranges, Float, and Char pass analysis", failures);

  rocket::Diagnostics assignmentDiagnostics;
  auto assignmentModule = rocket::test::parse(
      "fn main() -> Int:\n    let value = 1\n    value = 2\n    return 0\n", assignmentDiagnostics);
  rocket::SemanticAnalyzer assignmentAnalyzer(assignmentModule, assignmentDiagnostics);
  assignmentAnalyzer.analyze();
  rocket::test::expect(assignmentDiagnostics.hasErrors(), "assignment to let bindings is rejected", failures);

  rocket::Diagnostics controlDiagnostics;
  auto controlModule = rocket::test::parse(
      "fn main() -> Int:\n    break\n    return 0\n", controlDiagnostics);
  rocket::SemanticAnalyzer controlAnalyzer(controlModule, controlDiagnostics);
  controlAnalyzer.analyze();
  rocket::test::expect(controlDiagnostics.hasErrors(), "break outside a loop is rejected", failures);

  rocket::Diagnostics logicalDiagnostics;
  auto logicalModule = rocket::test::parse(
      "fn main() -> Int:\n"
      "    let enabled = true and not false\n"
      "    if enabled or false:\n"
      "        return 0\n"
      "    else:\n"
      "        return 1\n", logicalDiagnostics);
  rocket::SemanticAnalyzer logicalAnalyzer(logicalModule, logicalDiagnostics);
  logicalAnalyzer.analyze();
  rocket::test::expect(!logicalDiagnostics.hasErrors(), "Bool logical operators pass analysis", failures);

  rocket::Diagnostics badLogicalDiagnostics;
  auto badLogicalModule = rocket::test::parse(
      "fn main() -> Int:\n    let invalid = 1 and 2\n    return 0\n", badLogicalDiagnostics);
  rocket::SemanticAnalyzer badLogicalAnalyzer(badLogicalModule, badLogicalDiagnostics);
  badLogicalAnalyzer.analyze();
  rocket::test::expect(badLogicalDiagnostics.hasErrors(), "logical operators reject non-Bool operands", failures);

  rocket::Diagnostics collectionDiagnostics;
  auto collectionModule = rocket::test::parse(
      "fn head(values: Slice[String]) -> String:\n"
      "    return values[0]\n"
      "fn main() -> Int:\n"
      "    let values = [\"zero\", \"one\", \"two\"]\n"
      "    let tail = values[1..3]\n"
      "    print(head(tail))\n"
      "    return 0\n",
      collectionDiagnostics);
  rocket::SemanticAnalyzer collectionAnalyzer(collectionModule, collectionDiagnostics);
  collectionAnalyzer.analyze();
  rocket::test::expect(!collectionDiagnostics.hasErrors(),
                        "typed Array and Slice operations pass analysis", failures);

  rocket::Diagnostics mutationDiagnostics;
  auto mutationModule = rocket::test::parse(
      "fn main() -> Int:\n"
      "    var values = [1, 2]\n"
      "    values[0] = 3\n"
      "    return values[0]\n",
      mutationDiagnostics);
  rocket::SemanticAnalyzer mutationAnalyzer(mutationModule, mutationDiagnostics);
  mutationAnalyzer.analyze();
  rocket::test::expect(!mutationDiagnostics.hasErrors(),
                       "mutable Array elements accept their element type", failures);

  rocket::Diagnostics immutableMutationDiagnostics;
  auto immutableMutationModule = rocket::test::parse(
      "fn main() -> Int:\n"
      "    let values = [1, 2]\n"
      "    values[0] = 3\n"
      "    return 0\n",
      immutableMutationDiagnostics);
  rocket::SemanticAnalyzer immutableMutationAnalyzer(immutableMutationModule,
                                                       immutableMutationDiagnostics);
  immutableMutationAnalyzer.analyze();
  rocket::test::expect(immutableMutationDiagnostics.hasErrors(),
                       "immutable Array bindings reject element mutation", failures);

  rocket::Diagnostics wrongMutationDiagnostics;
  auto wrongMutationModule = rocket::test::parse(
      "fn main() -> Int:\n"
      "    var values = [1, 2]\n"
      "    values[true] = 3.0\n"
      "    return 0\n",
      wrongMutationDiagnostics);
  rocket::SemanticAnalyzer wrongMutationAnalyzer(wrongMutationModule,
                                                   wrongMutationDiagnostics);
  wrongMutationAnalyzer.analyze();
  rocket::test::expect(wrongMutationDiagnostics.hasErrors(),
                       "Array mutation checks index and element types", failures);

  rocket::Diagnostics sliceMutationDiagnostics;
  auto sliceMutationModule = rocket::test::parse(
      "fn main() -> Int:\n"
      "    let values = [1, 2]\n"
      "    var slice = values[0..2]\n"
      "    slice[0] = 3\n"
      "    return 0\n",
      sliceMutationDiagnostics);
  rocket::SemanticAnalyzer sliceMutationAnalyzer(sliceMutationModule,
                                                   sliceMutationDiagnostics);
  sliceMutationAnalyzer.analyze();
  rocket::test::expect(sliceMutationDiagnostics.hasErrors(),
                       "Slice values remain immutable views", failures);

  rocket::Diagnostics mixedArrayDiagnostics;
  auto mixedArrayModule = rocket::test::parse(
      "fn main() -> Int:\n    let values = [1, 2.0]\n    return 0\n",
      mixedArrayDiagnostics);
  rocket::SemanticAnalyzer mixedArrayAnalyzer(mixedArrayModule, mixedArrayDiagnostics);
  mixedArrayAnalyzer.analyze();
  rocket::test::expect(mixedArrayDiagnostics.hasErrors(),
                       "Array literals reject mixed element types", failures);

  rocket::Diagnostics badIndexDiagnostics;
  auto badIndexModule = rocket::test::parse(
      "fn main() -> Int:\n    let values = [1, 2]\n    print(values[true])\n    return 0\n",
      badIndexDiagnostics);
  rocket::SemanticAnalyzer badIndexAnalyzer(badIndexModule, badIndexDiagnostics);
  badIndexAnalyzer.analyze();
  rocket::test::expect(badIndexDiagnostics.hasErrors(),
                       "collection indices require Int", failures);

  rocket::Diagnostics literalRangeDiagnostics;
  auto literalRangeModule = rocket::test::parse(
      "fn main() -> Int:\n    let too_large = 9223372036854775808\n    return 0\n",
      literalRangeDiagnostics);
  rocket::SemanticAnalyzer literalRangeAnalyzer(literalRangeModule, literalRangeDiagnostics);
  literalRangeAnalyzer.analyze();
  rocket::test::expect(literalRangeDiagnostics.hasErrors(),
                       "out-of-range Int literals are rejected", failures);

  rocket::Diagnostics matchDiagnostics;
  auto matchModule = rocket::test::parse(
      "fn main() -> Int:\n"
      "    let value: Option[Int] = Some(1)\n"
      "    match value:\n"
      "        case Some(number):\n"
      "            return number\n",
      matchDiagnostics);
  rocket::SemanticAnalyzer matchAnalyzer(matchModule, matchDiagnostics);
  matchAnalyzer.analyze();
  rocket::test::expect(matchDiagnostics.hasErrors(),
                       "non-exhaustive enum matches are rejected", failures);

  rocket::Diagnostics propagationDiagnostics;
  auto propagationModule = rocket::test::parse(
      "fn bad(value: Result[Int, String]) -> Int:\n"
      "    return value?\n"
      "fn main() -> Int:\n"
      "    return 0\n",
      propagationDiagnostics);
  rocket::SemanticAnalyzer propagationAnalyzer(propagationModule, propagationDiagnostics);
  propagationAnalyzer.analyze();
  rocket::test::expect(propagationDiagnostics.hasErrors(),
                       "error propagation requires a compatible function return type", failures);
  return rocket::test::finish(failures, "sema");
}
