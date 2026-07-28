#include "runtime.h"
#include "test_support.h"

#include <cstdint>
#include <cstring>

int main() {
  int failures = 0;
  rocket::test::expect(rocket_rt_abi_version() == 1, "runtime reports ABI version 1", failures);
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "runtime begins with no live allocations", failures);

  const auto* utf8 = reinterpret_cast<const std::uint8_t*>("Rocket \xf0\x9f\x9a\x80");
  RocketString* first = rocket_rt_string_new(utf8, 11);
  RocketString* equal = rocket_rt_string_new(utf8, 11);
  const auto* otherBytes = reinterpret_cast<const std::uint8_t*>("Rocket");
  RocketString* other = rocket_rt_string_new(otherBytes, 6);
  rocket::test::expect(rocket_rt_debug_live_allocations() == 3,
                       "owned strings are tracked as live allocations", failures);
  rocket::test::expect(rocket_rt_string_byte_length(first) == 11,
                       "String stores its UTF-8 byte length", failures);
  rocket::test::expect(std::memcmp(rocket_rt_string_bytes(first), utf8, 11) == 0,
                       "String preserves UTF-8 bytes", failures);
  rocket::test::expect(rocket_rt_string_equal(first, equal) == 1 &&
                           rocket_rt_string_equal(first, other) == 0,
                       "String equality compares length and contents", failures);

  rocket_rt_retain(first);
  rocket_rt_release(first);
  rocket::test::expect(rocket_rt_debug_live_allocations() == 3,
                       "balanced retain/release preserves an owned value", failures);
  rocket_rt_release(first);
  rocket_rt_release(equal);
  rocket_rt_release(other);
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "final releases destroy every String allocation", failures);

  RocketString* left = rocket_rt_string_new(
      reinterpret_cast<const std::uint8_t*>("left"), 4);
  RocketString* right = rocket_rt_string_new(
      reinterpret_cast<const std::uint8_t*>("right"), 5);
  RocketArray* strings = rocket_rt_array_new(ROCKET_ELEMENT_STRING, 2);
  rocket_rt_array_set_string(strings, 0, left);
  rocket_rt_array_set_string(strings, 1, right);
  rocket_rt_release(left);
  rocket_rt_release(right);
  RocketSlice* tail = rocket_rt_slice_new(strings, 1, 2);
  rocket_rt_release(strings);
  rocket::test::expect(rocket_rt_collection_length(tail) == 1,
                       "Slice stores an exclusive bounded view", failures);
  RocketString* indexed = rocket_rt_index_string(tail, 0);
  RocketString* indexedCopy = rocket_rt_index_string(tail, 0);
  rocket::test::expect(rocket_rt_string_equal(indexed, indexedCopy) == 1,
                       "String indexing through a Slice preserves the owner", failures);
  rocket_rt_release(indexed);
  rocket_rt_release(indexedCopy);
  rocket_rt_release(tail);
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "Slice destruction releases its Array and managed elements", failures);

  RocketArray* integers = rocket_rt_array_new(ROCKET_ELEMENT_INT, 3);
  rocket_rt_array_set_int(integers, 0, 10);
  rocket_rt_array_set_int(integers, 1, 20);
  rocket_rt_array_set_int(integers, 2, 30);
  rocket::test::expect(rocket_rt_index_int(integers, 1) == 20,
                       "scalar Array indexing returns the stored value", failures);
  rocket_rt_release(integers);
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "scalar Array storage is destroyed deterministically", failures);

  RocketString* aggregateText = rocket_rt_string_new(
      reinterpret_cast<const std::uint8_t*>("payload"), 7);
  RocketAggregate* aggregate = rocket_rt_aggregate_new(3, 3, std::uint64_t{1} << 2);
  rocket_rt_aggregate_set_int(aggregate, 0, 42);
  rocket_rt_aggregate_set_float(aggregate, 1, 1.5);
  rocket_rt_aggregate_set_managed(aggregate, 2, aggregateText);
  rocket_rt_release(aggregateText);
  rocket::test::expect(rocket_rt_aggregate_tag(aggregate) == 3 &&
                           rocket_rt_aggregate_get_int(aggregate, 0) == 42 &&
                           rocket_rt_aggregate_get_float(aggregate, 1) == 1.5,
                       "aggregate runtime preserves tags and scalar fields", failures);
  void* aggregatePayload = rocket_rt_aggregate_get_managed(aggregate, 2);
  rocket::test::expect(rocket_rt_string_byte_length(
                           static_cast<RocketString*>(aggregatePayload)) == 7,
                       "managed aggregate fields return an owned value", failures);
  RocketArray* aggregates = rocket_rt_array_new(ROCKET_ELEMENT_MANAGED, 1);
  rocket_rt_array_set_managed(aggregates, 0, aggregate);
  rocket_rt_release(aggregate);
  void* indexedAggregate = rocket_rt_index_managed(aggregates, 0);
  rocket::test::expect(rocket_rt_aggregate_tag(
                           static_cast<RocketAggregate*>(indexedAggregate)) == 3,
                       "collections retain nested aggregate elements", failures);
  rocket_rt_release(aggregatePayload);
  rocket_rt_release(indexedAggregate);
  rocket_rt_release(aggregates);
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "aggregate destruction releases managed fields and nested collections",
                       failures);

  for (std::int64_t iteration = 0; iteration < 10000; ++iteration) {
    RocketString* value = rocket_rt_string_new(
        reinterpret_cast<const std::uint8_t*>("stress"), 6);
    RocketArray* array = rocket_rt_array_new(ROCKET_ELEMENT_STRING, 1);
    rocket_rt_array_set_string(array, 0, value);
    RocketSlice* slice = rocket_rt_slice_new(array, 0, 1);
    rocket_rt_release(value);
    rocket_rt_release(array);
    RocketString* throughSlice = rocket_rt_index_string(slice, 0);
    rocket_rt_release(throughSlice);
    rocket_rt_release(slice);

    RocketString* field = rocket_rt_string_new(
        reinterpret_cast<const std::uint8_t*>("field"), 5);
    RocketAggregate* aggregateValue = rocket_rt_aggregate_new(0, 1, 1);
    rocket_rt_aggregate_set_managed(aggregateValue, 0, field);
    rocket_rt_release(field);
    rocket_rt_release(aggregateValue);
  }
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "allocation stress leaves no String, Array, Slice, or aggregate leaks",
                       failures);
  return rocket::test::finish(failures, "runtime");
}
