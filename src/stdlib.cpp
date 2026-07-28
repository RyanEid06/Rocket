#include "runtime.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

std::string stringValue(const RocketString* value) {
  if (!value) return {};
  const auto* bytes = rocket_rt_string_bytes(value);
  return std::string(reinterpret_cast<const char*>(bytes),
                     static_cast<std::size_t>(rocket_rt_string_byte_length(value)));
}

RocketString* makeString(std::string_view value) {
  return rocket_rt_string_new(reinterpret_cast<const std::uint8_t*>(value.data()),
                              static_cast<std::uint64_t>(value.size()));
}

RocketAggregate* managedVariant(std::uint32_t tag, void* value) {
  RocketAggregate* result = rocket_rt_aggregate_new(tag, 1, 1);
  rocket_rt_aggregate_set_managed(result, 0, value);
  return result;
}

RocketAggregate* intVariant(std::uint32_t tag, std::int64_t value) {
  RocketAggregate* result = rocket_rt_aggregate_new(tag, 1, 0);
  rocket_rt_aggregate_set_int(result, 0, value);
  return result;
}

RocketAggregate* boolVariant(std::uint32_t tag, bool value) {
  RocketAggregate* result = rocket_rt_aggregate_new(tag, 1, 0);
  rocket_rt_aggregate_set_bool(result, 0, value ? 1 : 0);
  return result;
}

RocketAggregate* errorResult(const std::string& message) {
  RocketString* error = makeString(message);
  RocketAggregate* result = managedVariant(1, error);
  rocket_rt_release(error);
  return result;
}

RocketAggregate* okManaged(void* value) { return managedVariant(0, value); }
RocketAggregate* okInt(std::int64_t value) { return intVariant(0, value); }
RocketAggregate* okBool(bool value) { return boolVariant(0, value); }

std::filesystem::path pathValue(const RocketString* value) {
  const std::string utf8 = stringValue(value);
  const std::u8string native(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size());
  return std::filesystem::path(native);
}

std::string pathString(const std::filesystem::path& value) {
  const auto utf8 = value.generic_u8string();
  return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

RocketArray* stringArray(const std::vector<std::string>& values) {
  RocketArray* result = rocket_rt_array_new(ROCKET_ELEMENT_STRING, values.size());
  for (std::size_t index = 0; index < values.size(); ++index) {
    RocketString* value = makeString(values[index]);
    rocket_rt_array_set_string(result, static_cast<std::int64_t>(index), value);
    rocket_rt_release(value);
  }
  return result;
}

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

  RocketAggregate* parse() {
    skipWhitespace();
    RocketAggregate* value = parseValue(0);
    skipWhitespace();
    if (value && index_ != input_.size()) {
      rocket_rt_release(value);
      fail("unexpected characters after JSON value");
      return nullptr;
    }
    return value;
  }

  const std::string& error() const { return error_; }

private:
  void skipWhitespace() {
    while (index_ < input_.size() &&
           (input_[index_] == ' ' || input_[index_] == '\t' ||
            input_[index_] == '\r' || input_[index_] == '\n'))
      ++index_;
  }

  void fail(const std::string& message) {
    if (error_.empty()) error_ = message + " at byte " + std::to_string(index_);
  }

  bool consume(std::string_view text) {
    if (input_.substr(index_, text.size()) != text) return false;
    index_ += text.size();
    return true;
  }

  std::uint32_t hex4(bool& valid) {
    std::uint32_t value = 0;
    for (int digit = 0; digit < 4; ++digit) {
      if (index_ >= input_.size()) { valid = false; return 0; }
      const char character = input_[index_++];
      value <<= 4;
      if (character >= '0' && character <= '9') value |= character - '0';
      else if (character >= 'a' && character <= 'f') value |= character - 'a' + 10;
      else if (character >= 'A' && character <= 'F') value |= character - 'A' + 10;
      else { valid = false; return 0; }
    }
    return value;
  }

  bool parseString(std::string& output) {
    if (index_ >= input_.size() || input_[index_] != '"') return false;
    ++index_;
    while (index_ < input_.size()) {
      const unsigned char character = static_cast<unsigned char>(input_[index_++]);
      if (character == '"') return true;
      if (character < 0x20) { fail("control character in JSON string"); return false; }
      if (character != '\\') { output.push_back(static_cast<char>(character)); continue; }
      if (index_ >= input_.size()) { fail("unterminated JSON escape"); return false; }
      const char escaped = input_[index_++];
      switch (escaped) {
      case '"': output.push_back('"'); break;
      case '\\': output.push_back('\\'); break;
      case '/': output.push_back('/'); break;
      case 'b': output.push_back('\b'); break;
      case 'f': output.push_back('\f'); break;
      case 'n': output.push_back('\n'); break;
      case 'r': output.push_back('\r'); break;
      case 't': output.push_back('\t'); break;
      case 'u': {
        bool valid = true;
        std::uint32_t codepoint = hex4(valid);
        if (!valid) { fail("invalid JSON Unicode escape"); return false; }
        if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
          if (!consume("\\u")) { fail("missing low Unicode surrogate"); return false; }
          const std::uint32_t low = hex4(valid);
          if (!valid || low < 0xdc00 || low > 0xdfff) {
            fail("invalid low Unicode surrogate"); return false;
          }
          codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
        } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
          fail("unexpected low Unicode surrogate"); return false;
        }
        appendUtf8(output, codepoint);
        break;
      }
      default: fail("unknown JSON escape"); return false;
      }
    }
    fail("unterminated JSON string");
    return false;
  }

  RocketAggregate* makeJson(std::uint32_t tag) {
    return rocket_rt_aggregate_new(tag, 0, 0);
  }

  RocketAggregate* makeJsonManaged(std::uint32_t tag, void* payload) {
    return managedVariant(tag, payload);
  }

  RocketAggregate* parseArray(std::size_t depth) {
    ++index_;
    skipWhitespace();
    std::vector<RocketAggregate*> values;
    if (index_ < input_.size() && input_[index_] == ']') {
      ++index_;
    } else {
      while (true) {
        skipWhitespace();
        RocketAggregate* value = parseValue(depth + 1);
        if (!value) {
          for (auto* item : values) rocket_rt_release(item);
          return nullptr;
        }
        values.push_back(value);
        skipWhitespace();
        if (index_ < input_.size() && input_[index_] == ',') { ++index_; continue; }
        if (index_ < input_.size() && input_[index_] == ']') { ++index_; break; }
        fail("expected ',' or ']' in JSON array");
        for (auto* item : values) rocket_rt_release(item);
        return nullptr;
      }
    }
    RocketArray* array = rocket_rt_array_new(ROCKET_ELEMENT_MANAGED, values.size());
    for (std::size_t index = 0; index < values.size(); ++index) {
      rocket_rt_array_set_managed(array, static_cast<std::int64_t>(index), values[index]);
      rocket_rt_release(values[index]);
    }
    RocketAggregate* result = makeJsonManaged(5, array);
    rocket_rt_release(array);
    return result;
  }

  RocketAggregate* parseObject(std::size_t depth) {
    ++index_;
    skipWhitespace();
    std::vector<RocketAggregate*> fields;
    if (index_ < input_.size() && input_[index_] == '}') {
      ++index_;
    } else {
      while (true) {
        skipWhitespace();
        std::string key;
        if (!parseString(key)) {
          if (error_.empty()) fail("expected string key in JSON object");
          for (auto* field : fields) rocket_rt_release(field);
          return nullptr;
        }
        skipWhitespace();
        if (index_ >= input_.size() || input_[index_] != ':') {
          fail("expected ':' after JSON object key");
          for (auto* field : fields) rocket_rt_release(field);
          return nullptr;
        }
        ++index_;
        skipWhitespace();
        RocketAggregate* value = parseValue(depth + 1);
        if (!value) {
          for (auto* field : fields) rocket_rt_release(field);
          return nullptr;
        }
        RocketString* keyValue = makeString(key);
        RocketAggregate* field = rocket_rt_aggregate_new(0, 2, 3);
        rocket_rt_aggregate_set_managed(field, 0, keyValue);
        rocket_rt_aggregate_set_managed(field, 1, value);
        rocket_rt_release(keyValue);
        rocket_rt_release(value);
        fields.push_back(field);
        skipWhitespace();
        if (index_ < input_.size() && input_[index_] == ',') { ++index_; continue; }
        if (index_ < input_.size() && input_[index_] == '}') { ++index_; break; }
        fail("expected ',' or '}' in JSON object");
        for (auto* item : fields) rocket_rt_release(item);
        return nullptr;
      }
    }
    RocketArray* array = rocket_rt_array_new(ROCKET_ELEMENT_MANAGED, fields.size());
    for (std::size_t index = 0; index < fields.size(); ++index) {
      rocket_rt_array_set_managed(array, static_cast<std::int64_t>(index), fields[index]);
      rocket_rt_release(fields[index]);
    }
    RocketAggregate* result = makeJsonManaged(6, array);
    rocket_rt_release(array);
    return result;
  }

  RocketAggregate* parseNumber() {
    const std::size_t start = index_;
    if (index_ < input_.size() && input_[index_] == '-') ++index_;
    if (index_ >= input_.size()) { fail("incomplete JSON number"); return nullptr; }
    if (input_[index_] == '0') {
      ++index_;
    } else if (input_[index_] >= '1' && input_[index_] <= '9') {
      while (index_ < input_.size() && input_[index_] >= '0' && input_[index_] <= '9') ++index_;
    } else {
      fail("invalid JSON number"); return nullptr;
    }
    bool decimal = false;
    if (index_ < input_.size() && input_[index_] == '.') {
      decimal = true;
      ++index_;
      const std::size_t digits = index_;
      while (index_ < input_.size() && input_[index_] >= '0' && input_[index_] <= '9') ++index_;
      if (digits == index_) { fail("JSON fraction requires digits"); return nullptr; }
    }
    if (index_ < input_.size() && (input_[index_] == 'e' || input_[index_] == 'E')) {
      decimal = true;
      ++index_;
      if (index_ < input_.size() && (input_[index_] == '+' || input_[index_] == '-')) ++index_;
      const std::size_t digits = index_;
      while (index_ < input_.size() && input_[index_] >= '0' && input_[index_] <= '9') ++index_;
      if (digits == index_) { fail("JSON exponent requires digits"); return nullptr; }
    }
    const std::string spelling(input_.substr(start, index_ - start));
    if (!decimal) {
      std::int64_t integer = 0;
      const auto parsed = std::from_chars(spelling.data(), spelling.data() + spelling.size(), integer);
      if (parsed.ec == std::errc{} && parsed.ptr == spelling.data() + spelling.size()) {
        RocketAggregate* value = rocket_rt_aggregate_new(2, 1, 0);
        rocket_rt_aggregate_set_int(value, 0, integer);
        return value;
      }
    }
    char* end = nullptr;
    const double number = std::strtod(spelling.c_str(), &end);
    if (!end || *end != '\0' || !std::isfinite(number)) {
      fail("JSON number is outside the supported range"); return nullptr;
    }
    RocketAggregate* value = rocket_rt_aggregate_new(3, 1, 0);
    rocket_rt_aggregate_set_float(value, 0, number);
    return value;
  }

  RocketAggregate* parseValue(std::size_t depth) {
    if (depth > 256) { fail("JSON nesting exceeds 256 levels"); return nullptr; }
    skipWhitespace();
    if (consume("null")) return makeJson(0);
    if (consume("true")) {
      RocketAggregate* value = rocket_rt_aggregate_new(1, 1, 0);
      rocket_rt_aggregate_set_bool(value, 0, 1);
      return value;
    }
    if (consume("false")) {
      RocketAggregate* value = rocket_rt_aggregate_new(1, 1, 0);
      rocket_rt_aggregate_set_bool(value, 0, 0);
      return value;
    }
    if (index_ < input_.size() && input_[index_] == '"') {
      std::string text;
      if (!parseString(text)) return nullptr;
      RocketString* string = makeString(text);
      RocketAggregate* value = makeJsonManaged(4, string);
      rocket_rt_release(string);
      return value;
    }
    if (index_ < input_.size() && input_[index_] == '[') return parseArray(depth);
    if (index_ < input_.size() && input_[index_] == '{') return parseObject(depth);
    if (index_ < input_.size() &&
        (input_[index_] == '-' || (input_[index_] >= '0' && input_[index_] <= '9')))
      return parseNumber();
    fail("expected JSON value");
    return nullptr;
  }

  std::string_view input_;
  std::size_t index_ = 0;
  std::string error_;
};

void appendJsonString(std::string& output, const std::string& value) {
  output.push_back('"');
  static constexpr char hex[] = "0123456789abcdef";
  for (const unsigned char character : value) {
    switch (character) {
    case '"': output += "\\\""; break;
    case '\\': output += "\\\\"; break;
    case '\b': output += "\\b"; break;
    case '\f': output += "\\f"; break;
    case '\n': output += "\\n"; break;
    case '\r': output += "\\r"; break;
    case '\t': output += "\\t"; break;
    default:
      if (character < 0x20) {
        output += "\\u00";
        output.push_back(hex[character >> 4]);
        output.push_back(hex[character & 15]);
      } else {
        output.push_back(static_cast<char>(character));
      }
    }
  }
  output.push_back('"');
}

bool stringifyJson(RocketAggregate* value, std::string& output, std::size_t depth) {
  if (!value || depth > 256) return false;
  switch (rocket_rt_aggregate_tag(value)) {
  case 0: output += "null"; return true;
  case 1: output += rocket_rt_aggregate_get_bool(value, 0) ? "true" : "false"; return true;
  case 2: output += std::to_string(rocket_rt_aggregate_get_int(value, 0)); return true;
  case 3: {
    std::ostringstream stream;
    stream << std::setprecision(17) << rocket_rt_aggregate_get_float(value, 0);
    output += stream.str();
    return true;
  }
  case 4: {
    auto* string = static_cast<RocketString*>(rocket_rt_aggregate_get_managed(value, 0));
    appendJsonString(output, stringValue(string));
    rocket_rt_release(string);
    return true;
  }
  case 5: {
    auto* array = static_cast<RocketArray*>(rocket_rt_aggregate_get_managed(value, 0));
    output.push_back('[');
    const std::uint64_t length = rocket_rt_collection_length(array);
    for (std::uint64_t index = 0; index < length; ++index) {
      if (index) output.push_back(',');
      auto* item = static_cast<RocketAggregate*>(
          rocket_rt_index_managed(array, static_cast<std::int64_t>(index)));
      const bool valid = stringifyJson(item, output, depth + 1);
      rocket_rt_release(item);
      if (!valid) { rocket_rt_release(array); return false; }
    }
    output.push_back(']');
    rocket_rt_release(array);
    return true;
  }
  case 6: {
    auto* array = static_cast<RocketArray*>(rocket_rt_aggregate_get_managed(value, 0));
    output.push_back('{');
    const std::uint64_t length = rocket_rt_collection_length(array);
    for (std::uint64_t index = 0; index < length; ++index) {
      if (index) output.push_back(',');
      auto* field = static_cast<RocketAggregate*>(
          rocket_rt_index_managed(array, static_cast<std::int64_t>(index)));
      auto* key = static_cast<RocketString*>(rocket_rt_aggregate_get_managed(field, 0));
      auto* item = static_cast<RocketAggregate*>(rocket_rt_aggregate_get_managed(field, 1));
      appendJsonString(output, stringValue(key));
      output.push_back(':');
      const bool valid = stringifyJson(item, output, depth + 1);
      rocket_rt_release(key);
      rocket_rt_release(item);
      rocket_rt_release(field);
      if (!valid) { rocket_rt_release(array); return false; }
    }
    output.push_back('}');
    rocket_rt_release(array);
    return true;
  }
  default: return false;
  }
}

std::uint64_t randomState = 0x4d595df4d0f33173ULL;
std::vector<std::string> processArguments;

std::uint64_t nextRandom() {
  std::uint64_t value = randomState;
  value ^= value >> 12;
  value ^= value << 25;
  value ^= value >> 27;
  randomState = value;
  return value * 2685821657736338717ULL;
}

#ifdef _WIN32
std::wstring utf8ToWide(const std::string& value) {
  if (value.empty()) return {};
  const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0);
  if (length <= 0) return {};
  std::wstring result(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                      static_cast<int>(value.size()), result.data(), length);
  return result;
}

std::string wideToUtf8(const std::wstring& value) {
  if (value.empty()) return {};
  const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0,
                                         nullptr, nullptr);
  if (length <= 0) return {};
  std::string result(static_cast<std::size_t>(length), '\0');
  WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                      static_cast<int>(value.size()), result.data(), length,
                      nullptr, nullptr);
  return result;
}

std::wstring quoteWindowsArgument(const std::wstring& argument) {
  if (argument.empty()) return L"\"\"";
  if (argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) return argument;
  std::wstring result = L"\"";
  std::size_t backslashes = 0;
  for (wchar_t character : argument) {
    if (character == L'\\') {
      ++backslashes;
    } else if (character == L'\"') {
      result.append(backslashes * 2 + 1, L'\\');
      result.push_back(L'\"');
      backslashes = 0;
    } else {
      result.append(backslashes, L'\\');
      backslashes = 0;
      result.push_back(character);
    }
  }
  result.append(backslashes * 2, L'\\');
  result.push_back(L'\"');
  return result;
}
#endif

} // namespace

extern "C" {

std::int64_t rocket_std_string_byte_length(RocketString* value) {
  return static_cast<std::int64_t>(rocket_rt_string_byte_length(value));
}

RocketString* rocket_std_string_concat(RocketString* left, RocketString* right) {
  return makeString(stringValue(left) + stringValue(right));
}

std::uint8_t rocket_std_string_contains(RocketString* value, RocketString* needle) {
  return stringValue(value).find(stringValue(needle)) != std::string::npos;
}

std::uint8_t rocket_std_string_starts_with(RocketString* value, RocketString* prefix) {
  return stringValue(value).starts_with(stringValue(prefix));
}

std::uint8_t rocket_std_string_ends_with(RocketString* value, RocketString* suffix) {
  return stringValue(value).ends_with(stringValue(suffix));
}

RocketString* rocket_std_string_trim(RocketString* value) {
  const std::string input = stringValue(value);
  const std::size_t first = input.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return makeString({});
  return makeString(std::string_view(input).substr(first, input.find_last_not_of(" \t\r\n") - first + 1));
}

RocketArray* rocket_std_string_split(RocketString* value, RocketString* delimiter) {
  const std::string input = stringValue(value);
  const std::string separator = stringValue(delimiter);
  std::vector<std::string> pieces;
  if (separator.empty()) {
    pieces.push_back(input);
  } else {
    std::size_t start = 0;
    while (true) {
      const std::size_t next = input.find(separator, start);
      pieces.push_back(input.substr(start, next == std::string::npos
                                              ? std::string::npos
                                              : next - start));
      if (next == std::string::npos) break;
      start = next + separator.size();
    }
  }
  return stringArray(pieces);
}

std::uint8_t rocket_std_string_byte_at(RocketString* value, std::int64_t index) {
  const std::uint64_t length = rocket_rt_string_byte_length(value);
  if (index < 0 || static_cast<std::uint64_t>(index) >= length) {
    std::fprintf(stderr, "rocket runtime error: String byte index %lld out of bounds for length %llu\n",
                 static_cast<long long>(index), static_cast<unsigned long long>(length));
    std::exit(101);
  }
  return rocket_rt_string_bytes(value)[static_cast<std::uint64_t>(index)];
}

std::int64_t rocket_std_string_byte_value_at(RocketString* value, std::int64_t index) {
  return static_cast<std::int64_t>(rocket_std_string_byte_at(value, index));
}

RocketString* rocket_std_string_slice(RocketString* value, std::int64_t start,
                                      std::int64_t end) {
  const std::uint64_t length = rocket_rt_string_byte_length(value);
  if (start < 0 || end < start || static_cast<std::uint64_t>(end) > length) {
    std::fprintf(stderr,
                 "rocket runtime error: String byte slice %lld..%lld out of bounds for length %llu\n",
                 static_cast<long long>(start), static_cast<long long>(end),
                 static_cast<unsigned long long>(length));
    std::exit(101);
  }
  const auto* bytes = rocket_rt_string_bytes(value) + static_cast<std::uint64_t>(start);
  return rocket_rt_string_new(bytes, static_cast<std::uint64_t>(end - start));
}

RocketAggregate* rocket_std_string_parse_int(RocketString* value) {
  const std::string input = stringValue(value);
  std::int64_t parsed = 0;
  const auto result = std::from_chars(input.data(), input.data() + input.size(), parsed);
  if (input.empty() || result.ec != std::errc{} || result.ptr != input.data() + input.size())
    return errorResult("invalid Int text");
  return okInt(parsed);
}

RocketString* rocket_std_string_from_int(std::int64_t value) {
  return makeString(std::to_string(value));
}

std::int64_t rocket_std_collections_length(void* collection) {
  return static_cast<std::int64_t>(rocket_rt_collection_length(collection));
}

RocketArray* rocket_std_collections_reverse(void* collection) {
  const std::uint64_t length = rocket_rt_collection_length(collection);
  const std::uint32_t kind = rocket_rt_collection_element_kind(collection);
  RocketArray* result = rocket_rt_array_new(kind, length);
  for (std::uint64_t index = 0; index < length; ++index) {
    const std::int64_t source = static_cast<std::int64_t>(length - index - 1);
    const std::int64_t destination = static_cast<std::int64_t>(index);
    switch (kind) {
    case ROCKET_ELEMENT_INT:
      rocket_rt_array_set_int(result, destination, rocket_rt_index_int(collection, source)); break;
    case ROCKET_ELEMENT_FLOAT:
      rocket_rt_array_set_float(result, destination, rocket_rt_index_float(collection, source)); break;
    case ROCKET_ELEMENT_BOOL:
      rocket_rt_array_set_bool(result, destination, rocket_rt_index_bool(collection, source)); break;
    case ROCKET_ELEMENT_CHAR:
      rocket_rt_array_set_char(result, destination, rocket_rt_index_char(collection, source)); break;
    case ROCKET_ELEMENT_STRING: {
      RocketString* item = rocket_rt_index_string(collection, source);
      rocket_rt_array_set_string(result, destination, item);
      rocket_rt_release(item);
      break;
    }
    case ROCKET_ELEMENT_MANAGED: {
      void* item = rocket_rt_index_managed(collection, source);
      rocket_rt_array_set_managed(result, destination, item);
      rocket_rt_release(item);
      break;
    }
    default: break;
    }
  }
  return result;
}

RocketArray* rocket_std_collections_concat(void* left, void* right) {
  const std::uint32_t kind = rocket_rt_collection_element_kind(left);
  if (kind != rocket_rt_collection_element_kind(right)) {
    std::fputs("rocket runtime error: collection concatenation element kind mismatch\n", stderr);
    std::exit(101);
  }
  const std::uint64_t leftLength = rocket_rt_collection_length(left);
  const std::uint64_t rightLength = rocket_rt_collection_length(right);
  if (rightLength > (std::numeric_limits<std::uint64_t>::max)() - leftLength) {
    std::fputs("rocket runtime error: collection concatenation is too large\n", stderr);
    std::exit(101);
  }
  RocketArray* result = rocket_rt_array_new(kind, leftLength + rightLength);
  auto copy = [&](void* source, std::uint64_t sourceLength, std::uint64_t offset) {
    for (std::uint64_t index = 0; index < sourceLength; ++index) {
      const auto sourceIndex = static_cast<std::int64_t>(index);
      const auto resultIndex = static_cast<std::int64_t>(offset + index);
      switch (kind) {
      case ROCKET_ELEMENT_INT:
        rocket_rt_array_set_int(result, resultIndex, rocket_rt_index_int(source, sourceIndex));
        break;
      case ROCKET_ELEMENT_FLOAT:
        rocket_rt_array_set_float(result, resultIndex, rocket_rt_index_float(source, sourceIndex));
        break;
      case ROCKET_ELEMENT_BOOL:
        rocket_rt_array_set_bool(result, resultIndex, rocket_rt_index_bool(source, sourceIndex));
        break;
      case ROCKET_ELEMENT_CHAR:
        rocket_rt_array_set_char(result, resultIndex, rocket_rt_index_char(source, sourceIndex));
        break;
      case ROCKET_ELEMENT_STRING: {
        RocketString* item = rocket_rt_index_string(source, sourceIndex);
        rocket_rt_array_set_string(result, resultIndex, item);
        rocket_rt_release(item);
        break;
      }
      case ROCKET_ELEMENT_MANAGED: {
        void* item = rocket_rt_index_managed(source, sourceIndex);
        rocket_rt_array_set_managed(result, resultIndex, item);
        rocket_rt_release(item);
        break;
      }
      default:
        std::fputs("rocket runtime error: invalid collection concatenation element kind\n", stderr);
        std::exit(101);
      }
    }
  };
  copy(left, leftLength, 0);
  copy(right, rightLength, leftLength);
  return result;
}

RocketString* rocket_std_collections_join(RocketArray* values, RocketString* separator) {
  const std::uint64_t length = rocket_rt_collection_length(values);
  const std::string between = stringValue(separator);
  std::string output;
  for (std::uint64_t index = 0; index < length; ++index) {
    if (index) output += between;
    RocketString* value = rocket_rt_index_string(values, static_cast<std::int64_t>(index));
    output += stringValue(value);
    rocket_rt_release(value);
  }
  return makeString(output);
}

RocketAggregate* rocket_std_file_read_text(RocketString* path) {
  try {
    std::ifstream input(pathValue(path), std::ios::binary);
    if (!input) return errorResult("could not open file for reading");
    std::ostringstream contents;
    contents << input.rdbuf();
    const std::string text = contents.str();
    RocketString* value = makeString(text);
    RocketAggregate* result = okManaged(value);
    rocket_rt_release(value);
    return result;
  } catch (const std::exception& error) { return errorResult(error.what()); }
}

RocketAggregate* writeFile(RocketString* path, RocketString* contents,
                           std::ios::openmode mode) {
  try {
    std::ofstream output(pathValue(path), std::ios::binary | mode);
    if (!output) return errorResult("could not open file for writing");
    const std::string text = stringValue(contents);
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) return errorResult("could not write file contents");
    return okBool(true);
  } catch (const std::exception& error) { return errorResult(error.what()); }
}

RocketAggregate* rocket_std_file_write_text(RocketString* path, RocketString* contents) {
  return writeFile(path, contents, std::ios::trunc);
}

RocketAggregate* rocket_std_file_append_text(RocketString* path, RocketString* contents) {
  return writeFile(path, contents, std::ios::app);
}

std::uint8_t rocket_std_file_exists(RocketString* path) {
  std::error_code error;
  return std::filesystem::exists(pathValue(path), error) && !error;
}

RocketAggregate* rocket_std_file_remove(RocketString* path) {
  try { return okBool(std::filesystem::remove(pathValue(path))); }
  catch (const std::exception& error) { return errorResult(error.what()); }
}

RocketAggregate* rocket_std_file_list(RocketString* path) {
  try {
    std::vector<std::string> entries;
    for (const auto& entry : std::filesystem::directory_iterator(pathValue(path)))
      entries.push_back(pathString(entry.path().filename()));
    std::sort(entries.begin(), entries.end());
    RocketArray* values = stringArray(entries);
    RocketAggregate* result = okManaged(values);
    rocket_rt_release(values);
    return result;
  } catch (const std::exception& error) { return errorResult(error.what()); }
}

RocketAggregate* rocket_std_file_create_directory(RocketString* path) {
  try {
    std::error_code error;
    const bool created = std::filesystem::create_directories(pathValue(path), error);
    if (error) return errorResult(error.message());
    return okBool(created);
  } catch (const std::exception& error) { return errorResult(error.what()); }
}

RocketString* rocket_std_path_join(RocketString* left, RocketString* right) {
  return makeString(pathString((pathValue(left) / pathValue(right)).lexically_normal()));
}
RocketString* rocket_std_path_basename(RocketString* path) {
  return makeString(pathString(pathValue(path).filename()));
}
RocketString* rocket_std_path_extension(RocketString* path) {
  return makeString(pathString(pathValue(path).extension()));
}
RocketString* rocket_std_path_normalize(RocketString* path) {
  return makeString(pathString(pathValue(path).lexically_normal()));
}

RocketAggregate* rocket_std_json_parse(RocketString* text) {
  const std::string input = stringValue(text);
  JsonParser parser(input);
  RocketAggregate* value = parser.parse();
  if (!value) return errorResult(parser.error());
  RocketAggregate* result = okManaged(value);
  rocket_rt_release(value);
  return result;
}

RocketString* rocket_std_json_stringify(RocketAggregate* value) {
  std::string output;
  if (!stringifyJson(value, output, 0)) return makeString("null");
  return makeString(output);
}

RocketAggregate* rocket_std_csv_parse(RocketString* text) {
  const std::string input = stringValue(text);
  std::vector<std::vector<std::string>> rows;
  std::size_t index = 0;
  if (!input.empty()) {
    while (index <= input.size()) {
      std::vector<std::string> row;
      while (true) {
        std::string field;
        if (index < input.size() && input[index] == '"') {
          ++index;
          bool closed = false;
          while (index < input.size()) {
            if (input[index] == '"') {
              if (index + 1 < input.size() && input[index + 1] == '"') {
                field.push_back('"'); index += 2; continue;
              }
              ++index; closed = true; break;
            }
            field.push_back(input[index++]);
          }
          if (!closed) return errorResult("unterminated quoted CSV field");
          if (index < input.size() && input[index] != ',' && input[index] != '\r' &&
              input[index] != '\n')
            return errorResult("unexpected character after quoted CSV field");
        } else {
          while (index < input.size() && input[index] != ',' && input[index] != '\r' &&
                 input[index] != '\n') {
            if (input[index] == '"') return errorResult("quote inside unquoted CSV field");
            field.push_back(input[index++]);
          }
        }
        row.push_back(std::move(field));
        if (index < input.size() && input[index] == ',') { ++index; continue; }
        break;
      }
      rows.push_back(std::move(row));
      if (index >= input.size()) break;
      if (input[index] == '\r') {
        ++index;
        if (index < input.size() && input[index] == '\n') ++index;
      } else if (input[index] == '\n') {
        ++index;
      }
      if (index == input.size()) break;
    }
  }
  RocketArray* outer = rocket_rt_array_new(ROCKET_ELEMENT_MANAGED, rows.size());
  for (std::size_t row = 0; row < rows.size(); ++row) {
    RocketArray* fields = stringArray(rows[row]);
    rocket_rt_array_set_managed(outer, static_cast<std::int64_t>(row), fields);
    rocket_rt_release(fields);
  }
  RocketAggregate* result = okManaged(outer);
  rocket_rt_release(outer);
  return result;
}

RocketString* rocket_std_csv_encode(RocketArray* rows) {
  std::string output;
  const std::uint64_t rowCount = rocket_rt_collection_length(rows);
  for (std::uint64_t row = 0; row < rowCount; ++row) {
    if (row) output += "\r\n";
    auto* fields = static_cast<RocketArray*>(
        rocket_rt_index_managed(rows, static_cast<std::int64_t>(row)));
    const std::uint64_t fieldCount = rocket_rt_collection_length(fields);
    for (std::uint64_t field = 0; field < fieldCount; ++field) {
      if (field) output.push_back(',');
      RocketString* value = rocket_rt_index_string(fields, static_cast<std::int64_t>(field));
      const std::string text = stringValue(value);
      rocket_rt_release(value);
      const bool quote = text.find_first_of(",\"\r\n") != std::string::npos;
      if (quote) output.push_back('"');
      for (char character : text) {
        if (character == '"') output += "\"\"";
        else output.push_back(character);
      }
      if (quote) output.push_back('"');
    }
    rocket_rt_release(fields);
  }
  return makeString(output);
}

void rocket_std_random_seed(std::int64_t seed) {
  randomState = static_cast<std::uint64_t>(seed);
  if (randomState == 0) randomState = 0x4d595df4d0f33173ULL;
}

std::int64_t rocket_std_random_int(std::int64_t minimum, std::int64_t maximum) {
  if (minimum >= maximum) {
    std::fputs("rocket runtime error: random.int requires minimum < maximum\n", stderr);
    std::exit(101);
  }
  const std::uint64_t range = static_cast<std::uint64_t>(maximum) -
                              static_cast<std::uint64_t>(minimum);
  const std::uint64_t threshold = (~range + 1) % range;
  std::uint64_t value = 0;
  do { value = nextRandom(); } while (value < threshold);
  return static_cast<std::int64_t>(static_cast<std::uint64_t>(minimum) + value % range);
}

double rocket_std_random_float() {
  return static_cast<double>(nextRandom() >> 11) * (1.0 / 9007199254740992.0);
}

RocketAggregate* rocket_std_process_run(RocketString* program, RocketArray* arguments) {
#ifdef _WIN32
  const std::wstring executable = utf8ToWide(stringValue(program));
  if (executable.empty()) return errorResult("process program is empty or invalid UTF-8");
  std::wstring commandLine = quoteWindowsArgument(executable);
  const std::uint64_t count = rocket_rt_collection_length(arguments);
  for (std::uint64_t index = 0; index < count; ++index) {
    RocketString* argument = rocket_rt_index_string(arguments, static_cast<std::int64_t>(index));
    commandLine += L" " + quoteWindowsArgument(utf8ToWide(stringValue(argument)));
    rocket_rt_release(argument);
  }
  std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
  mutableCommandLine.push_back(L'\0');
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(nullptr, mutableCommandLine.data(), nullptr, nullptr,
                      FALSE, 0, nullptr, nullptr, &startup, &process))
    return errorResult("could not start process (Windows error " +
                       std::to_string(GetLastError()) + ")");
  WaitForSingleObject(process.hProcess, INFINITE);
  DWORD exitCode = 1;
  GetExitCodeProcess(process.hProcess, &exitCode);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return okInt(static_cast<std::int64_t>(exitCode));
#else
  (void)program; (void)arguments;
  return errorResult("process.run is only implemented on Windows x64");
#endif
}

void rocket_std_process_set_arguments(std::int32_t count, const char* const* arguments) {
  processArguments.clear();
  if (!arguments) return;
  for (std::int32_t index = 1; index < count; ++index)
    processArguments.emplace_back(arguments[index] ? arguments[index] : "");
}

RocketArray* rocket_std_process_arguments() { return stringArray(processArguments); }

RocketAggregate* rocket_std_process_environment(RocketString* name) {
#ifdef _WIN32
  const std::wstring variable = utf8ToWide(stringValue(name));
  SetLastError(ERROR_SUCCESS);
  const DWORD required = GetEnvironmentVariableW(variable.c_str(), nullptr, 0);
  if (required == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND)
    return rocket_rt_aggregate_new(1, 0, 0);
  std::wstring value(required, L'\0');
  const DWORD written = GetEnvironmentVariableW(variable.c_str(), value.data(), required);
  if (written >= required) return rocket_rt_aggregate_new(1, 0, 0);
  value.resize(written);
  RocketString* string = makeString(wideToUtf8(value));
  RocketAggregate* result = managedVariant(0, string);
  rocket_rt_release(string);
  return result;
#else
  const char* value = std::getenv(stringValue(name).c_str());
  if (!value) return rocket_rt_aggregate_new(1, 0, 0);
  RocketString* string = makeString(value);
  RocketAggregate* result = managedVariant(0, string);
  rocket_rt_release(string);
  return result;
#endif
}

RocketAggregate* rocket_std_process_working_directory() {
  try {
    RocketString* value = makeString(pathString(std::filesystem::current_path()));
    RocketAggregate* result = okManaged(value);
    rocket_rt_release(value);
    return result;
  } catch (const std::exception& error) { return errorResult(error.what()); }
}

std::int64_t rocket_std_time_unix_milliseconds() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch()).count();
}

std::int64_t rocket_std_time_monotonic_milliseconds() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch()).count();
}

void rocket_std_time_sleep_milliseconds(std::int64_t milliseconds) {
  if (milliseconds < 0) {
    std::fputs("rocket runtime error: sleep duration cannot be negative\n", stderr);
    std::exit(101);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

} // extern "C"
