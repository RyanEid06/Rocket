#include "runtime.h"
#include "platform_net.h"
#include "platform_crypto.h"
#include "platform_datetime.h"
#include "platform_compression.h"
#include "platform_sqlite.h"
#include "platform_unicode.h"
#include "safe_regex.h"
#include "safe_archive.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include <sys/wait.h>
#include <unistd.h>
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
std::string processExecutablePath;

std::string runningExecutablePath(const char* fallback) {
  try {
#ifdef _WIN32
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length > 0 && length < buffer.size()) {
      buffer.resize(length);
      return pathString(std::filesystem::path(buffer));
    }
#elif defined(__linux__)
    return pathString(std::filesystem::canonical("/proc/self/exe"));
#elif defined(__APPLE__)
    std::uint32_t length = 1024;
    std::vector<char> buffer(length);
    if (_NSGetExecutablePath(buffer.data(), &length) != 0) {
      buffer.resize(length);
      if (_NSGetExecutablePath(buffer.data(), &length) != 0) buffer.clear();
    }
    if (!buffer.empty())
      return pathString(std::filesystem::canonical(buffer.data()));
#endif
    if (fallback && *fallback)
      return pathString(std::filesystem::absolute(fallback).lexically_normal());
  } catch (const std::exception&) {
    if (fallback) return fallback;
  }
  return {};
}

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

struct MapArrays {
  RocketArray* keys;
  RocketArray* values;
};

MapArrays mapArrays(RocketAggregate* map) {
  auto* keys = static_cast<RocketArray*>(rocket_rt_aggregate_get_managed(map, 0));
  auto* values = static_cast<RocketArray*>(rocket_rt_aggregate_get_managed(map, 1));
  if (rocket_rt_collection_length(keys) != rocket_rt_collection_length(values)) {
    std::fputs("rocket runtime error: Map key/value length mismatch\n", stderr);
    std::exit(101);
  }
  return {keys, values};
}

void releaseMapArrays(MapArrays arrays) {
  rocket_rt_release(arrays.keys);
  rocket_rt_release(arrays.values);
}

std::int64_t findInt(RocketArray* values, std::int64_t key) {
  const std::uint64_t length = rocket_rt_collection_length(values);
  for (std::uint64_t index = 0; index < length; ++index)
    if (rocket_rt_index_int(values, static_cast<std::int64_t>(index)) == key)
      return static_cast<std::int64_t>(index);
  return -1;
}

std::int64_t findFloat(RocketArray* values, double key) {
  const std::uint64_t length = rocket_rt_collection_length(values);
  for (std::uint64_t index = 0; index < length; ++index)
    if (rocket_rt_index_float(values, static_cast<std::int64_t>(index)) == key)
      return static_cast<std::int64_t>(index);
  return -1;
}

std::int64_t findBool(RocketArray* values, std::uint8_t key) {
  const std::uint64_t length = rocket_rt_collection_length(values);
  for (std::uint64_t index = 0; index < length; ++index)
    if (rocket_rt_index_bool(values, static_cast<std::int64_t>(index)) == (key ? 1 : 0))
      return static_cast<std::int64_t>(index);
  return -1;
}

std::int64_t findChar(RocketArray* values, std::uint8_t key) {
  const std::uint64_t length = rocket_rt_collection_length(values);
  for (std::uint64_t index = 0; index < length; ++index)
    if (rocket_rt_index_char(values, static_cast<std::int64_t>(index)) == key)
      return static_cast<std::int64_t>(index);
  return -1;
}

std::int64_t findString(RocketArray* values, RocketString* key) {
  const std::uint64_t length = rocket_rt_collection_length(values);
  for (std::uint64_t index = 0; index < length; ++index) {
    RocketString* item = rocket_rt_index_string(values, static_cast<std::int64_t>(index));
    const bool equal = rocket_rt_string_equal(item, key) != 0;
    rocket_rt_release(item);
    if (equal) return static_cast<std::int64_t>(index);
  }
  return -1;
}

RocketAggregate* optionalIndex(std::int64_t index) {
  return index < 0 ? rocket_rt_aggregate_new(1, 0, 0) : intVariant(0, index);
}

RocketAggregate* optionalArrayValue(RocketArray* values, std::int64_t index) {
  if (index < 0) return rocket_rt_aggregate_new(1, 0, 0);
  switch (rocket_rt_collection_element_kind(values)) {
  case ROCKET_ELEMENT_INT: return intVariant(0, rocket_rt_index_int(values, index));
  case ROCKET_ELEMENT_FLOAT: {
    RocketAggregate* result = rocket_rt_aggregate_new(0, 1, 0);
    rocket_rt_aggregate_set_float(result, 0, rocket_rt_index_float(values, index));
    return result;
  }
  case ROCKET_ELEMENT_BOOL: return boolVariant(0, rocket_rt_index_bool(values, index) != 0);
  case ROCKET_ELEMENT_CHAR: {
    RocketAggregate* result = rocket_rt_aggregate_new(0, 1, 0);
    rocket_rt_aggregate_set_char(result, 0, rocket_rt_index_char(values, index));
    return result;
  }
  case ROCKET_ELEMENT_STRING: {
    RocketString* value = rocket_rt_index_string(values, index);
    RocketAggregate* result = managedVariant(0, value);
    rocket_rt_release(value);
    return result;
  }
  case ROCKET_ELEMENT_MANAGED: {
    void* value = rocket_rt_index_managed(values, index);
    RocketAggregate* result = managedVariant(0, value);
    rocket_rt_release(value);
    return result;
  }
  default:
    std::fputs("rocket runtime error: invalid Map value kind\n", stderr);
    std::exit(101);
  }
}

bool equalArrayElements(RocketArray* values, std::int64_t left, std::int64_t right) {
  switch (rocket_rt_collection_element_kind(values)) {
  case ROCKET_ELEMENT_INT:
    return rocket_rt_index_int(values, left) == rocket_rt_index_int(values, right);
  case ROCKET_ELEMENT_BOOL:
    return rocket_rt_index_bool(values, left) == rocket_rt_index_bool(values, right);
  case ROCKET_ELEMENT_CHAR:
    return rocket_rt_index_char(values, left) == rocket_rt_index_char(values, right);
  case ROCKET_ELEMENT_STRING: {
    RocketString* a = rocket_rt_index_string(values, left);
    RocketString* b = rocket_rt_index_string(values, right);
    const bool equal = rocket_rt_string_equal(a, b) != 0;
    rocket_rt_release(a);
    rocket_rt_release(b);
    return equal;
  }
  default:
    std::fputs("rocket runtime error: Set or Map key type is not hashable\n", stderr);
    std::exit(101);
  }
}

RocketArray* appendArrayElement(RocketArray* target, RocketArray* source,
                                std::int64_t index) {
  switch (rocket_rt_collection_element_kind(source)) {
  case ROCKET_ELEMENT_INT:
    return rocket_rt_array_append_int(target, rocket_rt_index_int(source, index));
  case ROCKET_ELEMENT_FLOAT:
    return rocket_rt_array_append_float(target, rocket_rt_index_float(source, index));
  case ROCKET_ELEMENT_BOOL:
    return rocket_rt_array_append_bool(target, rocket_rt_index_bool(source, index));
  case ROCKET_ELEMENT_CHAR:
    return rocket_rt_array_append_char(target, rocket_rt_index_char(source, index));
  case ROCKET_ELEMENT_STRING: {
    RocketString* value = rocket_rt_index_string(source, index);
    RocketArray* result = rocket_rt_array_append_string(target, value);
    rocket_rt_release(value);
    return result;
  }
  case ROCKET_ELEMENT_MANAGED: {
    void* value = rocket_rt_index_managed(source, index);
    RocketArray* result = rocket_rt_array_append_managed(target, value);
    rocket_rt_release(value);
    return result;
  }
  default:
    std::fputs("rocket runtime error: invalid collection element kind\n", stderr);
    std::exit(101);
  }
}

std::uint64_t stableHash(const std::uint8_t* bytes, std::size_t length) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (std::size_t index = 0; index < length; ++index) {
    hash ^= bytes[index];
    hash *= 1099511628211ULL;
  }
  return hash & 0x7fffffffffffffffULL;
}

RocketArray* byteArray(std::string_view bytes) {
  RocketArray* result = rocket_rt_array_new(ROCKET_ELEMENT_CHAR, bytes.size());
  for (std::size_t index = 0; index < bytes.size(); ++index)
    rocket_rt_array_set_char(result, static_cast<std::int64_t>(index),
                             static_cast<std::uint8_t>(bytes[index]));
  return result;
}

RocketAggregate* byteBuffer(RocketArray* bytes) {
  RocketAggregate* result = rocket_rt_aggregate_new(0, 1, 1);
  rocket_rt_aggregate_set_managed(result, 0, bytes);
  return result;
}

RocketArray* byteBufferBytes(RocketAggregate* buffer) {
  return static_cast<RocketArray*>(rocket_rt_aggregate_get_managed(buffer, 0));
}

std::string byteBufferValue(RocketAggregate* buffer) {
  RocketArray* bytes = byteBufferBytes(buffer);
  const std::uint64_t length = rocket_rt_collection_length(bytes);
  std::string result(static_cast<std::size_t>(length), '\0');
  for (std::uint64_t index = 0; index < length; ++index)
    result[static_cast<std::size_t>(index)] = static_cast<char>(
        rocket_rt_index_char(bytes, static_cast<std::int64_t>(index)));
  rocket_rt_release(bytes);
  return result;
}

bool validUtf8(std::string_view input) {
  std::size_t index = 0;
  while (index < input.size()) {
    const auto first = static_cast<std::uint8_t>(input[index]);
    if (first <= 0x7f) { ++index; continue; }
    std::size_t count = 0;
    std::uint32_t codepoint = 0;
    std::uint32_t minimum = 0;
    if (first >= 0xc2 && first <= 0xdf) {
      count = 2; codepoint = first & 0x1f; minimum = 0x80;
    } else if (first >= 0xe0 && first <= 0xef) {
      count = 3; codepoint = first & 0x0f; minimum = 0x800;
    } else if (first >= 0xf0 && first <= 0xf4) {
      count = 4; codepoint = first & 0x07; minimum = 0x10000;
    } else {
      return false;
    }
    if (count > input.size() - index) return false;
    for (std::size_t continuation = 1; continuation < count; ++continuation) {
      const auto byte = static_cast<std::uint8_t>(input[index + continuation]);
      if ((byte & 0xc0) != 0x80) return false;
      codepoint = (codepoint << 6) | (byte & 0x3f);
    }
    if (codepoint < minimum || codepoint > 0x10ffff ||
        (codepoint >= 0xd800 && codepoint <= 0xdfff))
      return false;
    index += count;
  }
  return true;
}

RocketAggregate* binaryRead(RocketAggregate* buffer, std::int64_t offset,
                            std::uint64_t width) {
  RocketArray* bytes = byteBufferBytes(buffer);
  const std::uint64_t length = rocket_rt_collection_length(bytes);
  if (offset < 0 || static_cast<std::uint64_t>(offset) > length ||
      width > length - static_cast<std::uint64_t>(offset)) {
    rocket_rt_release(bytes);
    return errorResult("binary read is outside the buffer");
  }
  std::uint64_t value = 0;
  for (std::uint64_t index = 0; index < width; ++index)
    value |= static_cast<std::uint64_t>(rocket_rt_index_char(
                 bytes, offset + static_cast<std::int64_t>(index)))
             << (index * 8);
  rocket_rt_release(bytes);
  return okInt(static_cast<std::int64_t>(value));
}

RocketAggregate* binaryWrite(std::int64_t value, std::uint64_t width) {
  const std::uint64_t maximum = width == 1 ? 0xffULL
                               : width == 2 ? 0xffffULL
                                            : 0xffffffffULL;
  if (value < 0 || static_cast<std::uint64_t>(value) > maximum)
    return errorResult("binary integer is outside the unsigned encoding range");
  RocketArray* bytes = rocket_rt_array_new(ROCKET_ELEMENT_CHAR, width);
  for (std::uint64_t index = 0; index < width; ++index)
    rocket_rt_array_set_char(bytes, static_cast<std::int64_t>(index),
                             static_cast<std::uint8_t>(
                                 static_cast<std::uint64_t>(value) >> (index * 8)));
  RocketAggregate* buffer = byteBuffer(bytes);
  rocket_rt_release(bytes);
  RocketAggregate* result = okManaged(buffer);
  rocket_rt_release(buffer);
  return result;
}

RocketAggregate* binaryReadBigEndian(RocketAggregate* buffer,
                                     std::int64_t offset,
                                     std::uint64_t width) {
  RocketArray* bytes = byteBufferBytes(buffer);
  const std::uint64_t length = rocket_rt_collection_length(bytes);
  if (offset < 0 || static_cast<std::uint64_t>(offset) > length ||
      width > length - static_cast<std::uint64_t>(offset)) {
    rocket_rt_release(bytes);
    return errorResult("binary read is outside the buffer");
  }
  std::uint64_t value = 0;
  for (std::uint64_t index = 0; index < width; ++index)
    value = (value << 8) | rocket_rt_index_char(
                               bytes, offset + static_cast<std::int64_t>(index));
  rocket_rt_release(bytes);
  return okInt(static_cast<std::int64_t>(value));
}

RocketAggregate* binaryWriteBigEndian(std::int64_t value,
                                      std::uint64_t width) {
  const std::uint64_t maximum = width == 2 ? 0xffffULL : 0xffffffffULL;
  if (value < 0 || static_cast<std::uint64_t>(value) > maximum)
    return errorResult("binary integer is outside the unsigned encoding range");
  RocketArray* bytes = rocket_rt_array_new(ROCKET_ELEMENT_CHAR, width);
  for (std::uint64_t index = 0; index < width; ++index) {
    const std::uint64_t shift = (width - index - 1) * 8;
    rocket_rt_array_set_char(bytes, static_cast<std::int64_t>(index),
                             static_cast<std::uint8_t>(
                                 static_cast<std::uint64_t>(value) >> shift));
  }
  RocketAggregate* buffer = byteBuffer(bytes);
  rocket_rt_release(bytes);
  RocketAggregate* result = okManaged(buffer);
  rocket_rt_release(buffer);
  return result;
}

struct Utf8Scalar {
  std::uint32_t value = 0;
  std::size_t start = 0;
  std::size_t end = 0;
};

std::vector<Utf8Scalar> utf8Scalars(std::string_view input) {
  std::vector<Utf8Scalar> result;
  for (std::size_t index = 0; index < input.size();) {
    const std::size_t start = index;
    const auto first = static_cast<std::uint8_t>(input[index++]);
    std::uint32_t scalar = first;
    std::size_t width = 1;
    if (first >= 0xc2 && first <= 0xdf) {
      scalar = first & 0x1f;
      width = 2;
    } else if (first >= 0xe0 && first <= 0xef) {
      scalar = first & 0x0f;
      width = 3;
    } else if (first >= 0xf0 && first <= 0xf4) {
      scalar = first & 0x07;
      width = 4;
    }
    for (std::size_t continuation = 1; continuation < width; ++continuation)
      scalar = (scalar << 6) |
               (static_cast<std::uint8_t>(input[index++]) & 0x3f);
    result.push_back({scalar, start, index});
  }
  return result;
}

void appendUtf8Scalar(std::string& output, std::uint32_t scalar) {
  if (scalar <= 0x7f) {
    output.push_back(static_cast<char>(scalar));
  } else if (scalar <= 0x7ff) {
    output.push_back(static_cast<char>(0xc0 | (scalar >> 6)));
    output.push_back(static_cast<char>(0x80 | (scalar & 0x3f)));
  } else if (scalar <= 0xffff) {
    output.push_back(static_cast<char>(0xe0 | (scalar >> 12)));
    output.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3f)));
    output.push_back(static_cast<char>(0x80 | (scalar & 0x3f)));
  } else {
    output.push_back(static_cast<char>(0xf0 | (scalar >> 18)));
    output.push_back(static_cast<char>(0x80 | ((scalar >> 12) & 0x3f)));
    output.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3f)));
    output.push_back(static_cast<char>(0x80 | (scalar & 0x3f)));
  }
}

bool graphemeExtension(std::uint32_t scalar) {
  return (scalar >= 0x0300 && scalar <= 0x036f) ||
         (scalar >= 0x1ab0 && scalar <= 0x1aff) ||
         (scalar >= 0x1dc0 && scalar <= 0x1dff) ||
         (scalar >= 0x20d0 && scalar <= 0x20ff) ||
         (scalar >= 0xfe00 && scalar <= 0xfe0f) ||
         (scalar >= 0xfe20 && scalar <= 0xfe2f) ||
         (scalar >= 0x1f3fb && scalar <= 0x1f3ff) || scalar == 0x200d;
}

std::vector<std::pair<std::size_t, std::size_t>> graphemeRanges(
    std::string_view input) {
  const auto scalars = utf8Scalars(input);
  std::vector<std::pair<std::size_t, std::size_t>> ranges;
  std::size_t regionalCount = 0;
  for (std::size_t index = 0; index < scalars.size(); ++index) {
    const std::uint32_t scalar = scalars[index].value;
    const bool regional = scalar >= 0x1f1e6 && scalar <= 0x1f1ff;
    bool joinsPrevious = index != 0 && graphemeExtension(scalar);
    if (index != 0 && scalars[index - 1].value == 0x200d) joinsPrevious = true;
    if (regional) {
      joinsPrevious = index != 0 && regionalCount % 2 == 1;
      ++regionalCount;
    } else {
      regionalCount = 0;
    }
    if (!joinsPrevious)
      ranges.push_back({scalars[index].start, scalars[index].end});
    else
      ranges.back().second = scalars[index].end;
  }
  return ranges;
}

#ifdef _WIN32
RocketAggregate* normalizeUnicode(RocketString* value, NORM_FORM form) {
  const std::string input = stringValue(value);
  const std::wstring wide = utf8ToWide(input);
  if (!input.empty() && wide.empty()) return errorResult("invalid UTF-8 text");
  using NormalizeFunction = int(WINAPI*)(NORM_FORM, LPCWSTR, int, LPWSTR, int);
  const HMODULE library = LoadLibraryW(L"Normaliz.dll");
  if (!library) return errorResult("Windows Unicode normalization is unavailable");
  const auto normalize = reinterpret_cast<NormalizeFunction>(
      GetProcAddress(library, "NormalizeString"));
  if (!normalize) {
    FreeLibrary(library);
    return errorResult("Windows Unicode normalization is unavailable");
  }
  const int required = normalize(form, wide.data(), static_cast<int>(wide.size()),
                                 nullptr, 0);
  if (required <= 0) {
    FreeLibrary(library);
    return errorResult("Unicode normalization failed");
  }
  std::wstring normalized(static_cast<std::size_t>(required), L'\0');
  const int written = normalize(form, wide.data(), static_cast<int>(wide.size()),
                                normalized.data(), required);
  FreeLibrary(library);
  if (written <= 0) return errorResult("Unicode normalization failed");
  normalized.resize(static_cast<std::size_t>(written));
  RocketString* text = makeString(wideToUtf8(normalized));
  RocketAggregate* result = okManaged(text);
  rocket_rt_release(text);
  return result;
}
#endif

struct BufferedReaderState {
  std::ifstream stream;
  std::vector<char> buffer;
};

struct BufferedWriterState {
  std::ofstream stream;
  std::vector<char> buffer;
};

std::unordered_map<std::int64_t, std::unique_ptr<BufferedReaderState>> readers;
std::unordered_map<std::int64_t, std::unique_ptr<BufferedWriterState>> writers;
std::int64_t nextStreamHandle = 1;

struct NetworkSocketState {
  rocket::platform_net::Socket socket = rocket::platform_net::invalidSocket;
  bool listener = false;
};

std::unordered_map<std::int64_t, NetworkSocketState> networkSockets;
std::int64_t nextNetworkHandle = 1;
std::unordered_map<std::int64_t, sqlite3*> sqliteDatabases;
std::int64_t nextSqliteHandle = 1;
std::unordered_set<std::string> testingTemporaryDirectories;
std::unordered_map<std::string, std::int64_t> testingCoverage;
std::mutex testingMutex;
std::mutex logMutex;

bool validStreamBufferSize(std::int64_t size) {
  return size >= 256 && size <= 16 * 1024 * 1024;
}

RocketAggregate* httpResponse(std::int64_t status, std::string_view body) {
  RocketArray* bytes = byteArray(body);
  RocketAggregate* buffer = byteBuffer(bytes);
  rocket_rt_release(bytes);
  RocketAggregate* response = rocket_rt_aggregate_new(0, 2, 2);
  rocket_rt_aggregate_set_int(response, 0, status);
  rocket_rt_aggregate_set_managed(response, 1, buffer);
  rocket_rt_release(buffer);
  return response;
}

bool parseHttpRequest(rocket::platform_net::Socket socket, std::int64_t maximum,
                      std::int64_t timeout, std::string& method,
                      std::string& path, std::string& body, std::string& error) {
  if (maximum < 1 || maximum > 16 * 1024 * 1024) {
    error = "HTTP request limit must be between 1 byte and 16 MiB";
    return false;
  }
  std::string input;
  std::size_t headerEnd = std::string::npos;
  std::size_t contentLength = 0;
  while (input.size() < static_cast<std::size_t>(maximum)) {
    std::string chunk;
    const std::size_t remaining = static_cast<std::size_t>(maximum) - input.size();
    if (!rocket::platform_net::receive(socket, (std::min)(remaining, std::size_t{8192}),
                                        timeout, chunk, error))
      return false;
    if (chunk.empty()) { error = "HTTP peer closed before the request completed"; return false; }
    input += chunk;
    if (headerEnd == std::string::npos) {
      headerEnd = input.find("\r\n\r\n");
      if (headerEnd == std::string::npos) continue;
      const std::size_t lineEnd = input.find("\r\n");
      if (lineEnd == std::string::npos || lineEnd > headerEnd) {
        error = "invalid HTTP request line";
        return false;
      }
      const std::string requestLine = input.substr(0, lineEnd);
      const std::size_t firstSpace = requestLine.find(' ');
      const std::size_t secondSpace = requestLine.find(' ', firstSpace + 1);
      if (firstSpace == std::string::npos || secondSpace == std::string::npos ||
          !requestLine.substr(secondSpace + 1).starts_with("HTTP/1.")) {
        error = "invalid HTTP request line";
        return false;
      }
      method = requestLine.substr(0, firstSpace);
      path = requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
      std::size_t cursor = lineEnd + 2;
      while (cursor < headerEnd) {
        const std::size_t end = input.find("\r\n", cursor);
        std::string line = input.substr(cursor, end - cursor);
        std::string lower = line;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char value) {
          return static_cast<char>(std::tolower(value));
        });
        if (lower.starts_with("transfer-encoding:") &&
            lower.find("chunked") != std::string::npos) {
          error = "chunked HTTP requests are not supported by the bounded server foundation";
          return false;
        }
        if (lower.starts_with("content-length:")) {
          const std::string text = line.substr(line.find(':') + 1);
          const std::size_t first = text.find_first_not_of(" \t");
          if (first == std::string::npos) { error = "invalid HTTP Content-Length"; return false; }
          std::uint64_t parsed = 0;
          const auto converted = std::from_chars(text.data() + first,
                                                  text.data() + text.size(), parsed);
          if (converted.ec != std::errc{} ||
              converted.ptr != text.data() + text.size() ||
              parsed > static_cast<std::uint64_t>(maximum)) {
            error = "invalid or excessive HTTP Content-Length";
            return false;
          }
          contentLength = static_cast<std::size_t>(parsed);
        }
        cursor = end + 2;
      }
    }
    const std::size_t bodyStart = headerEnd + 4;
    if (input.size() >= bodyStart + contentLength) {
      body = input.substr(bodyStart, contentLength);
      return true;
    }
  }
  error = "HTTP request exceeds its configured byte limit";
  return false;
}

std::string trimAscii(std::string_view value) {
  const std::size_t first = value.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) return {};
  const std::size_t last = value.find_last_not_of(" \t\r\n");
  return std::string(value.substr(first, last - first + 1));
}

RocketAggregate* optionalStringResult(bool found, std::string_view value) {
  RocketAggregate* option = nullptr;
  if (found) {
    RocketString* text = makeString(value);
    option = managedVariant(0, text);
    rocket_rt_release(text);
  } else {
    option = rocket_rt_aggregate_new(1, 0, 0);
  }
  RocketAggregate* result = okManaged(option);
  rocket_rt_release(option);
  return result;
}

bool validLogLevel(std::string_view level) {
  return level == "trace" || level == "debug" || level == "info" ||
         level == "warn" || level == "error" || level == "fatal";
}

std::string logLine(std::string_view level, std::string_view message,
                    std::string& error) {
  if (!validLogLevel(level)) {
    error = "log level must be trace, debug, info, warn, error, or fatal";
    return {};
  }
  if (message.size() > 1024 * 1024) {
    error = "log message exceeds the 1 MiB limit";
    return {};
  }
  const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
  std::string timestamp;
  if (!rocket::platform_datetime::formatUtc(now, timestamp, error)) return {};
  std::string escaped;
  escaped.reserve(message.size());
  for (const char character : message) {
    if (character == '\n') escaped += "\\n";
    else if (character == '\r') escaped += "\\r";
    else escaped.push_back(character);
  }
  return timestamp + " [" + std::string(level) + "] " + escaped + "\n";
}

bool configValue(std::string_view source, std::string_view requested,
                 bool& found, std::string& value, std::string& error) {
  found = false;
  value.clear();
  if (source.size() > 1024 * 1024) {
    error = "configuration text exceeds the 1 MiB limit";
    return false;
  }
  const std::string key = trimAscii(requested);
  if (key.empty() || key.size() > 256) {
    error = "configuration key must contain 1 through 256 bytes";
    return false;
  }
  std::unordered_map<std::string, std::string> values;
  std::string section;
  std::size_t cursor = 0;
  std::size_t lineNumber = 0;
  while (cursor <= source.size()) {
    const std::size_t end = source.find('\n', cursor);
    std::string line(source.substr(cursor, end == std::string_view::npos
                                              ? std::string_view::npos : end - cursor));
    ++lineNumber;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.size() > 65536) {
      error = "configuration line exceeds 64 KiB at line " +
              std::to_string(lineNumber);
      return false;
    }
    bool quoted = false;
    bool escaped = false;
    for (std::size_t index = 0; index < line.size(); ++index) {
      const char character = line[index];
      if (escaped) { escaped = false; continue; }
      if (quoted && character == '\\') { escaped = true; continue; }
      if (character == '"') { quoted = !quoted; continue; }
      if (!quoted && character == '#') { line.resize(index); break; }
    }
    line = trimAscii(line);
    if (!line.empty()) {
      if (line.front() == '[') {
        if (line.size() < 3 || line.back() != ']') {
          error = "invalid configuration section at line " +
                  std::to_string(lineNumber);
          return false;
        }
        section = trimAscii(std::string_view(line).substr(1, line.size() - 2));
        if (section.empty()) {
          error = "empty configuration section at line " +
                  std::to_string(lineNumber);
          return false;
        }
      } else {
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
          error = "configuration entry is missing '=' at line " +
                  std::to_string(lineNumber);
          return false;
        }
        const std::string local = trimAscii(std::string_view(line).substr(0, equals));
        std::string parsed = trimAscii(std::string_view(line).substr(equals + 1));
        if (local.empty()) {
          error = "configuration entry has an empty key at line " +
                  std::to_string(lineNumber);
          return false;
        }
        if (!parsed.empty() && parsed.front() == '"') {
          if (parsed.size() < 2 || parsed.back() != '"') {
            error = "unterminated configuration string at line " +
                    std::to_string(lineNumber);
            return false;
          }
          std::string decoded;
          for (std::size_t index = 1; index + 1 < parsed.size(); ++index) {
            char character = parsed[index];
            if (character != '\\') { decoded.push_back(character); continue; }
            if (++index + 1 >= parsed.size()) {
              error = "unterminated configuration escape at line " +
                      std::to_string(lineNumber);
              return false;
            }
            character = parsed[index];
            if (character == 'n') decoded.push_back('\n');
            else if (character == 'r') decoded.push_back('\r');
            else if (character == 't') decoded.push_back('\t');
            else if (character == '"' || character == '\\') decoded.push_back(character);
            else {
              error = "unsupported configuration escape at line " +
                      std::to_string(lineNumber);
              return false;
            }
          }
          parsed = std::move(decoded);
        }
        const std::string qualified = section.empty() ? local : section + "." + local;
        if (!values.emplace(qualified, parsed).second) {
          error = "duplicate configuration key '" + qualified + "'";
          return false;
        }
      }
    }
    if (end == std::string_view::npos) break;
    cursor = end + 1;
  }
  const auto selected = values.find(key);
  if (selected != values.end()) { found = true; value = selected->second; }
  return true;
}

std::vector<std::string> stringValues(RocketArray* values) {
  const std::uint64_t count = rocket_rt_collection_length(values);
  std::vector<std::string> result;
  result.reserve(static_cast<std::size_t>(count));
  for (std::uint64_t index = 0; index < count; ++index) {
    RocketString* value = rocket_rt_index_string(values,
                                                  static_cast<std::int64_t>(index));
    result.push_back(stringValue(value));
    rocket_rt_release(value);
  }
  return result;
}

bool safeTestingRelative(std::string_view value) {
  if (value.empty() || value.size() > 1024 || value.front() == '/' ||
      value.front() == '\\' || value.find(':') != std::string_view::npos ||
      value.find('\\') != std::string_view::npos) return false;
  std::size_t start = 0;
  while (start <= value.size()) {
    const std::size_t end = value.find('/', start);
    const std::string_view part = value.substr(
        start, end == std::string_view::npos ? std::string_view::npos : end - start);
    if (part.empty() || part == "." || part == "..") return false;
    if (end == std::string_view::npos) break;
    start = end + 1;
  }
  return true;
}

RocketAggregate* okString(std::string_view value) {
  RocketString* text = makeString(value);
  RocketAggregate* result = okManaged(text);
  rocket_rt_release(text);
  return result;
}

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

std::int64_t rocket_std_collections_capacity(RocketArray* values) {
  const std::uint64_t capacity = rocket_rt_array_capacity(values);
  if (capacity > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)())) {
    std::fputs("rocket runtime error: Array capacity exceeds Int range\n", stderr);
    std::exit(101);
  }
  return static_cast<std::int64_t>(capacity);
}

RocketArray* rocket_std_collections_reserve(RocketArray* values,
                                            std::int64_t minimumCapacity) {
  return rocket_rt_array_reserve(values, minimumCapacity);
}

RocketArray* rocket_std_collections_append_int(RocketArray* values, std::int64_t value) {
  return rocket_rt_array_append_int(values, value);
}

RocketArray* rocket_std_collections_append_float(RocketArray* values, double value) {
  return rocket_rt_array_append_float(values, value);
}

RocketArray* rocket_std_collections_append_bool(RocketArray* values, std::uint8_t value) {
  return rocket_rt_array_append_bool(values, value);
}

RocketArray* rocket_std_collections_append_char(RocketArray* values, std::uint8_t value) {
  return rocket_rt_array_append_char(values, value);
}

RocketArray* rocket_std_collections_append_string(RocketArray* values, RocketString* value) {
  return rocket_rt_array_append_string(values, value);
}

RocketArray* rocket_std_collections_append_managed(RocketArray* values, void* value) {
  return rocket_rt_array_append_managed(values, value);
}

RocketAggregate* rocket_std_collections_pop(RocketArray* values) {
  return rocket_rt_array_pop(values);
}

RocketArray* rocket_std_collections_insert_int(RocketArray* values, std::int64_t index,
                                               std::int64_t value) {
  return rocket_rt_array_insert_int(values, index, value);
}

RocketArray* rocket_std_collections_insert_float(RocketArray* values, std::int64_t index,
                                                 double value) {
  return rocket_rt_array_insert_float(values, index, value);
}

RocketArray* rocket_std_collections_insert_bool(RocketArray* values, std::int64_t index,
                                                std::uint8_t value) {
  return rocket_rt_array_insert_bool(values, index, value);
}

RocketArray* rocket_std_collections_insert_char(RocketArray* values, std::int64_t index,
                                                std::uint8_t value) {
  return rocket_rt_array_insert_char(values, index, value);
}

RocketArray* rocket_std_collections_insert_string(RocketArray* values, std::int64_t index,
                                                  RocketString* value) {
  return rocket_rt_array_insert_string(values, index, value);
}

RocketArray* rocket_std_collections_insert_managed(RocketArray* values, std::int64_t index,
                                                   void* value) {
  return rocket_rt_array_insert_managed(values, index, value);
}

RocketAggregate* rocket_std_collections_remove(RocketArray* values, std::int64_t index) {
  return rocket_rt_array_remove(values, index);
}

RocketArray* rocket_std_collections_clear(RocketArray* values) {
  return rocket_rt_array_clear(values);
}

RocketAggregate* rocket_std_collections_map_from_arrays(RocketArray* keys,
                                                        RocketArray* values) {
  if (rocket_rt_collection_length(keys) != rocket_rt_collection_length(values)) {
    std::fputs("rocket runtime error: Map key/value length mismatch\n", stderr);
    std::exit(101);
  }
  const std::uint64_t length = rocket_rt_collection_length(keys);
  RocketArray* uniqueKeys = rocket_rt_array_new(rocket_rt_collection_element_kind(keys), 0);
  RocketArray* uniqueValues = rocket_rt_array_new(rocket_rt_collection_element_kind(values), 0);
  for (std::uint64_t index = 0; index < length; ++index) {
    bool duplicate = false;
    for (std::uint64_t previous = 0; previous < index; ++previous)
      if (equalArrayElements(keys, static_cast<std::int64_t>(index),
                            static_cast<std::int64_t>(previous))) {
        duplicate = true;
        break;
      }
    if (!duplicate) {
      RocketArray* nextKeys = appendArrayElement(
          uniqueKeys, keys, static_cast<std::int64_t>(index));
      RocketArray* nextValues = appendArrayElement(
          uniqueValues, values, static_cast<std::int64_t>(index));
      rocket_rt_release(uniqueKeys);
      rocket_rt_release(uniqueValues);
      uniqueKeys = nextKeys;
      uniqueValues = nextValues;
    }
  }
  RocketAggregate* map = rocket_rt_aggregate_new(0, 2, 3);
  rocket_rt_aggregate_set_managed(map, 0, uniqueKeys);
  rocket_rt_aggregate_set_managed(map, 1, uniqueValues);
  rocket_rt_release(uniqueKeys);
  rocket_rt_release(uniqueValues);
  return map;
}

std::int64_t rocket_std_collections_map_length(RocketAggregate* map) {
  MapArrays arrays = mapArrays(map);
  const auto length = static_cast<std::int64_t>(rocket_rt_collection_length(arrays.keys));
  releaseMapArrays(arrays);
  return length;
}

#define ROCKET_MAP_LOOKUP(SUFFIX, TYPE, FIND)                                      \
  RocketAggregate* rocket_std_collections_map_find_##SUFFIX(RocketAggregate* map, \
                                                            TYPE key) {           \
    MapArrays arrays = mapArrays(map);                                             \
    const std::int64_t index = FIND(arrays.keys, key);                             \
    releaseMapArrays(arrays);                                                      \
    return optionalIndex(index);                                                   \
  }                                                                                \
  RocketAggregate* rocket_std_collections_map_get_##SUFFIX(RocketAggregate* map,  \
                                                           TYPE key) {            \
    MapArrays arrays = mapArrays(map);                                             \
    const std::int64_t index = FIND(arrays.keys, key);                             \
    RocketAggregate* result = optionalArrayValue(arrays.values, index);            \
    releaseMapArrays(arrays);                                                      \
    return result;                                                                 \
  }

ROCKET_MAP_LOOKUP(int, std::int64_t, findInt)
ROCKET_MAP_LOOKUP(bool, std::uint8_t, findBool)
ROCKET_MAP_LOOKUP(char, std::uint8_t, findChar)
ROCKET_MAP_LOOKUP(string, RocketString*, findString)
#undef ROCKET_MAP_LOOKUP

RocketArray* rocket_std_collections_map_keys(RocketAggregate* map) {
  return static_cast<RocketArray*>(rocket_rt_aggregate_get_managed(map, 0));
}

RocketArray* rocket_std_collections_map_values(RocketAggregate* map) {
  return static_cast<RocketArray*>(rocket_rt_aggregate_get_managed(map, 1));
}

RocketAggregate* rocket_std_collections_set_from_array(RocketArray* values) {
  const std::uint64_t length = rocket_rt_collection_length(values);
  RocketArray* unique = rocket_rt_array_new(rocket_rt_collection_element_kind(values), 0);
  for (std::uint64_t index = 0; index < length; ++index) {
    bool duplicate = false;
    for (std::uint64_t previous = 0; previous < index; ++previous) {
      if (equalArrayElements(values, static_cast<std::int64_t>(index),
                            static_cast<std::int64_t>(previous))) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate) {
      RocketArray* appended = appendArrayElement(unique, values,
                                                 static_cast<std::int64_t>(index));
      rocket_rt_release(unique);
      unique = appended;
    }
  }
  RocketAggregate* set = rocket_rt_aggregate_new(0, 1, 1);
  rocket_rt_aggregate_set_managed(set, 0, unique);
  rocket_rt_release(unique);
  return set;
}

#define ROCKET_SET_CONTAINS(SUFFIX, TYPE, FIND)                                    \
  std::uint8_t rocket_std_collections_set_contains_##SUFFIX(RocketAggregate* set,  \
                                                             TYPE value) {         \
    auto* values = static_cast<RocketArray*>(rocket_rt_aggregate_get_managed(set, 0)); \
    const bool found = FIND(values, value) >= 0;                                    \
    rocket_rt_release(values);                                                      \
    return found ? 1 : 0;                                                          \
  }

ROCKET_SET_CONTAINS(int, std::int64_t, findInt)
ROCKET_SET_CONTAINS(bool, std::uint8_t, findBool)
ROCKET_SET_CONTAINS(char, std::uint8_t, findChar)
ROCKET_SET_CONTAINS(string, RocketString*, findString)
#undef ROCKET_SET_CONTAINS

RocketArray* rocket_std_collections_set_values(RocketAggregate* set) {
  return static_cast<RocketArray*>(rocket_rt_aggregate_get_managed(set, 0));
}

std::int64_t rocket_std_collections_hash_int(std::int64_t value) {
  return static_cast<std::int64_t>(stableHash(
      reinterpret_cast<const std::uint8_t*>(&value), sizeof(value)));
}

std::int64_t rocket_std_collections_hash_bool(std::uint8_t value) {
  value = value ? 1 : 0;
  return static_cast<std::int64_t>(stableHash(&value, 1));
}

std::int64_t rocket_std_collections_hash_char(std::uint8_t value) {
  return static_cast<std::int64_t>(stableHash(&value, 1));
}

std::int64_t rocket_std_collections_hash_string(RocketString* value) {
  return static_cast<std::int64_t>(stableHash(
      rocket_rt_string_bytes(value),
      static_cast<std::size_t>(rocket_rt_string_byte_length(value))));
}

#define ROCKET_COLLECTION_SEARCH(SUFFIX, TYPE, FIND)                                \
  std::uint8_t rocket_std_collections_contains_##SUFFIX(RocketArray* values,       \
                                                         TYPE value) {              \
    return FIND(values, value) >= 0 ? 1 : 0;                                       \
  }                                                                                 \
  RocketAggregate* rocket_std_collections_find_##SUFFIX(RocketArray* values,       \
                                                         TYPE value) {              \
    return optionalIndex(FIND(values, value));                                     \
  }

ROCKET_COLLECTION_SEARCH(int, std::int64_t, findInt)
ROCKET_COLLECTION_SEARCH(float, double, findFloat)
ROCKET_COLLECTION_SEARCH(bool, std::uint8_t, findBool)
ROCKET_COLLECTION_SEARCH(char, std::uint8_t, findChar)
ROCKET_COLLECTION_SEARCH(string, RocketString*, findString)
#undef ROCKET_COLLECTION_SEARCH

#define ROCKET_COLLECTION_FILTER(SUFFIX, TYPE, GETTER, APPENDER)                    \
  RocketArray* rocket_std_collections_filter_equal_##SUFFIX(RocketArray* values,   \
                                                             TYPE wanted) {         \
    RocketArray* result = rocket_rt_array_new(rocket_rt_collection_element_kind(values), 0); \
    const std::uint64_t length = rocket_rt_collection_length(values);               \
    for (std::uint64_t index = 0; index < length; ++index) {                        \
      const TYPE item = GETTER(values, static_cast<std::int64_t>(index));           \
      if (item == wanted) {                                                         \
        RocketArray* appended = APPENDER(result, item);                             \
        rocket_rt_release(result);                                                  \
        result = appended;                                                          \
      }                                                                             \
    }                                                                               \
    return result;                                                                  \
  }

ROCKET_COLLECTION_FILTER(int, std::int64_t, rocket_rt_index_int, rocket_rt_array_append_int)
ROCKET_COLLECTION_FILTER(float, double, rocket_rt_index_float, rocket_rt_array_append_float)
ROCKET_COLLECTION_FILTER(bool, std::uint8_t, rocket_rt_index_bool, rocket_rt_array_append_bool)
ROCKET_COLLECTION_FILTER(char, std::uint8_t, rocket_rt_index_char, rocket_rt_array_append_char)
#undef ROCKET_COLLECTION_FILTER

RocketArray* rocket_std_collections_filter_equal_string(RocketArray* values,
                                                        RocketString* wanted) {
  RocketArray* result = rocket_rt_array_new(ROCKET_ELEMENT_STRING, 0);
  const std::uint64_t length = rocket_rt_collection_length(values);
  for (std::uint64_t index = 0; index < length; ++index) {
    RocketString* item = rocket_rt_index_string(values, static_cast<std::int64_t>(index));
    if (rocket_rt_string_equal(item, wanted)) {
      RocketArray* appended = rocket_rt_array_append_string(result, item);
      rocket_rt_release(result);
      result = appended;
    }
    rocket_rt_release(item);
  }
  return result;
}

RocketArray* rocket_std_collections_sort_int(RocketArray* values) {
  std::vector<std::int64_t> sorted;
  const std::uint64_t length = rocket_rt_collection_length(values);
  sorted.reserve(static_cast<std::size_t>(length));
  for (std::uint64_t index = 0; index < length; ++index)
    sorted.push_back(rocket_rt_index_int(values, static_cast<std::int64_t>(index)));
  std::sort(sorted.begin(), sorted.end());
  RocketArray* result = rocket_rt_array_new(ROCKET_ELEMENT_INT, length);
  for (std::uint64_t index = 0; index < length; ++index)
    rocket_rt_array_set_int(result, static_cast<std::int64_t>(index), sorted[index]);
  return result;
}

RocketArray* rocket_std_collections_sort_float(RocketArray* values) {
  std::vector<double> sorted;
  const std::uint64_t length = rocket_rt_collection_length(values);
  sorted.reserve(static_cast<std::size_t>(length));
  for (std::uint64_t index = 0; index < length; ++index)
    sorted.push_back(rocket_rt_index_float(values, static_cast<std::int64_t>(index)));
  std::stable_sort(sorted.begin(), sorted.end(), [](double left, double right) {
    if (std::isnan(left)) return false;
    if (std::isnan(right)) return true;
    return left < right;
  });
  RocketArray* result = rocket_rt_array_new(ROCKET_ELEMENT_FLOAT, length);
  for (std::uint64_t index = 0; index < length; ++index)
    rocket_rt_array_set_float(result, static_cast<std::int64_t>(index), sorted[index]);
  return result;
}

RocketArray* rocket_std_collections_sort_char(RocketArray* values) {
  std::vector<std::uint8_t> sorted;
  const std::uint64_t length = rocket_rt_collection_length(values);
  sorted.reserve(static_cast<std::size_t>(length));
  for (std::uint64_t index = 0; index < length; ++index)
    sorted.push_back(rocket_rt_index_char(values, static_cast<std::int64_t>(index)));
  std::sort(sorted.begin(), sorted.end());
  RocketArray* result = rocket_rt_array_new(ROCKET_ELEMENT_CHAR, length);
  for (std::uint64_t index = 0; index < length; ++index)
    rocket_rt_array_set_char(result, static_cast<std::int64_t>(index), sorted[index]);
  return result;
}

RocketArray* rocket_std_collections_sort_string(RocketArray* values) {
  std::vector<std::string> sorted;
  const std::uint64_t length = rocket_rt_collection_length(values);
  sorted.reserve(static_cast<std::size_t>(length));
  for (std::uint64_t index = 0; index < length; ++index) {
    RocketString* item = rocket_rt_index_string(values, static_cast<std::int64_t>(index));
    sorted.push_back(stringValue(item));
    rocket_rt_release(item);
  }
  std::sort(sorted.begin(), sorted.end());
  return stringArray(sorted);
}

#define ROCKET_MAP_HASH(SUFFIX, GETTER, HASHER)                                     \
  RocketArray* rocket_std_collections_map_hash_##SUFFIX(RocketArray* values) {     \
    const std::uint64_t length = rocket_rt_collection_length(values);               \
    RocketArray* result = rocket_rt_array_new(ROCKET_ELEMENT_INT, length);          \
    for (std::uint64_t index = 0; index < length; ++index)                          \
      rocket_rt_array_set_int(result, static_cast<std::int64_t>(index),             \
                              HASHER(GETTER(values, static_cast<std::int64_t>(index)))); \
    return result;                                                                  \
  }

ROCKET_MAP_HASH(int, rocket_rt_index_int, rocket_std_collections_hash_int)
ROCKET_MAP_HASH(bool, rocket_rt_index_bool, rocket_std_collections_hash_bool)
ROCKET_MAP_HASH(char, rocket_rt_index_char, rocket_std_collections_hash_char)
#undef ROCKET_MAP_HASH

RocketArray* rocket_std_collections_map_hash_string(RocketArray* values) {
  const std::uint64_t length = rocket_rt_collection_length(values);
  RocketArray* result = rocket_rt_array_new(ROCKET_ELEMENT_INT, length);
  for (std::uint64_t index = 0; index < length; ++index) {
    RocketString* value = rocket_rt_index_string(values, static_cast<std::int64_t>(index));
    rocket_rt_array_set_int(result, static_cast<std::int64_t>(index),
                            rocket_std_collections_hash_string(value));
    rocket_rt_release(value);
  }
  return result;
}

std::int64_t rocket_std_collections_fold_sum_int(RocketArray* values) {
  std::int64_t total = 0;
  const std::uint64_t length = rocket_rt_collection_length(values);
  for (std::uint64_t index = 0; index < length; ++index) {
    const std::int64_t value = rocket_rt_index_int(values, static_cast<std::int64_t>(index));
    if ((value > 0 && total > (std::numeric_limits<std::int64_t>::max)() - value) ||
        (value < 0 && total < (std::numeric_limits<std::int64_t>::min)() - value))
      rocket_rt_panic_integer_overflow();
    total += value;
  }
  return total;
}

double rocket_std_collections_fold_sum_float(RocketArray* values) {
  double total = 0.0;
  const std::uint64_t length = rocket_rt_collection_length(values);
  for (std::uint64_t index = 0; index < length; ++index)
    total += rocket_rt_index_float(values, static_cast<std::int64_t>(index));
  return total;
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

RocketAggregate* rocket_std_binary_from_string(RocketString* value) {
  RocketArray* bytes = byteArray(stringValue(value));
  RocketAggregate* result = byteBuffer(bytes);
  rocket_rt_release(bytes);
  return result;
}

RocketAggregate* rocket_std_binary_to_string(RocketAggregate* buffer) {
  const std::string bytes = byteBufferValue(buffer);
  if (!validUtf8(bytes)) return errorResult("buffer is not valid UTF-8");
  RocketString* value = makeString(bytes);
  RocketAggregate* result = okManaged(value);
  rocket_rt_release(value);
  return result;
}

std::int64_t rocket_std_binary_length(RocketAggregate* buffer) {
  RocketArray* bytes = byteBufferBytes(buffer);
  const auto length = static_cast<std::int64_t>(rocket_rt_collection_length(bytes));
  rocket_rt_release(bytes);
  return length;
}

RocketAggregate* rocket_std_binary_slice(RocketAggregate* buffer,
                                         std::int64_t offset,
                                         std::int64_t length) {
  RocketArray* bytes = byteBufferBytes(buffer);
  const std::uint64_t available = rocket_rt_collection_length(bytes);
  if (offset < 0 || length < 0 || static_cast<std::uint64_t>(offset) > available ||
      static_cast<std::uint64_t>(length) >
          available - static_cast<std::uint64_t>(offset)) {
    rocket_rt_release(bytes);
    return errorResult("binary slice is outside the buffer");
  }
  RocketArray* sliced = rocket_rt_array_new(ROCKET_ELEMENT_CHAR,
                                             static_cast<std::uint64_t>(length));
  for (std::int64_t index = 0; index < length; ++index)
    rocket_rt_array_set_char(sliced, index,
                             rocket_rt_index_char(bytes, offset + index));
  rocket_rt_release(bytes);
  RocketAggregate* slicedBuffer = byteBuffer(sliced);
  rocket_rt_release(sliced);
  RocketAggregate* result = okManaged(slicedBuffer);
  rocket_rt_release(slicedBuffer);
  return result;
}

RocketAggregate* rocket_std_binary_read_u8(RocketAggregate* buffer,
                                           std::int64_t offset) {
  return binaryRead(buffer, offset, 1);
}

RocketAggregate* rocket_std_binary_read_u16_le(RocketAggregate* buffer,
                                               std::int64_t offset) {
  return binaryRead(buffer, offset, 2);
}

RocketAggregate* rocket_std_binary_read_u32_le(RocketAggregate* buffer,
                                               std::int64_t offset) {
  return binaryRead(buffer, offset, 4);
}

RocketAggregate* rocket_std_binary_write_u8(std::int64_t value) {
  return binaryWrite(value, 1);
}

RocketAggregate* rocket_std_binary_write_u16_le(std::int64_t value) {
  return binaryWrite(value, 2);
}

RocketAggregate* rocket_std_binary_write_u32_le(std::int64_t value) {
  return binaryWrite(value, 4);
}

RocketAggregate* rocket_std_binary_concat(RocketAggregate* left,
                                           RocketAggregate* right) {
  const std::string joined = byteBufferValue(left) + byteBufferValue(right);
  RocketArray* bytes = byteArray(joined);
  RocketAggregate* result = byteBuffer(bytes);
  rocket_rt_release(bytes);
  return result;
}

RocketAggregate* rocket_std_binary_read_u16_be(RocketAggregate* buffer,
                                                std::int64_t offset) {
  return binaryReadBigEndian(buffer, offset, 2);
}

RocketAggregate* rocket_std_binary_read_u32_be(RocketAggregate* buffer,
                                                std::int64_t offset) {
  return binaryReadBigEndian(buffer, offset, 4);
}

RocketAggregate* rocket_std_binary_write_u16_be(std::int64_t value) {
  return binaryWriteBigEndian(value, 2);
}

RocketAggregate* rocket_std_binary_write_u32_be(std::int64_t value) {
  return binaryWriteBigEndian(value, 4);
}

RocketAggregate* rocket_std_stream_open_reader(RocketString* path,
                                                std::int64_t bufferSize) {
  if (!validStreamBufferSize(bufferSize))
    return errorResult("stream buffer size must be between 256 bytes and 16 MiB");
  auto state = std::make_unique<BufferedReaderState>();
  state->buffer.resize(static_cast<std::size_t>(bufferSize));
  state->stream.rdbuf()->pubsetbuf(state->buffer.data(),
                                   static_cast<std::streamsize>(state->buffer.size()));
  state->stream.open(pathValue(path), std::ios::binary);
  if (!state->stream) return errorResult("could not open buffered reader");
  const std::int64_t handle = nextStreamHandle++;
  readers.emplace(handle, std::move(state));
  return okInt(handle);
}

RocketAggregate* rocket_std_stream_read(std::int64_t handle,
                                        std::int64_t maximumBytes) {
  const auto found = readers.find(handle);
  if (found == readers.end()) return errorResult("invalid or closed reader handle");
  if (maximumBytes < 0 || maximumBytes > 64 * 1024 * 1024)
    return errorResult("stream read size must be between 0 bytes and 64 MiB");
  std::string bytes(static_cast<std::size_t>(maximumBytes), '\0');
  found->second->stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  const std::streamsize count = found->second->stream.gcount();
  if (found->second->stream.bad()) return errorResult("buffered reader failed");
  bytes.resize(static_cast<std::size_t>(count));
  RocketArray* array = byteArray(bytes);
  RocketAggregate* buffer = byteBuffer(array);
  rocket_rt_release(array);
  RocketAggregate* result = okManaged(buffer);
  rocket_rt_release(buffer);
  return result;
}

RocketAggregate* rocket_std_stream_close_reader(std::int64_t handle) {
  const auto found = readers.find(handle);
  if (found == readers.end()) return errorResult("invalid or closed reader handle");
  readers.erase(found);
  return okBool(true);
}

RocketAggregate* rocket_std_stream_open_writer(RocketString* path,
                                                std::int64_t bufferSize,
                                                std::uint8_t append) {
  if (!validStreamBufferSize(bufferSize))
    return errorResult("stream buffer size must be between 256 bytes and 16 MiB");
  auto state = std::make_unique<BufferedWriterState>();
  state->buffer.resize(static_cast<std::size_t>(bufferSize));
  state->stream.rdbuf()->pubsetbuf(state->buffer.data(),
                                   static_cast<std::streamsize>(state->buffer.size()));
  const auto mode = std::ios::binary | (append ? std::ios::app : std::ios::trunc);
  state->stream.open(pathValue(path), mode);
  if (!state->stream) return errorResult("could not open buffered writer");
  const std::int64_t handle = nextStreamHandle++;
  writers.emplace(handle, std::move(state));
  return okInt(handle);
}

RocketAggregate* rocket_std_stream_write(std::int64_t handle,
                                         RocketAggregate* buffer) {
  const auto found = writers.find(handle);
  if (found == writers.end()) return errorResult("invalid or closed writer handle");
  const std::string bytes = byteBufferValue(buffer);
  found->second->stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  if (!found->second->stream) return errorResult("buffered writer failed");
  return okBool(true);
}

RocketAggregate* rocket_std_stream_flush(std::int64_t handle) {
  const auto found = writers.find(handle);
  if (found == writers.end()) return errorResult("invalid or closed writer handle");
  found->second->stream.flush();
  if (!found->second->stream) return errorResult("buffered writer flush failed");
  return okBool(true);
}

RocketAggregate* rocket_std_stream_close_writer(std::int64_t handle) {
  const auto found = writers.find(handle);
  if (found == writers.end()) return errorResult("invalid or closed writer handle");
  found->second->stream.flush();
  const bool succeeded = static_cast<bool>(found->second->stream);
  writers.erase(found);
  if (!succeeded) return errorResult("buffered writer close failed");
  return okBool(true);
}

std::int64_t rocket_std_unicode_scalar_count(RocketString* value) {
  return static_cast<std::int64_t>(utf8Scalars(stringValue(value)).size());
}

RocketAggregate* rocket_std_unicode_scalar_at(RocketString* value,
                                               std::int64_t index) {
  const auto scalars = utf8Scalars(stringValue(value));
  if (index < 0 || static_cast<std::size_t>(index) >= scalars.size())
    return errorResult("Unicode scalar index is outside the string");
  return okInt(static_cast<std::int64_t>(scalars[static_cast<std::size_t>(index)].value));
}

RocketAggregate* rocket_std_unicode_from_scalar(std::int64_t scalar) {
  if (scalar < 0 || scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff))
    return errorResult("value is not a Unicode scalar");
  std::string result;
  appendUtf8Scalar(result, static_cast<std::uint32_t>(scalar));
  RocketString* text = makeString(result);
  RocketAggregate* wrapped = okManaged(text);
  rocket_rt_release(text);
  return wrapped;
}

RocketAggregate* rocket_std_unicode_normalize_nfc(RocketString* value) {
#ifdef _WIN32
  return normalizeUnicode(value, NormalizationC);
#else
  std::string normalized;
  std::string error;
  if (!rocket::platform_unicode::normalize(
          stringValue(value),
          rocket::platform_unicode::NormalizationForm::Nfc, normalized, error))
    return errorResult(error);
  RocketString* text = makeString(normalized);
  RocketAggregate* result = okManaged(text);
  rocket_rt_release(text);
  return result;
#endif
}

RocketAggregate* rocket_std_unicode_normalize_nfd(RocketString* value) {
#ifdef _WIN32
  return normalizeUnicode(value, NormalizationD);
#else
  std::string normalized;
  std::string error;
  if (!rocket::platform_unicode::normalize(
          stringValue(value),
          rocket::platform_unicode::NormalizationForm::Nfd, normalized, error))
    return errorResult(error);
  RocketString* text = makeString(normalized);
  RocketAggregate* result = okManaged(text);
  rocket_rt_release(text);
  return result;
#endif
}

std::int64_t rocket_std_unicode_grapheme_count(RocketString* value) {
  return static_cast<std::int64_t>(graphemeRanges(stringValue(value)).size());
}

RocketAggregate* rocket_std_unicode_grapheme_at(RocketString* value,
                                                 std::int64_t index) {
  const std::string input = stringValue(value);
  const auto ranges = graphemeRanges(input);
  if (index < 0 || static_cast<std::size_t>(index) >= ranges.size())
    return errorResult("Unicode grapheme index is outside the string");
  const auto [start, end] = ranges[static_cast<std::size_t>(index)];
  RocketString* text = makeString(std::string_view(input).substr(start, end - start));
  RocketAggregate* result = okManaged(text);
  rocket_rt_release(text);
  return result;
}

RocketAggregate* rocket_std_regex_is_match(RocketString* pattern,
                                            RocketString* value) {
  const auto found = rocket::safe_regex::search(stringValue(pattern), stringValue(value));
  if (!found.valid) return errorResult(found.error);
  return okBool(!found.matches.empty());
}

RocketAggregate* rocket_std_regex_find_all(RocketString* pattern,
                                            RocketString* value) {
  const std::string input = stringValue(value);
  const auto found = rocket::safe_regex::findAll(stringValue(pattern), input);
  if (!found.valid) return errorResult(found.error);
  std::vector<std::string> matches;
  matches.reserve(found.matches.size());
  for (const auto match : found.matches)
    matches.push_back(input.substr(match.start, match.end - match.start));
  RocketArray* array = stringArray(matches);
  RocketAggregate* result = okManaged(array);
  rocket_rt_release(array);
  return result;
}

RocketAggregate* rocket_std_regex_replace_all(RocketString* pattern,
                                               RocketString* value,
                                               RocketString* replacement) {
  std::string output;
  std::string error;
  if (!rocket::safe_regex::replaceAll(stringValue(pattern), stringValue(value),
                                      stringValue(replacement), output, error))
    return errorResult(error);
  RocketString* text = makeString(output);
  RocketAggregate* result = okManaged(text);
  rocket_rt_release(text);
  return result;
}

RocketAggregate* rocket_std_crypto_secure_bytes(std::int64_t length) {
  if (length < 0) return errorResult("secure random length must not be negative");
  std::vector<std::uint8_t> random;
  std::string error;
  if (!rocket::platform_crypto::secureRandom(static_cast<std::size_t>(length), random,
                                              error))
    return errorResult(error);
  RocketArray* bytes = rocket_rt_array_new(ROCKET_ELEMENT_CHAR, random.size());
  for (std::size_t index = 0; index < random.size(); ++index)
    rocket_rt_array_set_char(bytes, static_cast<std::int64_t>(index), random[index]);
  RocketAggregate* buffer = byteBuffer(bytes);
  rocket_rt_release(bytes);
  RocketAggregate* result = okManaged(buffer);
  rocket_rt_release(buffer);
  return result;
}

RocketAggregate* rocket_std_crypto_secure_int(std::int64_t minimum,
                                               std::int64_t maximum) {
  std::int64_t value = 0;
  std::string error;
  if (!rocket::platform_crypto::secureInt(minimum, maximum, value, error))
    return errorResult(error);
  return okInt(value);
}

RocketAggregate* rocket_std_crypto_sha256(RocketAggregate* value) {
  std::string digest;
  std::string error;
  if (!rocket::platform_crypto::sha256(byteBufferValue(value), digest, error))
    return errorResult(error);
  RocketString* text = makeString(digest);
  RocketAggregate* result = okManaged(text);
  rocket_rt_release(text);
  return result;
}

RocketAggregate* rocket_std_crypto_hmac_sha256(RocketAggregate* key,
                                                RocketAggregate* value) {
  std::string digest;
  std::string error;
  if (!rocket::platform_crypto::hmacSha256(byteBufferValue(key),
                                           byteBufferValue(value), digest, error))
    return errorResult(error);
  RocketString* text = makeString(digest);
  RocketAggregate* result = okManaged(text);
  rocket_rt_release(text);
  return result;
}

std::uint8_t rocket_std_crypto_constant_time_equal(RocketAggregate* left,
                                                    RocketAggregate* right) {
  return rocket::platform_crypto::constantTimeEqual(byteBufferValue(left),
                                                     byteBufferValue(right));
}

RocketAggregate* rocket_std_crypto_verify_signed_file(RocketString* path) {
  const std::string utf8 = stringValue(path);
#ifdef _WIN32
  const std::wstring wide = utf8ToWide(utf8);
  if (!utf8.empty() && wide.empty()) return errorResult("signed-file path is not valid UTF-8");
  const std::filesystem::path nativePath(wide);
#else
  const std::filesystem::path nativePath = std::filesystem::u8path(utf8);
#endif
  bool trusted = false;
  std::string error;
  if (!rocket::platform_crypto::verifySignedFile(nativePath, trusted, error))
    return errorResult(error);
  return okBool(trusted);
}

RocketAggregate* rocket_std_net_resolve(RocketString* host, RocketString* service) {
  std::vector<std::string> addresses;
  std::string error;
  if (!rocket::platform_net::resolve(stringValue(host), stringValue(service),
                                      addresses, error))
    return errorResult(error);
  RocketArray* values = stringArray(addresses);
  RocketAggregate* result = okManaged(values);
  rocket_rt_release(values);
  return result;
}

RocketAggregate* rocket_std_net_tcp_connect(RocketString* host, std::int64_t port,
                                             std::int64_t timeoutMilliseconds) {
  rocket::platform_net::Socket socket = rocket::platform_net::invalidSocket;
  std::string error;
  if (!rocket::platform_net::connect(stringValue(host), port, timeoutMilliseconds,
                                      socket, error))
    return errorResult(error);
  const std::int64_t token = nextNetworkHandle++;
  networkSockets.emplace(token, NetworkSocketState{socket, false});
  return okInt(token);
}

RocketAggregate* rocket_std_net_tcp_listen(RocketString* address, std::int64_t port,
                                            std::int64_t backlog) {
  rocket::platform_net::Socket socket = rocket::platform_net::invalidSocket;
  std::string error;
  if (!rocket::platform_net::listen(stringValue(address), port, backlog, socket, error))
    return errorResult(error);
  const std::int64_t token = nextNetworkHandle++;
  networkSockets.emplace(token, NetworkSocketState{socket, true});
  return okInt(token);
}

RocketAggregate* rocket_std_net_accept(std::int64_t listener,
                                        std::int64_t timeoutMilliseconds) {
  const auto found = networkSockets.find(listener);
  if (found == networkSockets.end() || !found->second.listener)
    return errorResult("network token is not an open TCP listener");
  rocket::platform_net::Socket client = rocket::platform_net::invalidSocket;
  std::string error;
  if (!rocket::platform_net::accept(found->second.socket, timeoutMilliseconds,
                                     client, error))
    return errorResult(error);
  const std::int64_t token = nextNetworkHandle++;
  networkSockets.emplace(token, NetworkSocketState{client, false});
  return okInt(token);
}

RocketAggregate* rocket_std_net_send(std::int64_t handle, RocketAggregate* buffer,
                                      std::int64_t timeoutMilliseconds) {
  const auto found = networkSockets.find(handle);
  if (found == networkSockets.end() || found->second.listener)
    return errorResult("network token is not an open TCP connection");
  const std::string bytes = byteBufferValue(buffer);
  std::size_t sent = 0;
  std::string error;
  if (!rocket::platform_net::send(found->second.socket, bytes, timeoutMilliseconds,
                                   sent, error))
    return errorResult(error);
  return okInt(static_cast<std::int64_t>(sent));
}

RocketAggregate* rocket_std_net_receive(std::int64_t handle,
                                         std::int64_t maximumBytes,
                                         std::int64_t timeoutMilliseconds) {
  const auto found = networkSockets.find(handle);
  if (found == networkSockets.end() || found->second.listener)
    return errorResult("network token is not an open TCP connection");
  if (maximumBytes < 0)
    return errorResult("TCP receive maximum must not be negative");
  std::string bytes;
  std::string error;
  if (!rocket::platform_net::receive(found->second.socket,
                                      static_cast<std::size_t>(maximumBytes),
                                      timeoutMilliseconds, bytes, error))
    return errorResult(error);
  RocketArray* values = byteArray(bytes);
  RocketAggregate* buffer = byteBuffer(values);
  rocket_rt_release(values);
  RocketAggregate* result = okManaged(buffer);
  rocket_rt_release(buffer);
  return result;
}

RocketAggregate* rocket_std_net_close(std::int64_t handle) {
  const auto found = networkSockets.find(handle);
  if (found == networkSockets.end()) return errorResult("network token is not open");
  std::string error;
  const bool closed = rocket::platform_net::close(found->second.socket, error);
  networkSockets.erase(found);
  if (!closed) return errorResult(error);
  return okBool(true);
}

RocketAggregate* rocket_std_net_cancel(std::int64_t handle) {
  return rocket_std_net_close(handle);
}

RocketAggregate* rocket_std_net_local_port(std::int64_t handle) {
  const auto found = networkSockets.find(handle);
  if (found == networkSockets.end()) return errorResult("network token is not open");
  std::int64_t port = 0;
  std::string error;
  if (!rocket::platform_net::localPort(found->second.socket, port, error))
    return errorResult(error);
  return okInt(port);
}

RocketAggregate* rocket_std_http_request(RocketString* method, RocketString* url,
                                          RocketAggregate* body,
                                          std::int64_t timeoutMilliseconds) {
  rocket::platform_net::HttpResponse response;
  std::string error;
  if (!rocket::platform_net::httpRequest(stringValue(method), stringValue(url),
                                          byteBufferValue(body), timeoutMilliseconds,
                                          response, error))
    return errorResult(error);
  RocketAggregate* value = httpResponse(response.status, response.body);
  RocketAggregate* result = okManaged(value);
  rocket_rt_release(value);
  return result;
}

RocketAggregate* rocket_std_http_read_request(std::int64_t handle,
                                               std::int64_t maximumBytes,
                                               std::int64_t timeoutMilliseconds) {
  const auto found = networkSockets.find(handle);
  if (found == networkSockets.end() || found->second.listener)
    return errorResult("network token is not an open TCP connection");
  std::string method;
  std::string path;
  std::string body;
  std::string error;
  if (!parseHttpRequest(found->second.socket, maximumBytes, timeoutMilliseconds,
                        method, path, body, error))
    return errorResult(error);
  RocketString* methodValue = makeString(method);
  RocketString* pathValue = makeString(path);
  RocketArray* bodyBytes = byteArray(body);
  RocketAggregate* bodyBuffer = byteBuffer(bodyBytes);
  rocket_rt_release(bodyBytes);
  RocketAggregate* request = rocket_rt_aggregate_new(0, 3, 7);
  rocket_rt_aggregate_set_managed(request, 0, methodValue);
  rocket_rt_aggregate_set_managed(request, 1, pathValue);
  rocket_rt_aggregate_set_managed(request, 2, bodyBuffer);
  rocket_rt_release(methodValue);
  rocket_rt_release(pathValue);
  rocket_rt_release(bodyBuffer);
  RocketAggregate* result = okManaged(request);
  rocket_rt_release(request);
  return result;
}

RocketAggregate* rocket_std_http_write_response(std::int64_t handle,
                                                 std::int64_t status,
                                                 RocketString* contentType,
                                                 RocketAggregate* body,
                                                 std::int64_t timeoutMilliseconds) {
  const auto found = networkSockets.find(handle);
  if (found == networkSockets.end() || found->second.listener)
    return errorResult("network token is not an open TCP connection");
  const std::string type = stringValue(contentType);
  const std::string bytes = byteBufferValue(body);
  if (status < 100 || status > 599)
    return errorResult("HTTP response status must be from 100 through 599");
  if (type.empty() || type.find('\r') != std::string::npos ||
      type.find('\n') != std::string::npos)
    return errorResult("HTTP content type must be a non-empty single-line value");
  const std::string response = "HTTP/1.1 " + std::to_string(status) +
      " Rocket\r\nContent-Type: " + type + "\r\nContent-Length: " +
      std::to_string(bytes.size()) + "\r\nConnection: close\r\n\r\n" + bytes;
  std::size_t sent = 0;
  std::string error;
  if (!rocket::platform_net::send(found->second.socket, response,
                                   timeoutMilliseconds, sent, error))
    return errorResult(error);
  return okBool(sent == response.size());
}

RocketAggregate* rocket_std_datetime_format_utc(std::int64_t unixMilliseconds) {
  std::string value;
  std::string error;
  if (!rocket::platform_datetime::formatUtc(unixMilliseconds, value, error))
    return errorResult(error);
  RocketString* text = makeString(value);
  RocketAggregate* result = okManaged(text);
  rocket_rt_release(text);
  return result;
}

RocketAggregate* rocket_std_datetime_parse_utc(RocketString* value) {
  std::int64_t milliseconds = 0;
  std::string error;
  if (!rocket::platform_datetime::parseUtc(stringValue(value), milliseconds, error))
    return errorResult(error);
  return okInt(milliseconds);
}

RocketAggregate* rocket_std_datetime_days_in_month(std::int64_t year,
                                                    std::int64_t month) {
  std::int64_t days = 0;
  std::string error;
  if (!rocket::platform_datetime::daysInMonth(year, month, days, error))
    return errorResult(error);
  return okInt(days);
}

RocketAggregate* rocket_std_datetime_weekday(std::int64_t year,
                                              std::int64_t month,
                                              std::int64_t day) {
  std::int64_t weekday = 0;
  std::string error;
  if (!rocket::platform_datetime::weekday(year, month, day, weekday, error))
    return errorResult(error);
  return okInt(weekday);
}

RocketAggregate* rocket_std_datetime_local_offset_minutes(
    std::int64_t unixMilliseconds) {
  std::int64_t offset = 0;
  std::string error;
  if (!rocket::platform_datetime::localOffsetMinutes(unixMilliseconds, offset, error))
    return errorResult(error);
  return okInt(offset);
}

RocketAggregate* rocket_std_datetime_timezone_name() {
  std::string value;
  std::string error;
  if (!rocket::platform_datetime::timezoneName(value, error))
    return errorResult(error);
  RocketString* text = makeString(value);
  RocketAggregate* result = okManaged(text);
  rocket_rt_release(text);
  return result;
}

RocketAggregate* rocket_std_log_write(RocketString* level, RocketString* message) {
  std::string error;
  const std::string line = logLine(stringValue(level), stringValue(message), error);
  if (!error.empty()) return errorResult(error);
  std::lock_guard<std::mutex> lock(logMutex);
  if (std::fwrite(line.data(), 1, line.size(), stderr) != line.size() ||
      std::fflush(stderr) != 0)
    return errorResult("could not write the log message to standard error");
  return okBool(true);
}

RocketAggregate* rocket_std_log_append(RocketString* path, RocketString* level,
                                        RocketString* message) {
  std::string error;
  const std::string line = logLine(stringValue(level), stringValue(message), error);
  if (!error.empty()) return errorResult(error);
  std::lock_guard<std::mutex> lock(logMutex);
  std::ofstream output(pathValue(path), std::ios::binary | std::ios::app);
  if (!output) return errorResult("could not open the log file for append");
  output.write(line.data(), static_cast<std::streamsize>(line.size()));
  output.flush();
  if (!output) return errorResult("could not append and flush the log message");
  return okBool(true);
}

std::uint8_t rocket_std_cli_has_flag(RocketArray* arguments, RocketString* name) {
  const std::string requested = stringValue(name);
  const std::uint64_t length = rocket_rt_collection_length(arguments);
  for (std::uint64_t index = 0; index < length; ++index) {
    RocketString* argument = rocket_rt_index_string(arguments,
                                                     static_cast<std::int64_t>(index));
    const std::string value = stringValue(argument);
    rocket_rt_release(argument);
    if (value == "--") break;
    if (value == requested) return 1;
  }
  return 0;
}

RocketAggregate* rocket_std_cli_option(RocketArray* arguments, RocketString* name) {
  const std::string requested = stringValue(name);
  if (!requested.starts_with("--") || requested.size() < 3 ||
      requested.find('=') != std::string::npos)
    return errorResult("CLI option name must use --name without '='");
  const std::string prefix = requested + "=";
  const std::uint64_t length = rocket_rt_collection_length(arguments);
  for (std::uint64_t index = 0; index < length; ++index) {
    RocketString* argument = rocket_rt_index_string(arguments,
                                                     static_cast<std::int64_t>(index));
    const std::string value = stringValue(argument);
    rocket_rt_release(argument);
    if (value == "--") break;
    if (value.starts_with(prefix)) return optionalStringResult(true, value.substr(prefix.size()));
    if (value == requested) {
      if (index + 1 >= length) return errorResult("CLI option is missing its value");
      RocketString* following = rocket_rt_index_string(
          arguments, static_cast<std::int64_t>(index + 1));
      const std::string selected = stringValue(following);
      rocket_rt_release(following);
      if (selected == "--") return errorResult("CLI option is missing its value");
      return optionalStringResult(true, selected);
    }
  }
  return optionalStringResult(false, {});
}

RocketArray* rocket_std_cli_positionals(RocketArray* arguments) {
  const std::uint64_t length = rocket_rt_collection_length(arguments);
  std::vector<std::string> values;
  bool afterSeparator = false;
  for (std::uint64_t index = 0; index < length; ++index) {
    RocketString* argument = rocket_rt_index_string(arguments,
                                                     static_cast<std::int64_t>(index));
    std::string value = stringValue(argument);
    rocket_rt_release(argument);
    if (!afterSeparator && value == "--") { afterSeparator = true; continue; }
    if (afterSeparator || value.empty() || value.front() != '-')
      values.push_back(std::move(value));
  }
  return stringArray(values);
}

RocketAggregate* rocket_std_config_get(RocketString* text, RocketString* key) {
  bool found = false;
  std::string value;
  std::string error;
  if (!configValue(stringValue(text), stringValue(key), found, value, error))
    return errorResult(error);
  return optionalStringResult(found, value);
}

RocketAggregate* rocket_std_config_load(RocketString* path, RocketString* key) {
  try {
    std::ifstream input(pathValue(path), std::ios::binary);
    if (!input) return errorResult("could not open configuration file");
    std::ostringstream contents;
    contents << input.rdbuf();
    if (!input.good() && !input.eof()) return errorResult("could not read configuration file");
    bool found = false;
    std::string value;
    std::string error;
    if (!configValue(contents.str(), stringValue(key), found, value, error))
      return errorResult(error);
    return optionalStringResult(found, value);
  } catch (const std::exception& error) {
    return errorResult(error.what());
  }
}

RocketAggregate* rocket_std_compression_xpress_compress(RocketAggregate* value) {
  std::string output;
  std::string error;
  if (!rocket::platform_compression::compressXpress(byteBufferValue(value), output,
                                                     error))
    return errorResult(error);
  RocketArray* bytes = byteArray(output);
  RocketAggregate* buffer = byteBuffer(bytes);
  rocket_rt_release(bytes);
  RocketAggregate* result = okManaged(buffer);
  rocket_rt_release(buffer);
  return result;
}

RocketAggregate* rocket_std_compression_xpress_decompress(RocketAggregate* value) {
  std::string output;
  std::string error;
  if (!rocket::platform_compression::decompressXpress(byteBufferValue(value), output,
                                                       error))
    return errorResult(error);
  RocketArray* bytes = byteArray(output);
  RocketAggregate* buffer = byteBuffer(bytes);
  rocket_rt_release(bytes);
  RocketAggregate* result = okManaged(buffer);
  rocket_rt_release(buffer);
  return result;
}

RocketAggregate* rocket_std_archive_tar_create(RocketString* path,
                                                RocketArray* names,
                                                RocketArray* contents) {
  const std::uint64_t count = rocket_rt_collection_length(names);
  if (count != rocket_rt_collection_length(contents))
    return errorResult("TAR entry names and contents must have equal lengths");
  std::vector<rocket::safe_archive::Entry> entries;
  entries.reserve(static_cast<std::size_t>(count));
  for (std::uint64_t index = 0; index < count; ++index) {
    RocketString* name = rocket_rt_index_string(names, static_cast<std::int64_t>(index));
    auto* content = static_cast<RocketAggregate*>(
        rocket_rt_index_managed(contents, static_cast<std::int64_t>(index)));
    entries.push_back({stringValue(name), byteBufferValue(content)});
    rocket_rt_release(content);
    rocket_rt_release(name);
  }
  std::string error;
  if (!rocket::safe_archive::create(stringValue(path), entries, error))
    return errorResult(error);
  return okBool(true);
}

RocketAggregate* rocket_std_archive_tar_list(RocketString* path) {
  std::vector<std::string> names;
  std::string error;
  if (!rocket::safe_archive::list(stringValue(path), names, error))
    return errorResult(error);
  RocketArray* values = stringArray(names);
  RocketAggregate* result = okManaged(values);
  rocket_rt_release(values);
  return result;
}

RocketAggregate* rocket_std_archive_tar_read(RocketString* path,
                                              RocketString* name) {
  std::string contents;
  std::string error;
  if (!rocket::safe_archive::read(stringValue(path), stringValue(name), contents,
                                   error))
    return errorResult(error);
  RocketArray* bytes = byteArray(contents);
  RocketAggregate* buffer = byteBuffer(bytes);
  rocket_rt_release(bytes);
  RocketAggregate* result = okManaged(buffer);
  rocket_rt_release(buffer);
  return result;
}

RocketAggregate* rocket_std_sqlite_open(RocketString* path) {
  sqlite3* database = nullptr;
  std::string error;
  if (!rocket::platform_sqlite::open(stringValue(path), database, error))
    return errorResult(error);
  const std::int64_t token = nextSqliteHandle++;
  sqliteDatabases.emplace(token, database);
  return okInt(token);
}

RocketAggregate* rocket_std_sqlite_execute(std::int64_t handle, RocketString* sql,
                                            RocketArray* parameters) {
  const auto found = sqliteDatabases.find(handle);
  if (found == sqliteDatabases.end()) return errorResult("SQLite token is not open");
  std::int64_t changes = 0;
  std::string error;
  if (!rocket::platform_sqlite::execute(found->second, stringValue(sql),
                                         stringValues(parameters), changes, error))
    return errorResult(error);
  return okInt(changes);
}

RocketAggregate* rocket_std_sqlite_query(std::int64_t handle, RocketString* sql,
                                          RocketArray* parameters) {
  const auto found = sqliteDatabases.find(handle);
  if (found == sqliteDatabases.end()) return errorResult("SQLite token is not open");
  std::vector<std::vector<std::string>> rows;
  std::string error;
  if (!rocket::platform_sqlite::query(found->second, stringValue(sql),
                                       stringValues(parameters), rows, error))
    return errorResult(error);
  RocketArray* outer = rocket_rt_array_new(ROCKET_ELEMENT_MANAGED, rows.size());
  for (std::size_t index = 0; index < rows.size(); ++index) {
    RocketArray* fields = stringArray(rows[index]);
    rocket_rt_array_set_managed(outer, static_cast<std::int64_t>(index), fields);
    rocket_rt_release(fields);
  }
  RocketAggregate* result = okManaged(outer);
  rocket_rt_release(outer);
  return result;
}

RocketAggregate* rocket_std_sqlite_close(std::int64_t handle) {
  const auto found = sqliteDatabases.find(handle);
  if (found == sqliteDatabases.end()) return errorResult("SQLite token is not open");
  std::string error;
  if (!rocket::platform_sqlite::close(found->second, error)) return errorResult(error);
  sqliteDatabases.erase(found);
  return okBool(true);
}

RocketAggregate* rocket_std_testing_assert(std::uint8_t condition,
                                            RocketString* message) {
  if (condition) return okBool(true);
  const std::string text = stringValue(message);
  return errorResult(text.empty() ? "assertion failed" : text);
}

RocketAggregate* rocket_std_testing_equal_int(std::int64_t expected,
                                               std::int64_t actual,
                                               RocketString* message) {
  if (expected == actual) return okBool(true);
  const std::string prefix = stringValue(message);
  return errorResult((prefix.empty() ? "integer assertion failed" : prefix) +
                     ": expected " + std::to_string(expected) + ", actual " +
                     std::to_string(actual));
}

RocketAggregate* rocket_std_testing_equal_string(RocketString* expected,
                                                  RocketString* actual,
                                                  RocketString* message) {
  const std::string expectedValue = stringValue(expected);
  const std::string actualValue = stringValue(actual);
  if (expectedValue == actualValue) return okBool(true);
  const std::string prefix = stringValue(message);
  return errorResult((prefix.empty() ? "string assertion failed" : prefix) +
                     ": expected '" + expectedValue + "', actual '" + actualValue + "'");
}

RocketAggregate* rocket_std_testing_temp_directory(RocketString* prefix) {
  const std::string requested = stringValue(prefix);
  if (requested.empty() || requested.size() > 32 ||
      !std::all_of(requested.begin(), requested.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '-' || character == '_';
      }))
    return errorResult("test temporary prefix must use 1 through 32 letters, digits, '-' or '_'");
  std::error_code filesystemError;
  const std::filesystem::path temporary = std::filesystem::temp_directory_path(filesystemError);
  if (filesystemError) return errorResult("could not locate the host temporary directory");
  for (int attempt = 0; attempt < 64; ++attempt) {
    std::vector<std::uint8_t> random;
    std::string error;
    if (!rocket::platform_crypto::secureRandom(8, random, error)) return errorResult(error);
    constexpr char digits[] = "0123456789abcdef";
    std::string suffix;
    suffix.reserve(16);
    for (const std::uint8_t byte : random) {
      suffix.push_back(digits[byte >> 4]);
      suffix.push_back(digits[byte & 15]);
    }
    const std::filesystem::path candidate = temporary / (requested + "-" + suffix);
    if (!std::filesystem::create_directory(candidate, filesystemError)) {
      if (!filesystemError) continue;
      return errorResult("could not create a test temporary directory");
    }
    const std::string normalized = pathString(candidate.lexically_normal());
    {
      std::lock_guard<std::mutex> lock(testingMutex);
      testingTemporaryDirectories.insert(normalized);
    }
    return okString(normalized);
  }
  return errorResult("could not allocate a unique test temporary directory");
}

RocketAggregate* rocket_std_testing_fixture_path(RocketString* root,
                                                  RocketString* relative) {
  const std::string rootValue = pathString(pathValue(root).lexically_normal());
  const std::string relativeValue = stringValue(relative);
  {
    std::lock_guard<std::mutex> lock(testingMutex);
    if (!testingTemporaryDirectories.contains(rootValue))
      return errorResult("test fixture root was not created by testing.temp_directory");
  }
  if (!safeTestingRelative(relativeValue))
    return errorResult("test fixture path must be a safe relative path");
  return okString(pathString((pathValue(root) / pathValue(relative)).lexically_normal()));
}

RocketAggregate* rocket_std_testing_cleanup_temp(RocketString* path) {
  const std::string normalized = pathString(pathValue(path).lexically_normal());
  {
    std::lock_guard<std::mutex> lock(testingMutex);
    if (!testingTemporaryDirectories.contains(normalized))
      return errorResult("test temporary path is unknown or already cleaned");
  }
  std::error_code error;
  std::filesystem::remove_all(pathValue(path), error);
  if (error) return errorResult("could not clean the test temporary directory");
  {
    std::lock_guard<std::mutex> lock(testingMutex);
    testingTemporaryDirectories.erase(normalized);
  }
  return okBool(true);
}

RocketAggregate* rocket_std_testing_coverage_hit(RocketString* name) {
  const std::string value = stringValue(name);
  if (value.empty() || value.size() > 256 ||
      !std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '_' || character == '-' ||
               character == '.' || character == ':';
      }))
    return errorResult("coverage point name contains unsupported bytes");
  std::lock_guard<std::mutex> lock(testingMutex);
  auto& count = testingCoverage[value];
  if (count == (std::numeric_limits<std::int64_t>::max)())
    return errorResult("coverage point counter overflowed");
  ++count;
  return okBool(true);
}

RocketAggregate* rocket_std_testing_coverage_write(RocketString* path) {
  std::vector<std::pair<std::string, std::int64_t>> points;
  {
    std::lock_guard<std::mutex> lock(testingMutex);
    points.assign(testingCoverage.begin(), testingCoverage.end());
  }
  std::sort(points.begin(), points.end());
  std::ofstream output(pathValue(path), std::ios::binary | std::ios::trunc);
  if (!output) return errorResult("could not create coverage output file");
  output << "{\"version\":1,\"points\":[";
  for (std::size_t index = 0; index < points.size(); ++index) {
    if (index) output << ',';
    output << "{\"name\":\"" << points[index].first << "\",\"hits\":"
           << points[index].second << '}';
  }
  output << "]}\n";
  output.flush();
  if (!output) return errorResult("could not write coverage output file");
  return okBool(true);
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

RocketAggregate* rocket_std_file_read_binary(RocketString* path) {
  try {
    std::ifstream input(pathValue(path), std::ios::binary);
    if (!input) return errorResult("could not open file for binary reading");
    std::ostringstream contents;
    contents << input.rdbuf();
    const std::string bytes = contents.str();
    RocketArray* array = byteArray(bytes);
    RocketAggregate* buffer = byteBuffer(array);
    rocket_rt_release(array);
    RocketAggregate* result = okManaged(buffer);
    rocket_rt_release(buffer);
    return result;
  } catch (const std::exception& error) { return errorResult(error.what()); }
}

RocketAggregate* writeBinaryFile(RocketString* path, RocketAggregate* buffer,
                                 std::ios::openmode mode) {
  try {
    std::ofstream output(pathValue(path), std::ios::binary | mode);
    if (!output) return errorResult("could not open file for binary writing");
    const std::string bytes = byteBufferValue(buffer);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!output) return errorResult("could not write binary file contents");
    return okBool(true);
  } catch (const std::exception& error) { return errorResult(error.what()); }
}

RocketAggregate* rocket_std_file_write_binary(RocketString* path,
                                              RocketAggregate* buffer) {
  return writeBinaryFile(path, buffer, std::ios::trunc);
}

RocketAggregate* rocket_std_file_append_binary(RocketString* path,
                                               RocketAggregate* buffer) {
  return writeBinaryFile(path, buffer, std::ios::app);
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
  std::vector<std::string> ownedArguments;
  ownedArguments.push_back(stringValue(program));
  if (ownedArguments.front().empty())
    return errorResult("process program is empty or invalid UTF-8");
  const std::uint64_t count = rocket_rt_collection_length(arguments);
  ownedArguments.reserve(static_cast<std::size_t>(count) + 1);
  for (std::uint64_t index = 0; index < count; ++index) {
    RocketString* argument = rocket_rt_index_string(
        arguments, static_cast<std::int64_t>(index));
    ownedArguments.push_back(stringValue(argument));
    rocket_rt_release(argument);
  }
  std::vector<char*> nativeArguments;
  nativeArguments.reserve(ownedArguments.size() + 1);
  for (auto& argument : ownedArguments) nativeArguments.push_back(argument.data());
  nativeArguments.push_back(nullptr);
  const pid_t child = ::fork();
  if (child < 0)
    return errorResult("could not fork process: " +
                       std::string(std::strerror(errno)));
  if (child == 0) {
    ::execvp(nativeArguments[0], nativeArguments.data());
    _exit(errno == ENOENT ? 127 : 126);
  }
  int status = 0;
  pid_t waited = -1;
  do {
    waited = ::waitpid(child, &status, 0);
  } while (waited < 0 && errno == EINTR);
  if (waited < 0)
    return errorResult("could not wait for process: " +
                       std::string(std::strerror(errno)));
  if (WIFEXITED(status)) return okInt(WEXITSTATUS(status));
  if (WIFSIGNALED(status)) return okInt(128 + WTERMSIG(status));
  return errorResult("process ended without an exit status");
#endif
}

void rocket_std_process_set_arguments(std::int32_t count, const char* const* arguments) {
  processArguments.clear();
  processExecutablePath.clear();
  if (!arguments) return;
  if (count > 0 && arguments[0])
    processExecutablePath = runningExecutablePath(arguments[0]);
  for (std::int32_t index = 1; index < count; ++index)
    processArguments.emplace_back(arguments[index] ? arguments[index] : "");
}

RocketArray* rocket_std_process_arguments() { return stringArray(processArguments); }

RocketAggregate* rocket_std_process_executable_path() {
  if (processExecutablePath.empty())
    return errorResult("the executable path is unavailable");
  RocketString* path = makeString(processExecutablePath);
  RocketAggregate* result = okManaged(path);
  rocket_rt_release(path);
  return result;
}

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

namespace {

struct NativeTargetValues {
  const char* alias;
  const char* triple;
  const char* operatingSystem;
  const char* architecture;
  const char* environment;
  const char* debugFormat;
  const char* baselineFeature;
};

constexpr NativeTargetValues nativeTargetValues() {
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
  return {"windows-x64", "x86_64-pc-windows-msvc", "windows", "x64",
          "msvc", "codeview", "sse2"};
#elif defined(__linux__) && defined(__x86_64__)
  return {"linux-x64", "x86_64-unknown-linux-gnu", "linux", "x64", "gnu",
          "dwarf", "sse2"};
#elif defined(__linux__) && (defined(__aarch64__) || defined(__arm64__))
  return {"linux-arm64", "aarch64-unknown-linux-gnu", "linux", "arm64",
          "gnu", "dwarf", "neon"};
#elif defined(__APPLE__) && defined(__MACH__) && (defined(__aarch64__) || defined(__arm64__))
  return {"macos-arm64", "arm64-apple-macosx", "macos", "arm64", "apple",
          "dwarf", "neon"};
#else
  return {"unsupported", "unknown-unknown-unknown", "unknown", "unknown",
          "unknown", "unknown", "unknown"};
#endif
}

} // namespace

RocketString* rocket_std_target_alias() {
  return makeString(nativeTargetValues().alias);
}

RocketString* rocket_std_target_triple() {
  return makeString(nativeTargetValues().triple);
}

RocketString* rocket_std_target_os() {
  return makeString(nativeTargetValues().operatingSystem);
}

RocketString* rocket_std_target_architecture() {
  return makeString(nativeTargetValues().architecture);
}

RocketString* rocket_std_target_environment() {
  return makeString(nativeTargetValues().environment);
}

std::int64_t rocket_std_target_pointer_width() {
  return static_cast<std::int64_t>(sizeof(void*) * 8U);
}

RocketString* rocket_std_target_endianness() { return makeString("little"); }

std::uint8_t rocket_std_target_has_feature(RocketString* name) {
  const std::string feature = stringValue(name);
  const auto values = nativeTargetValues();
  return static_cast<std::uint8_t>(
      feature == "threads" || feature == "dynamic-libraries" ||
      feature == values.debugFormat || feature == values.baselineFeature);
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
