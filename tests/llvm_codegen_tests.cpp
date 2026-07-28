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
    rocket::test::expect(ir.find("@rocket_rt_print_string") != std::string::npos &&
                             ir.find("@rocket_rt_print_float") != std::string::npos &&
                             ir.find("@printf") == std::string::npos,
                         "scalar print calls lower through the Rocket runtime ABI", failures);
    rocket::test::expect(ir.find("@rocket_rt_string_new") != std::string::npos &&
                             ir.find("@rocket_rt_release") != std::string::npos,
                         "owned String construction and cleanup are explicit in LLVM IR",
                         failures);
    rocket::test::expect(ir.find("@llvm.smul.with.overflow.i64") != std::string::npos &&
                             ir.find("@rocket_rt_panic_integer_overflow") != std::string::npos,
                         "Int arithmetic lowers with overflow checks", failures);
    rocket::test::expect(ir.find("define i32 @main(i32 %argc, ptr %argv)") != std::string::npos &&
                             ir.find("@rocket_std_process_set_arguments") != std::string::npos,
                         "module exports an argument-aware native C main entrypoint", failures);

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


  rocket::Diagnostics collectionDiagnostics;
  auto collectionMir = rocket::test::lowerToMir(
      "fn head(values: Slice[String]) -> String:\n"
      "    return values[0]\n"
      "fn main() -> Int:\n"
      "    let values = [\"zero\", \"one\", \"two\"]\n"
      "    let tail = values[1..3]\n"
      "    print(head(tail))\n"
      "    return 0\n",
      collectionDiagnostics);
  rocket::test::expect(collectionMir.has_value(),
                       "collection LLVM fixture lowers to MIR", failures);
  if (collectionMir.has_value()) {
    std::string error;
    std::string ir;
    rocket::test::expect(rocket::generateLlvmIr(*collectionMir, false, ir, error),
                         "collection MIR lowers to valid LLVM IR: " + error, failures);
    rocket::test::expect(ir.find("@rocket_rt_array_new") != std::string::npos &&
                             ir.find("@rocket_rt_array_set_string") != std::string::npos,
                         "Array literals lower through the runtime ABI", failures);
    rocket::test::expect(ir.find("@rocket_rt_slice_new") != std::string::npos &&
                             ir.find("@rocket_rt_index_string") != std::string::npos,
                         "Slice creation and checked indexing use runtime calls", failures);
    rocket::test::expect(ir.find("define ptr @rocket_fn_head_") != std::string::npos,
                         "managed collection functions use opaque pointer ABI values", failures);
  }
  return rocket::test::finish(failures, "llvm_codegen");
}
