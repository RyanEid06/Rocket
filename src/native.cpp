#include "native.h"

#include "type.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace rocket {
namespace {

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

std::string safeName(std::string value) {
  for (char& character : value)
    if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_')
      character = '_';
  return value;
}

std::vector<std::string> split(const std::string& value, char separator) {
  std::vector<std::string> result;
  std::size_t start = 0;
  while (start <= value.size()) {
    const auto end = value.find(separator, start);
    result.push_back(trim(value.substr(
        start, end == std::string::npos ? std::string::npos : end - start)));
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return result;
}

struct NativeTypes {
  std::unordered_map<std::string, std::string> names;
  std::unordered_set<std::string> opaque;
  std::unordered_set<std::string> structures;
  std::unordered_set<std::string> callbacks;
};

std::string cType(const Type& type, const NativeTypes& native, std::string& error) {
  if (type == Type::Int) return "int64_t";
  if (type == Type::Float) return "double";
  if (type == Type::Bool) return "rocket_bool";
  if (type == Type::Char) return "uint8_t";
  if (type == Type::Unit) return "void";
  if (type.kind == TypeKind::Pointer) {
    if (type.arguments.empty() || type.arguments[0] == Type::Unit) return "void*";
    std::string pointee = cType(type.arguments[0], native, error);
    return pointee.empty() ? std::string() : pointee + "*";
  }
  auto found = native.names.find(type.declaration);
  if (found == native.names.end()) {
    error = "unsupported native header type '" + typeName(type) + "'";
    return {};
  }
  if (native.opaque.contains(type.declaration)) return found->second + "*";
  return found->second;
}

std::string rocketType(std::string type,
                       const std::unordered_set<std::string>& opaque,
                       const std::unordered_set<std::string>& structures,
                       const std::unordered_set<std::string>& callbacks,
                       std::string& error) {
  type = trim(std::regex_replace(type, std::regex("\\bconst\\b"), ""));
  while (type.find("  ") != std::string::npos)
    type = std::regex_replace(type, std::regex("  +"), " ");
  if (type == "int64_t") return "Int";
  if (type == "double") return "Float";
  if (type == "rocket_bool") return "Bool";
  if (type == "uint8_t") return "Char";
  if (type == "void") return "Unit";
  if (type == "void*") return "Pointer[Unit]";
  if (!type.empty() && type.back() == '*') {
    const std::string base = trim(type.substr(0, type.size() - 1));
    if (opaque.contains(base)) return base;
    if (structures.contains(base)) return "Pointer[" + base + "]";
    std::string inner = rocketType(base, opaque, structures, callbacks, error);
    return inner.empty() ? std::string() : "Pointer[" + inner + "]";
  }
  if (opaque.contains(type) || structures.contains(type) || callbacks.contains(type))
    return type;
  error = "unsupported C binding type '" + type + "'";
  return {};
}

bool parseCParameter(const std::string& text, std::string& type, std::string& name) {
  const std::string clean = trim(text);
  const auto space = clean.find_last_of(" \t");
  if (space == std::string::npos) return false;
  type = trim(clean.substr(0, space));
  name = trim(clean.substr(space + 1));
  while (!name.empty() && name.front() == '*') {
    type += '*';
    name.erase(name.begin());
  }
  return !type.empty() && !name.empty();
}

} // namespace

bool generateNativeHeader(const Module& module, const std::string& packageName,
                          std::string& output, std::string& error) {
  error.clear();
  NativeTypes native;
  for (const auto& declaration : module.structs) {
    if (declaration.representation == StructRepresentation::Rocket) continue;
    native.names.emplace(declaration.name, declaration.nativeName);
    if (declaration.representation == StructRepresentation::Opaque)
      native.opaque.insert(declaration.name);
    else if (declaration.representation == StructRepresentation::Native)
      native.structures.insert(declaration.name);
    else if (declaration.representation == StructRepresentation::Callback)
      native.callbacks.insert(declaration.name);
  }

  std::string guard = "ROCKET_" + safeName(packageName) + "_H";
  std::transform(guard.begin(), guard.end(), guard.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  std::ostringstream out;
  out << "#ifndef " << guard << "\n#define " << guard
      << "\n\n#include <stdint.h>\n\ntypedef uint8_t rocket_bool;\n"
         "#ifndef ROCKET_API\n#define ROCKET_API\n#endif\n\n"
         "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n";

  for (const auto& declaration : module.structs) {
    if (declaration.representation == StructRepresentation::Opaque) {
      out << "typedef struct " << declaration.nativeName << ' '
          << declaration.nativeName << ";\n";
    } else if (declaration.representation == StructRepresentation::Native) {
      out << "typedef struct " << declaration.nativeName << " { ";
      for (const auto& field : declaration.fields) {
        const std::string type = cType(typeFromName(field.typeName), native, error);
        if (type.empty()) return false;
        out << type << ' ' << field.name << "; ";
      }
      out << "} " << declaration.nativeName << ";\n";
    } else if (declaration.representation == StructRepresentation::Callback) {
      const std::string result = cType(typeFromName(declaration.callbackReturnType), native,
                                       error);
      if (result.empty()) return false;
      out << "typedef " << result << " (*" << declaration.nativeName << ")(";
      if (declaration.callbackParameters.empty()) out << "void";
      for (std::size_t index = 0; index < declaration.callbackParameters.size(); ++index) {
        if (index) out << ", ";
        const auto& parameter = declaration.callbackParameters[index];
        const std::string type = cType(typeFromName(parameter.typeName), native, error);
        if (type.empty()) return false;
        out << type << ' ' << parameter.name;
      }
      out << ");\n";
    }
  }
  if (!module.structs.empty()) out << '\n';

  for (const auto& function : module.functions) {
    if (!function.nativeExport) continue;
    const std::string result = cType(typeFromName(function.returnType), native, error);
    if (result.empty()) return false;
    out << "ROCKET_API " << result << ' ' << function.nativeName << '(';
    if (function.parameters.empty()) out << "void";
    for (std::size_t index = 0; index < function.parameters.size(); ++index) {
      if (index) out << ", ";
      const auto& parameter = function.parameters[index];
      const std::string type = cType(typeFromName(parameter.typeName), native, error);
      if (type.empty()) return false;
      out << type << ' ' << parameter.name;
    }
    out << ");\n";
  }
  out << "\n#ifdef __cplusplus\n}\n#endif\n\n#endif\n";
  output = out.str();
  return true;
}

bool generateRocketBindings(const std::string& header, std::string& output,
                            std::string& error) {
  error.clear();
  std::unordered_set<std::string> opaque;
  std::unordered_set<std::string> structures;
  std::unordered_set<std::string> callbacks;
  std::vector<std::string> lines;
  std::istringstream input(header);
  std::string line;
  while (std::getline(input, line)) lines.push_back(trim(line));

  const std::regex opaquePattern(R"(^typedef struct ([A-Za-z_]\w*) \1;$)");
  const std::regex structPattern(
      R"(^typedef struct ([A-Za-z_]\w*) \{ (.*) \} \1;$)");
  const std::regex callbackPattern(
      R"(^typedef (.+) \(\*([A-Za-z_]\w*)\)\((.*)\);$)");
  std::smatch match;
  for (const auto& current : lines) {
    if (std::regex_match(current, match, opaquePattern)) opaque.insert(match[1]);
    else if (std::regex_match(current, match, structPattern)) structures.insert(match[1]);
    else if (std::regex_match(current, match, callbackPattern)) callbacks.insert(match[2]);
  }

  std::ostringstream out;
  out << "# Generated deterministically by rocketc bind.\n";
  for (const auto& current : lines) {
    const std::regex constantPattern(R"(^#define ([A-Za-z_]\w*) (-?\d+)$)");
    if (std::regex_match(current, match, constantPattern)) {
      out << "pub extern const " << match[1].str() << ": Int = " << match[2].str() << "\n";
      continue;
    }
    if (current.empty() || current.front() == '#' || current == "extern \"C\" {" ||
        current == "}" || current == "typedef uint8_t rocket_bool;")
      continue;
    if (std::regex_match(current, match, opaquePattern)) {
      out << "pub extern opaque " << match[1].str() << "\n";
      continue;
    }
    if (std::regex_match(current, match, structPattern)) {
      out << "pub extern struct " << match[1].str() << ":\n";
      for (const auto& field : split(match[2].str(), ';')) {
        if (field.empty()) continue;
        std::string ctype, name;
        if (!parseCParameter(field, ctype, name)) {
          error = "unsupported native struct field '" + field + "'";
          return false;
        }
        const std::string type = rocketType(ctype, opaque, structures, callbacks, error);
        if (type.empty()) return false;
        out << "    " << name << ": " << type << "\n";
      }
      continue;
    }
    if (std::regex_match(current, match, callbackPattern)) {
      const std::string result = rocketType(match[1], opaque, structures, callbacks, error);
      if (result.empty()) return false;
      out << "pub extern callback " << match[2].str() << '(';
      const auto parameters = split(match[3].str(), ',');
      if (!(parameters.size() == 1 && parameters[0] == "void")) {
        for (std::size_t index = 0; index < parameters.size(); ++index) {
          std::string ctype, name;
          if (!parseCParameter(parameters[index], ctype, name)) {
            error = "unsupported callback parameter '" + parameters[index] + "'";
            return false;
          }
          const std::string type = rocketType(ctype, opaque, structures, callbacks, error);
          if (type.empty()) return false;
          if (index) out << ", ";
          out << name << ": " << type;
        }
      }
      out << ") -> " << result << "\n";
      continue;
    }
    if (current.rfind("ROCKET_API ", 0) == 0 ||
        (current.ends_with(");") && current.rfind("typedef ", 0) != 0)) {
      const std::regex functionPattern(
          R"(^(?:ROCKET_API )?(.+) ([A-Za-z_]\w*)\((.*)\);$)");
      if (!std::regex_match(current, match, functionPattern)) {
        error = "unsupported C function declaration '" + current + "'";
        return false;
      }
      const std::string result = rocketType(match[1], opaque, structures, callbacks, error);
      if (result.empty()) return false;
      out << "pub extern fn " << match[2].str() << '(';
      const auto parameters = split(match[3].str(), ',');
      if (!(parameters.size() == 1 && parameters[0] == "void")) {
        for (std::size_t index = 0; index < parameters.size(); ++index) {
          std::string ctype, name;
          if (!parseCParameter(parameters[index], ctype, name)) {
            error = "unsupported C function parameter '" + parameters[index] + "'";
            return false;
          }
          const std::string type = rocketType(ctype, opaque, structures, callbacks, error);
          if (type.empty()) return false;
          if (index) out << ", ";
          out << name << ": " << type;
        }
      }
      out << ") -> " << result << "\n";
      continue;
    }
  }
  output = out.str();
  return true;
}

} // namespace rocket
