#pragma once

#include "target.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace rocket {

struct TargetToolchain {
  std::filesystem::path root;
  std::filesystem::path compiler;
  std::filesystem::path librarian;
  std::filesystem::path runtime;
  std::filesystem::path sysroot;
  std::vector<std::filesystem::path> libraryDirectories;
  bool cross = false;
  bool installedSdk = false;
};

struct TargetToolchainRequest {
  Target host;
  Target target;
  std::filesystem::path compilerDirectory;
  std::optional<std::filesystem::path> configuredSdkRoot;
  std::filesystem::path developmentCompiler;
  std::filesystem::path developmentLibrarian;
  std::filesystem::path developmentRuntime;
  std::optional<std::filesystem::path> nativeSysroot;
};

std::string targetSdkEnvironmentVariable(const Target& target);
std::optional<std::filesystem::path> configuredTargetSdk(
    const Target& target, const std::optional<std::filesystem::path>& cliRoot);
bool discoverTargetToolchain(const TargetToolchainRequest& request,
                             TargetToolchain& toolchain,
                             TargetError& error);

} // namespace rocket
