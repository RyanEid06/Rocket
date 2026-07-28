#include "llvm_codegen.h"
#include "test_support.h"

#include <filesystem>
#include <string>

int main() {
  int failures = 0;
  rocket::Diagnostics diagnostics;
  auto mir = rocket::test::lowerToMir(
      "fn choose(value: Int, enabled: Bool) -> Int:\n"
      "    if enabled and value > 0:\n"
      "        return value * 2\n"
      "    else:\n"
      "        return 1\n"
      "fn main() -> Int:\n"
      "    let text = \"rocket\"\n"
      "    let ratio = 1.5\n"
      "    let marker = 'R'\n"
      "    print(text)\n"
      "    print(ratio)\n"
      "    print(marker)\n"
      "    return choose(21, true)\n",
      diagnostics);

  rocket::test::expect(mir.has_value(), "LLVM fixture lowers to MIR", failures);
  if (mir.has_value()) {
    std::string error;
    std::string ir;
    rocket::test::expect(rocket::generateLlvmIr(*mir, false, ir, error),
                         "scalar MIR lowers to valid LLVM IR: " + error, failures);
    rocket::test::expect(ir.find("define i64 @rocket_fn_choose_") != std::string::npos,
                         "Int functions use the i64 ABI", failures);
    rocket::test::expect(ir.find("i1 %arg.") != std::string::npos,
                         "Bool parameters use the i1 ABI", failures);
    rocket::test::expect(ir.find("br i1") != std::string::npos,
                         "MIR branches lower to LLVM conditional branches", failures);
    rocket::test::expect(ir.find("call i32 (ptr, ...) @printf") != std::string::npos,
                         "scalar print calls lower through the C ABI", failures);
    rocket::test::expect(ir.find("define i32 @main()") != std::string::npos,
                         "module exports a native C main entrypoint", failures);

    std::string optimized;
    error.clear();
    rocket::test::expect(rocket::generateLlvmIr(*mir, true, optimized, error),
                         "LLVM O2 pipeline succeeds: " + error, failures);
    rocket::test::expect(optimized.find("alloca") == std::string::npos,
                         "optimization promotes scalar MIR locals", failures);

    const std::filesystem::path objectPath =
        std::filesystem::current_path() / "llvm_codegen_test.obj";
    error.clear();
    rocket::test::expect(
        rocket::emitLlvmFile(*mir, true, rocket::LlvmFileType::Object, objectPath, error),
        "LLVM emits a native object file: " + error, failures);
    rocket::test::expect(std::filesystem::exists(objectPath) &&
                             std::filesystem::file_size(objectPath) > 0,
                         "native object output is non-empty", failures);
    std::error_code removeError;
    std::filesystem::remove(objectPath, removeError);
  }
  return rocket::test::finish(failures, "llvm_codegen");
}
