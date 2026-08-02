#include "package.h"
#include "package_registry.h"
#include "package_git.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <unordered_set>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#endif

namespace rocket {

bool package_detail::commitCacheTransaction(
    const std::filesystem::path& temporary,
    const std::filesystem::path& destination, std::error_code& error) {
#ifdef _WIN32
  constexpr int maximumAttempts = 80;
  for (int attempt = 0; attempt < maximumAttempts; ++attempt) {
    error.clear();
    std::filesystem::rename(temporary, destination, error);
    if (!error) return true;
    const DWORD windowsError = static_cast<DWORD>(error.value());
    const bool transient = windowsError == ERROR_ACCESS_DENIED ||
                           windowsError == ERROR_SHARING_VIOLATION ||
                           windowsError == ERROR_LOCK_VIOLATION;
    if (!transient || attempt + 1 == maximumAttempts) return false;
    Sleep(25);
  }
  return false;
#else
  std::filesystem::rename(temporary, destination, error);
  return !error;
#endif
}

namespace {

constexpr std::uintmax_t MaximumManifestBytes = 1024U * 1024U;
constexpr std::size_t MaximumManifestLineBytes = 64U * 1024U;
constexpr std::size_t MaximumManifestEntries = 4096U;
constexpr std::size_t MaximumManifestDependencies = 1024U;
constexpr std::size_t MaximumDiscoveredSources = 4096U;
constexpr std::uintmax_t MaximumDiscoveredSourceBytes = 64U * 1024U * 1024U;

std::string trim(std::string value) {
  const std::size_t first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const std::size_t last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::string withoutComment(const std::string& line) {
  bool quoted = false;
  bool escaped = false;
  for (std::size_t index = 0; index < line.size(); ++index) {
    const char character = line[index];
    if (escaped) { escaped = false; continue; }
    if (quoted && character == '\\') { escaped = true; continue; }
    if (character == '"') { quoted = !quoted; continue; }
    if (!quoted && character == '#') return line.substr(0, index);
  }
  return line;
}

bool quotedValue(const std::string& text, std::string& value) {
  const std::string clean = trim(text);
  if (clean.size() < 2 || clean.front() != '"' || clean.back() != '"') return false;
  value.clear();
  bool escaped = false;
  for (std::size_t index = 1; index + 1 < clean.size(); ++index) {
    const char character = clean[index];
    if (escaped) {
      if (character == 'n') value.push_back('\n');
      else if (character == 't') value.push_back('\t');
      else if (character == '"' || character == '\\') value.push_back(character);
      else return false;
      escaped = false;
    } else if (character == '\\') {
      escaped = true;
    } else if (character == '"') {
      return false;
    } else {
      value.push_back(character);
    }
  }
  return !escaped;
}

bool validName(const std::string& name) {
  if (name.empty() || (!std::isalpha(static_cast<unsigned char>(name.front())) &&
                       name.front() != '_')) return false;
  return std::all_of(name.begin() + 1, name.end(), [](char character) {
    return std::isalnum(static_cast<unsigned char>(character)) ||
           character == '_' || character == '-';
  });
}

struct SemanticVersion {
  std::uint64_t major = 0;
  std::uint64_t minor = 0;
  std::uint64_t patch = 0;
  std::string prerelease;
};

bool parseNumber(const std::string& text, std::uint64_t& value) {
  if (text.empty() || (text.size() > 1 && text.front() == '0')) return false;
  value = 0;
  for (const char character : text) {
    if (!std::isdigit(static_cast<unsigned char>(character))) return false;
    const std::uint64_t digit = static_cast<std::uint64_t>(character - '0');
    if (value > (UINT64_MAX - digit) / 10) return false;
    value = value * 10 + digit;
  }
  return true;
}

bool validIdentifierList(const std::string& text, bool numericLeadingZeros) {
  if (text.empty()) return false;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t end = text.find('.', start);
    const std::string item = text.substr(
        start, end == std::string::npos ? std::string::npos : end - start);
    if (item.empty()) return false;
    bool numeric = true;
    for (const char character : item) {
      if (!std::isalnum(static_cast<unsigned char>(character)) && character != '-')
        return false;
      if (!std::isdigit(static_cast<unsigned char>(character))) numeric = false;
    }
    if (numericLeadingZeros && numeric && item.size() > 1 && item.front() == '0')
      return false;
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return true;
}

bool parseSemanticVersion(const std::string& text, SemanticVersion& version) {
  if (text.empty() || text.front() == 'v' || text.front() == 'V') return false;
  const std::size_t plus = text.find('+');
  const std::string withoutBuild = text.substr(0, plus);
  if (plus != std::string::npos &&
      !validIdentifierList(text.substr(plus + 1), false)) return false;
  const std::size_t dash = withoutBuild.find('-');
  const std::string core = withoutBuild.substr(0, dash);
  version.prerelease = dash == std::string::npos ? std::string{}
                                                  : withoutBuild.substr(dash + 1);
  if (dash != std::string::npos &&
      !validIdentifierList(version.prerelease, true)) return false;
  const std::size_t first = core.find('.');
  const std::size_t second = first == std::string::npos
                                 ? std::string::npos
                                 : core.find('.', first + 1);
  if (first == std::string::npos || second == std::string::npos ||
      core.find('.', second + 1) != std::string::npos) return false;
  return parseNumber(core.substr(0, first), version.major) &&
         parseNumber(core.substr(first + 1, second - first - 1), version.minor) &&
         parseNumber(core.substr(second + 1), version.patch);
}

int compareIdentifiers(const std::string& left, const std::string& right) {
  const auto digits = [](const std::string& value) {
    return !value.empty() &&
           std::all_of(value.begin(), value.end(), [](const char character) {
             return std::isdigit(static_cast<unsigned char>(character));
           });
  };
  const bool leftNumeric = digits(left);
  const bool rightNumeric = digits(right);
  if (leftNumeric && rightNumeric) {
    if (left.size() != right.size()) return left.size() < right.size() ? -1 : 1;
  } else if (leftNumeric != rightNumeric) {
    return leftNumeric ? -1 : 1;
  }
  if (left == right) return 0;
  return left < right ? -1 : 1;
}

int compareSemanticVersions(const SemanticVersion& left,
                            const SemanticVersion& right) {
  if (left.major != right.major) return left.major < right.major ? -1 : 1;
  if (left.minor != right.minor) return left.minor < right.minor ? -1 : 1;
  if (left.patch != right.patch) return left.patch < right.patch ? -1 : 1;
  if (left.prerelease.empty() != right.prerelease.empty())
    return left.prerelease.empty() ? 1 : -1;
  if (left.prerelease.empty()) return 0;
  auto identifiers = [](const std::string& value) {
    std::vector<std::string> result;
    std::size_t start = 0;
    while (start <= value.size()) {
      const std::size_t end = value.find('.', start);
      result.push_back(value.substr(
          start, end == std::string::npos ? std::string::npos : end - start));
      if (end == std::string::npos) break;
      start = end + 1;
    }
    return result;
  };
  const auto leftParts = identifiers(left.prerelease);
  const auto rightParts = identifiers(right.prerelease);
  const std::size_t count = std::min(leftParts.size(), rightParts.size());
  for (std::size_t index = 0; index < count; ++index) {
    const int compared = compareIdentifiers(leftParts[index], rightParts[index]);
    if (compared != 0) return compared;
  }
  if (leftParts.size() == rightParts.size()) return 0;
  return leftParts.size() < rightParts.size() ? -1 : 1;
}

bool containedPath(const std::filesystem::path& root,
                   const std::filesystem::path& candidate) {
  const auto relative = candidate.lexically_relative(root);
  return !relative.empty() && *relative.begin() != "..";
}

std::vector<std::string> listValue(const std::string& text) {
  std::vector<std::string> result;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t separator = text.find(';', start);
    const std::string item = trim(text.substr(
        start, separator == std::string::npos ? std::string::npos : separator - start));
    if (!item.empty()) result.push_back(item);
    if (separator == std::string::npos) break;
    start = separator + 1;
  }
  return result;
}

bool write(const std::filesystem::path& path, const std::string& contents,
           std::string& error) {
  std::ofstream output(path, std::ios::binary);
  if (!output) { error = "could not create '" + path.string() + "'"; return false; }
  output << contents;
  if (!output) { error = "could not write '" + path.string() + "'"; return false; }
  return true;
}

bool ignoredDirectory(const std::filesystem::path& path) {
  const std::string name = path.filename().string();
  return name == ".git" || name == ".rocketc" || name == "out" ||
         name == "build" || name == "dependencies" || name == "node_modules";
}

bool semanticRequirementMatches(const std::string& versionText,
                                const std::string& requirementText) {
  SemanticVersion version;
  if (!parseSemanticVersion(versionText, version)) return false;
  std::string operation;
  std::string requestedText = trim(requirementText);
  for (const std::string candidate : {">=", "<=", "^", "~", ">", "<", "="}) {
    if (requestedText.starts_with(candidate)) {
      operation = candidate;
      requestedText = trim(requestedText.substr(candidate.size()));
      break;
    }
  }
  SemanticVersion requested;
  if (!parseSemanticVersion(requestedText, requested)) return false;
  if (!version.prerelease.empty() && requested.prerelease.empty()) return false;
  const int lower = compareSemanticVersions(version, requested);
  if (operation.empty() || operation == "=") return lower == 0;
  if (operation == ">=") return lower >= 0;
  if (operation == ">") return lower > 0;
  if (operation == "<=") return lower <= 0;
  if (operation == "<") return lower < 0;
  SemanticVersion upper = requested;
  upper.prerelease.clear();
  if (operation == "~") {
    if (upper.minor == (std::numeric_limits<std::uint64_t>::max)()) {
      if (upper.major == (std::numeric_limits<std::uint64_t>::max)())
        return lower >= 0;
      ++upper.major;
      upper.minor = 0;
    } else {
      ++upper.minor;
    }
    upper.patch = 0;
  } else if (requested.major != 0) {
    if (upper.major == (std::numeric_limits<std::uint64_t>::max)())
      return lower >= 0;
    ++upper.major;
    upper.minor = 0;
    upper.patch = 0;
  } else if (requested.minor != 0) {
    if (upper.minor == (std::numeric_limits<std::uint64_t>::max)()) {
      upper.major = 1;
      upper.minor = 0;
    } else {
      ++upper.minor;
    }
    upper.patch = 0;
  } else {
    if (upper.patch == (std::numeric_limits<std::uint64_t>::max)()) {
      upper.minor = 1;
      upper.patch = 0;
    } else {
      ++upper.patch;
    }
  }
  return lower >= 0 && compareSemanticVersions(version, upper) < 0;
}

bool parseDependencySpec(const std::string& name, const std::string& value,
                         PackageDependency& dependency, std::string& error) {
  if (!validName(name)) {
    error = "dependency names must be valid Rocket package names";
    return false;
  }
  dependency.name = name;
  if (value.starts_with("path:")) {
    dependency.source = DependencySourceKind::Path;
    dependency.location = value.substr(5);
    if (dependency.location.empty() ||
        std::filesystem::path(dependency.location).is_absolute() ||
        dependency.location.find("://") != std::string::npos) {
      error = "path dependency '" + name +
              "' requires a relative filesystem location";
      return false;
    }
    return true;
  }
  if (value.starts_with("git:")) {
    dependency.source = DependencySourceKind::Git;
    const std::string git = value.substr(4);
    const std::size_t hash = git.rfind('#');
    if (hash == std::string::npos) {
      error = "Git dependency '" + name + "' must pin an immutable revision";
      return false;
    }
    dependency.location = git.substr(0, hash);
    dependency.revision = git.substr(hash + 1);
    const bool validRevision =
        (dependency.revision.size() == 40 || dependency.revision.size() == 64) &&
        std::all_of(dependency.revision.begin(), dependency.revision.end(),
                    [](const char character) {
                      return std::isxdigit(static_cast<unsigned char>(character));
                    });
    if (dependency.location.empty() || !validRevision) {
      error = "Git dependency '" + name +
              "' requires a 40- or 64-digit hexadecimal revision";
      return false;
    }
    return true;
  }
  dependency.source = DependencySourceKind::Registry;
  dependency.requirement = value;
  if (!semanticRequirementMatches("0.0.0", value) &&
      !semanticRequirementMatches("1.0.0", value) &&
      !semanticRequirementMatches("999.999.999", value)) {
    std::string requested = trim(value);
    for (const std::string prefix : {">=", "<=", "^", "~", ">", "<", "="}) {
      if (requested.starts_with(prefix)) {
        requested = trim(requested.substr(prefix.size()));
        break;
      }
    }
    SemanticVersion parsed;
    if (!parseSemanticVersion(requested, parsed)) {
      error = "registry dependency '" + name +
              "' has an invalid semantic-version requirement";
      return false;
    }
  }
  return true;
}

} // namespace

bool isValidSemanticVersion(const std::string& version) {
  SemanticVersion parsed;
  return parseSemanticVersion(version, parsed);
}

bool semanticVersionSatisfies(const std::string& version,
                              const std::string& requirement) {
  return semanticRequirementMatches(version, requirement);
}

int compareSemanticVersionText(const std::string& left,
                               const std::string& right) {
  SemanticVersion parsedLeft;
  SemanticVersion parsedRight;
  if (!parseSemanticVersion(left, parsedLeft) ||
      !parseSemanticVersion(right, parsedRight))
    return 0;
  return compareSemanticVersions(parsedLeft, parsedRight);
}

std::optional<Package> loadPackage(const std::filesystem::path& path,
                                   std::string& error) {
  const std::filesystem::path supplied = std::filesystem::absolute(path).lexically_normal();
  const std::filesystem::path manifest =
      supplied.filename() == "rocket.toml" ? supplied : supplied / "rocket.toml";
  std::error_code manifestSizeError;
  const std::uintmax_t manifestSize =
      std::filesystem::file_size(manifest, manifestSizeError);
  if (!manifestSizeError && manifestSize > MaximumManifestBytes) {
    error = manifest.string() + ": manifest exceeds the 1 MiB limit";
    return {};
  }
  std::ifstream input(manifest, std::ios::binary);
  if (!input) { error = "could not read package manifest '" + manifest.string() + "'"; return {}; }

  Package package;
  package.root = manifest.parent_path();
  package.version = "0.1.0";
  package.entry = "src/main.rocket";
  package.tests = "tests";
  std::string section;
  std::unordered_set<std::string> seen;
  std::string line;
  int lineNumber = 0;
  while (std::getline(input, line)) {
    ++lineNumber;
    if (line.size() > MaximumManifestLineBytes) {
      error = manifest.string() + ":" + std::to_string(lineNumber) +
              ": manifest line exceeds the 64 KiB limit";
      return {};
    }
    const std::string clean = trim(withoutComment(line));
    if (clean.empty()) continue;
    if (clean.front() == '[' && clean.back() == ']') {
      section = trim(clean.substr(1, clean.size() - 2));
      if (section != "package" && section != "test" && section != "build" &&
          section != "native.windows-x64" && section != "dependencies" &&
          section != "package-policy") {
        error = manifest.string() + ":" + std::to_string(lineNumber) +
                ": unsupported manifest section '" + section + "'";
        return {};
      }
      continue;
    }
    const std::size_t equal = clean.find('=');
    if (equal == std::string::npos) {
      error = manifest.string() + ":" + std::to_string(lineNumber) +
              ": expected key = \"value\"";
      return {};
    }
    const std::string key = trim(clean.substr(0, equal));
    const std::string qualified = section + "." + key;
    std::string value;
    if (!quotedValue(clean.substr(equal + 1), value)) {
      error = manifest.string() + ":" + std::to_string(lineNumber) +
              ": manifest values must be quoted strings";
      return {};
    }
    if (!seen.insert(qualified).second) {
      error = manifest.string() + ":" + std::to_string(lineNumber) +
              ": duplicate manifest key '" + qualified + "'";
      return {};
    }
    if (seen.size() > MaximumManifestEntries) {
      error = manifest.string() + ": manifest exceeds the 4096-entry limit";
      return {};
    }
    if (qualified == "package.name") package.name = value;
    else if (qualified == "package.namespace") package.namespaceName = value;
    else if (qualified == "package.version") package.version = value;
    else if (qualified == "package.license") package.license = value;
    else if (qualified == "package.registry") package.registry = value;
    else if (qualified == "package.registry-key") package.registryKey = value;
    else if (qualified == "package.entry") package.entry = value;
    else if (section == "dependencies") {
      PackageDependency dependency;
      std::string dependencyError;
      if (!parseDependencySpec(key, value, dependency, dependencyError)) {
        error = manifest.string() + ":" + std::to_string(lineNumber) +
                ": " + dependencyError;
        return {};
      }
      package.dependencies.push_back(std::move(dependency));
      if (package.dependencies.size() > MaximumManifestDependencies) {
        error = manifest.string() + ": manifest exceeds the 1024-dependency limit";
        return {};
      }
    }
    else if (qualified == "test.directory") package.tests = value;
    else if (qualified == "build.kind") {
      if (value == "executable") package.outputKind = PackageOutputKind::Executable;
      else if (value == "static-library") package.outputKind = PackageOutputKind::StaticLibrary;
      else if (value == "dynamic-library") package.outputKind = PackageOutputKind::DynamicLibrary;
      else {
        error = manifest.string() + ":" + std::to_string(lineNumber) +
                ": build.kind must be executable, static-library, or dynamic-library";
        return {};
      }
    }
    else if (qualified == "build.name") package.outputName = value;
    else if (qualified == "build.script") package.buildScript = value;
    else if (qualified == "package-policy.allowed-licenses")
      package.allowedLicenses = listValue(value);
    else if (qualified == "package-policy.allow-native")
      package.allowedNativeDependencies = listValue(value);
    else if (qualified == "package-policy.deny-yanked") {
      if (value != "true" && value != "false") {
        error = manifest.string() + ":" + std::to_string(lineNumber) +
                ": package-policy.deny-yanked must be \"true\" or \"false\"";
        return {};
      }
      package.denyYanked = value == "true";
    }
    else if (qualified == "package-policy.build-scripts")
      package.buildScripts = value;
    else if (qualified == "native.windows-x64.libraries")
      package.nativeLibraries = listValue(value);
    else if (qualified == "native.windows-x64.library-search") {
      for (const auto& item : listValue(value))
        package.nativeLibrarySearch.push_back(item);
    }
    else if (qualified == "native.windows-x64.headers") {
      for (const auto& item : listValue(value)) package.nativeHeaders.push_back(item);
    }
    else {
      error = manifest.string() + ":" + std::to_string(lineNumber) +
              ": unsupported manifest key '" + qualified + "'";
      return {};
    }
  }
  if (!validName(package.name)) {
    error = manifest.string() + ": package.name must be a valid Rocket package name";
    return {};
  }
  if (!validName(package.namespaceName)) {
    error = manifest.string() +
            ": package.namespace must be a valid Rocket registry namespace";
    return {};
  }
  if (package.buildScripts != "deny") {
    error = manifest.string() +
            ": Rocket 1.6 package-policy.build-scripts must be \"deny\"";
    return {};
  }
  if (!isValidSemanticVersion(package.version)) {
    error = manifest.string() + ": package.version must be semantic MAJOR.MINOR.PATCH";
    return {};
  }
  std::sort(package.dependencies.begin(), package.dependencies.end(),
            [](const PackageDependency& left, const PackageDependency& right) {
              return left.name < right.name;
            });
  if (package.outputName.empty())
    package.outputName = package.outputKind == PackageOutputKind::Executable
                             ? "main"
                             : package.name;
  if (!validName(package.outputName)) {
    error = manifest.string() + ": build.name must be a valid native artifact name";
    return {};
  }
  const auto entry = (package.root / package.entry).lexically_normal();
  const auto tests = (package.root / package.tests).lexically_normal();
  if (!containedPath(package.root, entry) || !containedPath(package.root, tests)) {
    error = manifest.string() + ": entry and test paths must stay inside the package";
    return {};
  }
  package.entry = entry;
  package.tests = tests;
  auto resolveContained = [&](std::filesystem::path path, const char* category)
      -> std::optional<std::filesystem::path> {
    if (path.is_absolute()) {
      error = manifest.string() + ": " + category + " paths must be package-relative";
      return std::nullopt;
    }
    auto resolved = (package.root / path).lexically_normal();
    if (!containedPath(package.root, resolved)) {
      error = manifest.string() + ": " + category + " paths must stay inside the package";
      return std::nullopt;
    }
    return resolved;
  };
  for (auto& search : package.nativeLibrarySearch) {
    auto resolved = resolveContained(search, "native library search");
    if (!resolved) return {};
    search = *resolved;
  }
  for (auto& header : package.nativeHeaders) {
    auto resolved = resolveContained(header, "native header");
    if (!resolved) return {};
    header = *resolved;
    if (!std::filesystem::is_regular_file(header)) {
      error = "native header does not exist: '" + header.string() + "'";
      return {};
    }
  }
  if (!std::filesystem::is_regular_file(package.entry)) {
    error = "package entry does not exist: '" + package.entry.string() + "'";
    return {};
  }
  return package;
}

bool createPackage(const std::filesystem::path& directory,
                   const std::string& requestedName, std::string& error) {
  const std::filesystem::path root = std::filesystem::absolute(directory).lexically_normal();
  const std::string name = requestedName.empty() ? root.filename().string() : requestedName;
  if (!validName(name)) { error = "package name must start with a letter or '_' and use letters, digits, '_' or '-'"; return false; }
  std::error_code filesystemError;
  if (std::filesystem::exists(root, filesystemError) &&
      !std::filesystem::is_empty(root, filesystemError)) {
    error = "destination already exists and is not empty: '" + root.string() + "'";
    return false;
  }
  std::filesystem::create_directories(root / "src", filesystemError);
  if (filesystemError) { error = "could not create package source directory"; return false; }
  std::filesystem::create_directories(root / "tests", filesystemError);
  if (filesystemError) { error = "could not create package test directory"; return false; }
  const std::string manifest = "[package]\nname = \"" + name +
      "\"\nversion = \"0.1.0\"\nentry = \"src/main.rocket\"\n\n"
      "[test]\ndirectory = \"tests\"\n\n"
      "[build]\nkind = \"executable\"\nname = \"" + name + "\"\n";
  if (!write(root / "rocket.toml", manifest, error)) return false;
  if (!write(root / "src/main.rocket",
             "fn main() -> Int:\n    print(\"Hello from " + name + "\")\n    return 0\n", error)) return false;
  return write(root / "tests/smoke_test.rocket",
               "fn main() -> Int:\n    return 0\n", error);
}

std::vector<std::filesystem::path> rocketSources(const std::filesystem::path& path,
                                                 std::string& error) {
  const std::filesystem::path absolute = std::filesystem::absolute(path).lexically_normal();
  if (std::filesystem::is_regular_file(absolute)) {
    if (absolute.extension() != ".rocket") error = "source files must use the .rocket extension";
    return error.empty() ? std::vector<std::filesystem::path>{absolute}
                         : std::vector<std::filesystem::path>{};
  }
  if (!std::filesystem::is_directory(absolute)) {
    error = "path does not exist: '" + absolute.string() + "'";
    return {};
  }
  std::vector<std::filesystem::path> result;
  std::uintmax_t totalBytes = 0;
  std::error_code filesystemError;
  std::filesystem::recursive_directory_iterator iterator(
      absolute, std::filesystem::directory_options::skip_permission_denied, filesystemError);
  const std::filesystem::recursive_directory_iterator end;
  while (iterator != end) {
    if (filesystemError) { error = filesystemError.message(); return {}; }
    if (iterator->is_directory() && ignoredDirectory(iterator->path())) {
      iterator.disable_recursion_pending();
    } else if (iterator->is_regular_file() && iterator->path().extension() == ".rocket") {
      const std::uintmax_t size = iterator->file_size(filesystemError);
      if (filesystemError) {
        error = filesystemError.message();
        return {};
      }
      if (result.size() >= MaximumDiscoveredSources) {
        error = "source discovery exceeds the 4096-file limit";
        return {};
      }
      if (size > MaximumDiscoveredSourceBytes - totalBytes) {
        error = "source discovery exceeds the 64 MiB limit";
        return {};
      }
      totalBytes += size;
      result.push_back(iterator->path().lexically_normal());
    }
    iterator.increment(filesystemError);
  }
  std::sort(result.begin(), result.end());
  return result;
}

namespace {

std::vector<std::filesystem::path> packageContentFiles(
    const std::filesystem::path& root, std::string& error) {
  std::vector<std::filesystem::path> files;
  std::error_code filesystemError;
  std::filesystem::recursive_directory_iterator iterator(
      root, std::filesystem::directory_options::skip_permission_denied,
      filesystemError);
  const std::filesystem::recursive_directory_iterator end;
  while (iterator != end) {
    if (filesystemError) {
      error = "could not inspect package source: " + filesystemError.message();
      return {};
    }
    const auto status = iterator->symlink_status(filesystemError);
    if (filesystemError) {
      error = "could not inspect package source entry: " +
              filesystemError.message();
      return {};
    }
    if (std::filesystem::is_symlink(status)) {
      error = "package sources cannot contain symbolic links: '" +
              iterator->path().string() + "'";
      return {};
    }
    if (std::filesystem::is_directory(status) &&
        ignoredDirectory(iterator->path())) {
      iterator.disable_recursion_pending();
    } else if (std::filesystem::is_regular_file(status)) {
      files.push_back(iterator->path().lexically_normal());
    }
    iterator.increment(filesystemError);
  }
  std::sort(files.begin(), files.end(), [&](const auto& left, const auto& right) {
    return left.lexically_relative(root).generic_string() <
           right.lexically_relative(root).generic_string();
  });
  return files;
}

#ifdef _WIN32
class Sha256 {
public:
  Sha256() {
    if (BCryptOpenAlgorithmProvider(&algorithm_, BCRYPT_SHA256_ALGORITHM,
                                    nullptr, 0) != 0) return;
    DWORD objectLength = 0;
    DWORD received = 0;
    if (BCryptGetProperty(algorithm_, BCRYPT_OBJECT_LENGTH,
                          reinterpret_cast<PUCHAR>(&objectLength),
                          sizeof(objectLength), &received, 0) != 0) return;
    object_.resize(objectLength);
    if (BCryptCreateHash(algorithm_, &hash_, object_.data(), objectLength,
                         nullptr, 0, 0) != 0) return;
    valid_ = true;
  }

  ~Sha256() {
    if (hash_ != nullptr) BCryptDestroyHash(hash_);
    if (algorithm_ != nullptr) BCryptCloseAlgorithmProvider(algorithm_, 0);
  }

  bool update(const void* data, std::size_t size) {
    if (!valid_ || size > ULONG_MAX) return false;
    return BCryptHashData(hash_,
                          reinterpret_cast<PUCHAR>(const_cast<void*>(data)),
                          static_cast<ULONG>(size), 0) == 0;
  }

  bool finish(std::string& result) {
    std::array<unsigned char, 32> bytes{};
    if (!valid_ || BCryptFinishHash(hash_, bytes.data(),
                                    static_cast<ULONG>(bytes.size()), 0) != 0)
      return false;
    static constexpr char hexadecimal[] = "0123456789abcdef";
    result.clear();
    result.reserve(bytes.size() * 2);
    for (const unsigned char byte : bytes) {
      result.push_back(hexadecimal[byte >> 4]);
      result.push_back(hexadecimal[byte & 15]);
    }
    return true;
  }

private:
  BCRYPT_ALG_HANDLE algorithm_ = nullptr;
  BCRYPT_HASH_HANDLE hash_ = nullptr;
  std::vector<unsigned char> object_;
  bool valid_ = false;
};
#endif

bool sha256Package(const std::filesystem::path& root, std::string& checksum,
                   std::string& error) {
#ifndef _WIN32
  error = "secure package hashing is currently supported on Windows x64 only";
  return false;
#else
  const auto files = packageContentFiles(root, error);
  if (!error.empty()) return false;
  Sha256 hash;
  std::array<char, 64 * 1024> buffer{};
  const char separator = '\0';
  for (const auto& path : files) {
    const std::string relative = path.lexically_relative(root).generic_string();
    if (!hash.update(relative.data(), relative.size()) ||
        !hash.update(&separator, 1)) {
      error = "could not hash package source path";
      return false;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
      error = "could not read package source '" + path.string() + "'";
      return false;
    }
    while (input) {
      input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
      const std::streamsize count = input.gcount();
      if (count > 0 &&
          !hash.update(buffer.data(), static_cast<std::size_t>(count))) {
        error = "could not hash package source contents";
        return false;
      }
    }
    if (!input.eof() || !hash.update(&separator, 1)) {
      error = "could not read package source while hashing";
      return false;
    }
  }
  if (!hash.finish(checksum)) {
    error = "Windows SHA-256 provider failed";
    return false;
  }
  return true;
#endif
}

bool copyPackageContent(const std::filesystem::path& source,
                        const std::filesystem::path& destination,
                        std::string& error) {
  const auto files = packageContentFiles(source, error);
  if (!error.empty()) return false;
  std::error_code filesystemError;
  std::filesystem::create_directories(destination, filesystemError);
  if (filesystemError) {
    error = "could not create package cache: " + filesystemError.message();
    return false;
  }
  for (const auto& path : files) {
    const auto target = destination / path.lexically_relative(source);
    std::filesystem::create_directories(target.parent_path(), filesystemError);
    if (filesystemError) {
      error = "could not create package cache directory: " +
              filesystemError.message();
      return false;
    }
    std::filesystem::copy_file(path, target,
                               std::filesystem::copy_options::none,
                               filesystemError);
    if (filesystemError) {
      error = "could not cache package source: " + filesystemError.message();
      return false;
    }
  }
  return true;
}

bool ensureCached(const std::filesystem::path& cacheRoot,
                  const std::filesystem::path& source,
                  const std::string& checksum, std::string& error) {
  const auto destination = cacheRoot / checksum;
  if (std::filesystem::exists(destination)) {
    std::string actual;
    if (!sha256Package(destination, actual, error)) return false;
    if (actual != checksum) {
      error = "cached package checksum mismatch for sha256:" + checksum;
      return false;
    }
    return true;
  }
  std::error_code filesystemError;
  std::filesystem::create_directories(cacheRoot, filesystemError);
  if (filesystemError) {
    error = "could not create content-addressed cache: " +
            filesystemError.message();
    return false;
  }
#ifdef _WIN32
  const auto processIdentifier = static_cast<unsigned long>(GetCurrentProcessId());
#else
  const auto processIdentifier = 0UL;
#endif
  const std::string suffix = ".partial-" + std::to_string(processIdentifier);
  const auto temporary = cacheRoot / (checksum + suffix);
  if (std::filesystem::exists(temporary)) {
    error = "incomplete cache transaction already exists: '" +
            temporary.string() + "'";
    return false;
  }
  auto discardTemporary = [&]() {
    std::error_code ignored;
    std::filesystem::remove_all(temporary, ignored);
  };
  if (!copyPackageContent(source, temporary, error)) {
    discardTemporary();
    return false;
  }
  std::string copiedChecksum;
  if (!sha256Package(temporary, copiedChecksum, error)) {
    discardTemporary();
    return false;
  }
  if (copiedChecksum != checksum) {
    discardTemporary();
    error = "package source changed while it was being cached";
    return false;
  }
  if (!package_detail::commitCacheTransaction(temporary, destination,
                                               filesystemError)) {
    if (std::filesystem::exists(destination)) {
      std::string installedChecksum;
      std::string installedError;
      if (sha256Package(destination, installedChecksum, installedError) &&
          installedChecksum == checksum) {
        discardTemporary();
        return true;
      }
    }
    discardTemporary();
    error = "could not commit package cache transaction: " +
            filesystemError.message();
    return false;
  }
  return true;
}

std::string joinList(const std::vector<std::string>& values) {
  std::string result;
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) result += ';';
    result += values[index];
  }
  return result;
}

std::string quoteLock(const std::string& value) {
  std::string result = "\"";
  for (const char character : value) {
    if (character == '\n') result += "\\n";
    else if (character == '\t') result += "\\t";
    else {
      if (character == '\\' || character == '"') result.push_back('\\');
      result.push_back(character);
    }
  }
  result.push_back('"');
  return result;
}

std::string renderPackageLock(PackageLock lock) {
  std::sort(lock.rootDependencies.begin(), lock.rootDependencies.end());
  std::sort(lock.packages.begin(), lock.packages.end(),
            [](const LockedPackage& left, const LockedPackage& right) {
              if (left.name != right.name) return left.name < right.name;
              return left.version < right.version;
            });
  std::ostringstream output;
  output << "# Generated by rocketc. Commit this file.\n"
         << "lock-version = \"2\"\n"
         << "root-namespace = " << quoteLock(lock.rootNamespace) << "\n"
         << "root = " << quoteLock(lock.rootName) << "\n"
         << "root-version = " << quoteLock(lock.rootVersion) << "\n"
         << "root-dependencies = " << quoteLock(joinList(lock.rootDependencies))
         << "\n";
  for (auto& package : lock.packages) {
    std::sort(package.dependencies.begin(), package.dependencies.end());
    output << "\n[[package]]\n"
           << "name = " << quoteLock(package.name) << "\n"
           << "namespace = " << quoteLock(package.namespaceName) << "\n"
           << "version = " << quoteLock(package.version) << "\n"
           << "source = " << quoteLock(package.source) << "\n"
           << "checksum = " << quoteLock("sha256:" + package.checksum) << "\n"
           << "license = " << quoteLock(package.license) << "\n"
           << "registry-key = " << quoteLock(package.registryKey) << "\n"
           << "publisher = " << quoteLock(package.publisher) << "\n"
           << "yanked = " << quoteLock(package.yanked ? "true" : "false") << "\n"
           << "dependencies = " << quoteLock(joinList(package.dependencies))
           << "\n";
  }
  return output.str();
}

bool validChecksum(const std::string& value) {
  return value.size() == 64 &&
         std::all_of(value.begin(), value.end(), [](const char character) {
           return std::isdigit(static_cast<unsigned char>(character)) ||
                  (character >= 'a' && character <= 'f');
         });
}

std::filesystem::path localSourcePath(const std::filesystem::path& parent,
                                      const std::string& location,
                                      std::string& error) {
  if (location.find("://") != std::string::npos &&
      !location.starts_with("file://")) {
    error = "network source requires a populated lockfile cache; direct network "
            "transport is not enabled in this Phase 16 foundation";
    return {};
  }
  std::string local = location.starts_with("file://") ? location.substr(7)
                                                       : location;
#ifdef _WIN32
  if (local.size() >= 3 && local.front() == '/' &&
      std::isalpha(static_cast<unsigned char>(local[1])) && local[2] == ':')
    local.erase(local.begin());
#endif
  std::filesystem::path path(local);
  if (path.is_relative()) path = parent / path;
  return std::filesystem::absolute(path).lexically_normal();
}

std::string registryFromEnvironment() {
#ifdef _WIN32
  char* value = nullptr;
  std::size_t size = 0;
  if (_dupenv_s(&value, &size, "ROCKET_REGISTRY") != 0 || value == nullptr)
    return {};
  const std::string result(value);
  std::free(value);
  return result;
#else
  const char* value = std::getenv("ROCKET_REGISTRY");
  return value == nullptr ? std::string{} : std::string(value);
#endif
}

struct ResolverContext {
  std::filesystem::path cacheRoot;
  std::map<std::string, std::size_t> selected;
  std::set<std::string> resolving;
};

struct RegistrySelectionCleanup {
  RegistrySelection* selection = nullptr;
  bool* active = nullptr;
  ~RegistrySelectionCleanup() {
    if (selection != nullptr && active != nullptr && *active)
      discardRegistrySelection(*selection);
  }
};

struct GitAcquisitionCleanup {
  GitAcquisition* acquisition = nullptr;
  bool* active = nullptr;
  ~GitAcquisitionCleanup() {
    if (acquisition != nullptr && active != nullptr && *active)
      discardGitAcquisition(*acquisition);
  }
};

bool resolveDependency(const PackageDependency& dependency,
                       const Package& parent, const std::string& inheritedRegistry,
                       const std::string& inheritedRegistryIdentity,
                       const std::string& inheritedRegistryKey,
                       ResolverContext& context, PackageLock& lock,
                       std::string& identifier, std::string& error) {
  std::filesystem::path sourceRoot;
  std::string sourceIdentity;
  std::string registry = inheritedRegistry.empty() ? parent.registry
                                                    : inheritedRegistry;
  std::string registryIdentity = inheritedRegistryIdentity.empty()
                                     ? parent.registry
                                     : inheritedRegistryIdentity;
  std::string registryKey = inheritedRegistryKey.empty() ? parent.registryKey
                                                          : inheritedRegistryKey;
  RegistrySelection registrySelection;
  bool temporaryRegistrySource = false;
  RegistrySelectionCleanup registryCleanup{&registrySelection,
                                            &temporaryRegistrySource};
  GitAcquisition gitAcquisition;
  bool temporaryGitSource = false;
  GitAcquisitionCleanup gitCleanup{&gitAcquisition, &temporaryGitSource};
  if (dependency.source == DependencySourceKind::Path) {
    sourceRoot = localSourcePath(parent.root, dependency.location, error);
    sourceIdentity = "path:" + dependency.location;
  } else if (dependency.source == DependencySourceKind::Git) {
    sourceIdentity = "git:" + dependency.location + "#" + dependency.revision;
    if (dependency.location.starts_with("https://")) {
      if (!acquireGitPackage(
              dependency.location, dependency.revision,
              context.cacheRoot.parent_path() / "acquire", gitAcquisition,
              error))
        return false;
      sourceRoot = gitAcquisition.sourceRoot;
      temporaryGitSource = true;
    } else {
      sourceRoot = localSourcePath(parent.root, dependency.location, error);
    }
    if (!temporaryGitSource && error.empty() && !sourceRoot.empty()) {
      std::ifstream revisionInput(sourceRoot / ".rocket-revision", std::ios::binary);
      std::ostringstream revisionBuffer;
      if (revisionInput) revisionBuffer << revisionInput.rdbuf();
      const std::string revision = revisionBuffer.str();
      if (!revisionInput.is_open()) {
        error = "local Git export for '" + dependency.name +
                "' must contain .rocket-revision";
      } else if (trim(revision) != dependency.revision) {
        error = "Git revision mismatch for dependency '" + dependency.name + "'";
      }
    }
  } else {
    if (registry.empty()) {
      const std::string configured = registryFromEnvironment();
      if (!configured.empty()) {
        registry = configured;
        registryIdentity = configured;
      }
    }
    if (registry.empty()) {
      error = "registry dependency '" + dependency.name +
              "' requires package.registry or ROCKET_REGISTRY";
      return false;
    }
    bool signedRegistry = registry.starts_with("https://");
    std::filesystem::path registryRoot;
    if (!signedRegistry) {
      registryRoot = localSourcePath(parent.root, registry, error);
      if (!error.empty()) return false;
      signedRegistry = std::filesystem::is_regular_file(registryRoot / "registry.toml");
    }
    if (signedRegistry) {
      if (!acquireRegistryPackage(
              parent.root, registry, registryKey, dependency.name,
              dependency.requirement, context.cacheRoot.parent_path() / "acquire",
              registrySelection, error))
        return false;
      sourceRoot = registrySelection.sourceRoot;
      sourceIdentity = registrySelection.sourceIdentity;
      temporaryRegistrySource = true;
    } else {
      registry = registryRoot.string();
      const auto packageRoot = registryRoot / dependency.name;
      std::vector<std::pair<SemanticVersion, std::filesystem::path>> candidates;
      std::error_code filesystemError;
      if (std::filesystem::is_directory(packageRoot)) {
        for (const auto& entry : std::filesystem::directory_iterator(
                 packageRoot, std::filesystem::directory_options::skip_permission_denied,
                 filesystemError)) {
          SemanticVersion parsed;
          const std::string candidate = entry.path().filename().string();
          if (!filesystemError && entry.is_directory() &&
              parseSemanticVersion(candidate, parsed) &&
              semanticRequirementMatches(candidate, dependency.requirement))
            candidates.push_back({parsed, entry.path()});
        }
      }
      if (filesystemError) {
        error = "could not inspect registry package '" + dependency.name + "': " +
                filesystemError.message();
        return false;
      }
      if (candidates.empty()) {
        error = "no registry version of '" + dependency.name + "' satisfies '" +
                dependency.requirement + "'";
        return false;
      }
      std::sort(candidates.begin(), candidates.end(), [](const auto& left,
                                                          const auto& right) {
        return compareSemanticVersions(left.first, right.first) > 0;
      });
      sourceRoot = candidates.front().second;
      sourceIdentity = "registry:" + registryIdentity + "/" + dependency.name;
    }
  }
  if (!error.empty()) return false;
  if (!std::filesystem::is_directory(sourceRoot)) {
    error = "dependency source does not exist for '" + dependency.name + "': '" +
            sourceRoot.string() + "'";
    return false;
  }
  std::error_code sourceStatusError;
  if (std::filesystem::is_symlink(
          std::filesystem::symlink_status(sourceRoot, sourceStatusError))) {
    error = "dependency source roots cannot be symbolic links: '" +
            sourceRoot.string() + "'";
    return false;
  }
  if (sourceStatusError) {
    error = "could not inspect dependency source root: " +
            sourceStatusError.message();
    return false;
  }
  auto resolved = loadPackage(sourceRoot, error);
  if (!resolved) return false;
  if (resolved->name != dependency.name) {
    error = "dependency key '" + dependency.name + "' resolved package '" +
            resolved->name + "'";
    return false;
  }
  if (dependency.source == DependencySourceKind::Registry &&
      !temporaryRegistrySource &&
      sourceRoot.filename().string() != resolved->version) {
    error = "registry directory version does not match manifest for '" +
            dependency.name + "'";
    return false;
  }
  if (dependency.source == DependencySourceKind::Registry &&
      !semanticRequirementMatches(resolved->version, dependency.requirement)) {
    error = "resolved version " + resolved->version + " of '" + dependency.name +
            "' does not satisfy '" + dependency.requirement + "'";
    return false;
  }
  if (temporaryRegistrySource &&
      (resolved->namespaceName != registrySelection.namespaceName ||
       resolved->license != registrySelection.license ||
       resolved->version != registrySelection.version)) {
    error = "signed registry metadata does not match the package manifest";
    return false;
  }
  std::string checksum;
  if (!sha256Package(sourceRoot, checksum, error)) return false;
  identifier = resolved->name + "@" + resolved->version;
  if (context.resolving.contains(resolved->name)) {
    error = "dependency cycle includes package '" + resolved->name + "'";
    return false;
  }
  const auto existing = context.selected.find(resolved->name);
  if (existing != context.selected.end()) {
    const auto& selected = lock.packages[existing->second];
    if (selected.version != resolved->version || selected.checksum != checksum) {
      error = "duplicate-version conflict for package '" + resolved->name +
              "': selected " + selected.version + " but also resolved " +
              resolved->version;
      return false;
    }
    return true;
  }
  if (!context.resolving.insert(resolved->name).second) {
    error = "dependency cycle includes package '" + resolved->name + "'";
    return false;
  }
  if (!ensureCached(context.cacheRoot, sourceRoot, checksum, error)) return false;
  LockedPackage locked{resolved->name, resolved->version, sourceIdentity, checksum,
                       resolved->license, {}};
  if (temporaryRegistrySource) {
    locked.namespaceName = registrySelection.namespaceName;
    locked.registryKey = registrySelection.registryKey;
    locked.publisher = registrySelection.publisher;
    locked.yanked = registrySelection.yanked;
  } else {
    locked.namespaceName = resolved->namespaceName;
  }
  const std::size_t index = lock.packages.size();
  lock.packages.push_back(std::move(locked));
  context.selected[resolved->name] = index;
  const std::string nextRegistry = registry.empty() ? resolved->registry : registry;
  const std::string nextRegistryIdentity = registryIdentity.empty()
                                               ? resolved->registry
                                               : registryIdentity;
  const std::string nextRegistryKey = registryKey.empty()
                                          ? resolved->registryKey
                                          : registryKey;
  for (const auto& child : resolved->dependencies) {
    std::string childIdentifier;
    if (!resolveDependency(child, *resolved, nextRegistry,
                           nextRegistryIdentity, nextRegistryKey, context, lock,
                           childIdentifier, error)) return false;
    lock.packages[index].dependencies.push_back(childIdentifier);
  }
  std::sort(lock.packages[index].dependencies.begin(),
            lock.packages[index].dependencies.end());
  context.resolving.erase(resolved->name);
  return true;
}

bool verifyOfflineCache(const Package& package, const PackageLock& lock,
                        std::string& error) {
  const auto cacheRoot = package.root / ".rocketc/cache/sha256";
  for (const auto& locked : lock.packages) {
    if (!validChecksum(locked.checksum)) {
      error = "lockfile contains an invalid checksum for '" + locked.name + "'";
      return false;
    }
    const auto cached = cacheRoot / locked.checksum;
    if (!std::filesystem::is_directory(cached)) {
      error = "offline cache is missing " + locked.name + "@" + locked.version +
              " (sha256:" + locked.checksum + ")";
      return false;
    }
    std::string actual;
    if (!sha256Package(cached, actual, error)) return false;
    if (actual != locked.checksum) {
      error = "cached package checksum mismatch for " + locked.name + "@" +
              locked.version;
      return false;
    }
  }
  return true;
}

std::vector<std::string> splitList(const std::string& value) {
  return listValue(value);
}

} // namespace

bool writePackageLock(const std::filesystem::path& path, const PackageLock& lock,
                      std::string& error) {
  return write(path, renderPackageLock(lock), error);
}

bool readPackageLock(const std::filesystem::path& path, PackageLock& lock,
                     std::string& error) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    error = "could not read package lockfile '" + path.string() + "'";
    return false;
  }
  lock = {};
  LockedPackage* current = nullptr;
  bool sawLockVersion = false;
  std::string line;
  int lineNumber = 0;
  while (std::getline(input, line)) {
    ++lineNumber;
    const std::string clean = trim(withoutComment(line));
    if (clean.empty()) continue;
    if (clean == "[[package]]") {
      lock.packages.push_back({});
      current = &lock.packages.back();
      continue;
    }
    const std::size_t equal = clean.find('=');
    std::string value;
    if (equal == std::string::npos ||
        !quotedValue(clean.substr(equal + 1), value)) {
      error = path.string() + ":" + std::to_string(lineNumber) +
              ": invalid lockfile entry";
      return false;
    }
    const std::string key = trim(clean.substr(0, equal));
    if (current == nullptr) {
      if (key == "lock-version" && value != "1" && value != "2") {
        error = "unsupported package lockfile version '" + value + "'";
        return false;
      }
      if (key == "lock-version") {
        sawLockVersion = true;
        lock.formatVersion = value == "2" ? 2 : 1;
      }
      if (key == "root-namespace") lock.rootNamespace = value;
      else if (key == "root") lock.rootName = value;
      else if (key == "root-version") lock.rootVersion = value;
      else if (key == "root-dependencies") lock.rootDependencies = splitList(value);
      else if (key != "lock-version") {
        error = "unsupported lockfile key '" + key + "'";
        return false;
      }
    } else if (key == "name") current->name = value;
    else if (key == "namespace") current->namespaceName = value;
    else if (key == "version") current->version = value;
    else if (key == "source") current->source = value;
    else if (key == "checksum") {
      if (!value.starts_with("sha256:")) {
        error = "lockfile checksums must use sha256";
        return false;
      }
      current->checksum = value.substr(7);
    } else if (key == "license") current->license = value;
    else if (key == "registry-key") current->registryKey = value;
    else if (key == "publisher") current->publisher = value;
    else if (key == "yanked") {
      if (value != "true" && value != "false") {
        error = "lockfile yanked metadata must be true or false";
        return false;
      }
      current->yanked = value == "true";
    }
    else if (key == "dependencies") current->dependencies = splitList(value);
    else {
      error = "unsupported locked package key '" + key + "'";
      return false;
    }
  }
  if (!sawLockVersion || lock.rootName.empty() ||
      !isValidSemanticVersion(lock.rootVersion)) {
    error = "lockfile is missing valid root package metadata";
    return false;
  }
  std::set<std::string> identities;
  for (const auto& package : lock.packages) {
    if (package.name.empty() || !isValidSemanticVersion(package.version) ||
        package.source.empty() || !validChecksum(package.checksum) ||
        !identities.insert(package.name + "@" + package.version).second) {
      error = "lockfile contains an incomplete or duplicate package entry";
      return false;
    }
  }
  return true;
}

bool resolvePackageDependencies(const Package& package,
                                const ResolveOptions& options,
                                PackageLock& lock, std::string& error) {
  const auto lockPath = package.root / "rocket.lock";
  if (options.offline) {
    if (!readPackageLock(lockPath, lock, error)) return false;
    if (lock.rootName != package.name || lock.rootVersion != package.version) {
      error = "lockfile root does not match package manifest";
      return false;
    }
    return verifyOfflineCache(package, lock, error);
  }
  PackageLock resolved;
  resolved.formatVersion = 2;
  resolved.rootNamespace = package.namespaceName;
  resolved.rootName = package.name;
  resolved.rootVersion = package.version;
  ResolverContext context{package.root / ".rocketc/cache/sha256", {}, {}};
  for (const auto& dependency : package.dependencies) {
    std::string identifier;
    if (!resolveDependency(dependency, package, package.registry,
                           package.registry, package.registryKey, context,
                           resolved, identifier,
                           error)) return false;
    resolved.rootDependencies.push_back(identifier);
  }
  std::sort(resolved.rootDependencies.begin(), resolved.rootDependencies.end());
  if (options.locked) {
    PackageLock existing;
    if (!readPackageLock(lockPath, existing, error)) return false;
    if (renderPackageLock(existing) != renderPackageLock(resolved)) {
      error = "rocket.lock is stale; run 'rocketc resolve' and commit the result";
      return false;
    }
    lock = std::move(existing);
    return true;
  }
  if (!writePackageLock(lockPath, resolved, error)) return false;
  lock = std::move(resolved);
  return true;
}

std::string packageDependencyTree(const PackageLock& lock) {
  std::map<std::string, const LockedPackage*> packages;
  for (const auto& package : lock.packages)
    packages[package.name + "@" + package.version] = &package;
  std::ostringstream output;
  output << lock.rootName << '@' << lock.rootVersion << '\n';
  std::set<std::string> active;
  auto render = [&](auto&& self, const std::string& identity,
                    const std::string& prefix, bool last) -> void {
    output << prefix << (last ? "`-- " : "|-- ") << identity << '\n';
    const auto found = packages.find(identity);
    if (found == packages.end() || !active.insert(identity).second) return;
    const auto& dependencies = found->second->dependencies;
    for (std::size_t index = 0; index < dependencies.size(); ++index)
      self(self, dependencies[index], prefix + (last ? "    " : "|   "),
           index + 1 == dependencies.size());
    active.erase(identity);
  };
  for (std::size_t index = 0; index < lock.rootDependencies.size(); ++index)
    render(render, lock.rootDependencies[index], "",
           index + 1 == lock.rootDependencies.size());
  return output.str();
}

bool auditPackageDependencies(const Package& package, const PackageLock& lock,
                              std::string& report, std::string& error) {
  if (lock.rootName != package.name || lock.rootVersion != package.version) {
    error = "lockfile root does not match package manifest";
    return false;
  }
  if (!verifyOfflineCache(package, lock, error)) return false;
  std::set<std::string> identities;
  std::set<std::string> names;
  int warnings = 0;
  std::vector<std::string> provenance;
  std::vector<std::string> yankWarnings;
  for (const auto& locked : lock.packages) {
    const std::string identity = locked.name + "@" + locked.version;
    if (!identities.insert(identity).second || !names.insert(locked.name).second) {
      error = "audit found duplicate package versions for '" + locked.name + "'";
      return false;
    }
    if (locked.source.starts_with("registry:") && locked.license.empty()) {
      error = "registry package " + identity + " has no license metadata";
      return false;
    }
    if (!locked.license.empty() && !isValidSpdxExpression(locked.license)) {
      error = "package " + identity + " has invalid SPDX license metadata";
      return false;
    }
    if (!package.allowedLicenses.empty() &&
        std::find(package.allowedLicenses.begin(), package.allowedLicenses.end(),
                  locked.license) == package.allowedLicenses.end()) {
      error = "license policy rejects " + identity + " license '" +
              locked.license + "'";
      return false;
    }
    if (locked.source.starts_with("registry:") &&
        !locked.registryKey.empty() && locked.source.find('|') != std::string::npos) {
      RegistryAuditStatus status;
      if (!auditLockedRegistryPackage(package.root, locked, status, error))
        return false;
      provenance.push_back(status.provenance);
      if (!status.compromisedAdvisories.empty()) {
        error = "compromised dependency " + locked.namespaceName + "/" +
                identity + " matches signed advisory " +
                status.compromisedAdvisories.front();
        return false;
      }
      if (status.yanked) {
        const std::string message = "yanked dependency " + locked.namespaceName +
            "/" + identity +
            (status.yankReason.empty() ? "" : ": " + status.yankReason);
        if (package.denyYanked) { error = message; return false; }
        yankWarnings.push_back(message);
      }
    }
    if (locked.license.empty()) ++warnings;
  }
  for (const auto& locked : lock.packages) {
    for (const auto& dependency : locked.dependencies) {
      if (!identities.contains(dependency)) {
        error = "lockfile dependency '" + dependency + "' is not present";
        return false;
      }
    }
  }
  for (const auto& dependency : lock.rootDependencies) {
    if (!identities.contains(dependency)) {
      error = "root lockfile dependency '" + dependency + "' is not present";
      return false;
    }
  }
  std::ostringstream output;
  output << "audit passed: " << lock.packages.size()
         << " locked package(s), SHA-256 cache verified";
  if (warnings != 0)
    output << "; " << warnings << " local package(s) have no license metadata";
  output << '\n';
  std::sort(provenance.begin(), provenance.end());
  for (const auto& item : provenance) output << "provenance: " << item << '\n';
  std::sort(yankWarnings.begin(), yankWarnings.end());
  for (const auto& item : yankWarnings) output << "warning: " << item << '\n';
  report = output.str();
  return true;
}

bool packageSourceChecksum(const std::filesystem::path& root,
                           std::string& checksum, std::string& error) {
  return sha256Package(root, checksum, error);
}

bool prepareLockedPackageDependencies(
    const Package& package, bool offline,
    std::vector<PackageDependencyRoot>& roots, PackageLock& lock,
    std::string& error) {
  roots.clear();
  if (package.dependencies.empty()) {
    lock = {};
    lock.rootName = package.name;
    lock.rootVersion = package.version;
    lock.rootNamespace = package.namespaceName;
    return true;
  }
  if (!readPackageLock(package.root / "rocket.lock", lock, error)) return false;
  if (lock.rootName != package.name || lock.rootVersion != package.version) {
    error = "lockfile root does not match package manifest";
    return false;
  }

  std::map<std::string, const LockedPackage*> byName;
  std::map<std::string, const LockedPackage*> byIdentity;
  for (const auto& locked : lock.packages) {
    if (!byName.emplace(locked.name, &locked).second) {
      error = "lockfile selects more than one version of package '" +
              locked.name + "'";
      return false;
    }
    byIdentity.emplace(locked.name + "@" + locked.version, &locked);
  }

  auto checkEdges = [&](const std::vector<PackageDependency>& declared,
                        const std::vector<std::string>& selected,
                        const std::string& owner) -> bool {
    if (declared.size() != selected.size()) {
      error = "rocket.lock dependency edges are stale for '" + owner + "'";
      return false;
    }
    std::set<std::string> selectedNames;
    for (const auto& identity : selected) {
      const auto found = byIdentity.find(identity);
      if (found == byIdentity.end()) {
        error = "lockfile dependency '" + identity + "' is not present";
        return false;
      }
      selectedNames.insert(found->second->name);
    }
    for (const auto& dependency : declared) {
      const auto found = byName.find(dependency.name);
      if (found == byName.end() || !selectedNames.contains(dependency.name)) {
        error = "rocket.lock is stale; dependency '" + dependency.name +
                "' is not an edge of '" + owner + "'";
        return false;
      }
      if (dependency.source == DependencySourceKind::Registry &&
          !semanticRequirementMatches(found->second->version,
                                      dependency.requirement)) {
        error = "rocket.lock is stale; " + found->second->name + "@" +
                found->second->version + " does not satisfy '" +
                dependency.requirement + "'";
        return false;
      }
    }
    return true;
  };

  if (!checkEdges(package.dependencies, lock.rootDependencies, package.name))
    return false;

  const auto cacheRoot = package.root / ".rocketc/cache/sha256";
  for (const auto& locked : lock.packages) {
    if (!validChecksum(locked.checksum)) {
      error = "lockfile contains an invalid checksum for '" + locked.name + "'";
      return false;
    }
    const auto cached = cacheRoot / locked.checksum;
    if (!std::filesystem::is_directory(cached)) {
      if (offline) {
        error = "offline cache is missing " + locked.name + "@" +
                locked.version + " (sha256:" + locked.checksum + ")";
        return false;
      }
      std::filesystem::path source;
      RegistrySelection lockedRegistrySelection;
      bool registryAcquired = false;
      GitAcquisition lockedGitAcquisition;
      bool gitAcquired = false;
      if (locked.source.starts_with("path:")) {
        source = localSourcePath(package.root, locked.source.substr(5), error);
      } else if (locked.source.starts_with("git:")) {
        std::string location = locked.source.substr(4);
        const std::size_t revision = location.rfind('#');
        const std::string object = revision == std::string::npos
                                       ? std::string{}
                                       : location.substr(revision + 1);
        if (revision != std::string::npos) location.resize(revision);
        if (location.starts_with("https://")) {
          if (!acquireGitPackage(location, object,
                                 cacheRoot.parent_path() / "acquire",
                                 lockedGitAcquisition, error))
            return false;
          source = lockedGitAcquisition.sourceRoot;
          gitAcquired = true;
        } else {
          source = localSourcePath(package.root, location, error);
        }
      } else if (locked.source.starts_with("registry:")) {
        if (locked.source.find('|') != std::string::npos) {
          if (!acquireLockedRegistryPackage(
                  package.root, locked, cacheRoot.parent_path() / "acquire",
                  lockedRegistrySelection, error))
            return false;
          source = lockedRegistrySelection.sourceRoot;
          registryAcquired = true;
        } else {
          source = localSourcePath(package.root, locked.source.substr(9), error) /
                   locked.version;
        }
      } else {
        error = "locked source for '" + locked.name +
                "' cannot refill a missing cache entry";
        return false;
      }
      if (!error.empty() || !std::filesystem::is_directory(source)) {
        if (registryAcquired) discardRegistrySelection(lockedRegistrySelection);
        if (gitAcquired) discardGitAcquisition(lockedGitAcquisition);
        if (error.empty())
          error = "locked source is unavailable for '" + locked.name + "'";
        return false;
      }
      std::string actual;
      if (!sha256Package(source, actual, error)) {
        if (registryAcquired) discardRegistrySelection(lockedRegistrySelection);
        if (gitAcquired) discardGitAcquisition(lockedGitAcquisition);
        return false;
      }
      if (actual != locked.checksum) {
        if (registryAcquired) discardRegistrySelection(lockedRegistrySelection);
        if (gitAcquired) discardGitAcquisition(lockedGitAcquisition);
        error = "locked source checksum mismatch for " + locked.name + "@" +
                locked.version;
        return false;
      }
      if (!ensureCached(cacheRoot, source, locked.checksum, error)) {
        if (registryAcquired) discardRegistrySelection(lockedRegistrySelection);
        if (gitAcquired) discardGitAcquisition(lockedGitAcquisition);
        return false;
      }
      if (registryAcquired) discardRegistrySelection(lockedRegistrySelection);
      if (gitAcquired) discardGitAcquisition(lockedGitAcquisition);
    }

    std::string actual;
    if (!sha256Package(cached, actual, error)) return false;
    if (actual != locked.checksum) {
      error = "cached package checksum mismatch for " + locked.name + "@" +
              locked.version;
      return false;
    }
    auto dependencyPackage = loadPackage(cached, error);
    if (!dependencyPackage) return false;
    if (dependencyPackage->name != locked.name ||
        dependencyPackage->version != locked.version) {
      error = "cached package manifest identity does not match rocket.lock for '" +
              locked.name + "'";
      return false;
    }
    if (!checkEdges(dependencyPackage->dependencies, locked.dependencies,
                    locked.name + "@" + locked.version))
      return false;
    if (!dependencyPackage->buildScript.empty()) {
      error = "dependency '" + locked.name +
              "' declares a build script; dependency code is never run implicitly";
      return false;
    }
    const bool hasNativeInputs = !dependencyPackage->nativeLibraries.empty() ||
                                 !dependencyPackage->nativeHeaders.empty() ||
                                 !dependencyPackage->nativeLibrarySearch.empty();
    const std::string nativeIdentity = dependencyPackage->namespaceName + "/" +
                                       locked.name + "@" + locked.version;
    if (hasNativeInputs &&
        std::find(package.allowedNativeDependencies.begin(),
                  package.allowedNativeDependencies.end(), nativeIdentity) ==
            package.allowedNativeDependencies.end()) {
      error = "dependency '" + nativeIdentity +
              "' declares native inputs but is absent from package-policy.allow-native";
      return false;
    }
    std::vector<std::string> nativeLibraries;
    if (hasNativeInputs) {
      for (const auto& library : dependencyPackage->nativeLibraries) {
        const std::filesystem::path declared(library);
        if (declared.is_absolute()) {
          error = "dependency native library paths must be package-relative";
          return false;
        }
        std::filesystem::path resolved;
        if (declared.has_parent_path()) {
          resolved = (cached / declared).lexically_normal();
        } else {
          for (const auto& search : dependencyPackage->nativeLibrarySearch) {
            const auto candidate = (search / declared).lexically_normal();
            if (std::filesystem::is_regular_file(candidate)) {
              resolved = candidate;
              break;
            }
          }
        }
        if (resolved.empty() || !containedPath(cached, resolved) ||
            !std::filesystem::is_regular_file(resolved)) {
          error = "dependency native library '" + library +
                  "' is not a declared file inside its verified source tree";
          return false;
        }
        nativeLibraries.push_back(resolved.string());
      }
    }
    std::vector<std::string> dependencyNames;
    for (const auto& identity : locked.dependencies) {
      const auto child = byIdentity.find(identity);
      if (child == byIdentity.end()) {
        error = "lockfile dependency '" + identity + "' is not present";
        return false;
      }
      dependencyNames.push_back(child->second->name);
    }
    std::sort(dependencyNames.begin(), dependencyNames.end());
    const std::string lockedIdentity = locked.name + "@" + locked.version;
    roots.push_back(PackageDependencyRoot{
        locked.name, locked.name + "@" + locked.version, cached,
        dependencyPackage->entry, std::move(dependencyNames),
        std::find(lock.rootDependencies.begin(), lock.rootDependencies.end(),
                  lockedIdentity) != lock.rootDependencies.end(),
        std::move(nativeLibraries), dependencyPackage->nativeLibrarySearch,
        dependencyPackage->nativeHeaders});
  }
  std::sort(roots.begin(), roots.end(),
            [](const PackageDependencyRoot& left,
               const PackageDependencyRoot& right) {
              return left.name < right.name;
            });
  return true;
}

std::vector<std::filesystem::path> packageTests(const Package& package,
                                                std::string& error) {
  if (!std::filesystem::is_directory(package.tests)) {
    error = "package test directory does not exist: '" + package.tests.string() + "'";
    return {};
  }
  auto tests = rocketSources(package.tests, error);
  if (error.empty() && tests.empty()) error = "package contains no .rocket tests";
  return tests;
}

} // namespace rocket
