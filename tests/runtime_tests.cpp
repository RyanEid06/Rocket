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
  RocketArray* sameIntegers = rocket_rt_array_update_int(integers, 1, 21);
  rocket_rt_release(integers);
  integers = sameIntegers;
  rocket::test::expect(rocket_rt_index_int(integers, 1) == 21,
                       "uniquely owned Array mutation updates its value", failures);
  RocketArray* integerAlias = integers;
  rocket_rt_retain(integerAlias);
  RocketSlice* integerSlice = rocket_rt_slice_new(integers, 0, 2);
  RocketArray* changedIntegers = rocket_rt_array_update_int(integers, 1, 99);
  rocket_rt_release(integers);
  integers = changedIntegers;
  rocket::test::expect(rocket_rt_index_int(integers, 1) == 99 &&
                           rocket_rt_index_int(integerAlias, 1) == 21 &&
                           rocket_rt_index_int(integerSlice, 1) == 21,
                       "shared aliases and Slices preserve the pre-mutation Array value",
                       failures);
  rocket_rt_release(integerAlias);
  rocket_rt_release(integerSlice);
  rocket_rt_release(integers);
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "scalar Array storage is destroyed deterministically", failures);

  RocketArray* growing = rocket_rt_array_new(ROCKET_ELEMENT_INT, 0);
  RocketArray* reserved = rocket_rt_array_reserve(growing, 8);
  rocket_rt_release(growing);
  growing = reserved;
  rocket::test::expect(rocket_rt_array_capacity(growing) >= 8 &&
                           rocket_rt_collection_length(growing) == 0,
                       "Array reserve exposes capacity without changing length", failures);
  RocketArray* growthAlias = growing;
  rocket_rt_retain(growthAlias);
  RocketArray* appended = rocket_rt_array_append_int(growing, 42);
  rocket::test::expect(rocket_rt_collection_length(growing) == 0,
                       "append preserves a uniquely owned input when result is separate",
                       failures);
  rocket_rt_release(growing);
  growing = appended;
  rocket::test::expect(rocket_rt_collection_length(growing) == 1 &&
                           rocket_rt_index_int(growing, 0) == 42 &&
                           rocket_rt_collection_length(growthAlias) == 0,
                       "append preserves shared Array snapshots", failures);
  RocketAggregate* popped = rocket_rt_array_pop(growing);
  rocket::test::expect(rocket_rt_collection_length(growing) == 1,
                       "pop preserves its input when the result is ignored", failures);
  rocket::test::expect(rocket_rt_aggregate_tag(popped) == 0,
                       "non-empty pop returns Some", failures);
  auto* popValue = static_cast<RocketAggregate*>(rocket_rt_aggregate_get_managed(popped, 0));
  auto* poppedArray = static_cast<RocketArray*>(rocket_rt_aggregate_get_managed(popValue, 0));
  rocket::test::expect(rocket_rt_collection_length(poppedArray) == 0 &&
                           rocket_rt_aggregate_get_int(popValue, 1) == 42,
                       "pop returns the shortened Array and removed value", failures);
  rocket_rt_release(poppedArray);
  rocket_rt_release(popValue);
  rocket_rt_release(popped);
  RocketAggregate* emptyPop = rocket_rt_array_pop(growthAlias);
  rocket::test::expect(rocket_rt_aggregate_tag(emptyPop) == 1,
                       "empty pop returns None", failures);
  rocket_rt_release(emptyPop);
  rocket_rt_release(growthAlias);
  rocket_rt_release(growing);
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "Array growth and pop release all owned results", failures);

  RocketString* oldText = rocket_rt_string_new(
      reinterpret_cast<const std::uint8_t*>("old"), 3);
  RocketString* newText = rocket_rt_string_new(
      reinterpret_cast<const std::uint8_t*>("new"), 3);
  RocketArray* managedValues = rocket_rt_array_new(ROCKET_ELEMENT_STRING, 1);
  rocket_rt_array_set_string(managedValues, 0, oldText);
  RocketArray* managedAlias = managedValues;
  rocket_rt_retain(managedAlias);
  RocketArray* changedManaged = rocket_rt_array_update_string(managedValues, 0, newText);
  rocket_rt_release(managedValues);
  managedValues = changedManaged;
  RocketString* currentText = rocket_rt_index_string(managedValues, 0);
  RocketString* aliasedText = rocket_rt_index_string(managedAlias, 0);
  rocket::test::expect(rocket_rt_string_equal(currentText, newText) == 1 &&
                           rocket_rt_string_equal(aliasedText, oldText) == 1,
                       "copy-on-write mutation retains managed elements correctly", failures);
  rocket_rt_release(currentText);
  rocket_rt_release(aliasedText);
  rocket_rt_release(oldText);
  rocket_rt_release(newText);
  rocket_rt_release(managedValues);
  rocket_rt_release(managedAlias);
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "managed Array mutation leaves no retained aliases", failures);

  RocketString* firstGrowthText = rocket_rt_string_new(
      reinterpret_cast<const std::uint8_t*>("first"), 5);
  RocketString* secondGrowthText = rocket_rt_string_new(
      reinterpret_cast<const std::uint8_t*>("second"), 6);
  RocketArray* managedGrowth = rocket_rt_array_new(ROCKET_ELEMENT_STRING, 1);
  rocket_rt_array_set_string(managedGrowth, 0, firstGrowthText);
  RocketArray* managedAppended =
      rocket_rt_array_append_string(managedGrowth, secondGrowthText);
  rocket_rt_release(managedGrowth);
  RocketArray* managedInserted =
      rocket_rt_array_insert_string(managedAppended, 1, firstGrowthText);
  rocket_rt_release(managedAppended);
  RocketAggregate* managedRemoved = rocket_rt_array_remove(managedInserted, 0);
  rocket_rt_release(managedInserted);
  auto* managedAfterRemove = static_cast<RocketArray*>(
      rocket_rt_aggregate_get_managed(managedRemoved, 0));
  auto* removedGrowthText = static_cast<RocketString*>(
      rocket_rt_aggregate_get_managed(managedRemoved, 1));
  RocketArray* managedCleared = rocket_rt_array_clear(managedAfterRemove);
  rocket::test::expect(rocket_rt_collection_length(managedAfterRemove) == 2 &&
                           rocket_rt_collection_length(managedCleared) == 0 &&
                           rocket_rt_string_equal(removedGrowthText,
                                                  firstGrowthText) == 1,
                       "managed append, insert, remove, and clear preserve values",
                       failures);
  rocket_rt_release(managedCleared);
  rocket_rt_release(removedGrowthText);
  rocket_rt_release(managedAfterRemove);
  rocket_rt_release(managedRemoved);
  rocket_rt_release(firstGrowthText);
  rocket_rt_release(secondGrowthText);
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "managed Array growth operations release every retained element",
                       failures);

  RocketString* keyA = rocket_rt_string_new(reinterpret_cast<const std::uint8_t*>("a"), 1);
  RocketString* keyB = rocket_rt_string_new(reinterpret_cast<const std::uint8_t*>("b"), 1);
  RocketString* valueA = rocket_rt_string_new(reinterpret_cast<const std::uint8_t*>("one"), 3);
  RocketString* valueB = rocket_rt_string_new(reinterpret_cast<const std::uint8_t*>("two"), 3);
  RocketArray* mapKeys = rocket_rt_array_new(ROCKET_ELEMENT_STRING, 2);
  RocketArray* mapValues = rocket_rt_array_new(ROCKET_ELEMENT_STRING, 2);
  rocket_rt_array_set_string(mapKeys, 0, keyA);
  rocket_rt_array_set_string(mapKeys, 1, keyB);
  rocket_rt_array_set_string(mapValues, 0, valueA);
  rocket_rt_array_set_string(mapValues, 1, valueB);
  RocketAggregate* map = rocket_std_collections_map_from_arrays(mapKeys, mapValues);
  RocketAggregate* foundValue = rocket_std_collections_map_get_string(map, keyB);
  auto* foundText = static_cast<RocketString*>(rocket_rt_aggregate_get_managed(foundValue, 0));
  rocket::test::expect(rocket_rt_string_equal(foundText, valueB) == 1,
                       "Map lookup preserves managed values", failures);
  RocketAggregate* set = rocket_std_collections_set_from_array(mapKeys);
  rocket::test::expect(rocket_std_collections_set_contains_string(set, keyA) == 1 &&
                           rocket_std_collections_set_contains_string(set, valueA) == 0,
                       "Set membership uses String contents", failures);
  rocket_rt_release(foundText);
  rocket_rt_release(foundValue);
  rocket_rt_release(set);
  rocket_rt_release(map);
  rocket_rt_release(mapKeys);
  rocket_rt_release(mapValues);
  rocket_rt_release(keyA);
  rocket_rt_release(keyB);
  rocket_rt_release(valueA);
  rocket_rt_release(valueB);
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "Map and Set release managed keys and values", failures);

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

    RocketString* mapKey = rocket_rt_string_new(
        reinterpret_cast<const std::uint8_t*>("key"), 3);
    RocketString* mapValue = rocket_rt_string_new(
        reinterpret_cast<const std::uint8_t*>("value"), 5);
    RocketArray* stressKeys = rocket_rt_array_new(ROCKET_ELEMENT_STRING, 1);
    RocketArray* stressValues = rocket_rt_array_new(ROCKET_ELEMENT_STRING, 1);
    rocket_rt_array_set_string(stressKeys, 0, mapKey);
    rocket_rt_array_set_string(stressValues, 0, mapValue);
    RocketAggregate* stressMap =
        rocket_std_collections_map_from_arrays(stressKeys, stressValues);
    RocketAggregate* stressSet = rocket_std_collections_set_from_array(stressKeys);
    RocketAggregate* stressFound =
        rocket_std_collections_map_get_string(stressMap, mapKey);
    void* stressFoundValue = rocket_rt_aggregate_get_managed(stressFound, 0);
    rocket_rt_release(stressFoundValue);
    rocket_rt_release(stressFound);
    rocket_rt_release(stressSet);
    rocket_rt_release(stressMap);
    rocket_rt_release(stressKeys);
    rocket_rt_release(stressValues);
    rocket_rt_release(mapKey);
    rocket_rt_release(mapValue);
  }
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "allocation stress leaves no collection or aggregate leaks",
                       failures);
  return rocket::test::finish(failures, "runtime");
}
