#include "language_server.h"

#include "diagnostic.h"
#include "formatter.h"
#include "lexer.h"
#include "module_loader.h"
#include "package.h"
#include "parser.h"
#include "sema.h"
#include "type.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace rocket {
namespace {

constexpr std::size_t MaximumMessageBytes = 16U * 1024U * 1024U;
constexpr std::size_t MaximumHeaderBytes = 16U * 1024U;
constexpr std::size_t MaximumDocumentBytes = 4U * 1024U * 1024U;
constexpr std::size_t DefaultMaximumProjectBytes = 64U * 1024U * 1024U;
constexpr std::size_t DefaultMaximumProjectFiles = 4096U;
constexpr std::size_t MaximumContentChanges = 1024U;

struct JsonNumber {
  std::string text;
};

struct Json {
  using Array = std::vector<Json>;
  using Object = std::map<std::string, Json>;
  using Value = std::variant<std::nullptr_t, bool, JsonNumber, std::string,
                             Array, Object>;

  Json() : value(nullptr) {}
  explicit Json(bool item) : value(item) {}
  explicit Json(JsonNumber item) : value(std::move(item)) {}
  explicit Json(std::string item) : value(std::move(item)) {}
  explicit Json(const char* item) : value(std::string(item)) {}
  explicit Json(Array item) : value(std::move(item)) {}
  explicit Json(Object item) : value(std::move(item)) {}

  static Json integer(long long value) {
    return Json(JsonNumber{std::to_string(value)});
  }

  Value value;
};

void appendUtf8(std::string& output, std::uint32_t codepoint) {
  if (codepoint <= 0x7f) {
    output.push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7ff) {
    output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
    output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
  } else if (codepoint <= 0xffff) {
    output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
    output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
    output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
  } else {
    output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
    output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
    output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
    output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
  }
}

class JsonParser {
public:
  explicit JsonParser(std::string_view input) : input_(input) {}

  std::optional<Json> parse(std::string& error) {
    skipWhitespace();
    auto result = parseValue(error, 0);
    skipWhitespace();
    if (result && index_ != input_.size()) {
      error = "unexpected content after JSON value";
      return std::nullopt;
    }
    return result;
  }

private:
  std::optional<Json> parseValue(std::string& error, int depth) {
    if (depth > 128) {
      error = "JSON nesting exceeds 128 levels";
      return std::nullopt;
    }
    skipWhitespace();
    if (index_ >= input_.size()) {
      error = "unexpected end of JSON input";
      return std::nullopt;
    }
    const char current = input_[index_];
    if (current == 'n' && consumeLiteral("null")) return Json();
    if (current == 't' && consumeLiteral("true")) return Json(true);
    if (current == 'f' && consumeLiteral("false")) return Json(false);
    if (current == '"') {
      std::string value;
      if (!parseString(value, error)) return std::nullopt;
      return Json(std::move(value));
    }
    if (current == '[') return parseArray(error, depth + 1);
    if (current == '{') return parseObject(error, depth + 1);
    if (current == '-' || std::isdigit(static_cast<unsigned char>(current)))
      return parseNumber(error);
    error = "invalid JSON value";
    return std::nullopt;
  }

  std::optional<Json> parseArray(std::string& error, int depth) {
    ++index_;
    Json::Array values;
    skipWhitespace();
    if (consume(']')) return Json(std::move(values));
    while (true) {
      auto value = parseValue(error, depth);
      if (!value) return std::nullopt;
      values.push_back(std::move(*value));
      skipWhitespace();
      if (consume(']')) return Json(std::move(values));
      if (!consume(',')) {
        error = "expected ',' or ']' in JSON array";
        return std::nullopt;
      }
    }
  }

  std::optional<Json> parseObject(std::string& error, int depth) {
    ++index_;
    Json::Object values;
    skipWhitespace();
    if (consume('}')) return Json(std::move(values));
    while (true) {
      std::string key;
      if (!parseString(key, error)) return std::nullopt;
      skipWhitespace();
      if (!consume(':')) {
        error = "expected ':' in JSON object";
        return std::nullopt;
      }
      auto value = parseValue(error, depth);
      if (!value) return std::nullopt;
      if (!values.emplace(std::move(key), std::move(*value)).second) {
        error = "duplicate JSON object key";
        return std::nullopt;
      }
      skipWhitespace();
      if (consume('}')) return Json(std::move(values));
      if (!consume(',')) {
        error = "expected ',' or '}' in JSON object";
        return std::nullopt;
      }
      skipWhitespace();
    }
  }

  std::optional<Json> parseNumber(std::string& error) {
    const std::size_t start = index_;
    if (consume('-') && index_ == input_.size()) {
      error = "incomplete JSON number";
      return std::nullopt;
    }
    if (consume('0')) {
      if (index_ < input_.size() &&
          std::isdigit(static_cast<unsigned char>(input_[index_]))) {
        error = "JSON numbers cannot contain leading zeroes";
        return std::nullopt;
      }
    } else {
      const std::size_t integerStart = index_;
      while (index_ < input_.size() &&
             std::isdigit(static_cast<unsigned char>(input_[index_])))
        ++index_;
      if (index_ == integerStart) {
        error = "invalid JSON number";
        return std::nullopt;
      }
    }
    if (consume('.')) {
      const std::size_t fractionStart = index_;
      while (index_ < input_.size() &&
             std::isdigit(static_cast<unsigned char>(input_[index_])))
        ++index_;
      if (index_ == fractionStart) {
        error = "invalid JSON number fraction";
        return std::nullopt;
      }
    }
    if (index_ < input_.size() &&
        (input_[index_] == 'e' || input_[index_] == 'E')) {
      ++index_;
      if (index_ < input_.size() &&
          (input_[index_] == '+' || input_[index_] == '-'))
        ++index_;
      const std::size_t exponentStart = index_;
      while (index_ < input_.size() &&
             std::isdigit(static_cast<unsigned char>(input_[index_])))
        ++index_;
      if (index_ == exponentStart) {
        error = "invalid JSON number exponent";
        return std::nullopt;
      }
    }
    return Json(JsonNumber{std::string(input_.substr(start, index_ - start))});
  }

  bool parseString(std::string& result, std::string& error) {
    if (!consume('"')) {
      error = "expected JSON string";
      return false;
    }
    result.clear();
    while (index_ < input_.size()) {
      const unsigned char current = static_cast<unsigned char>(input_[index_++]);
      if (current == '"') return true;
      if (current < 0x20) {
        error = "unescaped control character in JSON string";
        return false;
      }
      if (current != '\\') {
        result.push_back(static_cast<char>(current));
        continue;
      }
      if (index_ >= input_.size()) {
        error = "incomplete JSON string escape";
        return false;
      }
      const char escaped = input_[index_++];
      if (escaped == '"' || escaped == '\\' || escaped == '/')
        result.push_back(escaped);
      else if (escaped == 'b') result.push_back('\b');
      else if (escaped == 'f') result.push_back('\f');
      else if (escaped == 'n') result.push_back('\n');
      else if (escaped == 'r') result.push_back('\r');
      else if (escaped == 't') result.push_back('\t');
      else if (escaped == 'u') {
        std::uint32_t codepoint = 0;
        if (!parseHex(codepoint)) {
          error = "invalid JSON Unicode escape";
          return false;
        }
        if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
          if (index_ + 2 > input_.size() || input_[index_] != '\\' ||
              input_[index_ + 1] != 'u') {
            error = "JSON high surrogate requires a low surrogate";
            return false;
          }
          index_ += 2;
          std::uint32_t low = 0;
          if (!parseHex(low) || low < 0xdc00 || low > 0xdfff) {
            error = "invalid JSON low surrogate";
            return false;
          }
          codepoint = 0x10000 + ((codepoint - 0xd800) << 10) +
                      (low - 0xdc00);
        } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
          error = "unexpected JSON low surrogate";
          return false;
        }
        appendUtf8(result, codepoint);
      } else {
        error = "invalid JSON string escape";
        return false;
      }
    }
    error = "unterminated JSON string";
    return false;
  }

  bool parseHex(std::uint32_t& result) {
    if (index_ + 4 > input_.size()) return false;
    result = 0;
    for (int count = 0; count < 4; ++count) {
      const char value = input_[index_++];
      result <<= 4;
      if (value >= '0' && value <= '9') result |= value - '0';
      else if (value >= 'a' && value <= 'f') result |= value - 'a' + 10;
      else if (value >= 'A' && value <= 'F') result |= value - 'A' + 10;
      else return false;
    }
    return true;
  }

  bool consume(char expected) {
    if (index_ >= input_.size() || input_[index_] != expected) return false;
    ++index_;
    return true;
  }

  bool consumeLiteral(std::string_view expected) {
    if (input_.substr(index_, expected.size()) != expected) return false;
    index_ += expected.size();
    return true;
  }

  void skipWhitespace() {
    while (index_ < input_.size() &&
           (input_[index_] == ' ' || input_[index_] == '\t' ||
            input_[index_] == '\r' || input_[index_] == '\n'))
      ++index_;
  }

  std::string_view input_;
  std::size_t index_ = 0;
};

void appendJsonString(std::string& output, std::string_view value) {
  static constexpr char Hexadecimal[] = "0123456789abcdef";
  output.push_back('"');
  for (const unsigned char character : value) {
    if (character == '"') output += "\\\"";
    else if (character == '\\') output += "\\\\";
    else if (character == '\b') output += "\\b";
    else if (character == '\f') output += "\\f";
    else if (character == '\n') output += "\\n";
    else if (character == '\r') output += "\\r";
    else if (character == '\t') output += "\\t";
    else if (character < 0x20) {
      output += "\\u00";
      output.push_back(Hexadecimal[character >> 4]);
      output.push_back(Hexadecimal[character & 0xf]);
    } else {
      output.push_back(static_cast<char>(character));
    }
  }
  output.push_back('"');
}

void appendJson(std::string& output, const Json& value) {
  if (std::holds_alternative<std::nullptr_t>(value.value)) output += "null";
  else if (const auto* boolean = std::get_if<bool>(&value.value))
    output += *boolean ? "true" : "false";
  else if (const auto* number = std::get_if<JsonNumber>(&value.value))
    output += number->text;
  else if (const auto* text = std::get_if<std::string>(&value.value))
    appendJsonString(output, *text);
  else if (const auto* array = std::get_if<Json::Array>(&value.value)) {
    output.push_back('[');
    for (std::size_t index = 0; index < array->size(); ++index) {
      if (index != 0) output.push_back(',');
      appendJson(output, array->at(index));
    }
    output.push_back(']');
  } else {
    const auto& object = std::get<Json::Object>(value.value);
    output.push_back('{');
    std::size_t index = 0;
    for (const auto& [key, item] : object) {
      if (index++ != 0) output.push_back(',');
      appendJsonString(output, key);
      output.push_back(':');
      appendJson(output, item);
    }
    output.push_back('}');
  }
}

std::string serialize(const Json& value) {
  std::string output;
  appendJson(output, value);
  return output;
}

const Json::Object* asObject(const Json* value) {
  return value == nullptr ? nullptr : std::get_if<Json::Object>(&value->value);
}

const Json::Array* asArray(const Json* value) {
  return value == nullptr ? nullptr : std::get_if<Json::Array>(&value->value);
}

const std::string* asString(const Json* value) {
  return value == nullptr ? nullptr : std::get_if<std::string>(&value->value);
}

const Json* field(const Json::Object* object, const std::string& name) {
  if (object == nullptr) return nullptr;
  const auto found = object->find(name);
  return found == object->end() ? nullptr : &found->second;
}

bool integerValue(const Json* value, long long& result) {
  if (value == nullptr) return false;
  const auto* number = std::get_if<JsonNumber>(&value->value);
  if (number == nullptr || number->text.find_first_of(".eE") != std::string::npos)
    return false;
  const auto converted = std::from_chars(number->text.data(),
                                         number->text.data() + number->text.size(),
                                         result);
  return converted.ec == std::errc{} &&
         converted.ptr == number->text.data() + number->text.size();
}

bool booleanValue(const Json* value, bool& result) {
  if (value == nullptr) return false;
  const auto* boolean = std::get_if<bool>(&value->value);
  if (boolean == nullptr) return false;
  result = *boolean;
  return true;
}

std::string requestKey(const Json& value) {
  if (const auto* number = std::get_if<JsonNumber>(&value.value))
    return "n:" + number->text;
  if (const auto* string = std::get_if<std::string>(&value.value))
    return "s:" + *string;
  return {};
}

std::size_t utf16Length(std::string_view text) {
  std::size_t units = 0;
  for (std::size_t index = 0; index < text.size();) {
    const unsigned char first = static_cast<unsigned char>(text[index]);
    std::uint32_t codepoint = first;
    std::size_t width = 1;
    if ((first & 0xe0) == 0xc0 && index + 1 < text.size()) {
      codepoint = ((first & 0x1f) << 6) |
                  (static_cast<unsigned char>(text[index + 1]) & 0x3f);
      width = 2;
    } else if ((first & 0xf0) == 0xe0 && index + 2 < text.size()) {
      codepoint = ((first & 0x0f) << 12) |
                  ((static_cast<unsigned char>(text[index + 1]) & 0x3f) << 6) |
                  (static_cast<unsigned char>(text[index + 2]) & 0x3f);
      width = 3;
    } else if ((first & 0xf8) == 0xf0 && index + 3 < text.size()) {
      codepoint = ((first & 0x07) << 18) |
                  ((static_cast<unsigned char>(text[index + 1]) & 0x3f) << 12) |
                  ((static_cast<unsigned char>(text[index + 2]) & 0x3f) << 6) |
                  (static_cast<unsigned char>(text[index + 3]) & 0x3f);
      width = 4;
    }
    units += codepoint > 0xffff ? 2 : 1;
    index += width;
  }
  return units;
}

std::size_t utf8Width(std::string_view text, std::size_t index) {
  if (index >= text.size()) return 0;
  const unsigned char first = static_cast<unsigned char>(text[index]);
  if ((first & 0x80) == 0) return 1;
  if ((first & 0xe0) == 0xc0 && index + 1 < text.size()) return 2;
  if ((first & 0xf0) == 0xe0 && index + 2 < text.size()) return 3;
  if ((first & 0xf8) == 0xf0 && index + 3 < text.size()) return 4;
  return 1;
}

std::uint32_t utf8Codepoint(std::string_view text, std::size_t index,
                            std::size_t width) {
  const unsigned char first = static_cast<unsigned char>(text[index]);
  if (width == 1) return first;
  if (width == 2)
    return ((first & 0x1f) << 6) |
           (static_cast<unsigned char>(text[index + 1]) & 0x3f);
  if (width == 3)
    return ((first & 0x0f) << 12) |
           ((static_cast<unsigned char>(text[index + 1]) & 0x3f) << 6) |
           (static_cast<unsigned char>(text[index + 2]) & 0x3f);
  return ((first & 0x07) << 18) |
         ((static_cast<unsigned char>(text[index + 1]) & 0x3f) << 12) |
         ((static_cast<unsigned char>(text[index + 2]) & 0x3f) << 6) |
         (static_cast<unsigned char>(text[index + 3]) & 0x3f);
}

std::optional<std::size_t> offsetAtPosition(const std::string& source,
                                            long long line,
                                            long long character) {
  if (line < 0 || character < 0) return std::nullopt;
  std::size_t offset = 0;
  for (long long currentLine = 0; currentLine < line; ++currentLine) {
    const std::size_t newline = source.find('\n', offset);
    if (newline == std::string::npos) return std::nullopt;
    offset = newline + 1;
  }
  std::size_t end = source.find('\n', offset);
  if (end == std::string::npos) end = source.size();
  if (end > offset && source[end - 1] == '\r') --end;
  long long units = 0;
  std::size_t index = offset;
  while (index < end && units < character) {
    const std::size_t width = utf8Width(source, index);
    const long long next = units +
        (utf8Codepoint(source, index, width) > 0xffff ? 2 : 1);
    if (next > character) return std::nullopt;
    units = next;
    index += width;
  }
  return units == character ? std::optional<std::size_t>(index) : std::nullopt;
}

std::size_t lineStart(const std::string& source, int oneBasedLine) {
  std::size_t offset = 0;
  for (int line = 1; line < std::max(1, oneBasedLine); ++line) {
    const std::size_t newline = source.find('\n', offset);
    if (newline == std::string::npos) return source.size();
    offset = newline + 1;
  }
  return offset;
}

long long lspCharacter(const std::string& source, int line, int column) {
  const std::size_t start = lineStart(source, line);
  std::size_t end = source.find('\n', start);
  if (end == std::string::npos) end = source.size();
  if (end > start && source[end - 1] == '\r') --end;
  const std::size_t bytes = static_cast<std::size_t>(std::max(0, column - 1));
  const std::size_t target = std::min(end, start + bytes);
  return static_cast<long long>(utf16Length(
      std::string_view(source).substr(start, target - start)));
}

struct Analysis {
  std::vector<Token> tokens;
  Diagnostics diagnostics;
};

Analysis analyzeDocument(const std::string& uri, const std::string& source) {
  Analysis result;
  result.tokens = Lexer(uri, source, result.diagnostics).lex();
  if (result.diagnostics.hasErrors()) return result;
  Module module = Parser(result.tokens, result.diagnostics).parseModule();
  if (result.diagnostics.hasErrors() || !module.imports.empty()) return result;
  module.library = true;
  SemanticAnalyzer analyzer(module, result.diagnostics);
  analyzer.analyze();
  return result;
}

long long diagnosticSpan(const Diagnostic& diagnostic,
                         const std::vector<Token>& tokens) {
  for (const auto& token : tokens) {
    if (token.location.line == diagnostic.location.line &&
        token.location.column == diagnostic.location.column)
      return static_cast<long long>(std::max<std::size_t>(1, utf16Length(token.text)));
  }
  return 1;
}

Json diagnosticJson(const Diagnostic& diagnostic, const Analysis& analysis,
                    const std::string& source) {
  const long long line = std::max(0, diagnostic.location.line - 1);
  const long long character = lspCharacter(source, diagnostic.location.line,
                                            diagnostic.location.column);
  const long long span = diagnosticSpan(diagnostic, analysis.tokens);
  Json::Object start{{"character", Json::integer(character)},
                     {"line", Json::integer(line)}};
  Json::Object end{{"character", Json::integer(character + span)},
                   {"line", Json::integer(line)}};
  Json::Object range{{"end", Json(std::move(end))},
                     {"start", Json(std::move(start))}};
  return Json(Json::Object{
      {"code", Json(diagnosticCodeName(diagnostic.code))},
      {"message", Json(diagnostic.message)},
      {"range", Json(std::move(range))},
      {"severity", Json::integer(1)},
      {"source", Json("rocketc")},
  });
}

std::string percentDecode(std::string_view value) {
  std::string result;
  result.reserve(value.size());
  for (std::size_t index = 0; index < value.size(); ++index) {
    if (value[index] == '%' && index + 2 < value.size()) {
      unsigned decoded = 0;
      const auto conversion = std::from_chars(value.data() + index + 1,
                                               value.data() + index + 3,
                                               decoded, 16);
      if (conversion.ec == std::errc{}) {
        result.push_back(static_cast<char>(decoded));
        index += 2;
        continue;
      }
    }
    result.push_back(value[index]);
  }
  return result;
}

std::string percentEncodePath(std::string_view value) {
  static constexpr char Hex[] = "0123456789ABCDEF";
  std::string result;
  for (const unsigned char character : value) {
    if (std::isalnum(character) || character == '/' || character == ':' ||
        character == '-' || character == '_' || character == '.' ||
        character == '~') {
      result.push_back(static_cast<char>(character));
    } else {
      result.push_back('%');
      result.push_back(Hex[character >> 4]);
      result.push_back(Hex[character & 0xf]);
    }
  }
  return result;
}

std::optional<std::filesystem::path> pathFromUri(std::string_view uri) {
  if (!uri.starts_with("file://")) return std::nullopt;
  std::string path = percentDecode(uri.substr(7));
#ifdef _WIN32
  if (path.size() >= 3 && path[0] == '/' &&
      std::isalpha(static_cast<unsigned char>(path[1])) && path[2] == ':')
    path.erase(path.begin());
  std::replace(path.begin(), path.end(), '/', '\\');
#endif
  return std::filesystem::absolute(std::filesystem::path(path)).lexically_normal();
}

std::string uriFromPath(const std::filesystem::path& path) {
  std::string generic = std::filesystem::absolute(path).lexically_normal().generic_string();
#ifdef _WIN32
  if (generic.size() >= 2 && generic[1] == ':') generic.insert(generic.begin(), '/');
#endif
  return "file://" + percentEncodePath(generic);
}

std::string shortSymbolName(const std::string& name) {
  const std::size_t dot = name.rfind('.');
  return dot == std::string::npos ? name : name.substr(dot + 1);
}

bool validIdentifier(std::string_view value) {
  if (value.empty() ||
      !(std::isalpha(static_cast<unsigned char>(value.front())) ||
        value.front() == '_'))
    return false;
  for (const unsigned char character : value)
    if (!(std::isalnum(character) || character == '_')) return false;
  static const std::set<std::string> keywords{
      "fn", "let", "var", "if", "else", "while", "for", "in",
      "break", "continue", "return", "true", "false", "and", "or",
      "not", "struct", "enum", "trait", "impl", "where", "const",
      "match", "case", "pub", "import", "unsafe", "extern", "export",
      "opaque", "callback"};
  return !keywords.contains(std::string(value));
}

Json rangeJson(long long line, long long character, long long length) {
  Json::Object start{{"line", Json::integer(line)},
                     {"character", Json::integer(character)}};
  Json::Object end{{"line", Json::integer(line)},
                   {"character", Json::integer(character + std::max(1LL, length))}};
  return Json(Json::Object{{"start", Json(std::move(start))},
                           {"end", Json(std::move(end))}});
}

Json locationJson(const Location& location, std::size_t length = 1) {
  return Json(Json::Object{
      {"uri", Json(uriFromPath(location.file))},
      {"range", rangeJson(std::max(0, location.line - 1),
                          std::max(0, location.column - 1),
                          static_cast<long long>(length))}});
}

std::string documentationBefore(const std::string& source, int oneBasedLine) {
  std::vector<std::string> lines;
  std::istringstream input(source);
  std::string line;
  while (std::getline(input, line)) lines.push_back(line);
  int index = std::min<int>(oneBasedLine - 2, static_cast<int>(lines.size()) - 1);
  std::vector<std::string> documentation;
  while (index >= 0) {
    std::string current = lines[static_cast<std::size_t>(index)];
    const std::size_t first = current.find_first_not_of(" \t\r");
    if (first == std::string::npos) {
      if (documentation.empty()) { --index; continue; }
      break;
    }
    if (current[first] != '#') break;
    current = current.substr(first + 1);
    if (!current.empty() && current.front() == ' ') current.erase(current.begin());
    documentation.push_back(std::move(current));
    --index;
  }
  std::reverse(documentation.begin(), documentation.end());
  std::ostringstream joined;
  for (std::size_t item = 0; item < documentation.size(); ++item) {
    if (item) joined << '\n';
    joined << documentation[item];
  }
  return joined.str();
}

struct SemanticSymbol {
  std::string key;
  std::string name;
  std::string shortName;
  std::string kind;
  std::string detail;
  std::string documentation;
  Location location;
  std::vector<std::string> parameters;
  bool publicDeclaration = false;
  bool native = false;
};

struct SemanticOccurrence {
  std::string symbol;
  std::string name;
  Location location;
  std::size_t length = 1;
  bool definition = false;
};

struct SemanticSnapshot {
  std::map<std::string, SemanticSymbol> symbols;
  std::vector<SemanticOccurrence> occurrences;
  std::map<std::string, Analysis> analyses;
  std::map<std::string, std::string> sources;
  std::size_t filesAnalyzed = 0;
  std::size_t bytesAnalyzed = 0;
  std::size_t invalidatedFiles = 0;
  long long generation = 0;
  long long elapsedMilliseconds = 0;
};

std::string symbolKey(const Location& location, const std::string& name,
                      std::string_view category) {
  return std::filesystem::absolute(location.file).lexically_normal().generic_string() +
         ":" + std::to_string(location.line) + ":" +
         std::to_string(location.column) + ":" + std::string(category) + ":" + name;
}

std::string symbolKindName(SymbolKind kind) {
  switch (kind) {
  case SymbolKind::BuiltinFunction: return "function";
  case SymbolKind::Function: return "function";
  case SymbolKind::Parameter: return "parameter";
  case SymbolKind::Local: return "variable";
  case SymbolKind::LoopVariable: return "variable";
  case SymbolKind::PatternBinding: return "variable";
  }
  return "variable";
}

std::size_t identifierLength(const std::string& name) {
  return std::max<std::size_t>(1, shortSymbolName(name).size());
}

void addOccurrence(SemanticSnapshot& snapshot,
                   const std::vector<std::string>& symbolKeys, SymbolId symbol,
                   const Location& location, bool definition = false) {
  if (symbol == InvalidSymbol || symbol >= symbolKeys.size() ||
      symbolKeys[symbol].empty())
    return;
  const auto found = snapshot.symbols.find(symbolKeys[symbol]);
  if (found == snapshot.symbols.end()) return;
  Location resolvedLocation = location;
  const auto source = snapshot.sources.find(
      std::filesystem::absolute(location.file).lexically_normal().generic_string());
  if (source != snapshot.sources.end()) {
    const std::size_t start = lineStart(source->second, location.line);
    std::size_t end = source->second.find('\n', start);
    if (end == std::string::npos) end = source->second.size();
    const std::size_t searchStart = std::min(
        end, start + static_cast<std::size_t>(std::max(0, location.column - 1)));
    std::size_t name = source->second.find(found->second.shortName, searchStart);
    if (name == std::string::npos || name >= end) {
      name = source->second.rfind(found->second.shortName, end);
      if (name < start) name = std::string::npos;
    }
    if (name != std::string::npos && name < end)
      resolvedLocation.column = static_cast<int>(name - start + 1);
  }
  snapshot.occurrences.push_back({symbolKeys[symbol], found->second.shortName,
                                  resolvedLocation,
                                  identifierLength(found->second.shortName),
                                  definition});
}

void indexExpression(const HirExpr& expression, SemanticSnapshot& snapshot,
                     const std::vector<std::string>& symbolKeys);

void indexBlock(const HirBlock& block, SemanticSnapshot& snapshot,
                const std::vector<std::string>& symbolKeys) {
  for (const auto& statementPointer : block) {
    const HirStmt& statement = *statementPointer;
    switch (statement.kind) {
    case HirStmtKind::Binding: {
      const auto& item = static_cast<const HirBindingStmt&>(statement);
      indexExpression(*item.initializer, snapshot, symbolKeys);
      break;
    }
    case HirStmtKind::Assignment: {
      const auto& item = static_cast<const HirAssignmentStmt&>(statement);
      addOccurrence(snapshot, symbolKeys, item.target, item.location);
      indexExpression(*item.value, snapshot, symbolKeys);
      break;
    }
    case HirStmtKind::IndexAssignment: {
      const auto& item = static_cast<const HirIndexAssignmentStmt&>(statement);
      addOccurrence(snapshot, symbolKeys, item.target, item.location);
      indexExpression(*item.index, snapshot, symbolKeys);
      indexExpression(*item.value, snapshot, symbolKeys);
      break;
    }
    case HirStmtKind::Return:
      indexExpression(*static_cast<const HirReturnStmt&>(statement).value,
                      snapshot, symbolKeys);
      break;
    case HirStmtKind::Expression:
      indexExpression(*static_cast<const HirExprStmt&>(statement).expression,
                      snapshot, symbolKeys);
      break;
    case HirStmtKind::If: {
      const auto& item = static_cast<const HirIfStmt&>(statement);
      indexExpression(*item.condition, snapshot, symbolKeys);
      indexBlock(item.thenBody, snapshot, symbolKeys);
      indexBlock(item.elseBody, snapshot, symbolKeys);
      break;
    }
    case HirStmtKind::While: {
      const auto& item = static_cast<const HirWhileStmt&>(statement);
      indexExpression(*item.condition, snapshot, symbolKeys);
      indexBlock(item.body, snapshot, symbolKeys);
      break;
    }
    case HirStmtKind::For: {
      const auto& item = static_cast<const HirForStmt&>(statement);
      indexExpression(*item.start, snapshot, symbolKeys);
      indexExpression(*item.end, snapshot, symbolKeys);
      indexBlock(item.body, snapshot, symbolKeys);
      break;
    }
    case HirStmtKind::Match: {
      const auto& item = static_cast<const HirMatchStmt&>(statement);
      indexExpression(*item.value, snapshot, symbolKeys);
      for (const auto& matchCase : item.cases)
        indexBlock(matchCase.body, snapshot, symbolKeys);
      break;
    }
    case HirStmtKind::ForEach: {
      const auto& item = static_cast<const HirForEachStmt&>(statement);
      indexExpression(*item.iterator, snapshot, symbolKeys);
      indexExpression(*item.condition, snapshot, symbolKeys);
      indexExpression(*item.value, snapshot, symbolKeys);
      indexExpression(*item.advance, snapshot, symbolKeys);
      indexBlock(item.body, snapshot, symbolKeys);
      break;
    }
    case HirStmtKind::Unsafe:
      indexBlock(static_cast<const HirUnsafeStmt&>(statement).body, snapshot,
                 symbolKeys);
      break;
    case HirStmtKind::Break:
    case HirStmtKind::Continue:
      break;
    }
  }
}

void indexExpression(const HirExpr& expression, SemanticSnapshot& snapshot,
                     const std::vector<std::string>& symbolKeys) {
  switch (expression.kind) {
  case HirExprKind::Name:
    addOccurrence(snapshot, symbolKeys,
                  static_cast<const HirNameExpr&>(expression).symbol,
                  expression.location);
    break;
  case HirExprKind::FunctionRef:
    addOccurrence(snapshot, symbolKeys,
                  static_cast<const HirFunctionRefExpr&>(expression).symbol,
                  expression.location);
    break;
  case HirExprKind::Call: {
    const auto& item = static_cast<const HirCallExpr&>(expression);
    addOccurrence(snapshot, symbolKeys, item.callee, expression.location);
    for (const auto& argument : item.arguments)
      indexExpression(*argument, snapshot, symbolKeys);
    break;
  }
  case HirExprKind::AsyncCall: {
    const auto& item = static_cast<const HirAsyncCallExpr&>(expression);
    addOccurrence(snapshot, symbolKeys, item.callee, expression.location);
    for (const auto& argument : item.arguments)
      indexExpression(*argument, snapshot, symbolKeys);
    break;
  }
  case HirExprKind::Await:
    indexExpression(*static_cast<const HirAwaitExpr&>(expression).task,
                    snapshot, symbolKeys);
    break;
  case HirExprKind::Unary:
    indexExpression(*static_cast<const HirUnaryExpr&>(expression).operand,
                    snapshot, symbolKeys);
    break;
  case HirExprKind::Binary: {
    const auto& item = static_cast<const HirBinaryExpr&>(expression);
    indexExpression(*item.left, snapshot, symbolKeys);
    indexExpression(*item.right, snapshot, symbolKeys);
    break;
  }
  case HirExprKind::Array:
    for (const auto& element : static_cast<const HirArrayExpr&>(expression).elements)
      indexExpression(*element, snapshot, symbolKeys);
    break;
  case HirExprKind::Index: {
    const auto& item = static_cast<const HirIndexExpr&>(expression);
    indexExpression(*item.collection, snapshot, symbolKeys);
    indexExpression(*item.index, snapshot, symbolKeys);
    break;
  }
  case HirExprKind::Slice: {
    const auto& item = static_cast<const HirSliceExpr&>(expression);
    indexExpression(*item.collection, snapshot, symbolKeys);
    indexExpression(*item.start, snapshot, symbolKeys);
    indexExpression(*item.end, snapshot, symbolKeys);
    break;
  }
  case HirExprKind::Aggregate:
    for (const auto& argument :
         static_cast<const HirAggregateExpr&>(expression).arguments)
      indexExpression(*argument, snapshot, symbolKeys);
    break;
  case HirExprKind::Field:
    indexExpression(*static_cast<const HirFieldExpr&>(expression).value,
                    snapshot, symbolKeys);
    break;
  case HirExprKind::Propagate:
    indexExpression(*static_cast<const HirPropagateExpr&>(expression).value,
                    snapshot, symbolKeys);
    break;
  case HirExprKind::Literal:
    break;
  }
}

const std::string* sourceFor(const SemanticSnapshot& snapshot,
                             const Location& location) {
  const auto path = std::filesystem::absolute(location.file)
                        .lexically_normal().generic_string();
  const auto found = snapshot.sources.find(path);
  return found == snapshot.sources.end() ? nullptr : &found->second;
}

Location declarationNameLocation(const SemanticSnapshot& snapshot,
                                 Location location,
                                 const std::string& qualifiedName) {
  const auto* source = sourceFor(snapshot, location);
  if (source == nullptr) return location;
  const std::string name = shortSymbolName(qualifiedName);
  const std::size_t start = lineStart(*source, location.line);
  std::size_t end = source->find('\n', start);
  if (end == std::string::npos) end = source->size();
  const std::size_t found = source->find(name, start);
  if (found != std::string::npos && found < end)
    location.column = static_cast<int>(found - start + 1);
  return location;
}

void mergeHir(const HirModule& hir, const Module& ast,
              SemanticSnapshot& snapshot) {
  std::set<std::string> publicDeclarations;
  for (const auto& function : ast.functions)
    if (function.publicDeclaration)
      publicDeclarations.insert(symbolKey(function.location, function.name, "function"));
  for (const auto& structure : ast.structs)
    if (structure.publicDeclaration)
      publicDeclarations.insert(symbolKey(structure.location, structure.name, "type"));
  for (const auto& enumeration : ast.enums)
    if (enumeration.publicDeclaration)
      publicDeclarations.insert(symbolKey(enumeration.location, enumeration.name, "type"));
  for (const auto& trait : ast.traits)
    if (trait.publicDeclaration)
      publicDeclarations.insert(symbolKey(trait.location, trait.name, "trait"));

  std::vector<std::string> keys(hir.symbols.size());
  for (const auto& symbol : hir.symbols) {
    if (symbol.id >= keys.size() || symbol.location.file.empty() ||
        symbol.kind == SymbolKind::BuiltinFunction)
      continue;
    const std::string kind = symbolKindName(symbol.kind);
    const std::string key = symbolKey(symbol.location, symbol.name, kind);
    keys[symbol.id] = key;
    SemanticSymbol indexed;
    indexed.key = key;
    indexed.name = symbol.name;
    indexed.shortName = shortSymbolName(symbol.name);
    indexed.kind = kind;
    indexed.location = symbol.kind == SymbolKind::Function
                           ? declarationNameLocation(snapshot, symbol.location,
                                                     symbol.name)
                           : symbol.location;
    indexed.native = symbol.nativeImport || symbol.nativeExport;
    indexed.publicDeclaration =
        publicDeclarations.contains(symbolKey(symbol.location, symbol.name,
                                              "function"));
    if (kind == "function") {
      for (const auto& parameter : symbol.parameterTypes)
        indexed.parameters.push_back(typeName(parameter));
      std::ostringstream detail;
      detail << "fn " << indexed.shortName << '(';
      for (std::size_t parameter = 0; parameter < indexed.parameters.size();
           ++parameter) {
        if (parameter) detail << ", ";
        detail << indexed.parameters[parameter];
      }
      detail << ") -> " << typeName(symbol.type);
      indexed.detail = detail.str();
    } else {
      indexed.detail = indexed.shortName + ": " + typeName(symbol.type);
    }
    if (const auto* source = sourceFor(snapshot, symbol.location))
      indexed.documentation = documentationBefore(*source, symbol.location.line);
    snapshot.symbols.emplace(key, std::move(indexed));
  }

  for (const auto& declaration : hir.typeDeclarations) {
    if (declaration.builtin || declaration.location.file.empty()) continue;
    const std::string key = symbolKey(declaration.location, declaration.name, "type");
    const Location nameLocation = declarationNameLocation(
        snapshot, declaration.location, declaration.name);
    SemanticSymbol indexed{key, declaration.name,
                           shortSymbolName(declaration.name), "type",
                           declaration.kind == HirTypeDeclKind::Enum
                               ? "enum " + shortSymbolName(declaration.name)
                               : "struct " + shortSymbolName(declaration.name),
                           {}, nameLocation, {},
                           declaration.publicDeclaration,
                           declaration.kind == HirTypeDeclKind::NativeStruct ||
                               declaration.kind == HirTypeDeclKind::Opaque ||
                               declaration.kind == HirTypeDeclKind::Callback};
    if (const auto* source = sourceFor(snapshot, declaration.location))
      indexed.documentation = documentationBefore(*source, declaration.location.line);
    snapshot.symbols.emplace(key, indexed);
    snapshot.occurrences.push_back({key, indexed.shortName, indexed.location,
                                    identifierLength(indexed.shortName), true});
    for (const auto& field : declaration.fields) {
      const std::string fieldKey = symbolKey(field.location,
                                             declaration.name + "." + field.name,
                                             "field");
      SemanticSymbol fieldSymbol{fieldKey, declaration.name + "." + field.name,
                                 field.name, "field",
                                 field.name + ": " + typeName(field.type), {},
                                 field.location, {}, declaration.publicDeclaration,
                                 indexed.native};
      snapshot.symbols.emplace(fieldKey, fieldSymbol);
      snapshot.occurrences.push_back({fieldKey, field.name, field.location,
                                      identifierLength(field.name), true});
    }
  }
  for (const auto& declaration : hir.traitDeclarations) {
    if (declaration.location.file.empty()) continue;
    const std::string key = symbolKey(declaration.location, declaration.name, "trait");
    const Location nameLocation = declarationNameLocation(
        snapshot, declaration.location, declaration.name);
    SemanticSymbol indexed{key, declaration.name,
                           shortSymbolName(declaration.name), "trait",
                           "trait " + shortSymbolName(declaration.name), {},
                           nameLocation, {}, declaration.publicDeclaration,
                           false};
    if (const auto* source = sourceFor(snapshot, declaration.location))
      indexed.documentation = documentationBefore(*source, declaration.location.line);
    snapshot.symbols.emplace(key, indexed);
    snapshot.occurrences.push_back({key, indexed.shortName, indexed.location,
                                    identifierLength(indexed.shortName), true});
  }

  for (const auto& symbol : hir.symbols) {
    if (symbol.id < keys.size() && !keys[symbol.id].empty()) {
      const auto found = snapshot.symbols.find(keys[symbol.id]);
      if (found != snapshot.symbols.end())
        addOccurrence(snapshot, keys, symbol.id, found->second.location, true);
    }
  }
  for (const auto& function : hir.functions) indexBlock(function.body, snapshot, keys);
}

void mergeIncompleteAst(const Module& module, SemanticSnapshot& snapshot) {
  auto add = [&](const std::string& name, const std::string& kind,
                 const std::string& detail, const Location& location,
                 bool publicDeclaration) {
    const std::string key = symbolKey(location, name, kind);
    if (snapshot.symbols.contains(key)) return;
    const Location nameLocation = declarationNameLocation(snapshot, location, name);
    SemanticSymbol indexed{key, name, shortSymbolName(name), kind, detail, {},
                           nameLocation, {}, publicDeclaration, false};
    if (const auto* source = sourceFor(snapshot, location))
      indexed.documentation = documentationBefore(*source, location.line);
    snapshot.symbols.emplace(key, indexed);
    snapshot.occurrences.push_back({key, indexed.shortName, nameLocation,
                                    identifierLength(indexed.shortName), true});
  };
  for (const auto& function : module.functions) {
    std::ostringstream detail;
    detail << "fn " << shortSymbolName(function.name) << '(';
    for (std::size_t parameter = 0; parameter < function.parameters.size(); ++parameter) {
      if (parameter) detail << ", ";
      detail << function.parameters[parameter].name << ": "
             << function.parameters[parameter].typeName;
    }
    detail << ") -> " << function.returnType;
    add(function.name, "function", detail.str(), function.location,
        function.publicDeclaration);
  }
  for (const auto& structure : module.structs)
    add(structure.name, "type", "struct " + shortSymbolName(structure.name),
        structure.location, structure.publicDeclaration);
  for (const auto& enumeration : module.enums)
    add(enumeration.name, "type", "enum " + shortSymbolName(enumeration.name),
        enumeration.location, enumeration.publicDeclaration);
  for (const auto& trait : module.traits)
    add(trait.name, "trait", "trait " + shortSymbolName(trait.name),
        trait.location, trait.publicDeclaration);
}

void resolveFallbackOccurrences(SemanticSnapshot& snapshot) {
  std::map<std::string, std::vector<std::string>> byName;
  for (const auto& [key, symbol] : snapshot.symbols) {
    auto& matches = byName[symbol.shortName];
    const auto sameDefinition = std::find_if(
        matches.begin(), matches.end(), [&](const std::string& existingKey) {
          const auto& existing = snapshot.symbols.at(existingKey);
          return std::filesystem::absolute(existing.location.file).lexically_normal() ==
                     std::filesystem::absolute(symbol.location.file).lexically_normal() &&
                 existing.location.line == symbol.location.line &&
                 existing.location.column == symbol.location.column;
        });
    if (sameDefinition == matches.end()) matches.push_back(key);
    else if (symbol.name.size() > snapshot.symbols.at(*sameDefinition).name.size())
      *sameDefinition = key;
  }
  std::set<std::string> existing;
  for (const auto& occurrence : snapshot.occurrences)
    existing.insert(std::filesystem::absolute(occurrence.location.file)
                        .lexically_normal().generic_string() + ":" +
                    std::to_string(occurrence.location.line) + ":" +
                    std::to_string(occurrence.location.column));
  for (const auto& [uri, analysis] : snapshot.analyses) {
    for (const auto& token : analysis.tokens) {
      if (token.kind != TokenKind::Identifier) continue;
      const std::string locationKey =
          std::filesystem::absolute(token.location.file)
              .lexically_normal().generic_string() + ":" +
          std::to_string(token.location.line) + ":" +
          std::to_string(token.location.column);
      if (existing.contains(locationKey)) continue;
      const auto matches = byName.find(token.text);
      if (matches != byName.end() && matches->second.size() == 1)
        snapshot.occurrences.push_back({matches->second.front(), token.text,
                                        token.location, token.text.size(), false});
    }
  }
}

enum class FrameStatus { Message, End, Error };

FrameStatus readFrame(std::istream& input, std::string& body,
                      std::string& error) {
  std::optional<std::size_t> contentLength;
  std::size_t headerBytes = 0;
  bool sawHeader = false;
  while (true) {
    std::string line;
    bool sawLineByte = false;
    bool terminated = false;
    char character = '\0';
    while (input.get(character)) {
      sawHeader = true;
      sawLineByte = true;
      if (++headerBytes > MaximumHeaderBytes) {
        error = "LSP header exceeds 16 KiB";
        return FrameStatus::Error;
      }
      if (character == '\n') {
        terminated = true;
        break;
      }
      line.push_back(character);
    }
    if (!sawLineByte && input.eof()) {
      if (!sawHeader) return FrameStatus::End;
      error = "truncated LSP header";
      return FrameStatus::Error;
    }
    if (!terminated) {
      error = "truncated LSP header";
      return FrameStatus::Error;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) break;
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos) {
      error = "malformed LSP header";
      return FrameStatus::Error;
    }
    std::string name = line.substr(0, colon);
    std::transform(name.begin(), name.end(), name.begin(), [](char character) {
      return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    });
    std::string value = line.substr(colon + 1);
    const std::size_t first = value.find_first_not_of(" \t");
    value = first == std::string::npos ? std::string{} : value.substr(first);
    if (name == "content-length") {
      if (contentLength) {
        error = "duplicate Content-Length header";
        return FrameStatus::Error;
      }
      unsigned long long parsed = 0;
      const auto converted = std::from_chars(value.data(),
                                             value.data() + value.size(), parsed);
      if (converted.ec != std::errc{} ||
          converted.ptr != value.data() + value.size() ||
          parsed > MaximumMessageBytes) {
        error = "invalid or oversized Content-Length header";
        return FrameStatus::Error;
      }
      contentLength = static_cast<std::size_t>(parsed);
    }
  }
  if (!contentLength) {
    error = "missing Content-Length header";
    return FrameStatus::Error;
  }
  body.assign(*contentLength, '\0');
  input.read(body.data(), static_cast<std::streamsize>(*contentLength));
  if (static_cast<std::size_t>(input.gcount()) != *contentLength) {
    error = "truncated LSP message body";
    return FrameStatus::Error;
  }
  return FrameStatus::Message;
}

class Protocol01ServerSession {
public:
  Protocol01ServerSession(std::istream& input, std::ostream& output,
                          std::ostream& log)
      : input_(input), output_(output), log_(log) {}

  int run() {
    while (!exitReceived_) {
      std::string body;
      std::string frameError;
      const FrameStatus status = readFrame(input_, body, frameError);
      if (status == FrameStatus::End) break;
      if (status == FrameStatus::Error) {
        sendError(Json(), -32700, frameError);
        break;
      }
      std::string parseError;
      auto message = JsonParser(body).parse(parseError);
      if (!message) {
        sendError(Json(), -32700, parseError);
        continue;
      }
      handle(*message);
    }
    return exitReceived_ && !cleanExit_ ? 1 : 0;
  }

private:
  struct Document {
    std::string text;
    long long version = 0;
  };

  void send(const Json& message) {
    const std::string body = serialize(message);
    output_ << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    output_.flush();
  }

  void sendResult(const Json& id, Json result) {
    send(Json(Json::Object{{"id", id},
                           {"jsonrpc", Json("2.0")},
                           {"result", std::move(result)}}));
  }

  void sendError(const Json& id, long long code, const std::string& message) {
    Json::Object error{{"code", Json::integer(code)}, {"message", Json(message)}};
    send(Json(Json::Object{{"error", Json(std::move(error))},
                           {"id", id},
                           {"jsonrpc", Json("2.0")}}));
  }

  void invalidNotification(const std::string& method) {
    log_ << "rocket-lsp: ignored invalid " << method << " notification\n";
  }

  void handle(const Json& message) {
    const auto* object = asObject(&message);
    const Json* idValue = field(object, "id");
    const bool request = idValue != nullptr;
    const Json id = request ? *idValue : Json();
    const auto* version = asString(field(object, "jsonrpc"));
    const auto* method = asString(field(object, "method"));
    if (object == nullptr || version == nullptr || *version != "2.0" ||
        method == nullptr) {
      sendError(request ? id : Json(), -32600, "invalid JSON-RPC request");
      return;
    }

    if (*method == "exit") {
      if (request) {
        sendError(id, -32600, "exit must be a notification");
        return;
      }
      cleanExit_ = shutdownRequested_;
      exitReceived_ = true;
      return;
    }

    if (*method == "initialize") {
      if (!request || initialized_ || shutdownRequested_) {
        if (request) sendError(id, -32600, "server is already initialized");
        return;
      }
      Json::Object save{{"includeText", Json(true)}};
      Json::Object sync{{"change", Json::integer(1)},
                        {"openClose", Json(true)},
                        {"save", Json(std::move(save))}};
      Json::Object capabilities{{"positionEncoding", Json("utf-16")},
                                {"textDocumentSync", Json(std::move(sync))}};
      Json::Object serverInfo{{"name", Json("rocket-lsp")},
                              {"version", Json("1.0.0")}};
      sendResult(id, Json(Json::Object{
                         {"capabilities", Json(std::move(capabilities))},
                         {"serverInfo", Json(std::move(serverInfo))}}));
      initialized_ = true;
      return;
    }

    if (*method == "shutdown") {
      if (!request) return;
      if (!initialized_ || shutdownRequested_) {
        sendError(id, -32600, "server cannot be shut down in its current state");
        return;
      }
      shutdownRequested_ = true;
      sendResult(id, Json());
      return;
    }

    if (!initialized_) {
      if (request) sendError(id, -32002, "server not initialized");
      return;
    }
    if (shutdownRequested_) {
      if (request) sendError(id, -32600, "server is shutting down");
      return;
    }
    if (*method == "initialized") {
      if (request) sendError(id, -32600, "initialized must be a notification");
      return;
    }
    if (*method == "textDocument/didOpen") {
      if (request) {
        sendError(id, -32600, "didOpen must be a notification");
        return;
      }
      didOpen(field(object, "params"), *method);
      return;
    }
    if (*method == "textDocument/didChange") {
      if (request) {
        sendError(id, -32600, "didChange must be a notification");
        return;
      }
      didChange(field(object, "params"), *method);
      return;
    }
    if (*method == "textDocument/didSave") {
      if (request) {
        sendError(id, -32600, "didSave must be a notification");
        return;
      }
      didSave(field(object, "params"), *method);
      return;
    }
    if (*method == "textDocument/didClose") {
      if (request) {
        sendError(id, -32600, "didClose must be a notification");
        return;
      }
      didClose(field(object, "params"), *method);
      return;
    }
    if (request) sendError(id, -32601, "method not found: " + *method);
  }

  void didOpen(const Json* params, const std::string& method) {
    const auto* document = asObject(field(asObject(params), "textDocument"));
    const auto* uri = asString(field(document, "uri"));
    const auto* text = asString(field(document, "text"));
    long long version = 0;
    if (uri == nullptr || text == nullptr ||
        !integerValue(field(document, "version"), version)) {
      invalidNotification(method);
      return;
    }
    documents_[*uri] = {*text, version};
    publish(*uri, documents_[*uri]);
  }

  void didChange(const Json* params, const std::string& method) {
    const auto* paramsObject = asObject(params);
    const auto* document = asObject(field(paramsObject, "textDocument"));
    const auto* uri = asString(field(document, "uri"));
    long long version = 0;
    const auto* changes = asArray(field(paramsObject, "contentChanges"));
    if (uri == nullptr || !integerValue(field(document, "version"), version) ||
        changes == nullptr || changes->size() != 1) {
      invalidNotification(method);
      return;
    }
    const auto* change = asObject(&changes->front());
    const auto* text = asString(field(change, "text"));
    if (change == nullptr || text == nullptr || field(change, "range") != nullptr) {
      invalidNotification(method);
      return;
    }
    const auto found = documents_.find(*uri);
    if (found == documents_.end() || version <= found->second.version) return;
    found->second = {*text, version};
    publish(*uri, found->second);
  }

  void didSave(const Json* params, const std::string& method) {
    const auto* paramsObject = asObject(params);
    const auto* document = asObject(field(paramsObject, "textDocument"));
    const auto* uri = asString(field(document, "uri"));
    if (uri == nullptr) {
      invalidNotification(method);
      return;
    }
    const auto found = documents_.find(*uri);
    if (found == documents_.end()) return;
    if (const auto* text = asString(field(paramsObject, "text")))
      found->second.text = *text;
    publish(*uri, found->second);
  }

  void didClose(const Json* params, const std::string& method) {
    const auto* document = asObject(field(asObject(params), "textDocument"));
    const auto* uri = asString(field(document, "uri"));
    if (uri == nullptr) {
      invalidNotification(method);
      return;
    }
    long long version = 0;
    const auto found = documents_.find(*uri);
    if (found != documents_.end()) {
      version = found->second.version;
      documents_.erase(found);
    }
    sendDiagnostics(*uri, version, {});
  }

  void publish(const std::string& uri, const Document& document) {
    Analysis analysis = analyzeDocument(uri, document.text);
    Json::Array diagnostics;
    diagnostics.reserve(analysis.diagnostics.size());
    for (const auto& diagnostic : analysis.diagnostics.all())
      diagnostics.push_back(diagnosticJson(diagnostic, analysis, document.text));
    sendDiagnostics(uri, document.version, std::move(diagnostics));
  }

  void sendDiagnostics(const std::string& uri, long long version,
                       Json::Array diagnostics) {
    Json::Object params{{"diagnostics", Json(std::move(diagnostics))},
                        {"uri", Json(uri)},
                        {"version", Json::integer(version)}};
    send(Json(Json::Object{{"jsonrpc", Json("2.0")},
                           {"method", Json("textDocument/publishDiagnostics")},
                           {"params", Json(std::move(params))}}));
  }

  std::istream& input_;
  std::ostream& output_;
  std::ostream& log_;
  std::map<std::string, Document> documents_;
  bool initialized_ = false;
  bool shutdownRequested_ = false;
  bool exitReceived_ = false;
  bool cleanExit_ = false;
};

class ServerSession {
public:
  ServerSession(std::istream& input, std::ostream& output, std::ostream& log)
      : input_(input), output_(output), log_(log) {}

  int run() {
    while (!exitReceived_) {
      std::string body;
      std::string frameError;
      const FrameStatus status = readFrame(input_, body, frameError);
      if (status == FrameStatus::End) break;
      if (status == FrameStatus::Error) {
        sendError(Json(), -32700, frameError);
        break;
      }
      std::string parseError;
      auto message = JsonParser(body).parse(parseError);
      if (!message) {
        sendError(Json(), -32700, parseError);
        continue;
      }
      handle(*message);
    }
    return exitReceived_ && !cleanExit_ ? 1 : 0;
  }

private:
  struct Document {
    std::string text;
    long long version = 0;
    std::filesystem::path path;
  };

  struct Configuration {
    std::size_t maximumProjectFiles = DefaultMaximumProjectFiles;
    std::size_t maximumProjectBytes = DefaultMaximumProjectBytes;
    bool telemetry = true;
  };

  void send(const Json& message) {
    const std::string body = serialize(message);
    output_ << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    output_.flush();
  }

  void sendResult(const Json& id, Json result) {
    send(Json(Json::Object{{"id", id},
                           {"jsonrpc", Json("2.0")},
                           {"result", std::move(result)}}));
  }

  void sendError(const Json& id, long long code, const std::string& message) {
    Json::Object error{{"code", Json::integer(code)}, {"message", Json(message)}};
    send(Json(Json::Object{{"error", Json(std::move(error))},
                           {"id", id},
                           {"jsonrpc", Json("2.0")}}));
  }

  void notify(const std::string& method, Json params) {
    send(Json(Json::Object{{"jsonrpc", Json("2.0")},
                           {"method", Json(method)},
                           {"params", std::move(params)}}));
  }

  void invalidNotification(const std::string& method) {
    log_ << "rocket-lsp: ignored invalid " << method << " notification\n";
  }

  static bool readSourceFile(const std::filesystem::path& path,
                             std::string& source) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::ostringstream buffer;
    buffer << input.rdbuf();
    source = buffer.str();
    return static_cast<bool>(input) || input.eof();
  }

  void configureWorkspace(const Json* params) {
    const auto* object = asObject(params);
    const auto* rootUri = asString(field(object, "rootUri"));
    if (rootUri == nullptr) {
      if (const auto* folders = asArray(field(object, "workspaceFolders"));
          folders != nullptr && !folders->empty())
        rootUri = asString(field(asObject(&folders->front()), "uri"));
    }
    if (rootUri != nullptr) {
      if (const auto path = pathFromUri(*rootUri)) workspaceRoot_ = *path;
    }
    if (workspaceRoot_.empty()) {
      packageRoot_.clear();
      packageEntry_.clear();
      dependencyRoots_.clear();
      return;
    }
    if (std::filesystem::is_regular_file(workspaceRoot_))
      workspaceRoot_ = workspaceRoot_.parent_path();
    packageRoot_ = workspaceRoot_;
    packageEntry_.clear();
    dependencyRoots_.clear();
    std::string error;
    auto package = loadPackage(workspaceRoot_, error);
    if (!package) return;
    packageRoot_ = package->root;
    packageEntry_ = package->entry;
    PackageLock lock;
    if (!prepareLockedPackageDependencies(*package, true, dependencyRoots_,
                                          lock, error)) {
      // Opening a workspace remains offline and side-effect free. Missing or
      // poisoned cache state is reported through normal source analysis/logs;
      // the server never attempts a transport refill.
      log_ << "rocket-lsp: locked dependency graph unavailable: " << error << '\n';
      dependencyRoots_.clear();
    }
  }

  void applyConfiguration(const Json* params) {
    const Json* settings = field(asObject(params), "settings");
    const auto* root = asObject(settings);
    if (const auto* rocket = asObject(field(root, "rocket"))) root = rocket;
    if (const auto* languageServer = asObject(field(root, "languageServer")))
      root = languageServer;
    long long value = 0;
    if (integerValue(field(root, "maximumProjectFiles"), value) &&
        value >= 16 && value <= 16384)
      configuration_.maximumProjectFiles = static_cast<std::size_t>(value);
    if (integerValue(field(root, "maximumProjectBytes"), value) &&
        value >= 1024 * 1024 && value <= 256LL * 1024 * 1024)
      configuration_.maximumProjectBytes = static_cast<std::size_t>(value);
    bool enabled = true;
    if (booleanValue(field(root, "telemetry"), enabled))
      configuration_.telemetry = enabled;
  }

  void mergeDiagnostics(const Diagnostics& diagnostics,
                        SemanticSnapshot& snapshot) {
    for (const auto& diagnostic : diagnostics.all()) {
      const std::string uri = uriFromPath(diagnostic.location.file);
      auto found = snapshot.analyses.find(uri);
      if (found == snapshot.analyses.end()) {
        Analysis empty;
        if (const auto* source = sourceFor(snapshot, diagnostic.location))
          empty.tokens = Lexer(diagnostic.location.file, *source,
                               empty.diagnostics).lex();
        found = snapshot.analyses.emplace(uri, std::move(empty)).first;
      }
      found->second.diagnostics.error(diagnostic.location, diagnostic.message,
                                      diagnostic.code);
    }
  }

  void addSnapshotSource(SemanticSnapshot& snapshot,
                         const std::filesystem::path& path,
                         std::string source) {
    const auto normalized = std::filesystem::absolute(path).lexically_normal();
    const std::string key = normalized.generic_string();
    if (snapshot.sources.contains(key) ||
        source.size() > MaximumDocumentBytes ||
        snapshot.filesAnalyzed >= configuration_.maximumProjectFiles ||
        snapshot.bytesAnalyzed + source.size() > configuration_.maximumProjectBytes)
      return;
    snapshot.bytesAnalyzed += source.size();
    ++snapshot.filesAnalyzed;
    snapshot.sources.emplace(key, std::move(source));
  }

  void rebuildSnapshot() {
    const auto started = std::chrono::steady_clock::now();
    SemanticSnapshot next;
    next.generation = ++analysisGeneration_;
    SourceOverlays overlays;
    for (const auto& [uri, document] : documents_) {
      if (document.path.empty()) continue;
      overlays.emplace(document.path, document.text);
      addSnapshotSource(next, document.path, document.text);
    }

    std::string discoveryError;
    const auto files = workspaceRoot_.empty()
                           ? std::vector<std::filesystem::path>{}
                           : rocketSources(workspaceRoot_, discoveryError);
    for (const auto& file : files) {
      if (next.filesAnalyzed >= configuration_.maximumProjectFiles ||
          next.bytesAnalyzed >= configuration_.maximumProjectBytes)
        break;
      if (overlays.contains(std::filesystem::absolute(file).lexically_normal()))
        continue;
      std::error_code sizeError;
      const std::uintmax_t size = std::filesystem::file_size(file, sizeError);
      if (sizeError || size > MaximumDocumentBytes ||
          next.bytesAnalyzed + size > configuration_.maximumProjectBytes)
        continue;
      std::string source;
      if (readSourceFile(file, source)) addSnapshotSource(next, file, std::move(source));
    }
    for (const auto& dependency : dependencyRoots_) {
      std::string dependencyDiscoveryError;
      for (const auto& file : rocketSources(dependency.root,
                                             dependencyDiscoveryError)) {
        if (next.filesAnalyzed >= configuration_.maximumProjectFiles ||
            next.bytesAnalyzed >= configuration_.maximumProjectBytes)
          break;
        const auto normalized = std::filesystem::absolute(file).lexically_normal();
        if (overlays.contains(normalized)) continue;
        std::error_code sizeError;
        const std::uintmax_t size = std::filesystem::file_size(file, sizeError);
        if (sizeError || size > MaximumDocumentBytes ||
            next.bytesAnalyzed + size > configuration_.maximumProjectBytes)
          continue;
        std::string source;
        if (readSourceFile(file, source)) addSnapshotSource(next, file, std::move(source));
      }
    }

    for (const auto& [path, source] : next.sources) {
      Analysis analysis;
      analysis.tokens = Lexer(path, source, analysis.diagnostics).lex();
      Module module;
      if (!analysis.diagnostics.hasErrors())
        module = Parser(analysis.tokens, analysis.diagnostics).parseModule();
      const std::string uri = uriFromPath(path);
      next.analyses.emplace(uri, std::move(analysis));
      mergeIncompleteAst(module, next);
    }

    std::set<std::filesystem::path> roots;
    if (!packageEntry_.empty() && std::filesystem::is_regular_file(packageEntry_))
      roots.insert(std::filesystem::absolute(packageEntry_).lexically_normal());
    for (const auto& [uri, document] : documents_)
      if (!document.path.empty()) roots.insert(document.path);

    for (const auto& root : roots) {
      Diagnostics diagnostics;
      const auto analysisRoot = packageRoot_.empty() ? root.parent_path()
                                                     : packageRoot_;
      auto module = loadModuleGraph(root, analysisRoot, dependencyRoots_, overlays,
                                    diagnostics);
      if (module) {
        module->library = true;
        SemanticAnalyzer analyzer(*module, diagnostics);
        if (auto hir = analyzer.analyzeToHir()) mergeHir(*hir, *module, next);
      }
      mergeDiagnostics(diagnostics, next);
    }
    resolveFallbackOccurrences(next);
    next.invalidatedFiles = std::max<std::size_t>(1, roots.size());
    next.elapsedMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - started)
                                   .count();
    snapshot_ = std::move(next);
    publishAllDiagnostics();
    if (configuration_.telemetry && initialized_) {
      notify("rocket/analysisStatus", Json(Json::Object{
          {"bytes", Json::integer(static_cast<long long>(snapshot_.bytesAnalyzed))},
          {"elapsedMilliseconds", Json::integer(snapshot_.elapsedMilliseconds)},
          {"files", Json::integer(static_cast<long long>(snapshot_.filesAnalyzed))},
          {"generation", Json::integer(snapshot_.generation)},
          {"invalidatedFiles",
           Json::integer(static_cast<long long>(snapshot_.invalidatedFiles))}}));
    }
  }

  void publishAllDiagnostics() {
    for (const auto& [uri, document] : documents_) {
      Json::Array diagnostics;
      const auto found = snapshot_.analyses.find(uri);
      if (found != snapshot_.analyses.end()) {
        std::set<std::string> seen;
        for (const auto& diagnostic : found->second.diagnostics.all()) {
          const std::string key = diagnosticCodeName(diagnostic.code) + ":" +
                                  std::to_string(diagnostic.location.line) + ":" +
                                  std::to_string(diagnostic.location.column) + ":" +
                                  diagnostic.message;
          if (seen.insert(key).second)
            diagnostics.push_back(diagnosticJson(diagnostic, found->second,
                                                 document.text));
        }
      }
      sendDiagnostics(uri, document.version, std::move(diagnostics));
    }
  }

  void sendDiagnostics(const std::string& uri, long long version,
                       Json::Array diagnostics) {
    notify("textDocument/publishDiagnostics", Json(Json::Object{
        {"diagnostics", Json(std::move(diagnostics))}, {"uri", Json(uri)},
        {"version", Json::integer(version)}}));
  }

  Json capabilities() const {
    Json::Object save{{"includeText", Json(true)}};
    Json::Object sync{{"change", Json::integer(2)},
                      {"openClose", Json(true)},
                      {"save", Json(std::move(save))}};
    Json::Array completionTriggers{Json(".")};
    Json::Array signatureTriggers{Json("("), Json(",")};
    Json::Array tokenTypes{
        Json("namespace"), Json("type"), Json("struct"), Json("enum"),
        Json("interface"), Json("typeParameter"), Json("parameter"),
        Json("variable"), Json("property"), Json("function"), Json("method"),
        Json("keyword"), Json("string"), Json("number"), Json("comment"),
        Json("decorator")};
    Json::Array tokenModifiers{Json("declaration"), Json("definition"),
                               Json("readonly"), Json("static"),
                               Json("deprecated"), Json("abstract"),
                               Json("async"), Json("modification"),
                               Json("documentation"), Json("defaultLibrary")};
    Json::Object legend{{"tokenModifiers", Json(std::move(tokenModifiers))},
                        {"tokenTypes", Json(std::move(tokenTypes))}};
    Json::Object fullTokens{{"delta", Json(true)}};
    Json::Object semanticTokens{{"full", Json(std::move(fullTokens))},
                                {"legend", Json(std::move(legend))},
                                {"range", Json(false)}};
    Json::Array codeActionKinds{Json("quickfix"),
                                Json("source.fixAll.rocket"),
                                Json("source.format.rocket")};
    Json::Object codeActions{{"codeActionKinds", Json(std::move(codeActionKinds))},
                             {"resolveProvider", Json(false)}};
    Json::Object completion{{"resolveProvider", Json(false)},
                            {"triggerCharacters", Json(std::move(completionTriggers))}};
    Json::Object signature{{"retriggerCharacters", Json(Json::Array{Json(",")})},
                           {"triggerCharacters", Json(std::move(signatureTriggers))}};
    Json::Object rename{{"prepareProvider", Json(true)}};
    Json::Object workspaceFolders{{"changeNotifications", Json(true)},
                                  {"supported", Json(true)}};
    Json::Object workspace{{"workspaceFolders", Json(std::move(workspaceFolders))}};
    return Json(Json::Object{
        {"codeActionProvider", Json(std::move(codeActions))},
        {"completionProvider", Json(std::move(completion))},
        {"definitionProvider", Json(true)}, {"hoverProvider", Json(true)},
        {"positionEncoding", Json("utf-16")}, {"referencesProvider", Json(true)},
        {"renameProvider", Json(std::move(rename))},
        {"semanticTokensProvider", Json(std::move(semanticTokens))},
        {"signatureHelpProvider", Json(std::move(signature))},
        {"textDocumentSync", Json(std::move(sync))},
        {"workspace", Json(std::move(workspace))},
        {"workspaceSymbolProvider", Json(true)}});
  }

  void handle(const Json& message) {
    const auto* object = asObject(&message);
    const Json* idValue = field(object, "id");
    const bool request = idValue != nullptr;
    const Json id = request ? *idValue : Json();
    const auto* version = asString(field(object, "jsonrpc"));
    const auto* method = asString(field(object, "method"));
    if (object == nullptr || version == nullptr || *version != "2.0" ||
        method == nullptr) {
      sendError(request ? id : Json(), -32600, "invalid JSON-RPC request");
      return;
    }
    const Json* params = field(object, "params");

    if (*method == "$/cancelRequest") {
      if (request) {
        sendError(id, -32600, "$/cancelRequest must be a notification");
        return;
      }
      const Json* canceled = field(asObject(params), "id");
      if (canceled != nullptr && !requestKey(*canceled).empty())
        canceledRequests_.insert(requestKey(*canceled));
      return;
    }
    if (*method == "exit") {
      if (request) {
        sendError(id, -32600, "exit must be a notification");
        return;
      }
      cleanExit_ = shutdownRequested_;
      exitReceived_ = true;
      return;
    }
    if (*method == "initialize") {
      if (!request || initialized_ || shutdownRequested_) {
        if (request) sendError(id, -32600, "server is already initialized");
        return;
      }
      configureWorkspace(params);
      Json::Object serverInfo{{"name", Json("rocket-lsp")},
                              {"version", Json("1.0.0")}};
      sendResult(id, Json(Json::Object{{"capabilities", capabilities()},
                                      {"serverInfo", Json(std::move(serverInfo))}}));
      initialized_ = true;
      return;
    }
    if (*method == "shutdown") {
      if (!request) return;
      if (!initialized_ || shutdownRequested_) {
        sendError(id, -32600, "server cannot be shut down in its current state");
        return;
      }
      shutdownRequested_ = true;
      sendResult(id, Json());
      return;
    }
    if (!initialized_) {
      if (request) sendError(id, -32002, "server not initialized");
      return;
    }
    if (shutdownRequested_) {
      if (request) sendError(id, -32600, "server is shutting down");
      return;
    }
    if (request && canceledRequests_.erase(requestKey(id)) != 0) {
      sendError(id, -32800, "request cancelled");
      return;
    }
    if (*method == "initialized") {
      if (request) sendError(id, -32600, "initialized must be a notification");
      else rebuildSnapshot();
      return;
    }
    if (*method == "workspace/didChangeConfiguration") {
      if (request) sendError(id, -32600, "configuration change must be a notification");
      else { applyConfiguration(params); rebuildSnapshot(); }
      return;
    }
    if (*method == "workspace/didChangeWatchedFiles" ||
        *method == "workspace/didChangeWorkspaceFolders") {
      if (request) sendError(id, -32600, "workspace change must be a notification");
      else rebuildSnapshot();
      return;
    }
    if (*method == "textDocument/didOpen") {
      if (request) sendError(id, -32600, "didOpen must be a notification");
      else didOpen(params, *method);
      return;
    }
    if (*method == "textDocument/didChange") {
      if (request) sendError(id, -32600, "didChange must be a notification");
      else didChange(params, *method);
      return;
    }
    if (*method == "textDocument/didSave") {
      if (request) sendError(id, -32600, "didSave must be a notification");
      else didSave(params, *method);
      return;
    }
    if (*method == "textDocument/didClose") {
      if (request) sendError(id, -32600, "didClose must be a notification");
      else didClose(params, *method);
      return;
    }

    if (!request) return;
    if (*method == "textDocument/completion") completion(id, params);
    else if (*method == "textDocument/hover") hover(id, params);
    else if (*method == "textDocument/signatureHelp") signatureHelp(id, params);
    else if (*method == "textDocument/definition") definition(id, params);
    else if (*method == "textDocument/references") references(id, params);
    else if (*method == "textDocument/prepareRename") prepareRename(id, params);
    else if (*method == "textDocument/rename") rename(id, params);
    else if (*method == "textDocument/semanticTokens/full")
      semanticTokens(id, params, false);
    else if (*method == "textDocument/semanticTokens/full/delta")
      semanticTokens(id, params, true);
    else if (*method == "textDocument/codeAction") codeActions(id, params);
    else if (*method == "workspace/symbol") workspaceSymbols(id, params);
    else if (*method == "rocket/projectStatus") projectStatus(id);
    else sendError(id, -32601, "method not found: " + *method);
  }

  void didOpen(const Json* params, const std::string& method) {
    const auto* document = asObject(field(asObject(params), "textDocument"));
    const auto* uri = asString(field(document, "uri"));
    const auto* text = asString(field(document, "text"));
    long long version = 0;
    if (uri == nullptr || text == nullptr || text->size() > MaximumDocumentBytes ||
        !integerValue(field(document, "version"), version)) {
      invalidNotification(method);
      return;
    }
    const auto path = pathFromUri(*uri);
    documents_[*uri] = {*text, version, path.value_or(std::filesystem::path{})};
    rebuildSnapshot();
  }

  bool documentPosition(const Json* params, std::string& uri, long long& line,
                        long long& character) const {
    const auto* object = asObject(params);
    const auto* document = asObject(field(object, "textDocument"));
    const auto* uriValue = asString(field(document, "uri"));
    const auto* position = asObject(field(object, "position"));
    if (uriValue == nullptr || !integerValue(field(position, "line"), line) ||
        !integerValue(field(position, "character"), character) || line < 0 ||
        character < 0)
      return false;
    uri = *uriValue;
    return documents_.contains(uri) || snapshot_.analyses.contains(uri);
  }

  const std::string* sourceForUri(const std::string& uri) const {
    if (const auto document = documents_.find(uri); document != documents_.end())
      return &document->second.text;
    const auto path = pathFromUri(uri);
    if (!path) return nullptr;
    const auto found = snapshot_.sources.find(path->generic_string());
    return found == snapshot_.sources.end() ? nullptr : &found->second;
  }

  const SemanticOccurrence* occurrenceAt(const std::string& uri, long long line,
                                         long long character) const {
    const auto path = pathFromUri(uri);
    const auto* source = sourceForUri(uri);
    if (!path || source == nullptr) return nullptr;
    const std::string normalized = path->generic_string();
    const SemanticOccurrence* best = nullptr;
    for (const auto& occurrence : snapshot_.occurrences) {
      if (std::filesystem::absolute(occurrence.location.file)
              .lexically_normal().generic_string() != normalized ||
          occurrence.location.line - 1 != line)
        continue;
      const long long start = lspCharacter(*source, occurrence.location.line,
                                           occurrence.location.column);
      const long long end = start + static_cast<long long>(occurrence.length);
      if (character >= start && character <= end &&
          (best == nullptr || occurrence.length < best->length))
        best = &occurrence;
    }
    return best;
  }

  std::string completionPrefix(const std::string& source, long long line,
                               long long character) const {
    const auto offset = offsetAtPosition(source, line, character);
    if (!offset) return {};
    std::size_t start = *offset;
    while (start > 0) {
      const unsigned char previous = static_cast<unsigned char>(source[start - 1]);
      if (!(std::isalnum(previous) || previous == '_' || previous == '.')) break;
      --start;
    }
    return source.substr(start, *offset - start);
  }

  std::set<std::string> documentImports(const std::string& source) const {
    std::set<std::string> imports;
    std::istringstream input(source);
    std::string line;
    while (std::getline(input, line)) {
      const std::size_t first = line.find_first_not_of(" \t");
      if (first != std::string::npos && line.compare(first, 7, "import ") == 0) {
        std::string name = line.substr(first + 7);
        const std::size_t end = name.find_first_of(" \t\r#");
        if (end != std::string::npos) name.resize(end);
        imports.insert(name);
      }
    }
    return imports;
  }

  Json importTextEdit(const std::string& source, const std::string& module) const {
    long long insertionLine = 0;
    std::istringstream input(source);
    std::string line;
    long long current = 0;
    while (std::getline(input, line)) {
      const std::size_t first = line.find_first_not_of(" \t");
      if (first != std::string::npos && line.compare(first, 7, "import ") == 0)
        insertionLine = current + 1;
      else if (first != std::string::npos && line[first] != '#')
        break;
      ++current;
    }
    Json::Object position{{"line", Json::integer(insertionLine)},
                          {"character", Json::integer(0)}};
    Json::Object range{{"start", Json(position)}, {"end", Json(position)}};
    return Json(Json::Object{{"newText", Json("import " + module + "\n")},
                             {"range", Json(std::move(range))}});
  }

  static long long completionKind(const std::string& kind) {
    if (kind == "function") return 3;
    if (kind == "method") return 2;
    if (kind == "field") return 5;
    if (kind == "type" || kind == "trait") return 7;
    if (kind == "parameter") return 6;
    return 6;
  }

  void completion(const Json& id, const Json* params) {
    std::string uri;
    long long line = 0, character = 0;
    if (!documentPosition(params, uri, line, character)) {
      sendError(id, -32602, "completion requires an open Rocket document position");
      return;
    }
    const auto* source = sourceForUri(uri);
    if (source == nullptr) { sendResult(id, Json(Json::Array{})); return; }
    const std::string prefix = completionPrefix(*source, line, character);
    const std::string filter = prefix.find('.') == std::string::npos
                                   ? prefix
                                   : prefix.substr(prefix.rfind('.') + 1);
    const auto currentPath = pathFromUri(uri);
    const auto imports = documentImports(*source);
    Json::Array items;
    std::set<std::string> emitted;
    for (const auto& [key, symbol] : snapshot_.symbols) {
      if (!filter.empty() && !symbol.shortName.starts_with(filter)) continue;
      const bool local = currentPath &&
          std::filesystem::absolute(symbol.location.file).lexically_normal() == *currentPath;
      if (!local && !symbol.publicDeclaration) continue;
      std::string module;
      const std::size_t dot = symbol.name.rfind('.');
      if (dot != std::string::npos) module = symbol.name.substr(0, dot);
      const bool imported = module.empty() || imports.contains(module) || local;
      const std::string dedupe = symbol.shortName + "\n" + symbol.detail;
      if (!emitted.insert(dedupe).second) continue;
      Json::Object item{{"detail", Json(symbol.detail)},
                        {"filterText", Json(symbol.shortName)},
                        {"kind", Json::integer(completionKind(symbol.kind))},
                        {"label", Json(symbol.shortName)},
                        {"sortText", Json(std::string(local ? "0" : imported ? "1" : "2") +
                                          ":" + symbol.shortName + ":" + symbol.name)}};
      if (!symbol.documentation.empty())
        item.emplace("documentation", Json(Json::Object{
            {"kind", Json("markdown")}, {"value", Json(symbol.documentation)}}));
      if (!imported && !module.empty())
        item.emplace("additionalTextEdits",
                     Json(Json::Array{importTextEdit(*source, module)}));
      items.emplace_back(Json(std::move(item)));
      if (items.size() >= 512) break;
    }
    static const std::vector<std::string> keywords{
        "fn", "let", "var", "if", "else", "while", "for", "return",
        "match", "case", "struct", "enum", "trait", "impl", "import",
        "pub", "unsafe"};
    for (const auto& keyword : keywords)
      if (filter.empty() || keyword.starts_with(filter))
        items.emplace_back(Json(Json::Object{{"kind", Json::integer(14)},
                                            {"label", Json(keyword)},
                                            {"sortText", Json("9:" + keyword)}}));
    sendResult(id, Json(Json::Object{{"isIncomplete", Json(false)},
                                    {"items", Json(std::move(items))}}));
  }

  void hover(const Json& id, const Json* params) {
    std::string uri;
    long long line = 0, character = 0;
    if (!documentPosition(params, uri, line, character)) {
      sendError(id, -32602, "hover requires a Rocket document position"); return;
    }
    const auto* occurrence = occurrenceAt(uri, line, character);
    if (occurrence == nullptr) { sendResult(id, Json()); return; }
    const auto symbol = snapshot_.symbols.find(occurrence->symbol);
    if (symbol == snapshot_.symbols.end()) { sendResult(id, Json()); return; }
    std::string markdown = "```rocket\n" + symbol->second.detail + "\n```";
    if (!symbol->second.documentation.empty())
      markdown += "\n\n" + symbol->second.documentation;
    markdown += "\n\n[Versioned package documentation](rocket-doc://" +
                percentEncodePath(symbol->second.name) + ")";
    sendResult(id, Json(Json::Object{
        {"contents", Json(Json::Object{{"kind", Json("markdown")},
                                       {"value", Json(markdown)}})},
        {"range", rangeJson(line,
                            lspCharacter(*sourceForUri(uri),
                                         occurrence->location.line,
                                         occurrence->location.column),
                            static_cast<long long>(occurrence->length))}}));
  }

  void signatureHelp(const Json& id, const Json* params) {
    std::string uri;
    long long line = 0, character = 0;
    if (!documentPosition(params, uri, line, character)) {
      sendError(id, -32602, "signature help requires a Rocket document position"); return;
    }
    const auto* source = sourceForUri(uri);
    const auto offset = source == nullptr ? std::nullopt
                                          : offsetAtPosition(*source, line, character);
    if (!offset) { sendResult(id, Json(Json::Object{{"signatures", Json(Json::Array{})}})); return; }
    std::size_t open = *offset;
    int depth = 0;
    while (open > 0) {
      --open;
      if ((*source)[open] == ')') ++depth;
      else if ((*source)[open] == '(') {
        if (depth == 0) break;
        --depth;
      }
    }
    if ((*source)[open] != '(') { sendResult(id, Json(Json::Object{{"signatures", Json(Json::Array{})}})); return; }
    std::size_t nameEnd = open;
    while (nameEnd > 0 && std::isspace(static_cast<unsigned char>((*source)[nameEnd - 1]))) --nameEnd;
    std::size_t nameStart = nameEnd;
    while (nameStart > 0) {
      const unsigned char value = static_cast<unsigned char>((*source)[nameStart - 1]);
      if (!(std::isalnum(value) || value == '_' || value == '.')) break;
      --nameStart;
    }
    const std::string name = source->substr(nameStart, nameEnd - nameStart);
    long long activeParameter = 0;
    depth = 0;
    for (std::size_t index = open + 1; index < *offset; ++index) {
      if ((*source)[index] == '(' || (*source)[index] == '[') ++depth;
      else if ((*source)[index] == ')' || (*source)[index] == ']') --depth;
      else if ((*source)[index] == ',' && depth == 0) ++activeParameter;
    }
    Json::Array signatures;
    for (const auto& [key, symbol] : snapshot_.symbols) {
      if (symbol.kind != "function" ||
          (symbol.shortName != shortSymbolName(name) && symbol.name != name))
        continue;
      Json::Array parameters;
      for (const auto& parameter : symbol.parameters)
        parameters.emplace_back(Json(Json::Object{{"label", Json(parameter)}}));
      Json::Object signature{{"label", Json(symbol.detail)},
                             {"parameters", Json(std::move(parameters))}};
      if (!symbol.documentation.empty())
        signature.emplace("documentation", Json(symbol.documentation));
      signatures.emplace_back(Json(std::move(signature)));
    }
    sendResult(id, Json(Json::Object{
        {"activeParameter", Json::integer(activeParameter)},
        {"activeSignature", Json::integer(0)},
        {"signatures", Json(std::move(signatures))}}));
  }

  Json occurrenceRange(const SemanticOccurrence& occurrence) const {
    const auto path = std::filesystem::absolute(occurrence.location.file)
                          .lexically_normal();
    const auto source = snapshot_.sources.find(path.generic_string());
    const long long character = source == snapshot_.sources.end()
                                    ? std::max(0, occurrence.location.column - 1)
                                    : lspCharacter(source->second,
                                                   occurrence.location.line,
                                                   occurrence.location.column);
    return rangeJson(std::max(0, occurrence.location.line - 1), character,
                     static_cast<long long>(occurrence.length));
  }

  Json symbolLocation(const SemanticSymbol& symbol) const {
    SemanticOccurrence occurrence{symbol.key, symbol.shortName, symbol.location,
                                  identifierLength(symbol.shortName), true};
    return Json(Json::Object{{"uri", Json(uriFromPath(symbol.location.file))},
                             {"range", occurrenceRange(occurrence)}});
  }

  void definition(const Json& id, const Json* params) {
    std::string uri;
    long long line = 0, character = 0;
    if (!documentPosition(params, uri, line, character)) {
      sendError(id, -32602, "definition requires a Rocket document position"); return;
    }
    const auto* occurrence = occurrenceAt(uri, line, character);
    if (occurrence == nullptr) { sendResult(id, Json(Json::Array{})); return; }
    const auto symbol = snapshot_.symbols.find(occurrence->symbol);
    if (symbol == snapshot_.symbols.end()) { sendResult(id, Json(Json::Array{})); return; }
    sendResult(id, Json(Json::Array{symbolLocation(symbol->second)}));
  }

  void references(const Json& id, const Json* params) {
    std::string uri;
    long long line = 0, character = 0;
    if (!documentPosition(params, uri, line, character)) {
      sendError(id, -32602, "references requires a Rocket document position"); return;
    }
    const auto* occurrence = occurrenceAt(uri, line, character);
    if (occurrence == nullptr) { sendResult(id, Json(Json::Array{})); return; }
    bool includeDeclaration = true;
    booleanValue(field(asObject(field(asObject(params), "context")),
                       "includeDeclaration"), includeDeclaration);
    Json::Array locations;
    std::set<std::string> seen;
    for (const auto& reference : snapshot_.occurrences) {
      if (reference.symbol != occurrence->symbol ||
          (!includeDeclaration && reference.definition))
        continue;
      const std::string key = uriFromPath(reference.location.file) + ":" +
                              std::to_string(reference.location.line) + ":" +
                              std::to_string(reference.location.column);
      if (!seen.insert(key).second) continue;
      locations.emplace_back(Json(Json::Object{
          {"uri", Json(uriFromPath(reference.location.file))},
          {"range", occurrenceRange(reference)}}));
    }
    sendResult(id, Json(std::move(locations)));
  }

  bool pathInsideWorkspace(const std::filesystem::path& path) const {
    const auto normalized = std::filesystem::absolute(path).lexically_normal();
    if (workspaceRoot_.empty()) {
      for (const auto& [uri, document] : documents_)
        if (!document.path.empty() && document.path == normalized) return true;
      return false;
    }
    const auto relative = normalized.lexically_relative(workspaceRoot_);
    if (relative.empty() && normalized != workspaceRoot_) return false;
    for (const auto& component : relative)
      if (component == "..") return false;
    return !relative.is_absolute();
  }

  void prepareRename(const Json& id, const Json* params) {
    std::string uri;
    long long line = 0, character = 0;
    if (!documentPosition(params, uri, line, character)) {
      sendError(id, -32602, "prepareRename requires a Rocket document position"); return;
    }
    const auto* occurrence = occurrenceAt(uri, line, character);
    if (occurrence == nullptr) {
      sendError(id, -32602, "the selected token is not a resolved Rocket symbol"); return;
    }
    const auto symbol = snapshot_.symbols.find(occurrence->symbol);
    if (symbol == snapshot_.symbols.end() || symbol->second.native ||
        !pathInsideWorkspace(symbol->second.location.file)) {
      sendError(id, -32602,
                "native, standard-library, and locked-dependency symbols cannot be renamed");
      return;
    }
    sendResult(id, Json(Json::Object{{"placeholder", Json(symbol->second.shortName)},
                                    {"range", occurrenceRange(*occurrence)}}));
  }

  void rename(const Json& id, const Json* params) {
    std::string uri;
    long long line = 0, character = 0;
    if (!documentPosition(params, uri, line, character)) {
      sendError(id, -32602, "rename requires a Rocket document position"); return;
    }
    const auto* newName = asString(field(asObject(params), "newName"));
    if (newName == nullptr || !validIdentifier(*newName)) {
      sendError(id, -32602, "rename target must be a non-keyword Rocket identifier"); return;
    }
    const auto* occurrence = occurrenceAt(uri, line, character);
    if (occurrence == nullptr) {
      sendError(id, -32602, "the selected token is not a resolved Rocket symbol"); return;
    }
    const auto symbol = snapshot_.symbols.find(occurrence->symbol);
    if (symbol == snapshot_.symbols.end() || symbol->second.native ||
        !pathInsideWorkspace(symbol->second.location.file)) {
      sendError(id, -32602,
                "native, standard-library, and locked-dependency symbols cannot be renamed");
      return;
    }
    const std::string owner = symbol->second.name.substr(
        0, symbol->second.name.size() - symbol->second.shortName.size());
    for (const auto& [key, candidate] : snapshot_.symbols) {
      if (key != symbol->first && candidate.shortName == *newName &&
          candidate.name.starts_with(owner)) {
        sendError(id, -32602,
                  "rename would conflict with existing symbol '" +
                      candidate.name + "'");
        return;
      }
    }
    std::map<std::string, Json::Array> edits;
    std::set<std::string> seen;
    for (const auto& reference : snapshot_.occurrences) {
      if (reference.symbol != occurrence->symbol ||
          !pathInsideWorkspace(reference.location.file))
        continue;
      const std::string editUri = uriFromPath(reference.location.file);
      const std::string key = editUri + ":" +
                              std::to_string(reference.location.line) + ":" +
                              std::to_string(reference.location.column);
      if (!seen.insert(key).second) continue;
      edits[editUri].emplace_back(Json(Json::Object{
          {"newText", Json(*newName)}, {"range", occurrenceRange(reference)}}));
    }
    Json::Object changes;
    for (auto& [editUri, values] : edits)
      changes.emplace(editUri, Json(std::move(values)));
    sendResult(id, Json(Json::Object{{"changes", Json(std::move(changes))}}));
  }

  static bool isKeywordToken(TokenKind kind) {
    return kind >= TokenKind::KwFn && kind <= TokenKind::KwCallback;
  }

  std::pair<long long, long long> semanticClassification(
      const Token& token) const {
    if (isKeywordToken(token.kind)) return {11, 0};
    if (token.kind == TokenKind::String || token.kind == TokenKind::Character)
      return {12, 0};
    if (token.kind == TokenKind::Integer || token.kind == TokenKind::Float)
      return {13, 0};
    if (token.kind != TokenKind::Identifier) return {-1, 0};
    for (const auto& occurrence : snapshot_.occurrences) {
      if (occurrence.location.file != token.location.file ||
          occurrence.location.line != token.location.line ||
          occurrence.location.column != token.location.column)
        continue;
      const auto symbol = snapshot_.symbols.find(occurrence.symbol);
      if (symbol == snapshot_.symbols.end()) break;
      long long type = 7;
      if (symbol->second.kind == "function") type = 9;
      else if (symbol->second.kind == "method") type = 10;
      else if (symbol->second.kind == "parameter") type = 6;
      else if (symbol->second.kind == "field") type = 8;
      else if (symbol->second.kind == "type") type = 1;
      else if (symbol->second.kind == "trait") type = 4;
      long long modifiers = occurrence.definition ? 3 : 0;
      if (symbol->second.native) modifiers |= 1LL << 3;
      return {type, modifiers};
    }
    return {7, 0};
  }

  std::vector<long long> semanticTokenData(const std::string& uri) const {
    std::vector<long long> data;
    const auto analysis = snapshot_.analyses.find(uri);
    const auto* source = sourceForUri(uri);
    if (analysis == snapshot_.analyses.end() || source == nullptr) return data;
    long long previousLine = 0;
    long long previousCharacter = 0;
    bool first = true;
    for (const auto& token : analysis->second.tokens) {
      const auto [type, modifiers] = semanticClassification(token);
      if (type < 0 || token.text.empty()) continue;
      const long long line = std::max(0, token.location.line - 1);
      const long long character = lspCharacter(*source, token.location.line,
                                               token.location.column);
      const long long deltaLine = first ? line : line - previousLine;
      const long long deltaCharacter = first || deltaLine != 0
                                           ? character
                                           : character - previousCharacter;
      data.push_back(deltaLine);
      data.push_back(deltaCharacter);
      data.push_back(static_cast<long long>(std::max<std::size_t>(
          1, utf16Length(token.text))));
      data.push_back(type);
      data.push_back(modifiers);
      previousLine = line;
      previousCharacter = character;
      first = false;
    }
    return data;
  }

  static Json numbersJson(const std::vector<long long>& values) {
    Json::Array data;
    data.reserve(values.size());
    for (const long long value : values) data.emplace_back(Json::integer(value));
    return Json(std::move(data));
  }

  void semanticTokens(const Json& id, const Json* params, bool delta) {
    const auto* document = asObject(field(asObject(params), "textDocument"));
    const auto* uri = asString(field(document, "uri"));
    if (uri == nullptr || sourceForUri(*uri) == nullptr) {
      sendError(id, -32602, "semantic tokens require a Rocket document"); return;
    }
    std::vector<long long> data = semanticTokenData(*uri);
    const std::string resultId = std::to_string(snapshot_.generation) + ":" +
                                 std::to_string(documents_.contains(*uri)
                                                    ? documents_.at(*uri).version
                                                    : 0);
    const auto previous = semanticTokens_.find(*uri);
    if (delta && previous != semanticTokens_.end()) {
      Json::Array edits;
      if (previous->second.data != data) {
        edits.emplace_back(Json(Json::Object{
            {"data", numbersJson(data)},
            {"deleteCount", Json::integer(static_cast<long long>(
                                previous->second.data.size()))},
            {"start", Json::integer(0)}}));
      }
      semanticTokens_[*uri] = {resultId, std::move(data)};
      sendResult(id, Json(Json::Object{{"edits", Json(std::move(edits))},
                                      {"resultId", Json(resultId)}}));
      return;
    }
    semanticTokens_[*uri] = {resultId, data};
    sendResult(id, Json(Json::Object{{"data", numbersJson(data)},
                                    {"resultId", Json(resultId)}}));
  }

  static std::pair<long long, long long> documentEnd(const std::string& source) {
    long long line = 0;
    std::size_t start = 0;
    while (true) {
      const std::size_t newline = source.find('\n', start);
      if (newline == std::string::npos) break;
      ++line;
      start = newline + 1;
    }
    return {line, static_cast<long long>(utf16Length(
                      std::string_view(source).substr(start)))};
  }

  Json workspaceEdit(const std::string& uri, Json::Array edits) const {
    Json::Object changes;
    changes.emplace(uri, Json(std::move(edits)));
    return Json(Json::Object{{"changes", Json(std::move(changes))}});
  }

  void codeActions(const Json& id, const Json* params) {
    const auto* document = asObject(field(asObject(params), "textDocument"));
    const auto* uri = asString(field(document, "uri"));
    const auto* source = uri == nullptr ? nullptr : sourceForUri(*uri);
    if (uri == nullptr || source == nullptr) {
      sendError(id, -32602, "code actions require a Rocket document"); return;
    }
    Json::Array actions;
    const auto imports = documentImports(*source);
    const auto* context = asObject(field(asObject(params), "context"));
    if (const auto* diagnostics = asArray(field(context, "diagnostics"))) {
      for (const auto& diagnostic : *diagnostics) {
        const auto* diagnosticObject = asObject(&diagnostic);
        const auto* code = asString(field(diagnosticObject, "code"));
        const auto* message = asString(field(diagnosticObject, "message"));
        if (code == nullptr || *code != "R4002" || message == nullptr) continue;
        const std::size_t first = message->find('\'');
        const std::size_t last = first == std::string::npos
                                     ? std::string::npos
                                     : message->find('\'', first + 1);
        if (last == std::string::npos) continue;
        const std::string missing = message->substr(first + 1, last - first - 1);
        std::map<std::string, const SemanticSymbol*> matches;
        for (const auto& [key, symbol] : snapshot_.symbols)
          if (symbol.publicDeclaration && symbol.shortName == missing) {
            const std::size_t symbolDot = symbol.name.rfind('.');
            const std::string candidateModule = symbolDot == std::string::npos
                ? std::filesystem::path(symbol.location.file).stem().string()
                : symbol.name.substr(0, symbolDot);
            const auto existing = matches.find(candidateModule);
            if (existing == matches.end() ||
                (existing->second->name.find('.') == std::string::npos &&
                 symbolDot != std::string::npos))
              matches[candidateModule] = &symbol;
          }
        if (matches.size() != 1) continue;
        const std::string module = matches.begin()->first;
        if (imports.contains(module)) continue;
        Json::Array edits{importTextEdit(*source, module)};
        actions.emplace_back(Json(Json::Object{
            {"diagnostics", Json(Json::Array{diagnostic})},
            {"edit", workspaceEdit(*uri, std::move(edits))},
            {"isPreferred", Json(true)}, {"kind", Json("quickfix")},
            {"title", Json("Import " + module)}}));
      }
    }
    Diagnostics formatDiagnostics;
    const auto formatted = formatSource(*uri, *source, formatDiagnostics);
    if (formatted && *formatted != *source) {
      const auto [endLine, endCharacter] = documentEnd(*source);
      Json::Object start{{"line", Json::integer(0)},
                         {"character", Json::integer(0)}};
      Json::Object end{{"line", Json::integer(endLine)},
                       {"character", Json::integer(endCharacter)}};
      Json::Array edits{Json(Json::Object{
          {"newText", Json(*formatted)},
          {"range", Json(Json::Object{{"start", Json(std::move(start))},
                                      {"end", Json(std::move(end))}})}})};
      actions.emplace_back(Json(Json::Object{
          {"edit", workspaceEdit(*uri, std::move(edits))},
          {"kind", Json("source.format.rocket")},
          {"title", Json("Format Rocket document")}}));
    }
    sendResult(id, Json(std::move(actions)));
  }

  void workspaceSymbols(const Json& id, const Json* params) {
    std::string query;
    if (const auto* value = asString(field(asObject(params), "query"))) query = *value;
    std::transform(query.begin(), query.end(), query.begin(), [](unsigned char value) {
      return static_cast<char>(std::tolower(value));
    });
    Json::Array symbols;
    for (const auto& [key, symbol] : snapshot_.symbols) {
      if (!pathInsideWorkspace(symbol.location.file)) continue;
      std::string candidate = symbol.name;
      std::transform(candidate.begin(), candidate.end(), candidate.begin(),
                     [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
      if (!query.empty() && candidate.find(query) == std::string::npos) continue;
      symbols.emplace_back(Json(Json::Object{
          {"kind", Json::integer(completionKind(symbol.kind))},
          {"location", symbolLocation(symbol)}, {"name", Json(symbol.name)}}));
      if (symbols.size() >= 1024) break;
    }
    sendResult(id, Json(std::move(symbols)));
  }

  void projectStatus(const Json& id) {
    sendResult(id, Json(Json::Object{
        {"bytes", Json::integer(static_cast<long long>(snapshot_.bytesAnalyzed))},
        {"elapsedMilliseconds", Json::integer(snapshot_.elapsedMilliseconds)},
        {"files", Json::integer(static_cast<long long>(snapshot_.filesAnalyzed))},
        {"generation", Json::integer(snapshot_.generation)},
        {"maximumProjectBytes",
         Json::integer(static_cast<long long>(configuration_.maximumProjectBytes))},
        {"maximumProjectFiles",
         Json::integer(static_cast<long long>(configuration_.maximumProjectFiles))},
        {"symbols", Json::integer(static_cast<long long>(snapshot_.symbols.size()))}}));
  }

  bool applyChange(std::string& text, const Json::Object& change) {
    const auto* replacement = asString(field(&change, "text"));
    if (replacement == nullptr) return false;
    const Json* rangeValue = field(&change, "range");
    if (rangeValue == nullptr) {
      if (replacement->size() > MaximumDocumentBytes) return false;
      text = *replacement;
      return true;
    }
    const auto* range = asObject(rangeValue);
    const auto* start = asObject(field(range, "start"));
    const auto* end = asObject(field(range, "end"));
    long long startLine = 0, startCharacter = 0, endLine = 0, endCharacter = 0;
    if (!integerValue(field(start, "line"), startLine) ||
        !integerValue(field(start, "character"), startCharacter) ||
        !integerValue(field(end, "line"), endLine) ||
        !integerValue(field(end, "character"), endCharacter))
      return false;
    const auto startOffset = offsetAtPosition(text, startLine, startCharacter);
    const auto endOffset = offsetAtPosition(text, endLine, endCharacter);
    if (!startOffset || !endOffset || *startOffset > *endOffset ||
        text.size() - (*endOffset - *startOffset) + replacement->size() >
            MaximumDocumentBytes)
      return false;
    text.replace(*startOffset, *endOffset - *startOffset, *replacement);
    return true;
  }

  void didChange(const Json* params, const std::string& method) {
    const auto* paramsObject = asObject(params);
    const auto* document = asObject(field(paramsObject, "textDocument"));
    const auto* uri = asString(field(document, "uri"));
    long long version = 0;
    const auto* changes = asArray(field(paramsObject, "contentChanges"));
    if (uri == nullptr || !integerValue(field(document, "version"), version) ||
        changes == nullptr || changes->empty() ||
        changes->size() > MaximumContentChanges) {
      invalidNotification(method);
      return;
    }
    const auto found = documents_.find(*uri);
    if (found == documents_.end() || version <= found->second.version) return;
    std::string updated = found->second.text;
    for (const auto& changeValue : *changes) {
      const auto* change = asObject(&changeValue);
      if (change == nullptr || !applyChange(updated, *change)) {
        invalidNotification(method);
        return;
      }
    }
    found->second.text = std::move(updated);
    found->second.version = version;
    rebuildSnapshot();
  }

  void didSave(const Json* params, const std::string& method) {
    const auto* paramsObject = asObject(params);
    const auto* document = asObject(field(paramsObject, "textDocument"));
    const auto* uri = asString(field(document, "uri"));
    if (uri == nullptr) { invalidNotification(method); return; }
    const auto found = documents_.find(*uri);
    if (found == documents_.end()) return;
    if (const auto* text = asString(field(paramsObject, "text")); text != nullptr) {
      if (text->size() > MaximumDocumentBytes) { invalidNotification(method); return; }
      found->second.text = *text;
    }
    rebuildSnapshot();
  }

  void didClose(const Json* params, const std::string& method) {
    const auto* document = asObject(field(asObject(params), "textDocument"));
    const auto* uri = asString(field(document, "uri"));
    if (uri == nullptr) { invalidNotification(method); return; }
    long long version = 0;
    if (const auto found = documents_.find(*uri); found != documents_.end()) {
      version = found->second.version;
      documents_.erase(found);
    }
    sendDiagnostics(*uri, version, {});
    rebuildSnapshot();
  }

  struct SemanticTokenCache {
    std::string resultId;
    std::vector<long long> data;
  };

  std::istream& input_;
  std::ostream& output_;
  std::ostream& log_;
  std::map<std::string, Document> documents_;
  std::filesystem::path workspaceRoot_;
  std::filesystem::path packageRoot_;
  std::filesystem::path packageEntry_;
  std::vector<PackageDependencyRoot> dependencyRoots_;
  Configuration configuration_;
  SemanticSnapshot snapshot_;
  std::map<std::string, SemanticTokenCache> semanticTokens_;
  std::unordered_set<std::string> canceledRequests_;
  long long analysisGeneration_ = 0;
  bool initialized_ = false;
  bool shutdownRequested_ = false;
  bool exitReceived_ = false;
  bool cleanExit_ = false;
};

} // namespace

int LanguageServer::run() {
  return ServerSession(input_, output_, log_).run();
}

} // namespace rocket
