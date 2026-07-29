#pragma once

// This header is included by C++ emitted from the permanently preserved Stage 0
// backend. It implements the public standard-library surface with the Stage 0
// RAII value representation; the production backend uses stdlib.cpp and ABI v1.

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

inline RocketAggregate rocket_stage0_variant(std::uint32_t tag,
                                              std::vector<std::any> fields = {}) {
  return rocket_aggregate(tag, std::move(fields));
}

template <typename T> RocketAggregate rocket_stage0_ok(T value) {
  return rocket_stage0_variant(0, {std::move(value)});
}

inline RocketAggregate rocket_stage0_error(std::string message) {
  return rocket_stage0_variant(1, {std::move(message)});
}

inline std::int64_t rocket_std_string_byte_length(const std::string& value) {
  return static_cast<std::int64_t>(value.size());
}
inline std::string rocket_std_string_concat(const std::string& left,
                                            const std::string& right) {
  return left + right;
}
inline bool rocket_std_string_contains(const std::string& value,
                                       const std::string& needle) {
  return value.find(needle) != std::string::npos;
}
inline bool rocket_std_string_starts_with(const std::string& value,
                                          const std::string& prefix) {
  return value.starts_with(prefix);
}
inline bool rocket_std_string_ends_with(const std::string& value,
                                        const std::string& suffix) {
  return value.ends_with(suffix);
}
inline std::string rocket_std_string_trim(const std::string& value) {
  const std::size_t first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}
inline RocketArray<std::string> rocket_std_string_split(const std::string& value,
                                                        const std::string& delimiter) {
  auto result = std::make_shared<std::vector<std::string>>();
  if (delimiter.empty()) { result->push_back(value); return result; }
  std::size_t start = 0;
  while (true) {
    const std::size_t next = value.find(delimiter, start);
    result->push_back(value.substr(start, next == std::string::npos
                                             ? std::string::npos : next - start));
    if (next == std::string::npos) break;
    start = next + delimiter.size();
  }
  return result;
}
inline char rocket_std_string_byte_at(const std::string& value, std::int64_t index) {
  if (index < 0 || index >= static_cast<std::int64_t>(value.size())) rocket_bounds_error();
  return value[static_cast<std::size_t>(index)];
}
inline std::int64_t rocket_std_string_byte_value_at(const std::string& value,
                                                    std::int64_t index) {
  return static_cast<unsigned char>(rocket_std_string_byte_at(value, index));
}
inline std::string rocket_std_string_slice(const std::string& value, std::int64_t start,
                                           std::int64_t end) {
  if (start < 0 || end < start || end > static_cast<std::int64_t>(value.size()))
    rocket_bounds_error();
  return value.substr(static_cast<std::size_t>(start), static_cast<std::size_t>(end - start));
}
inline RocketAggregate rocket_std_string_parse_int(const std::string& value) {
  std::int64_t parsed = 0;
  const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (value.empty() || result.ec != std::errc{} || result.ptr != value.data() + value.size())
    return rocket_stage0_error("invalid Int text");
  return rocket_stage0_ok(parsed);
}
inline std::string rocket_std_string_from_int(std::int64_t value) {
  return std::to_string(value);
}
using RocketStringBuilder = std::shared_ptr<std::string>;
inline RocketStringBuilder rocket_std_string_builder() {
  return std::make_shared<std::string>();
}
inline RocketUnit rocket_std_string_builder_append(const RocketStringBuilder& builder,
                                                    const std::string& value) {
  builder->append(value);
  return {};
}
inline std::string rocket_std_string_builder_finish(const RocketStringBuilder& builder) {
  return *builder;
}

template <typename T>
inline std::int64_t rocket_std_collections_length(const RocketArray<T>& values) {
  return static_cast<std::int64_t>(values->size());
}
template <typename T>
inline std::int64_t rocket_std_collections_length(const RocketSlice<T>& values) {
  return values.length;
}
template <typename T>
inline std::int64_t rocket_std_collections_capacity(const RocketArray<T>& values) {
  return static_cast<std::int64_t>(values->capacity());
}
template <typename T>
inline RocketArray<T> rocket_std_collections_reserve(const RocketArray<T>& values,
                                                     std::int64_t minimum) {
  if (minimum < 0) rocket_integer_error("Array reserve capacity cannot be negative");
  if (static_cast<std::uint64_t>(minimum) <= values->capacity()) return values;
  RocketArray<T> result = rocket_array_clone(values, values->capacity());
  result->reserve(static_cast<std::size_t>(minimum));
  return result;
}
template <typename T>
inline RocketArray<T> rocket_std_collections_append(const RocketArray<T>& values, T value) {
  RocketArray<T> result = rocket_array_clone(values, values->capacity());
  result->push_back(std::move(value));
  return result;
}
template <typename T>
inline RocketAggregate rocket_std_collections_pop(const RocketArray<T>& values) {
  if (values->empty()) return rocket_stage0_variant(1);
  RocketArray<T> result = rocket_array_clone(values, values->capacity());
  T removed = std::move(result->back());
  result->pop_back();
  RocketAggregate popped = rocket_stage0_variant(0, {result, std::move(removed)});
  return rocket_stage0_variant(0, {std::move(popped)});
}
template <typename T>
inline RocketArray<T> rocket_std_collections_insert(const RocketArray<T>& values,
                                                    std::int64_t index, T value) {
  if (index < 0 || index > static_cast<std::int64_t>(values->size()))
    rocket_bounds_error();
  RocketArray<T> result = rocket_array_clone(values, values->capacity());
  result->insert(result->begin() + index, std::move(value));
  return result;
}
template <typename T>
inline RocketAggregate rocket_std_collections_remove(const RocketArray<T>& values,
                                                      std::int64_t index) {
  if (index < 0 || index >= static_cast<std::int64_t>(values->size()))
    rocket_bounds_error();
  RocketArray<T> result = rocket_array_clone(values, values->capacity());
  T removed = std::move((*result)[static_cast<std::size_t>(index)]);
  result->erase(result->begin() + index);
  return rocket_stage0_variant(0, {std::move(result), std::move(removed)});
}
template <typename T>
inline RocketArray<T> rocket_std_collections_clear(const RocketArray<T>& values) {
  auto result = std::make_shared<std::vector<T>>();
  result->reserve(values->capacity());
  return result;
}
template <typename K, typename V>
inline RocketAggregate rocket_std_collections_map_from_arrays(const RocketArray<K>& keys,
                                                              const RocketArray<V>& values) {
  if (keys->size() != values->size()) rocket_integer_error("Map key/value length mismatch");
  auto uniqueKeys = std::make_shared<std::vector<K>>();
  auto uniqueValues = std::make_shared<std::vector<V>>();
  uniqueKeys->reserve(keys->size());
  uniqueValues->reserve(values->size());
  for (std::size_t index = 0; index < keys->size(); ++index) {
    if (std::find(uniqueKeys->begin(), uniqueKeys->end(), (*keys)[index]) ==
        uniqueKeys->end()) {
      uniqueKeys->push_back((*keys)[index]);
      uniqueValues->push_back((*values)[index]);
    }
  }
  return rocket_stage0_variant(0, {std::move(uniqueKeys), std::move(uniqueValues)});
}
template <typename K, typename V>
inline std::int64_t rocket_std_collections_map_length(const RocketAggregate& map) {
  return static_cast<std::int64_t>(rocket_field<RocketArray<K>>(map, 0)->size());
}
template <typename K, typename V>
inline RocketAggregate rocket_std_collections_map_find(const RocketAggregate& map,
                                                       const K& key) {
  const auto keys = rocket_field<RocketArray<K>>(map, 0);
  const auto found = std::find(keys->begin(), keys->end(), key);
  if (found == keys->end()) return rocket_stage0_variant(1);
  return rocket_stage0_variant(0, {static_cast<std::int64_t>(found - keys->begin())});
}
template <typename K, typename V>
inline RocketAggregate rocket_std_collections_map_get(const RocketAggregate& map,
                                                      const K& key) {
  const auto keys = rocket_field<RocketArray<K>>(map, 0);
  const auto found = std::find(keys->begin(), keys->end(), key);
  if (found == keys->end()) return rocket_stage0_variant(1);
  const auto values = rocket_field<RocketArray<V>>(map, 1);
  return rocket_stage0_variant(0, {(*values)[static_cast<std::size_t>(found - keys->begin())]});
}
template <typename K, typename V>
inline RocketArray<K> rocket_std_collections_map_keys(const RocketAggregate& map) {
  return rocket_field<RocketArray<K>>(map, 0);
}
template <typename K, typename V>
inline RocketArray<V> rocket_std_collections_map_values(const RocketAggregate& map) {
  return rocket_field<RocketArray<V>>(map, 1);
}
template <typename T>
inline RocketAggregate rocket_std_collections_set_from_array(const RocketArray<T>& values) {
  auto unique = std::make_shared<std::vector<T>>();
  unique->reserve(values->size());
  for (const T& value : *values)
    if (std::find(unique->begin(), unique->end(), value) == unique->end())
      unique->push_back(value);
  return rocket_stage0_variant(0, {std::move(unique)});
}
template <typename T>
inline bool rocket_std_collections_set_contains(const RocketAggregate& set,
                                                const T& value) {
  const auto values = rocket_field<RocketArray<T>>(set, 0);
  return std::find(values->begin(), values->end(), value) != values->end();
}
template <typename T>
inline RocketArray<T> rocket_std_collections_set_values(const RocketAggregate& set) {
  return rocket_field<RocketArray<T>>(set, 0);
}
inline std::uint64_t rocket_stage0_hash_bytes(const unsigned char* bytes, std::size_t length) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (std::size_t index = 0; index < length; ++index) {
    hash ^= bytes[index];
    hash *= 1099511628211ULL;
  }
  return hash & 0x7fffffffffffffffULL;
}
inline std::int64_t rocket_std_collections_hash(std::int64_t value) {
  return static_cast<std::int64_t>(rocket_stage0_hash_bytes(
      reinterpret_cast<const unsigned char*>(&value), sizeof(value)));
}
inline std::int64_t rocket_std_collections_hash(bool value) {
  const unsigned char byte = value ? 1 : 0;
  return static_cast<std::int64_t>(rocket_stage0_hash_bytes(&byte, 1));
}
inline std::int64_t rocket_std_collections_hash(char value) {
  const unsigned char byte = static_cast<unsigned char>(value);
  return static_cast<std::int64_t>(rocket_stage0_hash_bytes(&byte, 1));
}
inline std::int64_t rocket_std_collections_hash(const std::string& value) {
  return static_cast<std::int64_t>(rocket_stage0_hash_bytes(
      reinterpret_cast<const unsigned char*>(value.data()), value.size()));
}
template <typename T>
inline bool rocket_std_collections_contains(const RocketArray<T>& values, const T& value) {
  return std::find(values->begin(), values->end(), value) != values->end();
}
template <typename T>
inline RocketAggregate rocket_std_collections_find(const RocketArray<T>& values,
                                                   const T& value) {
  const auto found = std::find(values->begin(), values->end(), value);
  if (found == values->end()) return rocket_stage0_variant(1);
  return rocket_stage0_variant(0, {static_cast<std::int64_t>(found - values->begin())});
}
template <typename T>
inline RocketArray<T> rocket_std_collections_filter_equal(const RocketArray<T>& values,
                                                          const T& wanted) {
  auto result = std::make_shared<std::vector<T>>();
  for (const T& value : *values)
    if (value == wanted) result->push_back(value);
  return result;
}
inline RocketArray<std::int64_t> rocket_std_collections_sort_int(
    const RocketArray<std::int64_t>& values) {
  auto result = rocket_array_clone(values, values->capacity());
  std::sort(result->begin(), result->end());
  return result;
}
inline RocketArray<double> rocket_std_collections_sort_float(
    const RocketArray<double>& values) {
  auto result = rocket_array_clone(values, values->capacity());
  std::stable_sort(result->begin(), result->end(), [](double left, double right) {
    if (std::isnan(left)) return false;
    if (std::isnan(right)) return true;
    return left < right;
  });
  return result;
}
inline RocketArray<char> rocket_std_collections_sort_char(const RocketArray<char>& values) {
  auto result = rocket_array_clone(values, values->capacity());
  std::sort(result->begin(), result->end());
  return result;
}
inline RocketArray<std::string> rocket_std_collections_sort_string(
    const RocketArray<std::string>& values) {
  auto result = rocket_array_clone(values, values->capacity());
  std::sort(result->begin(), result->end());
  return result;
}
template <typename T>
inline RocketArray<std::int64_t> rocket_std_collections_map_hash(
    const RocketArray<T>& values) {
  auto result = std::make_shared<std::vector<std::int64_t>>();
  result->reserve(values->size());
  for (const T& value : *values) result->push_back(rocket_std_collections_hash(value));
  return result;
}
inline std::int64_t rocket_std_collections_fold_sum_int(
    const RocketArray<std::int64_t>& values) {
  std::int64_t result = 0;
  for (std::int64_t value : *values) result = rocket_int_add(result, value);
  return result;
}
inline double rocket_std_collections_fold_sum_float(const RocketArray<double>& values) {
  double result = 0.0;
  for (double value : *values) result += value;
  return result;
}
template <typename T>
inline RocketArray<T> rocket_std_collections_reverse(const RocketArray<T>& values) {
  auto result = std::make_shared<std::vector<T>>(values->rbegin(), values->rend());
  return result;
}
template <typename T>
inline RocketArray<T> rocket_std_collections_concat(const RocketArray<T>& left,
                                                    const RocketArray<T>& right) {
  auto result = std::make_shared<std::vector<T>>();
  result->reserve(left->size() + right->size());
  result->insert(result->end(), left->begin(), left->end());
  result->insert(result->end(), right->begin(), right->end());
  return result;
}
inline std::string rocket_std_collections_join(const RocketArray<std::string>& values,
                                               const std::string& separator) {
  std::string result;
  for (std::size_t index = 0; index < values->size(); ++index) {
    if (index) result += separator;
    result += (*values)[index];
  }
  return result;
}

inline std::filesystem::path rocket_stage0_path(const std::string& value) {
  const std::u8string utf8(reinterpret_cast<const char8_t*>(value.data()), value.size());
  return std::filesystem::path(utf8);
}
inline std::string rocket_stage0_path_string(const std::filesystem::path& value) {
  const auto utf8 = value.generic_u8string();
  return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}
inline RocketAggregate rocket_std_file_read_text(const std::string& path) {
  try {
    std::ifstream input(rocket_stage0_path(path), std::ios::binary);
    if (!input) return rocket_stage0_error("could not open file for reading");
    std::ostringstream contents;
    contents << input.rdbuf();
    return rocket_stage0_ok(contents.str());
  } catch (const std::exception& error) { return rocket_stage0_error(error.what()); }
}
inline RocketAggregate rocket_stage0_write_file(const std::string& path,
                                                const std::string& contents,
                                                std::ios::openmode mode) {
  try {
    std::ofstream output(rocket_stage0_path(path), std::ios::binary | mode);
    if (!output) return rocket_stage0_error("could not open file for writing");
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!output) return rocket_stage0_error("could not write file contents");
    return rocket_stage0_ok(true);
  } catch (const std::exception& error) { return rocket_stage0_error(error.what()); }
}
inline RocketAggregate rocket_std_file_write_text(const std::string& path,
                                                   const std::string& contents) {
  return rocket_stage0_write_file(path, contents, std::ios::trunc);
}
inline RocketAggregate rocket_std_file_append_text(const std::string& path,
                                                    const std::string& contents) {
  return rocket_stage0_write_file(path, contents, std::ios::app);
}
inline bool rocket_std_file_exists(const std::string& path) {
  std::error_code error;
  return std::filesystem::exists(rocket_stage0_path(path), error) && !error;
}
inline RocketAggregate rocket_std_file_remove(const std::string& path) {
  try { return rocket_stage0_ok(std::filesystem::remove(rocket_stage0_path(path))); }
  catch (const std::exception& error) { return rocket_stage0_error(error.what()); }
}
inline RocketAggregate rocket_std_file_list(const std::string& path) {
  try {
    auto entries = std::make_shared<std::vector<std::string>>();
    for (const auto& entry : std::filesystem::directory_iterator(rocket_stage0_path(path)))
      entries->push_back(rocket_stage0_path_string(entry.path().filename()));
    std::sort(entries->begin(), entries->end());
    return rocket_stage0_ok(entries);
  } catch (const std::exception& error) { return rocket_stage0_error(error.what()); }
}
inline RocketAggregate rocket_std_file_create_directory(const std::string& path) {
  try {
    std::error_code error;
    const bool created = std::filesystem::create_directories(rocket_stage0_path(path), error);
    if (error) return rocket_stage0_error(error.message());
    return rocket_stage0_ok(created);
  } catch (const std::exception& error) { return rocket_stage0_error(error.what()); }
}

inline std::string rocket_std_path_join(const std::string& left, const std::string& right) {
  return rocket_stage0_path_string(rocket_stage0_path(left) / rocket_stage0_path(right));
}
inline std::string rocket_std_path_basename(const std::string& path) {
  return rocket_stage0_path_string(rocket_stage0_path(path).filename());
}
inline std::string rocket_std_path_extension(const std::string& path) {
  return rocket_stage0_path_string(rocket_stage0_path(path).extension());
}
inline std::string rocket_std_path_normalize(const std::string& path) {
  return rocket_stage0_path_string(rocket_stage0_path(path).lexically_normal());
}

inline void rocket_stage0_append_utf8(std::string& output, std::uint32_t codepoint) {
  if (codepoint <= 0x7f) output.push_back(static_cast<char>(codepoint));
  else if (codepoint <= 0x7ff) {
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

class RocketStage0JsonParser {
public:
  explicit RocketStage0JsonParser(std::string_view input) : input_(input) {}
  RocketAggregate parse() {
    skip();
    RocketAggregate result = value(0);
    skip();
    if (result && index_ != input_.size()) fail("unexpected characters after JSON value");
    return error_.empty() ? result : RocketAggregate{};
  }
  const std::string& error() const { return error_; }

private:
  void skip() {
    while (index_ < input_.size() &&
           (input_[index_] == ' ' || input_[index_] == '\t' ||
            input_[index_] == '\r' || input_[index_] == '\n')) ++index_;
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
    std::uint32_t result = 0;
    for (int digit = 0; digit < 4; ++digit) {
      if (index_ >= input_.size()) { valid = false; return 0; }
      const char character = input_[index_++];
      result <<= 4;
      if (character >= '0' && character <= '9') result |= character - '0';
      else if (character >= 'a' && character <= 'f') result |= character - 'a' + 10;
      else if (character >= 'A' && character <= 'F') result |= character - 'A' + 10;
      else { valid = false; return 0; }
    }
    return result;
  }
  bool string(std::string& output) {
    if (index_ >= input_.size() || input_[index_] != '"') return false;
    ++index_;
    while (index_ < input_.size()) {
      const unsigned char character = static_cast<unsigned char>(input_[index_++]);
      if (character == '"') return true;
      if (character < 0x20) { fail("control character in JSON string"); return false; }
      if (character != '\\') { output.push_back(static_cast<char>(character)); continue; }
      if (index_ >= input_.size()) { fail("unterminated JSON escape"); return false; }
      const char escaped = input_[index_++];
      if (escaped == '"' || escaped == '\\' || escaped == '/') output.push_back(escaped);
      else if (escaped == 'b') output.push_back('\b');
      else if (escaped == 'f') output.push_back('\f');
      else if (escaped == 'n') output.push_back('\n');
      else if (escaped == 'r') output.push_back('\r');
      else if (escaped == 't') output.push_back('\t');
      else if (escaped == 'u') {
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
        rocket_stage0_append_utf8(output, codepoint);
      } else { fail("unknown JSON escape"); return false; }
    }
    fail("unterminated JSON string");
    return false;
  }
  RocketAggregate array(std::size_t depth) {
    ++index_; skip();
    auto values = std::make_shared<std::vector<RocketAggregate>>();
    if (index_ < input_.size() && input_[index_] == ']') ++index_;
    else while (true) {
      skip();
      RocketAggregate item = value(depth + 1);
      if (!item) return {};
      values->push_back(item); skip();
      if (index_ < input_.size() && input_[index_] == ',') { ++index_; continue; }
      if (index_ < input_.size() && input_[index_] == ']') { ++index_; break; }
      fail("expected ',' or ']' in JSON array"); return {};
    }
    return rocket_stage0_variant(5, {values});
  }
  RocketAggregate object(std::size_t depth) {
    ++index_; skip();
    auto fields = std::make_shared<std::vector<RocketAggregate>>();
    if (index_ < input_.size() && input_[index_] == '}') ++index_;
    else while (true) {
      skip(); std::string key;
      if (!string(key)) { if (error_.empty()) fail("expected string key in JSON object"); return {}; }
      skip();
      if (index_ >= input_.size() || input_[index_] != ':') {
        fail("expected ':' after JSON object key"); return {};
      }
      ++index_; skip();
      RocketAggregate item = value(depth + 1);
      if (!item) return {};
      fields->push_back(rocket_stage0_variant(0, {key, item})); skip();
      if (index_ < input_.size() && input_[index_] == ',') { ++index_; continue; }
      if (index_ < input_.size() && input_[index_] == '}') { ++index_; break; }
      fail("expected ',' or '}' in JSON object"); return {};
    }
    return rocket_stage0_variant(6, {fields});
  }
  RocketAggregate number() {
    const std::size_t start = index_;
    if (index_ < input_.size() && input_[index_] == '-') ++index_;
    if (index_ >= input_.size()) { fail("incomplete JSON number"); return {}; }
    if (input_[index_] == '0') ++index_;
    else if (input_[index_] >= '1' && input_[index_] <= '9')
      while (index_ < input_.size() && input_[index_] >= '0' && input_[index_] <= '9') ++index_;
    else { fail("invalid JSON number"); return {}; }
    bool decimal = false;
    if (index_ < input_.size() && input_[index_] == '.') {
      decimal = true; ++index_; const std::size_t digits = index_;
      while (index_ < input_.size() && input_[index_] >= '0' && input_[index_] <= '9') ++index_;
      if (digits == index_) { fail("JSON fraction requires digits"); return {}; }
    }
    if (index_ < input_.size() && (input_[index_] == 'e' || input_[index_] == 'E')) {
      decimal = true; ++index_;
      if (index_ < input_.size() && (input_[index_] == '+' || input_[index_] == '-')) ++index_;
      const std::size_t digits = index_;
      while (index_ < input_.size() && input_[index_] >= '0' && input_[index_] <= '9') ++index_;
      if (digits == index_) { fail("JSON exponent requires digits"); return {}; }
    }
    const std::string spelling(input_.substr(start, index_ - start));
    if (!decimal) {
      std::int64_t integer = 0;
      const auto parsed = std::from_chars(spelling.data(), spelling.data() + spelling.size(), integer);
      if (parsed.ec == std::errc{} && parsed.ptr == spelling.data() + spelling.size())
        return rocket_stage0_variant(2, {integer});
    }
    char* end = nullptr;
    const double parsed = std::strtod(spelling.c_str(), &end);
    if (!end || *end != '\0' || !std::isfinite(parsed)) {
      fail("JSON number is outside the supported range"); return {};
    }
    return rocket_stage0_variant(3, {parsed});
  }
  RocketAggregate value(std::size_t depth) {
    if (depth > 256) { fail("JSON nesting exceeds 256 levels"); return {}; }
    skip();
    if (consume("null")) return rocket_stage0_variant(0);
    if (consume("true")) return rocket_stage0_variant(1, {true});
    if (consume("false")) return rocket_stage0_variant(1, {false});
    if (index_ < input_.size() && input_[index_] == '"') {
      std::string text;
      if (!string(text)) return {};
      return rocket_stage0_variant(4, {std::move(text)});
    }
    if (index_ < input_.size() && input_[index_] == '[') return array(depth);
    if (index_ < input_.size() && input_[index_] == '{') return object(depth);
    if (index_ < input_.size() && (input_[index_] == '-' ||
        (input_[index_] >= '0' && input_[index_] <= '9'))) return number();
    fail("expected JSON value"); return {};
  }
  std::string_view input_;
  std::size_t index_{};
  std::string error_;
};

inline void rocket_stage0_json_string(std::string& output, const std::string& value) {
  output.push_back('"');
  static constexpr char hex[] = "0123456789abcdef";
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
      output.push_back(hex[character >> 4]); output.push_back(hex[character & 15]);
    } else output.push_back(static_cast<char>(character));
  }
  output.push_back('"');
}
inline bool rocket_stage0_stringify_json(const RocketAggregate& value, std::string& output,
                                         std::size_t depth = 0) {
  if (!value || depth > 256) return false;
  if (value->tag == 0) { output += "null"; return true; }
  if (value->tag == 1) { output += std::any_cast<bool>(value->fields.at(0)) ? "true" : "false"; return true; }
  if (value->tag == 2) { output += std::to_string(std::any_cast<std::int64_t>(value->fields.at(0))); return true; }
  if (value->tag == 3) {
    std::ostringstream stream; stream << std::setprecision(17) << std::any_cast<double>(value->fields.at(0));
    output += stream.str(); return true;
  }
  if (value->tag == 4) { rocket_stage0_json_string(output, std::any_cast<std::string>(value->fields.at(0))); return true; }
  if (value->tag == 5) {
    auto values = std::any_cast<RocketArray<RocketAggregate>>(value->fields.at(0));
    output.push_back('[');
    for (std::size_t index = 0; index < values->size(); ++index) {
      if (index) output.push_back(',');
      if (!rocket_stage0_stringify_json((*values)[index], output, depth + 1)) return false;
    }
    output.push_back(']'); return true;
  }
  if (value->tag == 6) {
    auto fields = std::any_cast<RocketArray<RocketAggregate>>(value->fields.at(0));
    output.push_back('{');
    for (std::size_t index = 0; index < fields->size(); ++index) {
      if (index) output.push_back(',');
      rocket_stage0_json_string(output, std::any_cast<std::string>((*fields)[index]->fields.at(0)));
      output.push_back(':');
      if (!rocket_stage0_stringify_json(
              std::any_cast<RocketAggregate>((*fields)[index]->fields.at(1)), output, depth + 1)) return false;
    }
    output.push_back('}'); return true;
  }
  return false;
}
inline RocketAggregate rocket_std_json_parse(const std::string& text) {
  RocketStage0JsonParser parser(text);
  RocketAggregate value = parser.parse();
  return value ? rocket_stage0_ok(value) : rocket_stage0_error(parser.error());
}
inline std::string rocket_std_json_stringify(const RocketAggregate& value) {
  std::string result;
  return rocket_stage0_stringify_json(value, result) ? result : "null";
}

inline RocketAggregate rocket_std_csv_parse(const std::string& text) {
  auto rows = std::make_shared<std::vector<RocketArray<std::string>>>();
  if (text.empty()) return rocket_stage0_ok(rows);
  auto row = std::make_shared<std::vector<std::string>>();
  std::string field;
  bool quoted = false;
  for (std::size_t index = 0; index <= text.size(); ++index) {
    const char character = index < text.size() ? text[index] : '\n';
    if (quoted) {
      if (character == '"') {
        if (index + 1 < text.size() && text[index + 1] == '"') { field.push_back('"'); ++index; }
        else quoted = false;
      } else field.push_back(character);
      continue;
    }
    if (character == '"' && field.empty()) { quoted = true; continue; }
    if (character == ',') { row->push_back(std::move(field)); field.clear(); continue; }
    if (character == '\r' && index + 1 < text.size() && text[index + 1] == '\n') continue;
    if (character == '\n') {
      row->push_back(std::move(field)); field.clear(); rows->push_back(row);
      row = std::make_shared<std::vector<std::string>>(); continue;
    }
    field.push_back(character);
  }
  if (quoted) return rocket_stage0_error("unterminated quoted CSV field");
  return rocket_stage0_ok(rows);
}
inline std::string rocket_std_csv_encode(const RocketArray<RocketArray<std::string>>& rows) {
  std::string output;
  for (std::size_t row = 0; row < rows->size(); ++row) {
    if (row) output += "\r\n";
    for (std::size_t column = 0; column < (*rows)[row]->size(); ++column) {
      if (column) output.push_back(',');
      const std::string& field = (*(*rows)[row])[column];
      const bool quote = field.find_first_of(",\"\r\n") != std::string::npos;
      if (quote) output.push_back('"');
      for (const char character : field) {
        if (character == '"') output += "\"\""; else output.push_back(character);
      }
      if (quote) output.push_back('"');
    }
  }
  return output;
}

inline std::uint64_t rocket_stage0_random_state = 0x4d595df4d0f33173ULL;
inline std::uint64_t rocket_stage0_next_random() {
  std::uint64_t value = rocket_stage0_random_state;
  value ^= value >> 12; value ^= value << 25; value ^= value >> 27;
  rocket_stage0_random_state = value;
  return value * 2685821657736338717ULL;
}
inline RocketUnit rocket_std_random_seed(std::int64_t seed) {
  rocket_stage0_random_state = static_cast<std::uint64_t>(seed);
  if (!rocket_stage0_random_state) rocket_stage0_random_state = 0x4d595df4d0f33173ULL;
  return {};
}
inline std::int64_t rocket_std_random_int(std::int64_t minimum, std::int64_t maximum) {
  if (minimum >= maximum) rocket_integer_error("random.int requires minimum < maximum");
  const std::uint64_t range = static_cast<std::uint64_t>(maximum) - static_cast<std::uint64_t>(minimum);
  const std::uint64_t threshold = (~range + 1) % range;
  std::uint64_t value;
  do { value = rocket_stage0_next_random(); } while (value < threshold);
  return static_cast<std::int64_t>(static_cast<std::uint64_t>(minimum) + value % range);
}
inline double rocket_std_random_float() {
  return static_cast<double>(rocket_stage0_next_random() >> 11) * (1.0 / 9007199254740992.0);
}

#ifdef _WIN32
inline std::wstring rocket_stage0_wide(const std::string& value) {
  if (value.empty()) return {};
  const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0);
  if (length <= 0) return {};
  std::wstring result(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                      static_cast<int>(value.size()), result.data(), length);
  return result;
}
inline std::string rocket_stage0_utf8(const std::wstring& value) {
  if (value.empty()) return {};
  const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0,
                                         nullptr, nullptr);
  if (length <= 0) return {};
  std::string result(static_cast<std::size_t>(length), '\0');
  WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                      static_cast<int>(value.size()), result.data(), length, nullptr, nullptr);
  return result;
}
inline std::wstring rocket_stage0_quote(const std::wstring& argument) {
  if (argument.empty()) return L"\"\"";
  if (argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) return argument;
  std::wstring result = L"\""; std::size_t backslashes = 0;
  for (const wchar_t character : argument) {
    if (character == L'\\') ++backslashes;
    else if (character == L'\"') {
      result.append(backslashes * 2 + 1, L'\\'); result.push_back(L'\"'); backslashes = 0;
    } else { result.append(backslashes, L'\\'); backslashes = 0; result.push_back(character); }
  }
  result.append(backslashes * 2, L'\\'); result.push_back(L'\"'); return result;
}
#endif
inline RocketAggregate rocket_std_process_run(const std::string& program,
                                               const RocketArray<std::string>& arguments) {
#ifdef _WIN32
  const std::wstring executable = rocket_stage0_wide(program);
  if (executable.empty()) return rocket_stage0_error("process program is empty or invalid UTF-8");
  std::wstring command = rocket_stage0_quote(executable);
  for (const auto& argument : *arguments)
    command += L" " + rocket_stage0_quote(rocket_stage0_wide(argument));
  std::vector<wchar_t> mutableCommand(command.begin(), command.end()); mutableCommand.push_back(L'\0');
  STARTUPINFOW startup{}; startup.cb = sizeof(startup); PROCESS_INFORMATION process{};
  if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, 0,
                      nullptr, nullptr, &startup, &process))
    return rocket_stage0_error("could not start process (Windows error " +
                               std::to_string(GetLastError()) + ")");
  WaitForSingleObject(process.hProcess, INFINITE); DWORD code = 1;
  GetExitCodeProcess(process.hProcess, &code); CloseHandle(process.hThread); CloseHandle(process.hProcess);
  return rocket_stage0_ok(static_cast<std::int64_t>(code));
#else
  (void)program; (void)arguments;
  return rocket_stage0_error("process.run is only implemented on Windows x64");
#endif
}

inline std::vector<std::string> rocket_stage0_process_arguments;
inline std::string rocket_stage0_process_executable_path;
inline void rocket_std_process_set_arguments(int count, char** arguments) {
  rocket_stage0_process_arguments.clear();
  rocket_stage0_process_executable_path.clear();
  if (count > 0 && arguments && arguments[0]) {
    try {
      rocket_stage0_process_executable_path = rocket_stage0_path_string(
          std::filesystem::absolute(std::filesystem::path(arguments[0])).lexically_normal());
    } catch (const std::exception&) {
      rocket_stage0_process_executable_path = arguments[0];
    }
  }
  for (int index = 1; index < count; ++index)
    rocket_stage0_process_arguments.emplace_back(arguments[index] ? arguments[index] : "");
}
inline RocketArray<std::string> rocket_std_process_arguments() {
  return std::make_shared<std::vector<std::string>>(rocket_stage0_process_arguments);
}
inline RocketAggregate rocket_std_process_executable_path() {
  if (rocket_stage0_process_executable_path.empty())
    return rocket_stage0_error("the executable path is unavailable");
  return rocket_stage0_ok(rocket_stage0_process_executable_path);
}
inline RocketAggregate rocket_std_process_environment(const std::string& name) {
#ifdef _WIN32
  const std::wstring variable = rocket_stage0_wide(name);
  SetLastError(ERROR_SUCCESS);
  const DWORD required = GetEnvironmentVariableW(variable.c_str(), nullptr, 0);
  if (required == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND) return rocket_stage0_variant(1);
  std::wstring value(required, L'\0');
  const DWORD written = GetEnvironmentVariableW(variable.c_str(), value.data(), required);
  if (written >= required) return rocket_stage0_variant(1);
  value.resize(written); return rocket_stage0_variant(0, {rocket_stage0_utf8(value)});
#else
  const char* value = std::getenv(name.c_str());
  return value ? rocket_stage0_variant(0, {std::string(value)}) : rocket_stage0_variant(1);
#endif
}
inline RocketAggregate rocket_std_process_working_directory() {
  try { return rocket_stage0_ok(rocket_stage0_path_string(std::filesystem::current_path())); }
  catch (const std::exception& error) { return rocket_stage0_error(error.what()); }
}
inline std::int64_t rocket_std_time_unix_milliseconds() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
}
inline std::int64_t rocket_std_time_monotonic_milliseconds() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline RocketUnit rocket_std_time_sleep_milliseconds(std::int64_t milliseconds) {
  if (milliseconds < 0) rocket_integer_error("sleep duration cannot be negative");
  std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); return {};
}
