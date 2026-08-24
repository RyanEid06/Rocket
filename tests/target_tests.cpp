#include "target.h"
#include "module_loader.h"
#include "test_support.h"

#include <algorithm>
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
  rocket::TargetError error;

  const auto& targets = rocket::productionTargets();
  rocket::test::expect(targets.size() == 4,
                       "four production targets have a stable order", failures);

  for (const auto& expected : targets) {
    auto alias = rocket::parseTarget(expected.alias, error);
    rocket::test::expect(alias.has_value() && *alias == expected,
                         "target alias normalizes: " + expected.alias, failures);
    auto triple = rocket::parseTarget(expected.triple, error);
    rocket::test::expect(triple.has_value() && *triple == expected,
                         "target triple normalizes: " + expected.triple, failures);
    rocket::test::expect(
        rocket::targetFeatures(expected).size() == 4 &&
            rocket::targetHasFeature(expected, "threads") &&
            rocket::targetHasFeature(expected, "dynamic-libraries") &&
            !rocket::targetHasFeature(expected, "host-cpu"),
        "target features are explicit and reject ambient host features",
        failures);
  }

  rocket::test::expect(
      !rocket::parseTarget("WINDOWS-X64", error).has_value() &&
          error.code == rocket::DiagnosticCode::UnknownTarget &&
          error.message == "unknown target 'WINDOWS-X64'",
      "target spelling and R6001 diagnostic are deterministic", failures);
  rocket::test::expect(
      !rocket::parseTarget("windows-arm64", error).has_value() &&
          error.code == rocket::DiagnosticCode::UnsupportedTarget &&
          error.message.find("not production-supported") != std::string::npos,
      "Windows ARM64 is recognized evaluation work with R6002", failures);

  const auto windows = rocket::parseTarget("windows-x64", error);
  const auto linuxX64 = rocket::parseTarget("linux-x64", error);
  const auto linuxArm64 = rocket::parseTarget("linux-arm64", error);
  const auto macOS = rocket::parseTarget("macos-arm64", error);
  rocket::test::expect(
      windows && linuxX64 && linuxArm64 && macOS &&
          rocket::targetArtifacts(*windows).executableSuffix == ".exe" &&
          rocket::targetArtifacts(*linuxX64).dynamicLibrarySuffix == ".so" &&
          rocket::targetArtifacts(*linuxArm64).objectSuffix == ".o" &&
          rocket::targetArtifacts(*macOS).dynamicLibrarySuffix == ".dylib",
      "artifact suffixes derive from the normalized target", failures);

  rocket::test::expect(
      windows && linuxX64 && linuxArm64 && macOS &&
          rocket::supportsCrossCompilation(*windows, *linuxX64) &&
          rocket::supportsCrossCompilation(*windows, *linuxArm64) &&
          !rocket::supportsCrossCompilation(*windows, *macOS) &&
          rocket::supportsCrossCompilation(*linuxX64, *windows) &&
          rocket::supportsCrossCompilation(*linuxX64, *linuxArm64) &&
          !rocket::supportsNativeExecution(*linuxX64, *linuxArm64),
      "cross-compilation and execution policy matches TARGETS.md", failures);

  const auto host = rocket::detectHostTarget(error);
  rocket::test::expect(host.has_value() && host->productionSupported,
                       "the current native host normalizes to a production row",
                       failures);

  const auto selectionRoot =
      std::filesystem::current_path() / "target_test_source_selection";
  std::error_code filesystemError;
  std::filesystem::remove_all(selectionRoot, filesystemError);
  const auto linuxRoot = selectionRoot / "targets/linux-arm64";
  const auto inactiveWindowsRoot = selectionRoot / "targets/windows-x64";
  std::filesystem::create_directories(linuxRoot, filesystemError);
  std::filesystem::create_directories(inactiveWindowsRoot, filesystemError);
  write(selectionRoot / "main.rocket",
        "import platform\nfn main() -> Int:\n    return platform.answer()\n");
  write(selectionRoot / "platform.rocket", "this portable file is inactive !\n");
  write(linuxRoot / "platform.rocket",
        "pub fn answer() -> Int:\n    return 42\n");
  write(inactiveWindowsRoot / "platform.rocket",
        "this other target is also inactive !\n");
  rocket::Diagnostics selectionDiagnostics;
  const auto selectedModule = rocket::loadModuleGraph(
      selectionRoot / "main.rocket", selectionRoot, linuxRoot, {},
      selectionDiagnostics);
  bool selectedLinuxSource = false;
  if (selectedModule) {
    selectedLinuxSource = std::any_of(
        selectedModule->functions.begin(), selectedModule->functions.end(),
        [&](const rocket::Function& function) {
          return function.name == "platform.answer" &&
                 std::filesystem::path(function.location.file).lexically_normal() ==
                     (linuxRoot / "platform.rocket").lexically_normal();
        });
  }
  rocket::test::expect(
      selectedModule.has_value() && !selectionDiagnostics.hasErrors() &&
          selectedLinuxSource,
      "target source precedence excludes inactive invalid modules before parsing",
      failures);
  std::filesystem::remove_all(selectionRoot, filesystemError);

  rocket::test::expect(
      rocket::diagnosticCodeName(rocket::DiagnosticCode::UnknownTarget) ==
              "R6001" &&
          rocket::diagnosticCodeName(rocket::DiagnosticCode::UnsupportedTarget) ==
              "R6002" &&
          rocket::diagnosticCodeName(rocket::DiagnosticCode::TargetToolchain) ==
              "R6003" &&
          rocket::diagnosticCodeName(
              rocket::DiagnosticCode::HostTargetOperation) == "R6004" &&
          rocket::diagnosticCodeName(rocket::DiagnosticCode::TargetManifest) ==
              "R6005",
      "target diagnostic identities are stable", failures);

  return rocket::test::finish(failures, "target");
}
