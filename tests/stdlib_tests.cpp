#include "runtime.h"
#include "test_support.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace {

RocketString* string(const std::string& value) {
  return rocket_rt_string_new(reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
}

std::string value(const RocketString* text) {
  return std::string(reinterpret_cast<const char*>(rocket_rt_string_bytes(text)),
                     static_cast<std::size_t>(rocket_rt_string_byte_length(text)));
}

} // namespace

int main() {
  int failures = 0;
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "standard-library test starts without live objects", failures);

  RocketString* rocket = string("Rocket");
  RocketString* suffix = string(" Library");
  RocketString* joined = rocket_std_string_concat(rocket, suffix);
  rocket::test::expect(value(joined) == "Rocket Library" &&
                           rocket_std_string_byte_length(joined) == 14,
                       "string construction and byte length are deterministic", failures);
  RocketString* needle = string("Library");
  rocket::test::expect(rocket_std_string_contains(joined, needle) == 1 &&
                           rocket_std_string_ends_with(joined, needle) == 1,
                       "string search helpers operate on UTF-8 byte sequences", failures);
  RocketString* delimiter = string(" ");
  RocketArray* pieces = rocket_std_string_split(joined, delimiter);
  RocketArray* reversed = rocket_std_collections_reverse(pieces);
  RocketString* dash = string("-");
  RocketString* reverseJoined = rocket_std_collections_join(reversed, dash);
  rocket::test::expect(value(reverseJoined) == "Library-Rocket",
                       "collection reverse and string join preserve typed elements", failures);
  rocket_rt_release(reverseJoined);
  rocket_rt_release(dash);
  rocket_rt_release(reversed);
  rocket_rt_release(pieces);
  rocket_rt_release(delimiter);
  rocket_rt_release(needle);
  rocket_rt_release(joined);
  rocket_rt_release(suffix);
  rocket_rt_release(rocket);

  RocketString* integerText = string("9223372036854775807");
  RocketAggregate* parsedInteger = rocket_std_string_parse_int(integerText);
  rocket::test::expect(rocket_rt_aggregate_tag(parsedInteger) == 0 &&
                           rocket_rt_aggregate_get_int(parsedInteger, 0) == INT64_MAX,
                       "string.parse_int returns Result[Int, String]", failures);
  rocket_rt_release(parsedInteger);
  rocket_rt_release(integerText);

  RocketString* jsonText = string("{\"name\":\"Rocket\",\"values\":[1,2],\"ok\":true}");
  RocketAggregate* parsedJson = rocket_std_json_parse(jsonText);
  rocket::test::expect(rocket_rt_aggregate_tag(parsedJson) == 0,
                       "valid JSON returns Ok(Json)", failures);
  auto* json = static_cast<RocketAggregate*>(rocket_rt_aggregate_get_managed(parsedJson, 0));
  RocketString* encodedJson = rocket_std_json_stringify(json);
  rocket::test::expect(value(encodedJson) ==
                           "{\"name\":\"Rocket\",\"values\":[1,2],\"ok\":true}",
                       "JSON parse/stringify round-trips ordered values", failures);
  rocket_rt_release(encodedJson);
  rocket_rt_release(json);
  rocket_rt_release(parsedJson);
  rocket_rt_release(jsonText);
  RocketString* badJsonText = string("{]");
  RocketAggregate* badJson = rocket_std_json_parse(badJsonText);
  rocket::test::expect(rocket_rt_aggregate_tag(badJson) == 1,
                       "invalid JSON returns Err(String)", failures);
  rocket_rt_release(badJson);
  rocket_rt_release(badJsonText);

  RocketString* csvText = string("name,value\nrocket,7");
  RocketAggregate* parsedCsv = rocket_std_csv_parse(csvText);
  rocket::test::expect(rocket_rt_aggregate_tag(parsedCsv) == 0,
                       "valid CSV returns nested string rows", failures);
  auto* rows = static_cast<RocketArray*>(rocket_rt_aggregate_get_managed(parsedCsv, 0));
  RocketString* encodedCsv = rocket_std_csv_encode(rows);
  rocket::test::expect(value(encodedCsv) == "name,value\r\nrocket,7",
                       "CSV encoding uses deterministic RFC-style CRLF rows", failures);
  rocket_rt_release(encodedCsv);
  rocket_rt_release(rows);
  rocket_rt_release(parsedCsv);
  rocket_rt_release(csvText);

  rocket_std_random_seed(12345);
  const std::int64_t firstRandom = rocket_std_random_int(-10, 10);
  const double firstFloat = rocket_std_random_float();
  rocket_std_random_seed(12345);
  rocket::test::expect(firstRandom == rocket_std_random_int(-10, 10) &&
                           firstFloat == rocket_std_random_float() &&
                           firstRandom >= -10 && firstRandom < 10,
                       "seeded random sequences are reproducible and range-bounded", failures);

  const std::filesystem::path temporary =
      std::filesystem::current_path() / "rocket_stdlib_test_temp.txt";
  RocketString* path = string(temporary.string());
  RocketString* contents = string("phase-seven");
  RocketAggregate* wrote = rocket_std_file_write_text(path, contents);
  rocket::test::expect(rocket_rt_aggregate_tag(wrote) == 0 &&
                           rocket_std_file_exists(path) == 1,
                       "file.write_text and file.exists operate without exceptions", failures);
  RocketAggregate* read = rocket_std_file_read_text(path);
  auto* readText = static_cast<RocketString*>(rocket_rt_aggregate_get_managed(read, 0));
  rocket::test::expect(value(readText) == "phase-seven",
                       "file.read_text returns the bytes written", failures);
  RocketAggregate* removed = rocket_std_file_remove(path);
  rocket::test::expect(rocket_rt_aggregate_tag(removed) == 0 &&
                           rocket_rt_aggregate_get_bool(removed, 0) == 1,
                       "file.remove returns Ok(true) for an existing file", failures);
  rocket_rt_release(removed);
  rocket_rt_release(readText);
  rocket_rt_release(read);
  rocket_rt_release(wrote);
  rocket_rt_release(contents);
  rocket_rt_release(path);

  RocketString* environmentName = string("PATH");
  RocketAggregate* environment = rocket_std_process_environment(environmentName);
  rocket::test::expect(rocket_rt_aggregate_tag(environment) == 0,
                       "process.environment reads an existing variable as Option", failures);
  rocket_rt_release(environment);
  rocket_rt_release(environmentName);
  const std::int64_t before = rocket_std_time_monotonic_milliseconds();
  rocket_std_time_sleep_milliseconds(0);
  rocket::test::expect(rocket_std_time_unix_milliseconds() > 0 &&
                           rocket_std_time_monotonic_milliseconds() >= before,
                       "time clocks and non-negative sleep are available", failures);

  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "standard-library operations release every managed allocation", failures);
  return rocket::test::finish(failures, "stdlib");
}
