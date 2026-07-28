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
  return rocket::test::finish(failures, "parser");
}
