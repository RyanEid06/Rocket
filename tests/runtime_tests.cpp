#include "runtime.h"
#include "test_support.h"

#include <cstdint>
#include <cstring>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace {

void* phase18TaskEntry(void* context) {
  const std::int64_t value = rocket_rt_aggregate_get_int(
      static_cast<RocketAggregate*>(context), 0);
  RocketAggregate* result = rocket_rt_aggregate_new(0, 1, 0);
  rocket_rt_aggregate_set_int(result, 0, value + 1);
  return result;
}

void* phase18ManagedTaskEntry(void* context) {
  RocketAggregate* result = rocket_rt_aggregate_new(0, 1, 1);
  rocket_rt_aggregate_set_managed(result, 0, context);
  return result;
}

} // namespace

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

  RocketAggregate* shared = rocket_rt_aggregate_new(0, 1, 0);
  rocket_rt_aggregate_set_int(shared, 0, 42);
  rocket_rt_promote(shared);
  std::vector<std::thread> contenders;
  for (int worker = 0; worker < 8; ++worker) {
    contenders.emplace_back([shared] {
      for (int iteration = 0; iteration < 25000; ++iteration) {
        rocket_rt_retain(shared);
        rocket_rt_release(shared);
      }
    });
  }
  for (auto& contender : contenders) contender.join();
  rocket::test::expect(rocket_rt_aggregate_get_int(shared, 0) == 42,
                       "atomic shared ARC survives high-contention retain/release",
                       failures);
  rocket_rt_release(shared);
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "atomic shared ARC destroys the payload exactly once", failures);

  RocketAggregate* weakTarget = rocket_rt_aggregate_new(0, 1, 0);
  RocketWeak* weak = rocket_rt_weak_new(weakTarget);
  std::atomic<bool> beginWeakRace = false;
  std::atomic<std::uint64_t> successfulUpgrades = 0;
  contenders.clear();
  for (int worker = 0; worker < 8; ++worker) {
    contenders.emplace_back([weak, &beginWeakRace, &successfulUpgrades] {
      while (!beginWeakRace.load(std::memory_order_acquire)) std::this_thread::yield();
      for (int iteration = 0; iteration < 10000; ++iteration) {
        if (void* upgraded = rocket_rt_weak_upgrade(weak)) {
          successfulUpgrades.fetch_add(1, std::memory_order_relaxed);
          rocket_rt_release(upgraded);
        } else {
          break;
        }
      }
    });
  }
  beginWeakRace.store(true, std::memory_order_release);
  rocket_rt_release(weakTarget);
  for (auto& contender : contenders) contender.join();
  rocket::test::expect(rocket_rt_weak_expired(weak) == 1 &&
                           rocket_rt_weak_upgrade(weak) == nullptr,
                       "Weak upgrade is all-or-nothing during concurrent destruction",
                       failures);
  rocket::test::expect(successfulUpgrades.load(std::memory_order_relaxed) <= 80000,
                       "Weak contention completes without duplicate ownership", failures);
  rocket_rt_release(weak);
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "Weak control storage is released after expiration", failures);

  for (int iteration = 0; iteration < 10000; ++iteration) {
    RocketAggregate* self = rocket_rt_aggregate_new(0, 1, 1);
    RocketWeak* backToSelf = rocket_rt_weak_new(self);
    rocket_rt_aggregate_set_managed(self, 0, backToSelf);
    rocket_rt_release(backToSelf);
    rocket_rt_release(self);
  }
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "Weak self-cycles destroy deterministically under stress", failures);

  RocketAggregate* cycleLeft = rocket_rt_aggregate_new(0, 1, 1);
  RocketAggregate* cycleRight = rocket_rt_aggregate_new(0, 1, 1);
  RocketWeak* weakLeft = rocket_rt_weak_new(cycleLeft);
  rocket_rt_aggregate_set_managed(cycleLeft, 0, cycleRight);
  rocket_rt_aggregate_set_managed(cycleRight, 0, weakLeft);
  rocket_rt_release(weakLeft);
  rocket_rt_release(cycleRight);
  rocket_rt_release(cycleLeft);
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "a Weak back edge breaks a multi-object ownership cycle", failures);

  RocketAggregate* taskContext = rocket_rt_aggregate_new(0, 1, 0);
  rocket_rt_aggregate_set_int(taskContext, 0, 41);
  RocketTask* task = rocket_rt_task_spawn(
      reinterpret_cast<void*>(&phase18TaskEntry), taskContext);
  RocketAggregate* taskResult = rocket_std_task_join(task);
  rocket::test::expect(rocket_rt_aggregate_tag(taskResult) == 0 &&
                           rocket_rt_aggregate_get_int(taskResult, 0) == 42,
                       "bounded task executor owns and joins typed results", failures);
  rocket_rt_release(taskResult);
  rocket_rt_release(task);
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                        "Task completion releases captured context and result", failures);

  RocketCancellation* taskCancellation = rocket_std_cancel_token();
  RocketTask* cancellableTask = rocket_std_async_time_sleep(1000, taskCancellation);
  rocket::test::expect(rocket_std_task_cancel(cancellableTask) == 1,
                       "Task cancellation wins before timer completion", failures);
  RocketAggregate* cancelledTaskResult = rocket_std_task_join(cancellableTask);
  rocket::test::expect(rocket_rt_aggregate_tag(cancelledTaskResult) == 1 &&
                           rocket_std_task_cancel(cancellableTask) == 0,
                       "cancelled Task completes once and rejects late cancellation", failures);
  rocket_rt_release(cancelledTaskResult);
  rocket_rt_release(cancellableTask);
  rocket_rt_release(taskCancellation);

  RocketAggregate* threadContext = rocket_rt_aggregate_new(0, 1, 0);
  rocket_rt_aggregate_set_int(threadContext, 0, 9);
  RocketTask* threadTask = rocket_rt_task_spawn(
      reinterpret_cast<void*>(&phase18TaskEntry), threadContext);
  RocketAggregate* threadSpawned = rocket_std_thread_spawn(threadTask);
  auto* runtimeThread = reinterpret_cast<RocketThread*>(
      rocket_rt_aggregate_get_managed(threadSpawned, 0));
  RocketAggregate* threadResult = rocket_std_thread_join(runtimeThread);
  rocket::test::expect(rocket_rt_aggregate_tag(threadResult) == 0 &&
                           rocket_rt_aggregate_get_int(threadResult, 0) == 10,
                       "Thread join transfers one completed task result", failures);
  rocket_rt_release(threadResult);
  rocket_rt_release(runtimeThread);
  rocket_rt_release(threadSpawned);
  rocket_rt_release(threadTask);
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "joined Thread and underlying Task release exactly once", failures);

  RocketString* firstTaskValue = rocket_rt_string_new(
      reinterpret_cast<const std::uint8_t*>("first"), 5);
  RocketString* secondTaskValue = rocket_rt_string_new(
      reinterpret_cast<const std::uint8_t*>("second"), 6);
  RocketTask* firstTask = rocket_rt_task_spawn(
      reinterpret_cast<void*>(&phase18ManagedTaskEntry), firstTaskValue);
  RocketTask* secondTask = rocket_rt_task_spawn(
      reinterpret_cast<void*>(&phase18ManagedTaskEntry), secondTaskValue);
  RocketArray* groupedTasks = rocket_rt_array_new(ROCKET_ELEMENT_MANAGED, 2);
  rocket_rt_array_set_managed(groupedTasks, 0, firstTask);
  rocket_rt_array_set_managed(groupedTasks, 1, secondTask);
  RocketTaskGroup* group = rocket_std_task_group_string(groupedTasks);
  RocketAggregate* groupResult = rocket_std_task_group_join(group);
  auto* groupValues = reinterpret_cast<RocketArray*>(
      rocket_rt_aggregate_get_managed(groupResult, 0));
  RocketString* groupedFirst = rocket_rt_index_string(groupValues, 0);
  RocketString* groupedSecond = rocket_rt_index_string(groupValues, 1);
  rocket::test::expect(rocket_rt_string_equal(groupedFirst, firstTaskValue) == 1 &&
                           rocket_rt_string_equal(groupedSecond, secondTaskValue) == 1,
                       "TaskGroup joins every child in spawn order", failures);
  rocket_rt_release(groupedSecond);
  rocket_rt_release(groupedFirst);
  rocket_rt_release(groupValues);
  rocket_rt_release(groupResult);
  rocket_rt_release(group);
  rocket_rt_release(groupedTasks);
  rocket_rt_release(secondTask);
  rocket_rt_release(firstTask);
  // Task contexts are consumed by spawn; the Task objects release these strings.
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "TaskGroup structured cleanup releases every child", failures);

  RocketCancellation* abandonedToken = rocket_std_cancel_token();
  RocketTask* abandonedFirst = rocket_std_async_time_sleep(1000, abandonedToken);
  RocketTask* abandonedSecond = rocket_std_async_time_sleep(1000, abandonedToken);
  RocketArray* abandonedTasks = rocket_rt_array_new(ROCKET_ELEMENT_MANAGED, 2);
  rocket_rt_array_set_managed(abandonedTasks, 0, abandonedFirst);
  rocket_rt_array_set_managed(abandonedTasks, 1, abandonedSecond);
  RocketTaskGroup* abandonedGroup = rocket_std_task_group(abandonedTasks);
  const auto abandonStarted = std::chrono::steady_clock::now();
  rocket_rt_release(abandonedGroup);
  const auto abandonElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - abandonStarted);
  rocket::test::expect(abandonElapsed < std::chrono::milliseconds(500),
                       "dropping an unjoined TaskGroup cancels and joins its children",
                       failures);
  rocket_rt_release(abandonedTasks);
  rocket_rt_release(abandonedSecond);
  rocket_rt_release(abandonedFirst);
  rocket_rt_release(abandonedToken);
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "unjoined TaskGroup cleanup leaves no child tasks", failures);

  RocketCancellation* cancellation = rocket_std_cancel_token();
  RocketCancellation* childCancellation = rocket_std_cancel_child(cancellation);
  rocket::test::expect(rocket_std_cancel_is_cancelled(childCancellation) == 0 &&
                           rocket_std_cancel_cancel(cancellation) == 1 &&
                           rocket_std_cancel_is_cancelled(childCancellation) == 1 &&
                           rocket_std_cancel_cancel(cancellation) == 0,
                       "cancellation is idempotent and propagates to children", failures);
  RocketTask* cancelledTimer = rocket_std_async_time_sleep(1000, childCancellation);
  RocketAggregate* cancelledTimerResult = rocket_std_task_join(cancelledTimer);
  rocket::test::expect(rocket_rt_aggregate_tag(cancelledTimerResult) == 1,
                       "timer observes cancellation without waiting for its deadline", failures);
  rocket_rt_release(cancelledTimerResult);
  rocket_rt_release(cancelledTimer);
  rocket_rt_release(childCancellation);
  rocket_rt_release(cancellation);

  RocketString* protectedValue = rocket_rt_string_new(
      reinterpret_cast<const std::uint8_t*>("before"), 6);
  RocketString* replacementValue = rocket_rt_string_new(
      reinterpret_cast<const std::uint8_t*>("after"), 5);
  RocketMutex* mutex = rocket_std_sync_mutex(protectedValue);
  RocketCancellation* waitToken = rocket_std_cancel_token();
  RocketAggregate* locked = rocket_std_sync_lock(
      mutex, rocket_std_time_monotonic_milliseconds() + 1000, waitToken);
  auto* guard = reinterpret_cast<RocketGuard*>(rocket_rt_aggregate_get_managed(locked, 0));
  auto* readBefore = reinterpret_cast<RocketString*>(rocket_std_sync_guard_get(guard));
  rocket::test::expect(rocket_rt_string_equal(readBefore, protectedValue) == 1 &&
                           rocket_std_sync_guard_set(guard, replacementValue) == 1,
                       "Mutex guards serialize managed get and set", failures);
  RocketAggregate* timedLock = rocket_std_sync_lock(
      mutex, rocket_std_time_monotonic_milliseconds() + 10, waitToken);
  rocket::test::expect(rocket_rt_aggregate_tag(timedLock) == 1,
                       "contended Mutex lock reaches a finite timeout", failures);
  rocket_rt_release(timedLock);
  RocketAggregate* unlocked = rocket_std_sync_unlock(guard);
  rocket::test::expect(rocket_rt_aggregate_tag(unlocked) == 0,
                       "LockGuard unlock succeeds exactly once", failures);
  rocket_rt_release(unlocked);
  rocket_rt_release(readBefore);
  rocket_rt_release(guard);
  rocket_rt_release(locked);
  rocket_rt_release(waitToken);
  rocket_rt_release(mutex);
  rocket_rt_release(replacementValue);
  rocket_rt_release(protectedValue);

  RocketAtomicInt* atomicValue = rocket_std_sync_atomic_int(0);
  contenders.clear();
  for (int worker = 0; worker < 8; ++worker)
    contenders.emplace_back([atomicValue] {
      for (int iteration = 0; iteration < 10000; ++iteration)
        rocket_std_sync_atomic_fetch_add(atomicValue, 1);
    });
  for (auto& contender : contenders) contender.join();
  rocket::test::expect(rocket_std_sync_atomic_load(atomicValue) == 80000,
                       "AtomicInt is sequentially consistent under contention", failures);
  rocket_rt_release(atomicValue);

  RocketString* onceValue = rocket_rt_string_new(
      reinterpret_cast<const std::uint8_t*>("once"), 4);
  RocketString* ignoredOnceValue = rocket_rt_string_new(
      reinterpret_cast<const std::uint8_t*>("ignored"), 7);
  RocketOnce* once = rocket_std_sync_once(onceValue);
  std::atomic<int> onceReaders = 0;
  contenders.clear();
  for (int worker = 0; worker < 8; ++worker)
    contenders.emplace_back([once, &onceReaders] {
      for (int iteration = 0; iteration < 1000; ++iteration) {
        RocketAggregate* observed = rocket_std_sync_once_get(once);
        if (rocket_rt_aggregate_tag(observed) == 0) {
          void* value = rocket_rt_aggregate_get_managed(observed, 0);
          rocket_rt_release(value);
          onceReaders.fetch_add(1, std::memory_order_relaxed);
        }
        rocket_rt_release(observed);
      }
    });
  for (auto& contender : contenders) contender.join();
  RocketAggregate* onceSetAgain = rocket_std_sync_once_set(once, ignoredOnceValue);
  rocket::test::expect(onceReaders.load(std::memory_order_relaxed) == 8000 &&
                           rocket_rt_aggregate_tag(onceSetAgain) == 0 &&
                           rocket_rt_aggregate_get_bool(onceSetAgain, 0) == 0,
                       "Once publishes one immutable managed value to all readers", failures);
  rocket_rt_release(onceSetAgain);
  rocket_rt_release(once);
  rocket_rt_release(ignoredOnceValue);
  rocket_rt_release(onceValue);

  RocketEvent* event = rocket_std_sync_event(1, 0);
  RocketCancellation* eventToken = rocket_std_cancel_token();
  std::atomic<bool> eventObserved = false;
  std::thread waiter([&] {
    RocketAggregate* waited = rocket_std_sync_event_wait(
        event, rocket_std_time_monotonic_milliseconds() + 1000, eventToken);
    eventObserved.store(rocket_rt_aggregate_tag(waited) == 0, std::memory_order_release);
    rocket_rt_release(waited);
  });
  rocket_std_sync_event_set(event);
  waiter.join();
  rocket::test::expect(eventObserved.load(std::memory_order_acquire),
                       "Event wait cannot lose a concurrent set", failures);
  rocket_rt_release(eventToken);
  rocket_rt_release(event);

  RocketEvent* cancelledEvent = rocket_std_sync_event(1, 0);
  RocketCancellation* cancelledEventToken = rocket_std_cancel_token();
  std::atomic<bool> cancelledWaitObserved = false;
  std::thread cancelledWaiter([&] {
    RocketAggregate* waited = rocket_std_sync_event_wait(
        cancelledEvent, rocket_std_time_monotonic_milliseconds() + 1000,
        cancelledEventToken);
    cancelledWaitObserved.store(rocket_rt_aggregate_tag(waited) == 1,
                                std::memory_order_release);
    rocket_rt_release(waited);
  });
  rocket_std_cancel_cancel(cancelledEventToken);
  cancelledWaiter.join();
  rocket::test::expect(cancelledWaitObserved.load(std::memory_order_acquire),
                       "Event wait observes cancellation under contention", failures);
  rocket_rt_release(cancelledEventToken);
  rocket_rt_release(cancelledEvent);

  RocketString* initialValue = rocket_rt_string_new(
      reinterpret_cast<const std::uint8_t*>("first"), 5);
  RocketString* sentValue = rocket_rt_string_new(
      reinterpret_cast<const std::uint8_t*>("second"), 6);
  RocketArray* initialValues = rocket_rt_array_new(ROCKET_ELEMENT_STRING, 1);
  rocket_rt_array_set_string(initialValues, 0, initialValue);
  RocketAggregate* channelResult = rocket_std_channel_bounded(initialValues, 1);
  auto* channel = reinterpret_cast<RocketAggregate*>(
      rocket_rt_aggregate_get_managed(channelResult, 0));
  RocketSender* sender = rocket_std_channel_sender(channel);
  RocketReceiver* receiver = rocket_std_channel_receiver(channel);
  RocketCancellation* channelToken = rocket_std_cancel_token();
  RocketAggregate* firstReceived = rocket_std_channel_receive(
      receiver, rocket_std_time_monotonic_milliseconds() + 1000, channelToken);
  RocketAggregate* firstOption = reinterpret_cast<RocketAggregate*>(
      rocket_rt_aggregate_get_managed(firstReceived, 0));
  auto* firstPayload = reinterpret_cast<RocketString*>(
      rocket_rt_aggregate_get_managed(firstOption, 0));
  RocketAggregate* sent = rocket_std_channel_send(
      sender, sentValue, rocket_std_time_monotonic_milliseconds() + 1000, channelToken);
  rocket::test::expect(rocket_rt_string_equal(firstPayload, initialValue) == 1 &&
                           rocket_rt_aggregate_tag(sent) == 0,
                       "bounded Channel preserves FIFO order and releases backpressure",
                       failures);
  rocket_rt_release(sent);
  rocket_rt_release(firstPayload);
  rocket_rt_release(firstOption);
  rocket_rt_release(firstReceived);
  RocketAggregate* senderClosed = rocket_std_channel_close_sender(sender);
  RocketAggregate* receiverClosed = rocket_std_channel_close_receiver(receiver);
  rocket_rt_release(senderClosed);
  rocket_rt_release(receiverClosed);
  rocket_rt_release(channelToken);
  rocket_rt_release(receiver);
  rocket_rt_release(sender);
  rocket_rt_release(channel);
  rocket_rt_release(channelResult);
  rocket_rt_release(initialValues);
  rocket_rt_release(sentValue);
  rocket_rt_release(initialValue);
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "Phase 18 synchronization and channel handles leave no leaks",
                       failures);

  RocketString* blockedInitial = rocket_rt_string_new(
      reinterpret_cast<const std::uint8_t*>("queued"), 6);
  RocketString* blockedValue = rocket_rt_string_new(
      reinterpret_cast<const std::uint8_t*>("released"), 8);
  RocketArray* blockedInitialValues = rocket_rt_array_new(ROCKET_ELEMENT_STRING, 1);
  rocket_rt_array_set_string(blockedInitialValues, 0, blockedInitial);
  RocketAggregate* blockedCreated = rocket_std_channel_bounded(blockedInitialValues, 1);
  auto* blockedChannel = reinterpret_cast<RocketAggregate*>(
      rocket_rt_aggregate_get_managed(blockedCreated, 0));
  RocketSender* blockedSender = rocket_std_channel_sender(blockedChannel);
  RocketReceiver* blockedReceiver = rocket_std_channel_receiver(blockedChannel);
  RocketCancellation* blockedToken = rocket_std_cancel_token();
  std::atomic<bool> sendStarted = false;
  std::atomic<bool> sendFinished = false;
  std::atomic<bool> sendSucceeded = false;
  std::thread blockedProducer([&] {
    sendStarted.store(true, std::memory_order_release);
    RocketAggregate* result = rocket_std_channel_send(
        blockedSender, blockedValue,
        rocket_std_time_monotonic_milliseconds() + 1000, blockedToken);
    sendSucceeded.store(rocket_rt_aggregate_tag(result) == 0,
                        std::memory_order_release);
    rocket_rt_release(result);
    sendFinished.store(true, std::memory_order_release);
  });
  const auto producerWatchdog = std::chrono::steady_clock::now() +
      std::chrono::milliseconds(100);
  while (!sendStarted.load(std::memory_order_acquire) &&
         std::chrono::steady_clock::now() < producerWatchdog)
    std::this_thread::yield();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  rocket::test::expect(sendStarted.load(std::memory_order_acquire) &&
                           !sendFinished.load(std::memory_order_acquire),
                       "bounded Channel applies producer backpressure", failures);
  RocketAggregate* releasedSlot = rocket_std_channel_receive(
      blockedReceiver, rocket_std_time_monotonic_milliseconds() + 1000, blockedToken);
  rocket_rt_release(releasedSlot);
  blockedProducer.join();
  rocket::test::expect(sendSucceeded.load(std::memory_order_acquire),
                       "bounded send resumes after a receiver releases capacity", failures);
  RocketAggregate* blockedSenderClosed = rocket_std_channel_close_sender(blockedSender);
  RocketAggregate* blockedReceiverClosed = rocket_std_channel_close_receiver(blockedReceiver);
  rocket_rt_release(blockedReceiverClosed);
  rocket_rt_release(blockedSenderClosed);
  rocket_rt_release(blockedToken);
  rocket_rt_release(blockedReceiver);
  rocket_rt_release(blockedSender);
  rocket_rt_release(blockedChannel);
  rocket_rt_release(blockedCreated);
  rocket_rt_release(blockedInitialValues);
  rocket_rt_release(blockedValue);
  rocket_rt_release(blockedInitial);
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "backpressure and close paths release queued channel values", failures);
  return rocket::test::finish(failures, "runtime");
}
