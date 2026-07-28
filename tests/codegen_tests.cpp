#include "codegen.h"
#include "test_support.h"

#include <string>

int main() {
  int failures = 0;
  rocket::Diagnostics diagnostics;
  auto mir = rocket::test::lowerToMir(
      "fn twice(value: Int) -> Int:\n"
      "    return value * 2\n"
      "fn main() -> Int:\n"
      "    return twice(21)\n", diagnostics);

  rocket::test::expect(mir.has_value(), "codegen fixture lowers to MIR", failures);
  if (mir.has_value()) {
    const std::string generated = rocket::BootstrapCodeGenerator(*mir).generate();
    rocket::test::expect(generated.find("rocket_fn_twice_") != std::string::npos,
                         "backend emits declaration-ID-qualified functions", failures);
    rocket::test::expect(generated.find("std::int64_t") != std::string::npos,
                         "Int functions use the bootstrap integer type", failures);
    rocket::test::expect(generated.find("rocket_bb_0") != std::string::npos,
                         "backend emits MIR basic blocks", failures);
  }

  rocket::Diagnostics scalarDiagnostics;
  auto scalarMir = rocket::test::lowerToMir(
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
  rocket::test::expect(scalarMir.has_value(), "scalar codegen fixture lowers to MIR", failures);
  if (scalarMir.has_value()) {
    const std::string generated = rocket::BootstrapCodeGenerator(*scalarMir).generate();
    rocket::test::expect(generated.find("goto rocket_bb_") != std::string::npos,
                         "range loops emit MIR control-flow labels", failures);
    rocket::test::expect(generated.find("double rocket_l_") != std::string::npos,
                         "typed Float locals are emitted from MIR", failures);
    rocket::test::expect(generated.find("char rocket_l_") != std::string::npos,
                         "typed Char locals are emitted from MIR", failures);
    rocket::test::expect(generated.find("if (") != std::string::npos &&
                             generated.find(" && ") == std::string::npos,
                         "short-circuit MIR emits branches", failures);
  }
  return rocket::test::finish(failures, "codegen");
}
