#include "package.h"
#include "test_support.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

bool write(const std::filesystem::path& path, const std::string& contents) {
  std::ofstream output(path, std::ios::binary);
  output << contents;
  return static_cast<bool>(output);
}

} // namespace

int main() {
  int failures = 0;
  const auto workspace = std::filesystem::current_path() / "package_test_workspace";
  std::error_code errorCode;
  std::filesystem::remove_all(workspace, errorCode);

  std::string error;
  rocket::test::expect(rocket::createPackage(workspace, "sample_package", error),
                       "package scaffolding creates a new package", failures);
  auto package = rocket::loadPackage(workspace, error);
  rocket::test::expect(package.has_value() && package->name == "sample_package" &&
                           package->version == "0.1.0" &&
                           package->entry == workspace / "src/main.rocket",
                       "package manifest resolves stable metadata and entry paths", failures);
  if (package) {
    auto tests = rocket::packageTests(*package, error);
    rocket::test::expect(error.empty() && tests.size() == 1 &&
                             tests.front().filename() == "smoke_test.rocket",
                         "package test discovery is deterministic", failures);
  }

  std::filesystem::create_directories(workspace / ".rocketc", errorCode);
  write(workspace / ".rocketc/ignored.rocket", "fn main() -> Int:\n    return 0\n");
  auto sources = rocket::rocketSources(workspace, error);
  rocket::test::expect(error.empty() && sources.size() == 2,
                       "recursive source discovery ignores generated artifact directories",
                       failures);

  const auto invalid = std::filesystem::current_path() / "package_test_invalid";
  std::filesystem::remove_all(invalid, errorCode);
  std::filesystem::create_directories(invalid / "src", errorCode);
  write(invalid / "src/main.rocket", "fn main() -> Int:\n    return 0\n");
  write(invalid / "rocket.toml",
        "[package]\nname = \"invalid\"\nentry = \"../outside.rocket\"\n");
  error.clear();
  rocket::test::expect(!rocket::loadPackage(invalid, error).has_value() &&
                           error.find("must stay inside") != std::string::npos,
                       "manifest paths cannot escape the package root", failures);

  const auto native = std::filesystem::current_path() / "package_test_native";
  std::filesystem::remove_all(native, errorCode);
  std::filesystem::create_directories(native / "src", errorCode);
  std::filesystem::create_directories(native / "native/lib", errorCode);
  write(native / "src/library.rocket", "export fn answer() -> Int:\n    return 42\n");
  write(native / "native/api.h", "int64_t answer(void);\n");
  write(native / "rocket.toml",
        "[package]\nname = \"native_package\"\nentry = \"src/library.rocket\"\n"
        "[build]\nkind = \"static-library\"\nname = \"native_math\"\n"
        "[native.windows-x64]\nlibraries = \"first.lib; second.lib\"\n"
        "library-search = \"native/lib\"\nheaders = \"native/api.h\"\n");
  error.clear();
  auto nativePackage = rocket::loadPackage(native, error);
  rocket::test::expect(
      nativePackage.has_value() &&
          nativePackage->outputKind == rocket::PackageOutputKind::StaticLibrary &&
          nativePackage->outputName == "native_math" &&
          nativePackage->nativeLibraries.size() == 2 &&
          nativePackage->nativeLibraries[0] == "first.lib" &&
          nativePackage->nativeLibraries[1] == "second.lib" &&
          nativePackage->nativeLibrarySearch.size() == 1 &&
          nativePackage->nativeHeaders.size() == 1,
      "target-aware native inputs and library products load deterministically: " + error,
      failures);

  std::filesystem::remove_all(workspace, errorCode);
  std::filesystem::remove_all(invalid, errorCode);
  std::filesystem::remove_all(native, errorCode);
  return rocket::test::finish(failures, "package");
}
