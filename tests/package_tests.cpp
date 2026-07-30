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

  rocket::test::expect(
      rocket::isValidSemanticVersion("1.2.3") &&
          rocket::isValidSemanticVersion("2.0.0-beta.1+build.7") &&
          !rocket::isValidSemanticVersion("1.2") &&
          !rocket::isValidSemanticVersion("01.2.3") &&
          rocket::semanticVersionSatisfies("1.9.0", "^1.2.3") &&
          !rocket::semanticVersionSatisfies("2.0.0", "^1.2.3") &&
          rocket::semanticVersionSatisfies("1.2.9", "~1.2.3") &&
          !rocket::semanticVersionSatisfies("1.3.0", "~1.2.3"),
      "semantic versions and deterministic constraints follow the Phase 16 contract",
      failures);

  const auto phase16 = std::filesystem::current_path() / "package_test_phase16";
  std::filesystem::remove_all(phase16, errorCode);
  const auto registry = phase16 / "registry";
  const auto makeLibrary = [&](const std::filesystem::path& root,
                               const std::string& name,
                               const std::string& version,
                               const std::string& dependencies = {}) {
    std::filesystem::create_directories(root / "src", errorCode);
    write(root / "src/main.rocket", "pub fn answer() -> Int:\n    return 42\n");
    write(root / "rocket.toml",
          "[package]\nname = \"" + name + "\"\nversion = \"" + version +
              "\"\nlicense = \"MIT\"\nentry = \"src/main.rocket\"\n" +
              dependencies);
  };
  makeLibrary(registry / "utility/1.0.0", "utility", "1.0.0");
  makeLibrary(registry / "math/1.0.0", "math", "1.0.0",
              "\n[dependencies]\nutility = \"^1.0.0\"\n");
  makeLibrary(registry / "math/1.2.0", "math", "1.2.0",
              "\n[dependencies]\nutility = \"^1.0.0\"\n");
  makeLibrary(phase16 / "local_text", "local_text", "0.4.0");
  std::filesystem::create_directories(phase16 / "app/src", errorCode);
  write(phase16 / "app/src/main.rocket", "fn main() -> Int:\n    return 0\n");
  write(phase16 / "app/rocket.toml",
        "[package]\nname = \"phase16_app\"\nversion = \"1.6.0\"\n"
        "license = \"MIT\"\nentry = \"src/main.rocket\"\n"
        "registry = \"../registry\"\n\n[dependencies]\n"
        "local_text = \"path:../local_text\"\nmath = \"^1.0.0\"\n");
  error.clear();
  auto phase16Package = rocket::loadPackage(phase16 / "app", error);
  rocket::PackageLock phase16Lock;
  bool resolved = phase16Package.has_value() &&
                  rocket::resolvePackageDependencies(*phase16Package, {},
                                                     phase16Lock, error);
  bool selectedNewest = false;
  for (const auto& locked : phase16Lock.packages)
    if (locked.name == "math" && locked.version == "1.2.0") selectedNewest = true;
  rocket::test::expect(
      resolved && phase16Lock.packages.size() == 3 && selectedNewest &&
          std::filesystem::is_regular_file(phase16 / "app/rocket.lock"),
      "resolver selects the highest compatible graph and writes a lockfile: " + error,
      failures);
  rocket::PackageLock readLock;
  error.clear();
  rocket::test::expect(
      rocket::readPackageLock(phase16 / "app/rocket.lock", readLock, error) &&
          rocket::packageDependencyTree(readLock).find("utility@1.0.0") !=
              std::string::npos,
      "lockfiles round-trip and dependency trees include transitive packages: " + error,
      failures);
  std::string auditReport;
  error.clear();
  rocket::test::expect(
      phase16Package &&
          rocket::auditPackageDependencies(*phase16Package, readLock,
                                           auditReport, error) &&
          auditReport.find("SHA-256 cache verified") != std::string::npos,
      "dependency audit verifies checksums and registry license metadata: " + error,
      failures);
  error.clear();
  rocket::PackageLock offlineLock;
  rocket::test::expect(
      phase16Package && rocket::resolvePackageDependencies(
                              *phase16Package, {.offline = true},
                              offlineLock, error),
      "offline resolution reproduces the committed graph from cache: " + error,
      failures);
  if (phase16Package && !readLock.packages.empty()) {
    const auto poisoned = phase16 / "app/.rocketc/cache/sha256" /
                          readLock.packages.front().checksum / "rocket.toml";
    std::ofstream output(poisoned, std::ios::binary | std::ios::app);
    output << "# tampered\n";
    output.close();
    error.clear();
    rocket::PackageLock poisonedLock;
    rocket::test::expect(
        !rocket::resolvePackageDependencies(*phase16Package, {.offline = true},
                                            poisonedLock, error) &&
            error.find("checksum mismatch") != std::string::npos,
        "offline resolution rejects a poisoned content-addressed cache", failures);
  }

  const auto conflict = phase16 / "conflict";
  makeLibrary(conflict / "common_v1", "common", "1.0.0");
  makeLibrary(conflict / "common_v2", "common", "2.0.0");
  makeLibrary(conflict / "left", "left", "1.0.0",
              "\n[dependencies]\ncommon = \"path:../common_v1\"\n");
  makeLibrary(conflict / "right", "right", "1.0.0",
              "\n[dependencies]\ncommon = \"path:../common_v2\"\n");
  std::filesystem::create_directories(conflict / "app/src", errorCode);
  write(conflict / "app/src/main.rocket", "fn main() -> Int:\n    return 0\n");
  write(conflict / "app/rocket.toml",
        "[package]\nname = \"conflict_app\"\nversion = \"1.0.0\"\n"
        "entry = \"src/main.rocket\"\n\n[dependencies]\n"
        "left = \"path:../left\"\nright = \"path:../right\"\n");
  error.clear();
  auto conflictPackage = rocket::loadPackage(conflict / "app", error);
  rocket::PackageLock conflictLock;
  rocket::test::expect(
      conflictPackage &&
          !rocket::resolvePackageDependencies(*conflictPackage, {}, conflictLock,
                                              error) &&
          error.find("duplicate-version conflict") != std::string::npos,
      "resolver rejects graphs containing two versions of one package name",
      failures);

  std::filesystem::remove_all(workspace, errorCode);
  std::filesystem::remove_all(invalid, errorCode);
  std::filesystem::remove_all(native, errorCode);
  std::filesystem::remove_all(phase16, errorCode);
  return rocket::test::finish(failures, "package");
}
