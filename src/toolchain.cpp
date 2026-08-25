#include "toolchain.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace rocket {
namespace {

#ifdef _WIN32
constexpr std::string_view ExecutableSuffix = ".exe";
#else
constexpr std::string_view ExecutableSuffix = "";
#endif

std::filesystem::path executable(const std::filesystem::path& directory,
                                 const std::string_view name) {
  return directory / (std::string(name) + std::string(ExecutableSuffix));
}

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

bool targetMetadata(const std::filesystem::path& root, const Target& target) {
  std::ifstream input(root / "share" / "rocket" / "target.txt");
  if (!input) return false;
  std::string schema;
  std::string alias;
  std::string triple;
  std::getline(input, schema);
  std::getline(input, alias);
  std::getline(input, triple);
  std::string trailing;
  while (std::getline(input, trailing)) {
    if (!trim(trailing).empty()) return false;
  }
  return trim(schema) == "rocket-target-sdk-1" &&
         trim(alias) == "alias=" + target.alias &&
         trim(triple) == "triple=" + target.triple;
}

bool regular(const std::filesystem::path& path) {
  std::error_code error;
  return std::filesystem::is_regular_file(path, error) && !error;
}

bool directory(const std::filesystem::path& path) {
  std::error_code error;
  return std::filesystem::is_directory(path, error) && !error;
}

std::filesystem::path runtimePath(const std::filesystem::path& root,
                                  const Target& target) {
  return root / "lib" /
         ("rocket_runtime" + targetArtifacts(target).staticLibrarySuffix);
}

bool installedLayout(const std::filesystem::path& root,
                     const TargetToolchainRequest& request,
                     const bool requireMetadata, TargetToolchain& result) {
  if (!directory(root) ||
      (requireMetadata && !targetMetadata(root, request.target))) {
    return false;
  }
  const auto bin = root / "bin";
  const auto compiler = executable(bin, "clang");
  const auto librarian = executable(
      bin, request.target.operatingSystem == TargetOperatingSystem::Windows
               ? "llvm-lib"
               : "llvm-ar");
  const auto runtime = runtimePath(root, request.target);
  if (!regular(compiler) || !regular(librarian) || !regular(runtime))
    return false;

  result = {};
  result.root = std::filesystem::absolute(root).lexically_normal();
  result.compiler = compiler;
  result.librarian = librarian;
  result.runtime = runtime;
  result.cross = !isNativeTarget(request.host, request.target);
  result.installedSdk = true;
  const auto sysroot = root / "sysroot";
  if (directory(sysroot)) result.sysroot = sysroot;
  if (request.target.operatingSystem == TargetOperatingSystem::Windows) {
    for (const char* name : {"msvc", "ucrt", "um"}) {
      const auto candidate = root / "lib" / name;
      if (directory(candidate)) result.libraryDirectories.push_back(candidate);
    }
  } else {
    const auto nativeLibraries = root / "lib";
    if (directory(nativeLibraries))
      result.libraryDirectories.push_back(nativeLibraries);
  }
  return true;
}

bool validateCrossInputs(const TargetToolchainRequest& request,
                         const TargetToolchain& result) {
  if (isNativeTarget(request.host, request.target)) return true;
  if (request.target.operatingSystem == TargetOperatingSystem::Linux)
    return directory(result.sysroot);
  if (request.target.operatingSystem == TargetOperatingSystem::Windows)
    return result.libraryDirectories.size() == 3;
  return false;
}

bool applyNativeMacOSSysroot(const TargetToolchainRequest& request,
                             TargetToolchain& result, TargetError& error) {
  if (!isNativeTarget(request.host, request.target) ||
      request.target.operatingSystem != TargetOperatingSystem::MacOS ||
      !result.sysroot.empty()) {
    return true;
  }
  if (!request.nativeSysroot || request.nativeSysroot->empty()) {
    error.code = DiagnosticCode::TargetToolchain;
    error.message =
        "native macOS SDK root is not configured; set ROCKET_MACOS_SDK_ROOT";
    return false;
  }
  const auto sysroot =
      std::filesystem::absolute(*request.nativeSysroot).lexically_normal();
  if (!directory(sysroot)) {
    error.code = DiagnosticCode::TargetToolchain;
    error.message = "native macOS SDK root does not exist: '" +
                    sysroot.string() + "'";
    return false;
  }
  result.sysroot = sysroot;
  return true;
}

} // namespace

std::string targetSdkEnvironmentVariable(const Target& target) {
  std::string result = "ROCKET_TARGET_SDK_";
  for (const char character : target.alias) {
    result.push_back(character == '-'
                         ? '_'
                         : static_cast<char>(std::toupper(
                               static_cast<unsigned char>(character))));
  }
  return result;
}

std::optional<std::filesystem::path> configuredTargetSdk(
    const Target& target, const std::optional<std::filesystem::path>& cliRoot) {
  if (cliRoot && !cliRoot->empty())
    return std::filesystem::absolute(*cliRoot).lexically_normal();
  const std::string name = targetSdkEnvironmentVariable(target);
#ifdef _WIN32
  char* value = nullptr;
  std::size_t size = 0;
  if (_dupenv_s(&value, &size, name.c_str()) != 0 || value == nullptr)
    return std::nullopt;
  const std::filesystem::path result =
      std::filesystem::absolute(std::filesystem::u8path(value)).lexically_normal();
  std::free(value);
  return result;
#else
  const char* value = std::getenv(name.c_str());
  if (value == nullptr || *value == '\0') return std::nullopt;
  return std::filesystem::absolute(std::filesystem::u8path(value))
      .lexically_normal();
#endif
}

bool discoverTargetToolchain(const TargetToolchainRequest& request,
                             TargetToolchain& toolchain,
                             TargetError& error) {
  toolchain = {};
  error = {};
  const bool native = isNativeTarget(request.host, request.target);
  if (!native && !supportsCrossCompilation(request.host, request.target)) {
    error.code = DiagnosticCode::HostTargetOperation;
    error.message = "host '" + request.host.alias + "' cannot produce target '" +
                    request.target.alias + "'";
    return false;
  }

  const auto configured = configuredTargetSdk(
      request.target, request.configuredSdkRoot);
  if (configured) {
    if (!installedLayout(*configured, request, true, toolchain) ||
        !validateCrossInputs(request, toolchain)) {
      error.code = DiagnosticCode::TargetToolchain;
      error.message = "configured target SDK for '" + request.target.alias +
                      "' is incomplete or has mismatched target metadata";
      return false;
    }
    return applyNativeMacOSSysroot(request, toolchain, error);
  }

  if (!native) {
    error.code = DiagnosticCode::TargetToolchain;
    error.message = "target SDK for '" + request.target.alias +
                    "' is not configured; use --target-sdk or " +
                    targetSdkEnvironmentVariable(request.target);
    return false;
  }

  const auto installRoot = request.compilerDirectory.parent_path();
  if (installedLayout(installRoot, request, false, toolchain))
    return applyNativeMacOSSysroot(request, toolchain, error);

  // Rocket 2.0 placed toolchain files beside the compiler. Keep this fallback
  // for source compatibility while all Rocket 2.1 packages use the root layout.
  const auto legacyCompiler =
      executable(request.compilerDirectory / "toolchain", "clang");
  const auto legacyLibrarian = executable(
      request.compilerDirectory / "toolchain",
      request.target.operatingSystem == TargetOperatingSystem::Windows
          ? "llvm-lib"
          : "llvm-ar");
  const auto legacyRuntime = request.compilerDirectory /
      ("rocket_runtime" + targetArtifacts(request.target).staticLibrarySuffix);
  if (regular(legacyCompiler) && regular(legacyLibrarian) &&
      regular(legacyRuntime)) {
    toolchain.compiler = legacyCompiler;
    toolchain.librarian = legacyLibrarian;
    toolchain.runtime = legacyRuntime;
    return applyNativeMacOSSysroot(request, toolchain, error);
  }

  if (regular(request.developmentCompiler) &&
      regular(request.developmentLibrarian) &&
      regular(request.developmentRuntime)) {
    toolchain.compiler = request.developmentCompiler;
    toolchain.librarian = request.developmentLibrarian;
    toolchain.runtime = request.developmentRuntime;
    return applyNativeMacOSSysroot(request, toolchain, error);
  }

  error.code = DiagnosticCode::TargetToolchain;
  error.message = "native toolchain for '" + request.target.alias +
                  "' is incomplete; install the Rocket SDK or configure its "
                  "compiler, librarian, and runtime";
  return false;
}

} // namespace rocket
