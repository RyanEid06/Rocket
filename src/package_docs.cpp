#include "package_docs.h"

#include "diagnostic.h"
#include "lexer.h"
#include "parser.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <set>
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
  int line = 1;
  std::vector<std::string> relatedTypes;
  std::vector<std::string> parameters;
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
  std::string result = item.module + "-" + item.kind + "-" + item.name;
  for (char& character : result)
    if (!std::isalnum(static_cast<unsigned char>(character))) character = '-';
  return result;
}

std::string documentationBefore(const std::vector<std::string>& lines,
                                int oneBasedLine) {
  std::vector<std::string> comments;
  int index = std::min<int>(oneBasedLine - 2, static_cast<int>(lines.size()) - 1);
  while (index >= 0) {
    const std::string clean = trim(lines[static_cast<std::size_t>(index)]);
    if (clean.empty()) {
      if (comments.empty()) { --index; continue; }
      break;
    }
    if (!clean.starts_with("#")) break;
    comments.push_back(trim(clean.substr(1)));
    --index;
  }
  std::reverse(comments.begin(), comments.end());
  std::ostringstream result;
  for (std::size_t item = 0; item < comments.size(); ++item) {
    if (item) result << '\n';
    result << comments[item];
  }
  return result.str();
}

std::string renderDocumentation(const std::string& text) {
  if (text.empty()) return {};
  std::ostringstream output;
  std::istringstream input(text);
  std::string line;
  bool code = false;
  while (std::getline(input, line)) {
    if (line.starts_with("```")) {
      output << (code ? "</code></pre>" : "<pre><code>");
      code = !code;
      continue;
    }
    if (code) output << html(line) << '\n';
    else if (!line.empty()) output << "<p>" << html(line) << "</p>";
  }
  if (code) output << "</code></pre>";
  return output.str();
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
  for (const auto& source : sources) {
    std::ifstream input(source, std::ios::binary);
    if (!input) { error = "could not read documentation source"; return false; }
    std::ostringstream bytes;
    bytes << input.rdbuf();
    std::vector<std::string> lines;
    std::istringstream lineInput(bytes.str());
    std::string line;
    while (std::getline(lineInput, line)) lines.push_back(line);
    Diagnostics diagnostics;
    auto tokens = Lexer(source.string(), bytes.str(), diagnostics).lex();
    Module module = Parser(tokens, diagnostics).parseModule();
    if (diagnostics.hasErrors()) {
      const auto& first = diagnostics.all().front();
      error = diagnosticCodeName(first.code) + " at " +
              source.lexically_relative(package.root).generic_string() + ":" +
              std::to_string(first.location.line) + ":" +
              std::to_string(first.location.column) + ": " + first.message;
      return false;
    }
    const std::string modulePath =
        source.lexically_relative(package.root).generic_string();
    auto sourceLine = [&](int oneBasedLine) {
      return oneBasedLine > 0 &&
                     static_cast<std::size_t>(oneBasedLine) <= lines.size()
                 ? trim(lines[static_cast<std::size_t>(oneBasedLine - 1)])
                 : std::string{};
    };
    for (const auto& function : module.functions) {
      if (!function.publicDeclaration) continue;
      std::string kind = function.associatedConstant || function.nativeConstant
                             ? "const"
                             : function.nativeImport ? "extern-fn" : "fn";
      std::vector<std::string> related{function.returnType};
      std::vector<std::string> parameters;
      for (const auto& parameter : function.parameters)
        related.push_back(parameter.typeName), parameters.push_back(parameter.name);
      items.push_back({modulePath, kind, function.name,
                       sourceLine(function.location.line),
                       documentationBefore(lines, function.location.line),
                       function.location.line, std::move(related),
                       std::move(parameters)});
    }
    for (const auto& structure : module.structs) {
      if (!structure.publicDeclaration) continue;
      std::string kind = "struct";
      if (structure.representation == StructRepresentation::Opaque) kind = "opaque";
      else if (structure.representation == StructRepresentation::Callback) kind = "callback";
      else if (structure.representation == StructRepresentation::Native) kind = "extern-struct";
      std::vector<std::string> related;
      for (const auto& field : structure.fields) related.push_back(field.typeName);
      items.push_back({modulePath, kind, structure.name,
                       sourceLine(structure.location.line),
                       documentationBefore(lines, structure.location.line),
                       structure.location.line, std::move(related)});
    }
    for (const auto& enumeration : module.enums) {
      if (!enumeration.publicDeclaration) continue;
      std::vector<std::string> related;
      for (const auto& variant : enumeration.variants)
        related.insert(related.end(), variant.payloadTypes.begin(),
                       variant.payloadTypes.end());
      items.push_back({modulePath, "enum", enumeration.name,
                       sourceLine(enumeration.location.line),
                       documentationBefore(lines, enumeration.location.line),
                       enumeration.location.line, std::move(related)});
    }
    for (const auto& trait : module.traits) {
      if (!trait.publicDeclaration) continue;
      std::vector<std::string> related;
      for (const auto& method : trait.methods) {
        related.push_back(method.returnType);
        for (const auto& parameter : method.parameters)
          related.push_back(parameter.typeName);
      }
      items.push_back({modulePath, "trait", trait.name,
                       sourceLine(trait.location.line),
                       documentationBefore(lines, trait.location.line),
                       trait.location.line, std::move(related)});
    }
  }
  std::sort(items.begin(), items.end(), [](const auto& left, const auto& right) {
    if (left.module != right.module) return left.module < right.module;
    if (left.name != right.name) return left.name < right.name;
    return left.kind < right.kind;
  });
  std::map<std::string, std::string> typeAnchors;
  for (const auto& item : items)
    if (item.kind == "struct" || item.kind == "enum" || item.kind == "trait" ||
        item.kind == "extern-struct" || item.kind == "opaque" ||
        item.kind == "callback")
      typeAnchors.emplace(item.name, anchor(item));
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
       << html(package.version) << "</strong> &middot; SPDX <strong>" << html(package.license)
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
         << ":" << item.line << "</p><pre><code>" << html(item.declaration)
         << "</code></pre><p><a href=\"source://" << html(item.module) << "#L"
         << item.line << "\">Source</a></p>";
    if (!item.documentation.empty())
      page << renderDocumentation(item.documentation);
    std::set<std::string> related;
    for (const auto& spelling : item.relatedTypes)
      for (const auto& [name, target] : typeAnchors)
        if (spelling.find(name) != std::string::npos && name != item.name)
          related.insert(name);
    if (!related.empty()) {
      page << "<p>Related: ";
      std::size_t relatedIndex = 0;
      for (const auto& name : related) {
        if (relatedIndex++) page << ", ";
        page << "<a href=\"#" << html(typeAnchors.at(name)) << "\"><code>"
             << html(name) << "</code></a>";
      }
      page << "</p>";
    }
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
           << json(item.module) << "\", \"line\": " << item.line
           << ", \"declaration\": \"" << json(item.declaration)
           << "\", \"documentation\": \"" << json(item.documentation)
           << "\", \"parameters\": [";
    for (std::size_t parameter = 0; parameter < item.parameters.size(); ++parameter)
      search << (parameter ? ", " : "") << "\""
             << json(item.parameters[parameter]) << "\"";
    search << "]"
           << ", \"packageVersion\": \"" << json(package.version)
           << "\", \"href\": \"index.html#" << json(anchor(item)) << "\"}";
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
