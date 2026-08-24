#include "llvm_codegen.h"
#include "test_support.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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

    for (const auto& target : rocket::productionTargets()) {
      std::string targetIr;
      error.clear();
      rocket::test::expect(
          rocket::generateLlvmIr(*mir, false, target, targetIr, error),
          "LLVM creates target machine for " + target.alias + ": " + error,
          failures);
      rocket::test::expect(
          targetIr.find("target triple = \"" + target.triple + "\"") !=
                  std::string::npos &&
              targetIr.find("target datalayout = ") != std::string::npos,
          "IR records explicit triple and data layout for " + target.alias,
          failures);
      const auto names = rocket::targetArtifacts(target);
      const auto targetObject = std::filesystem::current_path() /
                                ("llvm_codegen_" + target.alias +
                                 names.objectSuffix);
      error.clear();
      rocket::test::expect(
          rocket::emitLlvmFile(*mir, false, rocket::LlvmFileType::Object,
                               target, targetObject, error),
          "LLVM emits object for " + target.alias + ": " + error, failures);
      std::ifstream object(targetObject, std::ios::binary);
      std::vector<unsigned char> bytes(20, 0);
      object.read(reinterpret_cast<char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
      bool objectFormat = false;
      if (target.operatingSystem == rocket::TargetOperatingSystem::Windows)
        objectFormat = bytes[0] == 0x64 && bytes[1] == 0x86;
      else if (target.operatingSystem == rocket::TargetOperatingSystem::MacOS)
        objectFormat = bytes[0] == 0xcf && bytes[1] == 0xfa &&
                       bytes[2] == 0xed && bytes[3] == 0xfe;
      else
        objectFormat = bytes[0] == 0x7f && bytes[1] == 'E' && bytes[2] == 'L' &&
                       bytes[3] == 'F' &&
                       (target.architecture == rocket::TargetArchitecture::X64
                            ? bytes[18] == 0x3e
                            : bytes[18] == 0xb7);
      rocket::test::expect(objectFormat,
                           "object format and architecture match " + target.alias,
                           failures);
      std::filesystem::remove(targetObject, removeError);
    }
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
  rocket::Diagnostics mutationDiagnostics;
  auto mutationMir = rocket::test::lowerToMir(
      "fn main() -> Int:\n"
      "    var values = [1, 2]\n"
      "    values[0] = 3\n"
      "    return values[0]\n",
      mutationDiagnostics);
  rocket::test::expect(mutationMir.has_value(),
                       "Array mutation LLVM fixture lowers to MIR", failures);
  if (mutationMir.has_value()) {
    std::string error;
    std::string ir;
    rocket::test::expect(rocket::generateLlvmIr(*mutationMir, false, ir, error),
                         "Array mutation lowers to valid LLVM IR: " + error, failures);
    rocket::test::expect(ir.find("@rocket_rt_array_update_int") != std::string::npos,
                         "Array mutation lowers through the copy-on-write runtime ABI",
                         failures);
  }

  rocket::Diagnostics nativeDiagnostics;
  auto nativeMir = rocket::test::lowerToMir(
      "extern callback Unary(value: Int) -> Int\n"
      "extern fn native_apply(action: Unary, value: Int) -> Int\n"
      "fn twice(value: Int) -> Int:\n"
      "    return value * 2\n"
      "export fn rocket_twice(value: Int) -> Int:\n"
      "    return value * 2\n"
      "fn main() -> Int:\n"
      "    unsafe:\n"
      "        return native_apply(twice, 21)\n",
      nativeDiagnostics);
  rocket::test::expect(nativeMir.has_value(),
                       "native import/export/callback fixture lowers to MIR", failures);
  if (nativeMir.has_value()) {
    std::string error;
    std::string ir;
    rocket::test::expect(rocket::generateLlvmIr(*nativeMir, false, ir, error),
                         "native MIR lowers to LLVM IR: " + error, failures);
    rocket::test::expect(ir.find("declare i64 @native_apply(ptr, i64)") != std::string::npos &&
                             ir.find("define internal i64 @rocket_callback_") != std::string::npos &&
                             ir.find("define dllexport i64 @rocket_twice") != std::string::npos,
                         "LLVM emits stable C declarations, callback trampolines, and exports",
                         failures);
    rocket::TargetError targetError;
    const auto linux = rocket::parseTarget("linux-x64", targetError);
    std::string linuxIr;
    error.clear();
    rocket::test::expect(
        linux && rocket::generateLlvmIr(*nativeMir, false, *linux, linuxIr, error) &&
            linuxIr.find("define dllexport i64 @rocket_twice") ==
                std::string::npos &&
            linuxIr.find("define i64 @rocket_twice") != std::string::npos,
        "non-Windows C exports do not carry PE/COFF dllexport storage", failures);
  }
  rocket::Diagnostics asyncDiagnostics;
  auto asyncMir = rocket::test::lowerToMir(
      "async fn leaf(value: Int) -> Result[Int, String]:\n"
      "    return Ok(value)\n"
      "async fn parent() -> Result[Int, String]:\n"
      "    let result = await leaf(42)\n"
      "    return result\n"
      "fn main() -> Int:\n"
      "    return 0\n",
      asyncDiagnostics);
  rocket::test::expect(asyncMir.has_value(),
                       "async LLVM fixture lowers to MIR", failures);
  if (asyncMir.has_value()) {
    std::string error;
    std::string ir;
    rocket::test::expect(rocket::generateLlvmIr(*asyncMir, false, ir, error),
                         "async MIR lowers to valid LLVM IR: " + error, failures);
    rocket::test::expect(ir.find("@rocket_rt_task_spawn") != std::string::npos &&
                             ir.find("@rocket_rt_task_await") != std::string::npos &&
                             ir.find("rocket_async_entry_") != std::string::npos,
                         "LLVM emits runtime task calls and an owned async entry thunk",
                         failures);
  }
  return rocket::test::finish(failures, "llvm_codegen");
}
