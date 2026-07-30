#include "package.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace rocket {
namespace {

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

} // namespace

std::optional<Package> loadPackage(const std::filesystem::path& path,
                                   std::string& error) {
  const std::filesystem::path supplied = std::filesystem::absolute(path).lexically_normal();
  const std::filesystem::path manifest =
      supplied.filename() == "rocket.toml" ? supplied : supplied / "rocket.toml";
  std::ifstream input(manifest, std::ios::binary);
  if (!input) { error = "could not read package manifest '" + manifest.string() + "'"; return {}; }

  Package package{manifest.parent_path(), {}, "0.1.0", "src/main.rocket", "tests"};
  std::string section;
  std::unordered_set<std::string> seen;
  std::string line;
  int lineNumber = 0;
  while (std::getline(input, line)) {
    ++lineNumber;
    const std::string clean = trim(withoutComment(line));
    if (clean.empty()) continue;
    if (clean.front() == '[' && clean.back() == ']') {
      section = trim(clean.substr(1, clean.size() - 2));
      if (section != "package" && section != "test" && section != "build" &&
          section != "native.windows-x64") {
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
    if (qualified == "package.name") package.name = value;
    else if (qualified == "package.version") package.version = value;
    else if (qualified == "package.entry") package.entry = value;
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
  std::error_code filesystemError;
  std::filesystem::recursive_directory_iterator iterator(
      absolute, std::filesystem::directory_options::skip_permission_denied, filesystemError);
  const std::filesystem::recursive_directory_iterator end;
  while (iterator != end) {
    if (filesystemError) { error = filesystemError.message(); return {}; }
    if (iterator->is_directory() && ignoredDirectory(iterator->path())) {
      iterator.disable_recursion_pending();
    } else if (iterator->is_regular_file() && iterator->path().extension() == ".rocket") {
      result.push_back(iterator->path().lexically_normal());
    }
    iterator.increment(filesystemError);
  }
  std::sort(result.begin(), result.end());
  return result;
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
