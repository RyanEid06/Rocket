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
  return rocket::test::finish(failures, "sema");
}
