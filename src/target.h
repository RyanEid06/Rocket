#pragma once

#include "diagnostic.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rocket {

enum class TargetOperatingSystem { Windows, Linux, MacOS };
enum class TargetArchitecture { X64, Arm64 };
enum class TargetEnvironment { Msvc, Gnu, Apple };

struct Target {
  std::string alias;
  std::string triple;
  TargetOperatingSystem operatingSystem = TargetOperatingSystem::Windows;
  TargetArchitecture architecture = TargetArchitecture::X64;
  TargetEnvironment environment = TargetEnvironment::Msvc;
  int pointerWidth = 64;
  bool littleEndian = true;
  bool productionSupported = true;

  bool operator==(const Target& other) const {
    return alias == other.alias && triple == other.triple;
  }
};

struct TargetError {
  DiagnosticCode code = DiagnosticCode::UnknownTarget;
  std::string message;
};

struct TargetArtifacts {
  std::string executableSuffix;
  std::string dynamicLibrarySuffix;
  std::string staticLibrarySuffix;
  std::string objectSuffix;
};

const std::vector<Target>& productionTargets();
std::optional<Target> parseTarget(std::string_view value, TargetError& error);
std::optional<Target> detectHostTarget(TargetError& error);

std::string targetOperatingSystemName(TargetOperatingSystem value);
std::string targetArchitectureName(TargetArchitecture value);
std::string targetEnvironmentName(TargetEnvironment value);
std::string targetEndiannessName(const Target& target);
std::vector<std::string> targetFeatures(const Target& target);
bool targetHasFeature(const Target& target, std::string_view feature);
TargetArtifacts targetArtifacts(const Target& target);

bool isNativeTarget(const Target& host, const Target& target);
bool supportsCrossCompilation(const Target& host, const Target& target);
bool supportsNativeExecution(const Target& host, const Target& target);

} // namespace rocket
