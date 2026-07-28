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
  return rocket::test::finish(failures, "sema");
}
