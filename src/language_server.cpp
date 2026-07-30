#include "language_server.h"

#include "diagnostic.h"
#include "lexer.h"
#include "parser.h"
#include "sema.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace rocket {
namespace {

constexpr std::size_t MaximumMessageBytes = 16U * 1024U * 1024U;
constexpr std::size_t MaximumHeaderBytes = 16U * 1024U;

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

enum class FrameStatus { Message, End, Error };

FrameStatus readFrame(std::istream& input, std::string& body,
                      std::string& error) {
  std::optional<std::size_t> contentLength;
  std::size_t headerBytes = 0;
  std::string line;
  bool sawHeader = false;
  while (std::getline(input, line)) {
    sawHeader = true;
    headerBytes += line.size() + 1;
    if (headerBytes > MaximumHeaderBytes) {
      error = "LSP header exceeds 16 KiB";
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
  if (!sawHeader && input.eof()) return FrameStatus::End;
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
      if (request) sendError(id, -32600, "invalid JSON-RPC request");
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
                              {"version", Json("0.1.0")}};
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

} // namespace

int LanguageServer::run() {
  return ServerSession(input_, output_, log_).run();
}

} // namespace rocket
