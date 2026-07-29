#pragma once

#include <cstdint>

// Rocket runtime ABI v1. These declarations intentionally use only fixed-width
// C-compatible values. Managed objects are opaque outside the runtime.
extern "C" {

struct RocketString;
struct RocketArray;
struct RocketSlice;
struct RocketAggregate;
struct RocketStringBuilder;

enum RocketElementKind : std::uint32_t {
  ROCKET_ELEMENT_INT = 1,
  ROCKET_ELEMENT_FLOAT = 2,
  ROCKET_ELEMENT_BOOL = 3,
  ROCKET_ELEMENT_CHAR = 4,
  ROCKET_ELEMENT_STRING = 5,
  ROCKET_ELEMENT_MANAGED = 6,
};

std::uint32_t rocket_rt_abi_version();

void rocket_rt_retain(void* object);
void rocket_rt_release(void* object);

RocketString* rocket_rt_string_new(const std::uint8_t* bytes, std::uint64_t length);
std::uint8_t rocket_rt_string_equal(const RocketString* left, const RocketString* right);
std::uint64_t rocket_rt_string_byte_length(const RocketString* string);
const std::uint8_t* rocket_rt_string_bytes(const RocketString* string);

RocketArray* rocket_rt_array_new(std::uint32_t elementKind, std::uint64_t length);
void rocket_rt_array_set_int(RocketArray* array, std::int64_t index, std::int64_t value);
void rocket_rt_array_set_float(RocketArray* array, std::int64_t index, double value);
void rocket_rt_array_set_bool(RocketArray* array, std::int64_t index, std::uint8_t value);
void rocket_rt_array_set_char(RocketArray* array, std::int64_t index, std::uint8_t value);
void rocket_rt_array_set_string(RocketArray* array, std::int64_t index, RocketString* value);
void rocket_rt_array_set_managed(RocketArray* array, std::int64_t index, void* value);
RocketArray* rocket_rt_array_update_int(RocketArray* array, std::int64_t index,
                                        std::int64_t value);
RocketArray* rocket_rt_array_update_float(RocketArray* array, std::int64_t index,
                                          double value);
RocketArray* rocket_rt_array_update_bool(RocketArray* array, std::int64_t index,
                                         std::uint8_t value);
RocketArray* rocket_rt_array_update_char(RocketArray* array, std::int64_t index,
                                         std::uint8_t value);
RocketArray* rocket_rt_array_update_string(RocketArray* array, std::int64_t index,
                                           RocketString* value);
RocketArray* rocket_rt_array_update_managed(RocketArray* array, std::int64_t index,
                                            void* value);

RocketSlice* rocket_rt_slice_new(void* collection, std::int64_t start, std::int64_t end);
std::uint64_t rocket_rt_collection_length(const void* collection);
std::uint32_t rocket_rt_collection_element_kind(const void* collection);
std::int64_t rocket_rt_index_int(const void* collection, std::int64_t index);
double rocket_rt_index_float(const void* collection, std::int64_t index);
std::uint8_t rocket_rt_index_bool(const void* collection, std::int64_t index);
std::uint8_t rocket_rt_index_char(const void* collection, std::int64_t index);
RocketString* rocket_rt_index_string(const void* collection, std::int64_t index);
void* rocket_rt_index_managed(const void* collection, std::int64_t index);

RocketAggregate* rocket_rt_aggregate_new(std::uint32_t tag, std::uint32_t fieldCount,
                                         std::uint64_t managedMask);
std::uint32_t rocket_rt_aggregate_tag(const RocketAggregate* aggregate);
void rocket_rt_aggregate_set_int(RocketAggregate* aggregate, std::uint32_t field,
                                 std::int64_t value);
void rocket_rt_aggregate_set_float(RocketAggregate* aggregate, std::uint32_t field,
                                   double value);
void rocket_rt_aggregate_set_bool(RocketAggregate* aggregate, std::uint32_t field,
                                  std::uint8_t value);
void rocket_rt_aggregate_set_char(RocketAggregate* aggregate, std::uint32_t field,
                                  std::uint8_t value);
void rocket_rt_aggregate_set_managed(RocketAggregate* aggregate, std::uint32_t field,
                                     void* value);
std::int64_t rocket_rt_aggregate_get_int(const RocketAggregate* aggregate,
                                         std::uint32_t field);
double rocket_rt_aggregate_get_float(const RocketAggregate* aggregate,
                                     std::uint32_t field);
std::uint8_t rocket_rt_aggregate_get_bool(const RocketAggregate* aggregate,
                                          std::uint32_t field);
std::uint8_t rocket_rt_aggregate_get_char(const RocketAggregate* aggregate,
                                          std::uint32_t field);
void* rocket_rt_aggregate_get_managed(const RocketAggregate* aggregate,
                                      std::uint32_t field);

[[noreturn]] void rocket_rt_panic_integer_overflow();
[[noreturn]] void rocket_rt_panic_division_by_zero();

void rocket_rt_print_int(std::int64_t value);
void rocket_rt_print_float(double value);
void rocket_rt_print_bool(std::uint8_t value);
void rocket_rt_print_char(std::uint8_t value);
void rocket_rt_print_string(const RocketString* value);
void rocket_rt_print_unit();

// Exposed for compiler/runtime lifetime regressions. It is not a language API.
std::uint64_t rocket_rt_debug_live_allocations();

// Phase 7 standard-library ABI. Failures are returned as ordinary Rocket
// Option/Result aggregates; only programmer contract violations panic.
std::int64_t rocket_std_string_byte_length(RocketString* value);
RocketString* rocket_std_string_concat(RocketString* left, RocketString* right);
std::uint8_t rocket_std_string_contains(RocketString* value, RocketString* needle);
std::uint8_t rocket_std_string_starts_with(RocketString* value, RocketString* prefix);
std::uint8_t rocket_std_string_ends_with(RocketString* value, RocketString* suffix);
RocketString* rocket_std_string_trim(RocketString* value);
RocketArray* rocket_std_string_split(RocketString* value, RocketString* delimiter);
std::uint8_t rocket_std_string_byte_at(RocketString* value, std::int64_t index);
std::int64_t rocket_std_string_byte_value_at(RocketString* value, std::int64_t index);
RocketString* rocket_std_string_slice(RocketString* value, std::int64_t start,
                                      std::int64_t end);
RocketAggregate* rocket_std_string_parse_int(RocketString* value);
RocketString* rocket_std_string_from_int(std::int64_t value);
RocketStringBuilder* rocket_std_string_builder();
void rocket_std_string_builder_append(RocketStringBuilder* builder, RocketString* value);
RocketString* rocket_std_string_builder_finish(RocketStringBuilder* builder);

std::int64_t rocket_std_collections_length(void* collection);
RocketArray* rocket_std_collections_reverse(void* collection);
RocketArray* rocket_std_collections_concat(void* left, void* right);
RocketString* rocket_std_collections_join(RocketArray* values, RocketString* separator);

RocketAggregate* rocket_std_file_read_text(RocketString* path);
RocketAggregate* rocket_std_file_write_text(RocketString* path, RocketString* contents);
RocketAggregate* rocket_std_file_append_text(RocketString* path, RocketString* contents);
std::uint8_t rocket_std_file_exists(RocketString* path);
RocketAggregate* rocket_std_file_remove(RocketString* path);
RocketAggregate* rocket_std_file_list(RocketString* path);
RocketAggregate* rocket_std_file_create_directory(RocketString* path);

RocketString* rocket_std_path_join(RocketString* left, RocketString* right);
RocketString* rocket_std_path_basename(RocketString* path);
RocketString* rocket_std_path_extension(RocketString* path);
RocketString* rocket_std_path_normalize(RocketString* path);

RocketAggregate* rocket_std_json_parse(RocketString* text);
RocketString* rocket_std_json_stringify(RocketAggregate* value);
RocketAggregate* rocket_std_csv_parse(RocketString* text);
RocketString* rocket_std_csv_encode(RocketArray* rows);

void rocket_std_random_seed(std::int64_t seed);
std::int64_t rocket_std_random_int(std::int64_t minimum, std::int64_t maximum);
double rocket_std_random_float();

RocketAggregate* rocket_std_process_run(RocketString* program, RocketArray* arguments);
void rocket_std_process_set_arguments(std::int32_t count, const char* const* arguments);
RocketArray* rocket_std_process_arguments();
RocketAggregate* rocket_std_process_executable_path();
RocketAggregate* rocket_std_process_environment(RocketString* name);
RocketAggregate* rocket_std_process_working_directory();

std::int64_t rocket_std_time_unix_milliseconds();
std::int64_t rocket_std_time_monotonic_milliseconds();
void rocket_std_time_sleep_milliseconds(std::int64_t milliseconds);

} // extern "C"
