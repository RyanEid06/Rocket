#include "codegen.h"
#include "test_support.h"

#include <string>

int main() {
  int failures = 0;
  rocket::Diagnostics diagnostics;
  auto module = rocket::test::parse(
      "fn twice(value: Int) -> Int:\n"
      "    return value * 2\n"
      "fn main() -> Int:\n"
      "    return twice(21)\n", diagnostics);

  rocket::test::expect(!diagnostics.hasErrors(), "codegen fixture parses", failures);
  const std::string generated = rocket::BootstrapCodeGenerator(module).generate();
  rocket::test::expect(generated.find("rocket_fn_twice") != std::string::npos,
                       "backend emits prefixed functions", failures);
  rocket::test::expect(generated.find("std::int64_t") != std::string::npos,
                       "Int functions use the bootstrap integer type", failures);
  return rocket::test::finish(failures, "codegen");
}
