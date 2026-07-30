#include "native.h"
#include "test_support.h"

#include <string>

int main() {
  int failures = 0;
  rocket::Diagnostics diagnostics;
  auto module = rocket::test::parse(
      "extern opaque Handle\n"
      "extern struct Point:\n"
      "    x: Int\n"
      "    y: Int\n"
      "extern callback Unary(value: Int) -> Int\n"
      "export fn rocket_add(left: Int, right: Int) -> Int:\n"
      "    return left + right\n",
      diagnostics);
  module.library = true;

  std::string first;
  std::string second;
  std::string error;
  rocket::test::expect(!diagnostics.hasErrors() &&
                           rocket::generateNativeHeader(module, "native-test", first, error),
                       "native C header generation succeeds: " + error, failures);
  error.clear();
  rocket::test::expect(rocket::generateNativeHeader(module, "native-test", second, error) &&
                           first == second,
                       "native C header generation is byte deterministic", failures);
  rocket::test::expect(first.find("typedef struct Handle Handle;") != std::string::npos &&
                           first.find("typedef struct Point { int64_t x; int64_t y; }") !=
                               std::string::npos &&
                           first.find("ROCKET_API int64_t rocket_add") != std::string::npos,
                       "generated header records opaque, layout, and exported ABI declarations",
                       failures);

  const std::string handwritten =
      "#include <stdint.h>\n"
      "typedef uint8_t rocket_bool;\n"
      "#define NATIVE_BIAS 5\n"
      "typedef struct Handle Handle;\n"
      "typedef struct Point { int64_t x; int64_t y; } Point;\n"
      "typedef int64_t (*Unary)(int64_t value);\n"
      "int64_t native_apply(Unary action, int64_t value);\n";
  std::string bindings;
  error.clear();
  rocket::test::expect(rocket::generateRocketBindings(handwritten, bindings, error),
                       "handwritten narrow C headers generate Rocket bindings: " + error,
                       failures);
  rocket::test::expect(
      bindings.find("pub extern const NATIVE_BIAS: Int = 5") != std::string::npos &&
          bindings.find("pub extern opaque Handle") != std::string::npos &&
          bindings.find("pub extern struct Point:") != std::string::npos &&
          bindings.find("pub extern callback Unary(value: Int) -> Int") != std::string::npos &&
          bindings.find("pub extern fn native_apply(action: Unary, value: Int) -> Int") !=
              std::string::npos,
      "bindings cover constants, handles, structures, callbacks, and plain C prototypes",
      failures);

  return rocket::test::finish(failures, "native");
}
