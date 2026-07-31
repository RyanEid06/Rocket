#include "package_docs.h"

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>
#include <vector>

namespace rocket {
namespace {

struct ApiItem {
  std::string module;
  std::string kind;
  std::string name;
  std::string declaration;
  std::string documentation;
};

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::string html(const std::string& text) {
  std::string output;
  for (char character : text) {
    if (character == '&') output += "&amp;";
    else if (character == '<') output += "&lt;";
    else if (character == '>') output += "&gt;";
    else if (character == '"') output += "&quot;";
    else output.push_back(character);
  }
  return output;
}

std::string json(const std::string& text) {
  std::string output;
  for (unsigned char character : text) {
    if (character == '"' || character == '\\') {
      output.push_back('\\'); output.push_back(static_cast<char>(character));
    } else if (character == '\n') output += "\\n";
    else if (character == '\r') output += "\\r";
    else if (character == '\t') output += "\\t";
    else if (character >= 0x20) output.push_back(static_cast<char>(character));
  }
  return output;
}

std::string anchor(const ApiItem& item) {
  return item.kind + "-" + item.name;
}

bool write(const std::filesystem::path& path, const std::string& bytes,
           std::string& error) {
  std::error_code filesystemError;
  std::filesystem::create_directories(path.parent_path(), filesystemError);
  if (filesystemError) {
    error = "could not create documentation output directory";
    return false;
  }
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  if (!output) {
    error = "could not write package documentation '" + path.string() + "'";
    return false;
  }
  return true;
}

} // namespace

bool generatePackageDocumentation(const Package& package,
                                  const std::filesystem::path& outputDirectory,
                                  std::string& report, std::string& error) {
  std::vector<std::filesystem::path> sources = rocketSources(package.root, error);
  if (!error.empty()) return false;
  std::vector<ApiItem> items;
  const std::regex declaration(
      R"(^pub\s+(fn|struct|enum|trait|const|extern\s+(?:fn|struct|opaque|callback|const))\s+([A-Za-z_][A-Za-z0-9_]*))");
  for (const auto& source : sources) {
    std::ifstream input(source, std::ios::binary);
    if (!input) { error = "could not read documentation source"; return false; }
    std::vector<std::string> comments;
    std::string line;
    while (std::getline(input, line)) {
      const std::string clean = trim(line);
      if (clean.starts_with("#")) {
        comments.push_back(trim(clean.substr(1)));
        continue;
      }
      std::smatch match;
      if (std::regex_search(clean, match, declaration)) {
        items.push_back({source.lexically_relative(package.root).generic_string(),
                         match[1].str(), match[2].str(), clean,
                         comments.empty() ? std::string{} : [&] {
                           std::ostringstream joined;
                           for (std::size_t i = 0; i < comments.size(); ++i) {
                             if (i) joined << '\n';
                             joined << comments[i];
                           }
                           return joined.str();
                         }()});
      }
      if (!clean.empty()) comments.clear();
    }
  }
  std::sort(items.begin(), items.end(), [](const auto& left, const auto& right) {
    if (left.module != right.module) return left.module < right.module;
    if (left.name != right.name) return left.name < right.name;
    return left.kind < right.kind;
  });
  PackageLock lock;
  std::string lockError;
  const bool hasLock = std::filesystem::is_regular_file(package.root / "rocket.lock") &&
                       readPackageLock(package.root / "rocket.lock", lock,
                                       lockError);

  std::ostringstream page;
  page << "<!doctype html>\n<html lang=\"en\"><head><meta charset=\"utf-8\">"
          "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
          "<title>" << html(package.namespaceName + "/" + package.name + " " +
                            package.version)
       << "</title><style>body{font:16px system-ui;max-width:70rem;margin:2rem auto;"
          "padding:0 1rem;color:#17202a}code,pre{font-family:ui-monospace,monospace}"
          "pre{background:#f4f6f7;padding:1rem;overflow:auto}.api{border-top:1px solid #ccd1d1;"
          "padding:1rem 0}.muted{color:#566573}</style></head><body>\n<h1>"
       << html(package.namespaceName + "/" + package.name) << "</h1><p>Version <strong>"
       << html(package.version) << "</strong> · SPDX <strong>" << html(package.license)
       << "</strong></p>\n";
  if (hasLock && !lock.packages.empty()) {
    page << "<h2>Locked dependencies</h2><ul>\n";
    for (const auto& dependency : lock.packages)
      page << "<li><a href=\"../../../" << html(dependency.namespaceName) << "/"
           << html(dependency.name) << "/" << html(dependency.version)
           << "/index.html\"><code>" << html(dependency.namespaceName + "/" +
                                           dependency.name + "@" +
                                           dependency.version)
           << "</code></a></li>\n";
    page << "</ul>\n";
  }
  page << "<h2>Public API</h2><ul>\n";
  for (const auto& item : items)
    page << "<li><a href=\"#" << html(anchor(item)) << "\"><code>"
         << html(item.name) << "</code></a> <span class=\"muted\">"
         << html(item.module) << "</span></li>\n";
  page << "</ul>\n";
  for (const auto& item : items) {
    page << "<section class=\"api\" id=\"" << html(anchor(item)) << "\"><h3><code>"
         << html(item.name) << "</code></h3><p class=\"muted\">" << html(item.module)
         << "</p><pre><code>" << html(item.declaration) << "</code></pre>";
    if (!item.documentation.empty())
      page << "<p>" << html(item.documentation) << "</p>";
    page << "</section>\n";
  }
  page << "<footer><p>Generated deterministically by rocketc doc. Examples are displayed, "
          "never executed.</p></footer></body></html>\n";

  std::ostringstream search;
  search << "{\n  \"package\": \"" << json(package.namespaceName + "/" + package.name)
         << "\",\n  \"version\": \"" << json(package.version) << "\",\n  \"items\": [";
  for (std::size_t index = 0; index < items.size(); ++index) {
    const auto& item = items[index];
    search << (index ? "," : "") << "\n    {\"name\": \"" << json(item.name)
           << "\", \"kind\": \"" << json(item.kind) << "\", \"module\": \""
           << json(item.module) << "\", \"href\": \"index.html#"
           << json(anchor(item)) << "\"}";
  }
  search << (items.empty() ? "" : "\n  ") << "]\n}\n";

  if (!write(outputDirectory / "index.html", page.str(), error) ||
      !write(outputDirectory / "search.json", search.str(), error))
    return false;
  report = "generated documentation for " + package.namespaceName + "/" +
           package.name + "@" + package.version + " with " +
           std::to_string(items.size()) + " public API item(s) in " +
           outputDirectory.string() + "\n";
  return true;
}

} // namespace rocket
