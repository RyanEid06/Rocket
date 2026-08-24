#include "target.h"

#include <algorithm>

namespace rocket {
namespace {

const Target WindowsX64{
    "windows-x64", "x86_64-pc-windows-msvc", TargetOperatingSystem::Windows,
    TargetArchitecture::X64, TargetEnvironment::Msvc, 64, true, true};
const Target LinuxX64{
    "linux-x64", "x86_64-unknown-linux-gnu", TargetOperatingSystem::Linux,
    TargetArchitecture::X64, TargetEnvironment::Gnu, 64, true, true};
const Target LinuxArm64{
    "linux-arm64", "aarch64-unknown-linux-gnu", TargetOperatingSystem::Linux,
    TargetArchitecture::Arm64, TargetEnvironment::Gnu, 64, true, true};
const Target MacOSArm64{
    "macos-arm64", "arm64-apple-macosx", TargetOperatingSystem::MacOS,
    TargetArchitecture::Arm64, TargetEnvironment::Apple, 64, true, true};
const Target WindowsArm64{
    "windows-arm64", "aarch64-pc-windows-msvc",
    TargetOperatingSystem::Windows, TargetArchitecture::Arm64,
    TargetEnvironment::Msvc, 64, true, false};

bool matches(const Target& target, const std::string_view value) {
  return value == target.alias || value == target.triple;
}

} // namespace

const std::vector<Target>& productionTargets() {
  static const std::vector<Target> targets{
      WindowsX64, LinuxX64, LinuxArm64, MacOSArm64};
  return targets;
}

std::optional<Target> parseTarget(const std::string_view value,
                                  TargetError& error) {
  error = {};
  for (const Target& target : productionTargets()) {
    if (matches(target, value)) return target;
  }
  if (matches(WindowsArm64, value)) {
    error.code = DiagnosticCode::UnsupportedTarget;
    error.message =
        "target 'windows-arm64' is recognized but is not production-supported";
    return std::nullopt;
  }
  error.code = DiagnosticCode::UnknownTarget;
  error.message = "unknown target '" + std::string(value) + "'";
  return std::nullopt;
}

std::optional<Target> detectHostTarget(TargetError& error) {
#if defined(_WIN32) && defined(_M_X64)
  return parseTarget("windows-x64", error);
#elif defined(_WIN32) && (defined(_M_ARM64) || defined(__aarch64__) || defined(__arm64__))
  return parseTarget("windows-arm64", error);
#elif defined(__linux__) && defined(__x86_64__)
  return parseTarget("linux-x64", error);
#elif defined(__linux__) && (defined(__aarch64__) || defined(__arm64__))
  return parseTarget("linux-arm64", error);
#elif defined(__APPLE__) && defined(__MACH__) && (defined(__aarch64__) || defined(__arm64__))
  return parseTarget("macos-arm64", error);
#else
  error.code = DiagnosticCode::UnsupportedTarget;
  error.message = "the native host is not a production-supported Rocket target";
  return std::nullopt;
#endif
}

std::string targetOperatingSystemName(const TargetOperatingSystem value) {
  switch (value) {
  case TargetOperatingSystem::Windows: return "windows";
  case TargetOperatingSystem::Linux: return "linux";
  case TargetOperatingSystem::MacOS: return "macos";
  }
  return {};
}

std::string targetArchitectureName(const TargetArchitecture value) {
  switch (value) {
  case TargetArchitecture::X64: return "x64";
  case TargetArchitecture::Arm64: return "arm64";
  }
  return {};
}

std::string targetEnvironmentName(const TargetEnvironment value) {
  switch (value) {
  case TargetEnvironment::Msvc: return "msvc";
  case TargetEnvironment::Gnu: return "gnu";
  case TargetEnvironment::Apple: return "apple";
  }
  return {};
}

std::string targetEndiannessName(const Target& target) {
  return target.littleEndian ? "little" : "big";
}

std::vector<std::string> targetFeatures(const Target& target) {
  std::vector<std::string> result{"threads", "dynamic-libraries"};
  result.push_back(target.operatingSystem == TargetOperatingSystem::Windows
                       ? "codeview"
                       : "dwarf");
  result.push_back(target.architecture == TargetArchitecture::X64 ? "sse2"
                                                                  : "neon");
  return result;
}

bool targetHasFeature(const Target& target, const std::string_view feature) {
  const auto features = targetFeatures(target);
  return std::find(features.begin(), features.end(), feature) != features.end();
}

TargetArtifacts targetArtifacts(const Target& target) {
  switch (target.operatingSystem) {
  case TargetOperatingSystem::Windows:
    return {".exe", ".dll", ".lib", ".obj"};
  case TargetOperatingSystem::Linux:
    return {"", ".so", ".a", ".o"};
  case TargetOperatingSystem::MacOS:
    return {"", ".dylib", ".a", ".o"};
  }
  return {};
}

bool isNativeTarget(const Target& host, const Target& target) {
  return host == target;
}

bool supportsCrossCompilation(const Target& host, const Target& target) {
  if (isNativeTarget(host, target)) return true;
  if (host.alias == "windows-x64") {
    return target.alias == "linux-x64" || target.alias == "linux-arm64";
  }
  if (host.alias == "linux-x64") {
    return target.alias == "linux-arm64" || target.alias == "windows-x64";
  }
  return false;
}

bool supportsNativeExecution(const Target& host, const Target& target) {
  return isNativeTarget(host, target);
}

} // namespace rocket
