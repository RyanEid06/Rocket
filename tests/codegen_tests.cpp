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

  rocket::Diagnostics scalarDiagnostics;
  auto scalarModule = rocket::test::parse(
      "fn main() -> Int:\n"
      "    var total = 0\n"
      "    for index in 0..3:\n"
      "        total = total + index\n"
      "    let ratio = 1.5\n"
      "    let marker = 'x'\n"
      "    let enabled = true and not false\n"
      "    print(ratio)\n"
      "    print(marker)\n"
      "    return total\n", scalarDiagnostics);
  rocket::test::expect(!scalarDiagnostics.hasErrors(), "scalar codegen fixture parses", failures);
  const std::string scalarGenerated = rocket::BootstrapCodeGenerator(scalarModule).generate();
  rocket::test::expect(scalarGenerated.find("for (std::int64_t rocket_v_index") != std::string::npos,
                       "for ranges lower to C++ for loops", failures);
  rocket::test::expect(scalarGenerated.find("const auto rocket_v_ratio = 1.5") != std::string::npos,
                       "Float literals lower through inferred bindings", failures);
  rocket::test::expect(scalarGenerated.find("const auto rocket_v_marker = 'x'") != std::string::npos,
                       "Char literals lower through inferred bindings", failures);
  rocket::test::expect(scalarGenerated.find("true && (!false)") != std::string::npos,
                       "logical operators lower to short-circuiting C++ operators", failures);
  return rocket::test::finish(failures, "codegen");
}
