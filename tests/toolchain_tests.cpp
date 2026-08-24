#include "toolchain.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace {

int failures = 0;

void expect(bool condition, const std::string& message) {
  if (condition) return;
  ++failures;
  std::cerr << "FAIL: " << message << '\n';
}

void touch(const fs::path& path) {
  fs::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary);
  output << "test";
}

rocket::Target parse(const std::string& value) {
  rocket::TargetError error;
  const auto target = rocket::parseTarget(value, error);
  if (!target) {
    std::cerr << "could not parse test target " << value << '\n';
    std::exit(2);
  }
  return *target;
}

} // namespace

int main() {
  expect(rocket::targetSdkEnvironmentVariable(parse("linux-arm64")) ==
             "ROCKET_TARGET_SDK_LINUX_ARM64",
         "target SDK environment name is stable");

  const fs::path root = fs::absolute("phase19-toolchain-test").lexically_normal();
  std::error_code filesystemError;
  fs::remove_all(root, filesystemError);
  const auto windows = parse("windows-x64");
  const auto linux = parse("linux-x64");
  rocket::TargetToolchainRequest request{
      windows, linux, root / "compiler", root / "linux-sdk", {}, {}, {}};
  rocket::TargetToolchain toolchain;
  rocket::TargetError error;
  expect(!rocket::discoverTargetToolchain(request, toolchain, error) &&
             error.code == rocket::DiagnosticCode::TargetToolchain,
         "missing configured SDK is R6003");

  const fs::path sdk = root / "linux-sdk";
#ifdef _WIN32
  constexpr const char* suffix = ".exe";
#else
  constexpr const char* suffix = "";
#endif
  touch(sdk / "bin" / (std::string("clang") + suffix));
  touch(sdk / "bin" / (std::string("llvm-ar") + suffix));
  touch(sdk / "lib" / "rocket_runtime.a");
  fs::create_directories(sdk / "sysroot");
  fs::create_directories(sdk / "share" / "rocket");
  {
    std::ofstream metadata(sdk / "share" / "rocket" / "target.txt");
    metadata << "rocket-target-sdk-1\nalias=linux-x64\n"
                "triple=x86_64-unknown-linux-gnu\n";
  }
  expect(rocket::discoverTargetToolchain(request, toolchain, error),
         "complete explicit Linux SDK is accepted");
  expect(toolchain.cross && toolchain.installedSdk &&
             toolchain.root == sdk && toolchain.sysroot == sdk / "sysroot" &&
             toolchain.libraryDirectories ==
                 std::vector<fs::path>{sdk / "lib"},
         "cross SDK identity, sysroot, and packaged libraries are preserved");

  {
    std::ofstream metadata(sdk / "share" / "rocket" / "target.txt",
                           std::ios::trunc);
    metadata << "rocket-target-sdk-1\nalias=linux-arm64\n"
                "triple=aarch64-unknown-linux-gnu\n";
  }
  expect(!rocket::discoverTargetToolchain(request, toolchain, error) &&
             error.message.find("mismatched target metadata") !=
                 std::string::npos,
         "mismatched SDK target is rejected deterministically");

  const fs::path development = root / "development";
  touch(development / (std::string("clang") + suffix));
  touch(development / (std::string("llvm-lib") + suffix));
  touch(development / "rocket_runtime.lib");
  rocket::TargetToolchainRequest nativeRequest{
      windows, windows, root / "compiler", std::nullopt,
      development / (std::string("clang") + suffix),
      development / (std::string("llvm-lib") + suffix),
      development / "rocket_runtime.lib"};
  expect(rocket::discoverTargetToolchain(nativeRequest, toolchain, error) &&
             !toolchain.cross && !toolchain.installedSdk,
         "native development toolchain fallback remains available");

  fs::remove_all(root, filesystemError);
  if (failures != 0) {
    std::cerr << failures << " toolchain test(s) failed\n";
    return 1;
  }
  std::cout << "toolchain tests passed\n";
  return 0;
}
