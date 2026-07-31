#include "package.h"
#include "package_git.h"
#include "package_registry.h"
#include "safe_archive.h"
#include "test_support.h"

#include <algorithm>
#include <array>
#include <cstdio>
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

  rocket::test::expect(
      rocket::isValidSpdxExpression("MIT") &&
          rocket::isValidSpdxExpression("Apache-2.0 OR MIT") &&
          rocket::isValidSpdxExpression("GPL-2.0-only WITH Classpath-exception-2.0") &&
          !rocket::isValidSpdxExpression("MIT OR Proprietary") &&
          !rocket::isValidSpdxExpression("MIT AND"),
      "SPDX expressions accept documented licenses, operators, and exceptions",
      failures);
  rocket::GitAcquisition gitAcquisition;
  error.clear();
  rocket::test::expect(
      !rocket::acquireGitPackage("file:///untrusted/repository", std::string(40, 'a'),
                                 workspace, gitAcquisition, error) &&
          error.find("HTTPS") != std::string::npos,
      "remote Git acquisition rejects local protocols before starting git",
      failures);
  error.clear();
  rocket::test::expect(
      !rocket::acquireGitPackage("https://user@example.invalid/repository.git",
                                 "moving-main", workspace, gitAcquisition,
                                 error) &&
          (error.find("credential-free") != std::string::npos ||
           error.find("immutable") != std::string::npos),
      "remote Git acquisition rejects credentials and moving revisions",
      failures);

  const auto archive = std::filesystem::current_path() / "package_test_hostile.tar";
  error.clear();
  rocket::test::expect(
      !rocket::safe_archive::validEntryName("../escape") &&
          !rocket::safe_archive::validEntryName("C:/escape") &&
          !rocket::safe_archive::validEntryName("folder\\escape") &&
          !rocket::safe_archive::validEntryName("CON.txt") &&
          !rocket::safe_archive::validEntryName("trailing. ") &&
          !rocket::safe_archive::create(
              archive.string(), {{"Api.rocket", "a"}, {"api.rocket", "b"}},
              error),
      "archive creation rejects traversal, device paths, and case collisions",
      failures);
  error.clear();
  bool hostileCreated = rocket::safe_archive::create(
      archive.string(), {{"src/main.rocket", "fn main() -> Int:\n    return 0\n"}},
      error);
  if (hostileCreated) {
    std::fstream stream(archive, std::ios::binary | std::ios::in | std::ios::out);
    std::array<char, 512> header{};
    stream.read(header.data(), static_cast<std::streamsize>(header.size()));
    header[156] = '2';
    std::fill(header.begin() + 148, header.begin() + 156, ' ');
    unsigned long long checksum = 0;
    for (char byte : header) checksum += static_cast<unsigned char>(byte);
    std::snprintf(header.data() + 148, 7, "%06llo", checksum);
    header[155] = ' ';
    stream.seekp(0);
    stream.write(header.data(), static_cast<std::streamsize>(header.size()));
    stream.close();
  }
  std::vector<rocket::safe_archive::Entry> hostileEntries;
  error.clear();
  rocket::test::expect(
      hostileCreated &&
          !rocket::safe_archive::readAll(archive.string(), hostileEntries, error) &&
          error.find("non-regular") != std::string::npos,
      "archive reading rejects link entries even with a valid header checksum",
      failures);
  error.clear();
  bool trailingCreated = rocket::safe_archive::create(
      archive.string(), {{"safe.txt", "safe"}}, error);
  if (trailingCreated) {
    std::ofstream trailing(archive, std::ios::binary | std::ios::app);
    std::array<char, 512> garbage{};
    garbage[0] = 'x';
    trailing.write(garbage.data(), static_cast<std::streamsize>(garbage.size()));
  }
  error.clear();
  rocket::test::expect(
      trailingCreated &&
          !rocket::safe_archive::readAll(archive.string(), hostileEntries, error) &&
          error.find("data after") != std::string::npos,
      "archive reading rejects trailing data after the two end blocks", failures);

  const auto phase16 = std::filesystem::current_path() / "package_test_phase16";
  std::filesystem::remove_all(phase16, errorCode);
  std::filesystem::remove(archive, errorCode);
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

  const auto policy = phase16 / "policy";
  const auto nativeDependency = policy / "native_dep";
  std::filesystem::create_directories(nativeDependency / "src", errorCode);
  std::filesystem::create_directories(nativeDependency / "native", errorCode);
  write(nativeDependency / "src/main.rocket",
        "pub fn native_answer() -> Int:\n    return 42\n");
  write(nativeDependency / "native/reviewed.lib", "reviewed native input\n");
  write(nativeDependency / "rocket.toml",
        "[package]\nname = \"native_dep\"\nnamespace = \"vendor\"\n"
        "version = \"1.0.0\"\nlicense = \"MIT\"\n"
        "entry = \"src/main.rocket\"\n\n[native.windows-x64]\n"
        "libraries = \"native/reviewed.lib\"\n");
  std::filesystem::create_directories(policy / "app/src", errorCode);
  write(policy / "app/src/main.rocket", "fn main() -> Int:\n    return 0\n");
  const std::string policyManifest =
      "[package]\nname = \"policy_app\"\nversion = \"1.0.0\"\n"
      "license = \"MIT\"\nentry = \"src/main.rocket\"\n\n"
      "[dependencies]\nnative_dep = \"path:../native_dep\"\n";
  write(policy / "app/rocket.toml", policyManifest);
  error.clear();
  auto policyPackage = rocket::loadPackage(policy / "app", error);
  rocket::PackageLock policyLock;
  bool policyResolved = policyPackage && rocket::resolvePackageDependencies(
                                             *policyPackage, {}, policyLock, error);
  std::vector<rocket::PackageDependencyRoot> policyRoots;
  error.clear();
  rocket::test::expect(
      policyResolved &&
          !rocket::prepareLockedPackageDependencies(
              *policyPackage, false, policyRoots, policyLock, error) &&
          error.find("package-policy.allow-native") != std::string::npos,
      "dependency native inputs require an exact root-package approval", failures);
  write(policy / "app/rocket.toml",
        policyManifest +
            "\n[package-policy]\nallow-native = \"vendor/native_dep@1.0.0\"\n");
  error.clear();
  policyPackage = rocket::loadPackage(policy / "app", error);
  policyRoots.clear();
  rocket::test::expect(
      policyPackage && rocket::prepareLockedPackageDependencies(
                           *policyPackage, false, policyRoots, policyLock, error) &&
          policyRoots.size() == 1 && policyRoots.front().nativeLibraries.size() == 1,
      "an exactly approved native dependency exposes only verified package files: " +
          error,
      failures);

  const auto scriptedDependency = policy / "scripted_dep";
  makeLibrary(scriptedDependency, "scripted_dep", "1.0.0");
  write(scriptedDependency / "rocket.toml",
        "[package]\nname = \"scripted_dep\"\nversion = \"1.0.0\"\n"
        "license = \"MIT\"\nentry = \"src/main.rocket\"\n\n"
        "[build]\nscript = \"tools/generate.rocket\"\n");
  std::filesystem::create_directories(policy / "script_app/src", errorCode);
  write(policy / "script_app/src/main.rocket",
        "fn main() -> Int:\n    return 0\n");
  write(policy / "script_app/rocket.toml",
        "[package]\nname = \"script_app\"\nversion = \"1.0.0\"\n"
        "license = \"MIT\"\nentry = \"src/main.rocket\"\n\n"
        "[dependencies]\nscripted_dep = \"path:../scripted_dep\"\n");
  error.clear();
  auto scriptPackage = rocket::loadPackage(policy / "script_app", error);
  rocket::PackageLock scriptLock;
  bool scriptResolved = scriptPackage && rocket::resolvePackageDependencies(
                                             *scriptPackage, {}, scriptLock, error);
  std::vector<rocket::PackageDependencyRoot> scriptRoots;
  error.clear();
  rocket::test::expect(
      scriptResolved &&
          !rocket::prepareLockedPackageDependencies(
              *scriptPackage, false, scriptRoots, scriptLock, error) &&
          error.find("dependency code is never run implicitly") != std::string::npos,
      "dependency build scripts are rejected before compilation", failures);

  std::filesystem::remove_all(workspace, errorCode);
  std::filesystem::remove_all(invalid, errorCode);
  std::filesystem::remove_all(native, errorCode);
  std::filesystem::remove_all(phase16, errorCode);
  return rocket::test::finish(failures, "package");
}
