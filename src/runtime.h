#pragma once

#include <cstdint>

// Rocket runtime ABI v1. These declarations intentionally use only fixed-width
// C-compatible values. Managed objects are opaque outside the runtime.
extern "C" {

struct RocketString;
struct RocketArray;
struct RocketSlice;

enum RocketElementKind : std::uint32_t {
  ROCKET_ELEMENT_INT = 1,
  ROCKET_ELEMENT_FLOAT = 2,
  ROCKET_ELEMENT_BOOL = 3,
  ROCKET_ELEMENT_CHAR = 4,
  ROCKET_ELEMENT_STRING = 5,
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

RocketSlice* rocket_rt_slice_new(void* collection, std::int64_t start, std::int64_t end);
std::uint64_t rocket_rt_collection_length(const void* collection);
std::int64_t rocket_rt_index_int(const void* collection, std::int64_t index);
double rocket_rt_index_float(const void* collection, std::int64_t index);
std::uint8_t rocket_rt_index_bool(const void* collection, std::int64_t index);
std::uint8_t rocket_rt_index_char(const void* collection, std::int64_t index);
RocketString* rocket_rt_index_string(const void* collection, std::int64_t index);

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

} // extern "C"
