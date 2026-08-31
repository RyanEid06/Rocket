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
struct RocketWeak;
struct RocketTask;
struct RocketCancellation;
struct RocketMutex;
struct RocketGuard;
struct RocketEvent;
struct RocketAtomicInt;
struct RocketOnce;
struct RocketSender;
struct RocketReceiver;
struct RocketTaskGroup;
struct RocketThread;

enum RocketElementKind : std::uint32_t {
  ROCKET_ELEMENT_INT = 1,
  ROCKET_ELEMENT_FLOAT = 2,
  ROCKET_ELEMENT_BOOL = 3,
  ROCKET_ELEMENT_CHAR = 4,
  ROCKET_ELEMENT_STRING = 5,
  ROCKET_ELEMENT_MANAGED = 6,
};

std::uint32_t rocket_rt_abi_version();
std::uint8_t rocket_rt_debug_executor_cycles(std::int64_t cycles);

void rocket_rt_retain(void* object);
void rocket_rt_release(void* object);
void rocket_rt_promote(void* object);
RocketWeak* rocket_rt_weak_new(void* object);
void* rocket_rt_weak_upgrade(RocketWeak* weak);
std::uint8_t rocket_rt_weak_expired(RocketWeak* weak);
RocketTask* rocket_rt_task_spawn(void* entry, void* context);
RocketTask* rocket_rt_task_ready(void* result);
void* rocket_rt_task_await(RocketTask* task);
RocketAggregate* rocket_std_task_join(RocketTask* task);
std::uint8_t rocket_std_task_is_complete(RocketTask* task);
std::uint8_t rocket_std_task_cancel(RocketTask* task);
RocketTaskGroup* rocket_std_task_group(RocketArray* tasks);
RocketTaskGroup* rocket_std_task_group_string(RocketArray* tasks);
RocketTaskGroup* rocket_std_task_group_managed(RocketArray* tasks);
RocketTaskGroup* rocket_std_task_group_int(RocketArray* tasks);
RocketTaskGroup* rocket_std_task_group_float(RocketArray* tasks);
RocketTaskGroup* rocket_std_task_group_bool(RocketArray* tasks);
RocketTaskGroup* rocket_std_task_group_char(RocketArray* tasks);
RocketAggregate* rocket_std_task_group_join(RocketTaskGroup* group);
std::uint8_t rocket_std_task_group_cancel(RocketTaskGroup* group);
RocketAggregate* rocket_std_thread_spawn(RocketTask* task);
RocketAggregate* rocket_std_thread_join(RocketThread* thread);
RocketAggregate* rocket_std_thread_detach(RocketThread* thread);
std::uint8_t rocket_std_thread_is_complete(RocketThread* thread);
RocketWeak* rocket_std_ownership_downgrade(void* object);
RocketAggregate* rocket_std_ownership_upgrade(RocketWeak* weak);
std::uint8_t rocket_std_ownership_expired(RocketWeak* weak);
RocketArray* rocket_std_buffer_thaw(RocketArray* values);
std::int64_t rocket_std_buffer_length(RocketArray* buffer);
std::int64_t rocket_std_buffer_capacity(RocketArray* buffer);
std::int64_t rocket_std_buffer_get_int(RocketArray* buffer, std::int64_t index);
double rocket_std_buffer_get_float(RocketArray* buffer, std::int64_t index);
std::uint8_t rocket_std_buffer_get_bool(RocketArray* buffer, std::int64_t index);
std::uint8_t rocket_std_buffer_get_char(RocketArray* buffer, std::int64_t index);
RocketString* rocket_std_buffer_get_string(RocketArray* buffer, std::int64_t index);
void* rocket_std_buffer_get_managed(RocketArray* buffer, std::int64_t index);
RocketArray* rocket_std_buffer_set_int(RocketArray* buffer, std::int64_t index,
                                       std::int64_t value);
RocketArray* rocket_std_buffer_set_float(RocketArray* buffer, std::int64_t index,
                                         double value);
RocketArray* rocket_std_buffer_set_bool(RocketArray* buffer, std::int64_t index,
                                        std::uint8_t value);
RocketArray* rocket_std_buffer_set_char(RocketArray* buffer, std::int64_t index,
                                        std::uint8_t value);
RocketArray* rocket_std_buffer_set_string(RocketArray* buffer, std::int64_t index,
                                          RocketString* value);
RocketArray* rocket_std_buffer_set_managed(RocketArray* buffer, std::int64_t index,
                                           void* value);
RocketArray* rocket_std_buffer_append_int(RocketArray* buffer, std::int64_t value);
RocketArray* rocket_std_buffer_append_float(RocketArray* buffer, double value);
RocketArray* rocket_std_buffer_append_bool(RocketArray* buffer, std::uint8_t value);
RocketArray* rocket_std_buffer_append_char(RocketArray* buffer, std::uint8_t value);
RocketArray* rocket_std_buffer_append_string(RocketArray* buffer, RocketString* value);
RocketArray* rocket_std_buffer_append_managed(RocketArray* buffer, void* value);
RocketArray* rocket_std_buffer_slice(RocketArray* buffer, std::int64_t start,
                                     std::int64_t end);
RocketArray* rocket_std_buffer_freeze(RocketArray* buffer);
RocketCancellation* rocket_std_cancel_token();
RocketCancellation* rocket_std_cancel_child(RocketCancellation* parent);
RocketCancellation* rocket_std_cancel_current();
std::uint8_t rocket_std_cancel_cancel(RocketCancellation* token);
std::uint8_t rocket_std_cancel_is_cancelled(RocketCancellation* token);
RocketAggregate* rocket_std_cancel_check(RocketCancellation* token);
RocketAggregate* rocket_std_async_time_deadline_after(std::int64_t milliseconds);
std::int64_t rocket_std_async_time_remaining(std::int64_t deadline);
RocketTask* rocket_std_async_time_sleep(std::int64_t milliseconds,
                                        RocketCancellation* token);
RocketTask* rocket_std_async_time_sleep_until(std::int64_t deadline,
                                              RocketCancellation* token);
RocketTask* rocket_std_async_file_read(RocketString* path, std::int64_t maximum,
                                       RocketCancellation* token);
RocketTask* rocket_std_async_file_write(RocketString* path, RocketArray* buffer,
                                        std::uint8_t append,
                                        RocketCancellation* token);
RocketTask* rocket_std_async_net_connect(RocketString* host, std::int64_t port,
                                         std::int64_t deadline,
                                         RocketCancellation* token);
RocketTask* rocket_std_async_net_accept(std::int64_t listener,
                                        std::int64_t deadline,
                                        RocketCancellation* token);
RocketTask* rocket_std_async_net_receive(std::int64_t socket,
                                         std::int64_t maximum,
                                         std::int64_t deadline,
                                         RocketCancellation* token);
RocketTask* rocket_std_async_net_send(std::int64_t socket, RocketArray* bytes,
                                      std::int64_t deadline,
                                      RocketCancellation* token);
RocketTask* rocket_std_async_process_run(RocketString* program,
                                         RocketArray* arguments,
                                         std::int64_t deadline,
                                         RocketCancellation* token);
RocketMutex* rocket_std_sync_mutex(void* value);
RocketMutex* rocket_std_sync_mutex_managed(void* value);
RocketMutex* rocket_std_sync_mutex_string(RocketString* value);
RocketMutex* rocket_std_sync_mutex_int(std::int64_t value);
RocketMutex* rocket_std_sync_mutex_float(double value);
RocketMutex* rocket_std_sync_mutex_bool(std::uint8_t value);
RocketMutex* rocket_std_sync_mutex_char(std::uint8_t value);
RocketAggregate* rocket_std_sync_lock(RocketMutex* mutex, std::int64_t deadline,
                                      RocketCancellation* token);
void* rocket_std_sync_guard_get(RocketGuard* guard);
void* rocket_std_sync_guard_get_managed(RocketGuard* guard);
RocketString* rocket_std_sync_guard_get_string(RocketGuard* guard);
std::int64_t rocket_std_sync_guard_get_int(RocketGuard* guard);
double rocket_std_sync_guard_get_float(RocketGuard* guard);
std::uint8_t rocket_std_sync_guard_get_bool(RocketGuard* guard);
std::uint8_t rocket_std_sync_guard_get_char(RocketGuard* guard);
std::uint8_t rocket_std_sync_guard_set(RocketGuard* guard, void* value);
std::uint8_t rocket_std_sync_guard_set_managed(RocketGuard* guard, void* value);
std::uint8_t rocket_std_sync_guard_set_string(RocketGuard* guard, RocketString* value);
std::uint8_t rocket_std_sync_guard_set_int(RocketGuard* guard, std::int64_t value);
std::uint8_t rocket_std_sync_guard_set_float(RocketGuard* guard, double value);
std::uint8_t rocket_std_sync_guard_set_bool(RocketGuard* guard, std::uint8_t value);
std::uint8_t rocket_std_sync_guard_set_char(RocketGuard* guard, std::uint8_t value);
RocketAggregate* rocket_std_sync_unlock(RocketGuard* guard);
RocketEvent* rocket_std_sync_event(std::uint8_t manualReset,
                                   std::uint8_t initiallySet);
std::uint8_t rocket_std_sync_event_set(RocketEvent* event);
std::uint8_t rocket_std_sync_event_reset(RocketEvent* event);
RocketAggregate* rocket_std_sync_event_wait(RocketEvent* event,
                                            std::int64_t deadline,
                                            RocketCancellation* token);
RocketAtomicInt* rocket_std_sync_atomic_int(std::int64_t value);
std::int64_t rocket_std_sync_atomic_load(RocketAtomicInt* value);
void rocket_std_sync_atomic_store(RocketAtomicInt* value, std::int64_t replacement);
std::int64_t rocket_std_sync_atomic_fetch_add(RocketAtomicInt* value,
                                              std::int64_t delta);
std::uint8_t rocket_std_sync_atomic_compare_exchange(RocketAtomicInt* value,
                                                     std::int64_t expected,
                                                     std::int64_t replacement);
RocketOnce* rocket_std_sync_once(void* value);
RocketOnce* rocket_std_sync_once_managed(void* value);
RocketOnce* rocket_std_sync_once_string(RocketString* value);
RocketOnce* rocket_std_sync_once_int(std::int64_t value);
RocketOnce* rocket_std_sync_once_float(double value);
RocketOnce* rocket_std_sync_once_bool(std::uint8_t value);
RocketOnce* rocket_std_sync_once_char(std::uint8_t value);
RocketOnce* rocket_std_sync_once_empty(void* witness);
RocketOnce* rocket_std_sync_once_empty_managed(void* witness);
RocketOnce* rocket_std_sync_once_empty_string(RocketString* witness);
RocketOnce* rocket_std_sync_once_empty_int(std::int64_t witness);
RocketOnce* rocket_std_sync_once_empty_float(double witness);
RocketOnce* rocket_std_sync_once_empty_bool(std::uint8_t witness);
RocketOnce* rocket_std_sync_once_empty_char(std::uint8_t witness);
RocketAggregate* rocket_std_sync_once_set(RocketOnce* cell, void* value);
RocketAggregate* rocket_std_sync_once_set_managed(RocketOnce* cell, void* value);
RocketAggregate* rocket_std_sync_once_set_string(RocketOnce* cell, RocketString* value);
RocketAggregate* rocket_std_sync_once_set_int(RocketOnce* cell, std::int64_t value);
RocketAggregate* rocket_std_sync_once_set_float(RocketOnce* cell, double value);
RocketAggregate* rocket_std_sync_once_set_bool(RocketOnce* cell, std::uint8_t value);
RocketAggregate* rocket_std_sync_once_set_char(RocketOnce* cell, std::uint8_t value);
RocketAggregate* rocket_std_sync_once_get(RocketOnce* cell);
RocketAggregate* rocket_std_sync_once_get_managed(RocketOnce* cell);
RocketAggregate* rocket_std_sync_once_get_string(RocketOnce* cell);
RocketAggregate* rocket_std_sync_once_get_int(RocketOnce* cell);
RocketAggregate* rocket_std_sync_once_get_float(RocketOnce* cell);
RocketAggregate* rocket_std_sync_once_get_bool(RocketOnce* cell);
RocketAggregate* rocket_std_sync_once_get_char(RocketOnce* cell);
RocketAggregate* rocket_std_channel_bounded(RocketArray* initial,
                                            std::int64_t capacity);
RocketAggregate* rocket_std_channel_unbounded(RocketArray* initial);
RocketSender* rocket_std_channel_sender(RocketAggregate* channel);
RocketReceiver* rocket_std_channel_receiver(RocketAggregate* channel);
RocketSender* rocket_std_channel_clone_sender(RocketSender* sender);
RocketReceiver* rocket_std_channel_clone_receiver(RocketReceiver* receiver);
RocketAggregate* rocket_std_channel_send(RocketSender* sender, void* value,
                                         std::int64_t deadline,
                                         RocketCancellation* token);
RocketAggregate* rocket_std_channel_send_managed(RocketSender* sender, void* value,
                                                 std::int64_t deadline,
                                                 RocketCancellation* token);
RocketAggregate* rocket_std_channel_send_string(RocketSender* sender,
                                                RocketString* value,
                                                std::int64_t deadline,
                                                RocketCancellation* token);
RocketAggregate* rocket_std_channel_send_int(RocketSender* sender,
                                             std::int64_t value,
                                             std::int64_t deadline,
                                             RocketCancellation* token);
RocketAggregate* rocket_std_channel_send_float(RocketSender* sender, double value,
                                               std::int64_t deadline,
                                               RocketCancellation* token);
RocketAggregate* rocket_std_channel_send_bool(RocketSender* sender,
                                              std::uint8_t value,
                                              std::int64_t deadline,
                                              RocketCancellation* token);
RocketAggregate* rocket_std_channel_send_char(RocketSender* sender,
                                              std::uint8_t value,
                                              std::int64_t deadline,
                                              RocketCancellation* token);
RocketAggregate* rocket_std_channel_receive(RocketReceiver* receiver,
                                            std::int64_t deadline,
                                            RocketCancellation* token);
RocketAggregate* rocket_std_channel_close_sender(RocketSender* sender);
RocketAggregate* rocket_std_channel_close_receiver(RocketReceiver* receiver);

RocketString* rocket_rt_string_new(const std::uint8_t* bytes, std::uint64_t length);
std::uint8_t rocket_rt_string_equal(const RocketString* left, const RocketString* right);
std::uint64_t rocket_rt_string_byte_length(const RocketString* string);
const std::uint8_t* rocket_rt_string_bytes(const RocketString* string);

RocketArray* rocket_rt_array_new(std::uint32_t elementKind, std::uint64_t length);
std::uint64_t rocket_rt_array_capacity(const RocketArray* array);
RocketArray* rocket_rt_array_reserve(RocketArray* array, std::int64_t minimumCapacity);
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
RocketArray* rocket_rt_array_append_int(RocketArray* array, std::int64_t value);
RocketArray* rocket_rt_array_append_float(RocketArray* array, double value);
RocketArray* rocket_rt_array_append_bool(RocketArray* array, std::uint8_t value);
RocketArray* rocket_rt_array_append_char(RocketArray* array, std::uint8_t value);
RocketArray* rocket_rt_array_append_string(RocketArray* array, RocketString* value);
RocketArray* rocket_rt_array_append_managed(RocketArray* array, void* value);
RocketAggregate* rocket_rt_array_pop(RocketArray* array);
RocketArray* rocket_rt_array_insert_int(RocketArray* array, std::int64_t index,
                                        std::int64_t value);
RocketArray* rocket_rt_array_insert_float(RocketArray* array, std::int64_t index,
                                          double value);
RocketArray* rocket_rt_array_insert_bool(RocketArray* array, std::int64_t index,
                                         std::uint8_t value);
RocketArray* rocket_rt_array_insert_char(RocketArray* array, std::int64_t index,
                                         std::uint8_t value);
RocketArray* rocket_rt_array_insert_string(RocketArray* array, std::int64_t index,
                                           RocketString* value);
RocketArray* rocket_rt_array_insert_managed(RocketArray* array, std::int64_t index,
                                            void* value);
RocketAggregate* rocket_rt_array_remove(RocketArray* array, std::int64_t index);
RocketArray* rocket_rt_array_clear(RocketArray* array);

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

// Opt-in compiler instrumentation hook. Kind 1 records a coverage point and
// kind 2 records a symbol sample. Reports are written at process exit only
// when the corresponding ROCKET_COVERAGE_FILE/ROCKET_PROFILE_FILE variable is
// explicitly set by a tooling command.
void rocket_rt_tooling_hit(const char* source, std::int64_t line,
                           const char* symbol, std::uint32_t kind);

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
std::int64_t rocket_std_collections_capacity(RocketArray* values);
RocketArray* rocket_std_collections_reserve(RocketArray* values,
                                            std::int64_t minimumCapacity);
RocketArray* rocket_std_collections_append_int(RocketArray* values, std::int64_t value);
RocketArray* rocket_std_collections_append_float(RocketArray* values, double value);
RocketArray* rocket_std_collections_append_bool(RocketArray* values, std::uint8_t value);
RocketArray* rocket_std_collections_append_char(RocketArray* values, std::uint8_t value);
RocketArray* rocket_std_collections_append_string(RocketArray* values, RocketString* value);
RocketArray* rocket_std_collections_append_managed(RocketArray* values, void* value);
RocketAggregate* rocket_std_collections_pop(RocketArray* values);
RocketArray* rocket_std_collections_insert_int(RocketArray* values, std::int64_t index,
                                               std::int64_t value);
RocketArray* rocket_std_collections_insert_float(RocketArray* values, std::int64_t index,
                                                 double value);
RocketArray* rocket_std_collections_insert_bool(RocketArray* values, std::int64_t index,
                                                std::uint8_t value);
RocketArray* rocket_std_collections_insert_char(RocketArray* values, std::int64_t index,
                                                std::uint8_t value);
RocketArray* rocket_std_collections_insert_string(RocketArray* values, std::int64_t index,
                                                  RocketString* value);
RocketArray* rocket_std_collections_insert_managed(RocketArray* values, std::int64_t index,
                                                   void* value);
RocketAggregate* rocket_std_collections_remove(RocketArray* values, std::int64_t index);
RocketArray* rocket_std_collections_clear(RocketArray* values);
RocketAggregate* rocket_std_collections_map_from_arrays(RocketArray* keys,
                                                        RocketArray* values);
std::int64_t rocket_std_collections_map_length(RocketAggregate* map);
RocketAggregate* rocket_std_collections_map_find_int(RocketAggregate* map, std::int64_t key);
RocketAggregate* rocket_std_collections_map_find_bool(RocketAggregate* map, std::uint8_t key);
RocketAggregate* rocket_std_collections_map_find_char(RocketAggregate* map, std::uint8_t key);
RocketAggregate* rocket_std_collections_map_find_string(RocketAggregate* map, RocketString* key);
RocketAggregate* rocket_std_collections_map_get_int(RocketAggregate* map, std::int64_t key);
RocketAggregate* rocket_std_collections_map_get_bool(RocketAggregate* map, std::uint8_t key);
RocketAggregate* rocket_std_collections_map_get_char(RocketAggregate* map, std::uint8_t key);
RocketAggregate* rocket_std_collections_map_get_string(RocketAggregate* map, RocketString* key);
RocketArray* rocket_std_collections_map_keys(RocketAggregate* map);
RocketArray* rocket_std_collections_map_values(RocketAggregate* map);
RocketAggregate* rocket_std_collections_set_from_array(RocketArray* values);
std::uint8_t rocket_std_collections_set_contains_int(RocketAggregate* set, std::int64_t value);
std::uint8_t rocket_std_collections_set_contains_bool(RocketAggregate* set, std::uint8_t value);
std::uint8_t rocket_std_collections_set_contains_char(RocketAggregate* set, std::uint8_t value);
std::uint8_t rocket_std_collections_set_contains_string(RocketAggregate* set,
                                                        RocketString* value);
RocketArray* rocket_std_collections_set_values(RocketAggregate* set);
std::int64_t rocket_std_collections_hash_int(std::int64_t value);
std::int64_t rocket_std_collections_hash_bool(std::uint8_t value);
std::int64_t rocket_std_collections_hash_char(std::uint8_t value);
std::int64_t rocket_std_collections_hash_string(RocketString* value);
std::uint8_t rocket_std_collections_contains_int(RocketArray* values, std::int64_t value);
std::uint8_t rocket_std_collections_contains_float(RocketArray* values, double value);
std::uint8_t rocket_std_collections_contains_bool(RocketArray* values, std::uint8_t value);
std::uint8_t rocket_std_collections_contains_char(RocketArray* values, std::uint8_t value);
std::uint8_t rocket_std_collections_contains_string(RocketArray* values, RocketString* value);
RocketAggregate* rocket_std_collections_find_int(RocketArray* values, std::int64_t value);
RocketAggregate* rocket_std_collections_find_float(RocketArray* values, double value);
RocketAggregate* rocket_std_collections_find_bool(RocketArray* values, std::uint8_t value);
RocketAggregate* rocket_std_collections_find_char(RocketArray* values, std::uint8_t value);
RocketAggregate* rocket_std_collections_find_string(RocketArray* values, RocketString* value);
RocketArray* rocket_std_collections_filter_equal_int(RocketArray* values, std::int64_t value);
RocketArray* rocket_std_collections_filter_equal_float(RocketArray* values, double value);
RocketArray* rocket_std_collections_filter_equal_bool(RocketArray* values, std::uint8_t value);
RocketArray* rocket_std_collections_filter_equal_char(RocketArray* values, std::uint8_t value);
RocketArray* rocket_std_collections_filter_equal_string(RocketArray* values, RocketString* value);
RocketArray* rocket_std_collections_sort_int(RocketArray* values);
RocketArray* rocket_std_collections_sort_float(RocketArray* values);
RocketArray* rocket_std_collections_sort_char(RocketArray* values);
RocketArray* rocket_std_collections_sort_string(RocketArray* values);
RocketArray* rocket_std_collections_map_hash_int(RocketArray* values);
RocketArray* rocket_std_collections_map_hash_bool(RocketArray* values);
RocketArray* rocket_std_collections_map_hash_char(RocketArray* values);
RocketArray* rocket_std_collections_map_hash_string(RocketArray* values);
std::int64_t rocket_std_collections_fold_sum_int(RocketArray* values);
double rocket_std_collections_fold_sum_float(RocketArray* values);
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
RocketAggregate* rocket_std_file_read_binary(RocketString* path);
RocketAggregate* rocket_std_file_write_binary(RocketString* path,
                                               RocketAggregate* buffer);
RocketAggregate* rocket_std_file_append_binary(RocketString* path,
                                                RocketAggregate* buffer);

RocketAggregate* rocket_std_binary_from_string(RocketString* value);
RocketAggregate* rocket_std_binary_to_string(RocketAggregate* buffer);
std::int64_t rocket_std_binary_length(RocketAggregate* buffer);
RocketAggregate* rocket_std_binary_slice(RocketAggregate* buffer,
                                         std::int64_t offset,
                                         std::int64_t length);
RocketAggregate* rocket_std_binary_read_u8(RocketAggregate* buffer,
                                           std::int64_t offset);
RocketAggregate* rocket_std_binary_read_u16_le(RocketAggregate* buffer,
                                               std::int64_t offset);
RocketAggregate* rocket_std_binary_read_u32_le(RocketAggregate* buffer,
                                               std::int64_t offset);
RocketAggregate* rocket_std_binary_write_u8(std::int64_t value);
RocketAggregate* rocket_std_binary_write_u16_le(std::int64_t value);
RocketAggregate* rocket_std_binary_write_u32_le(std::int64_t value);
RocketAggregate* rocket_std_binary_concat(RocketAggregate* left,
                                           RocketAggregate* right);
RocketAggregate* rocket_std_binary_read_u16_be(RocketAggregate* buffer,
                                                std::int64_t offset);
RocketAggregate* rocket_std_binary_read_u32_be(RocketAggregate* buffer,
                                                std::int64_t offset);
RocketAggregate* rocket_std_binary_write_u16_be(std::int64_t value);
RocketAggregate* rocket_std_binary_write_u32_be(std::int64_t value);

RocketAggregate* rocket_std_stream_open_reader(RocketString* path,
                                                std::int64_t bufferSize);
RocketAggregate* rocket_std_stream_read(std::int64_t handle,
                                        std::int64_t maximumBytes);
RocketAggregate* rocket_std_stream_close_reader(std::int64_t handle);
RocketAggregate* rocket_std_stream_open_writer(RocketString* path,
                                                std::int64_t bufferSize,
                                                std::uint8_t append);
RocketAggregate* rocket_std_stream_write(std::int64_t handle,
                                         RocketAggregate* buffer);
RocketAggregate* rocket_std_stream_flush(std::int64_t handle);
RocketAggregate* rocket_std_stream_close_writer(std::int64_t handle);

std::int64_t rocket_std_unicode_scalar_count(RocketString* value);
RocketAggregate* rocket_std_unicode_scalar_at(RocketString* value,
                                               std::int64_t index);
RocketAggregate* rocket_std_unicode_from_scalar(std::int64_t scalar);
RocketAggregate* rocket_std_unicode_normalize_nfc(RocketString* value);
RocketAggregate* rocket_std_unicode_normalize_nfd(RocketString* value);
std::int64_t rocket_std_unicode_grapheme_count(RocketString* value);
RocketAggregate* rocket_std_unicode_grapheme_at(RocketString* value,
                                                 std::int64_t index);

RocketAggregate* rocket_std_regex_is_match(RocketString* pattern,
                                            RocketString* value);
RocketAggregate* rocket_std_regex_find_all(RocketString* pattern,
                                            RocketString* value);
RocketAggregate* rocket_std_regex_replace_all(RocketString* pattern,
                                               RocketString* value,
                                               RocketString* replacement);

RocketAggregate* rocket_std_crypto_secure_bytes(std::int64_t length);
RocketAggregate* rocket_std_crypto_secure_int(std::int64_t minimum,
                                               std::int64_t maximum);
RocketAggregate* rocket_std_crypto_sha256(RocketAggregate* value);
RocketAggregate* rocket_std_crypto_hmac_sha256(RocketAggregate* key,
                                                RocketAggregate* value);
std::uint8_t rocket_std_crypto_constant_time_equal(RocketAggregate* left,
                                                    RocketAggregate* right);
RocketAggregate* rocket_std_crypto_verify_signed_file(RocketString* path);

RocketAggregate* rocket_std_net_resolve(RocketString* host, RocketString* service);
RocketAggregate* rocket_std_net_tcp_connect(RocketString* host, std::int64_t port,
                                             std::int64_t timeoutMilliseconds);
RocketAggregate* rocket_std_net_tcp_listen(RocketString* address, std::int64_t port,
                                            std::int64_t backlog);
RocketAggregate* rocket_std_net_accept(std::int64_t listener,
                                        std::int64_t timeoutMilliseconds);
RocketAggregate* rocket_std_net_send(std::int64_t handle, RocketAggregate* buffer,
                                      std::int64_t timeoutMilliseconds);
RocketAggregate* rocket_std_net_receive(std::int64_t handle,
                                         std::int64_t maximumBytes,
                                         std::int64_t timeoutMilliseconds);
RocketAggregate* rocket_std_net_close(std::int64_t handle);
RocketAggregate* rocket_std_net_cancel(std::int64_t handle);
RocketAggregate* rocket_std_net_local_port(std::int64_t handle);

RocketAggregate* rocket_std_http_request(RocketString* method, RocketString* url,
                                          RocketAggregate* body,
                                          std::int64_t timeoutMilliseconds);
RocketAggregate* rocket_std_http_read_request(std::int64_t handle,
                                               std::int64_t maximumBytes,
                                               std::int64_t timeoutMilliseconds);
RocketAggregate* rocket_std_http_write_response(std::int64_t handle,
                                                 std::int64_t status,
                                                 RocketString* contentType,
                                                 RocketAggregate* body,
                                                 std::int64_t timeoutMilliseconds);

RocketAggregate* rocket_std_datetime_format_utc(std::int64_t unixMilliseconds);
RocketAggregate* rocket_std_datetime_parse_utc(RocketString* value);
RocketAggregate* rocket_std_datetime_days_in_month(std::int64_t year,
                                                    std::int64_t month);
RocketAggregate* rocket_std_datetime_weekday(std::int64_t year,
                                              std::int64_t month,
                                              std::int64_t day);
RocketAggregate* rocket_std_datetime_local_offset_minutes(
    std::int64_t unixMilliseconds);
RocketAggregate* rocket_std_datetime_timezone_name();
RocketAggregate* rocket_std_log_write(RocketString* level, RocketString* message);
RocketAggregate* rocket_std_log_append(RocketString* path, RocketString* level,
                                        RocketString* message);
std::uint8_t rocket_std_cli_has_flag(RocketArray* arguments, RocketString* name);
RocketAggregate* rocket_std_cli_option(RocketArray* arguments, RocketString* name);
RocketArray* rocket_std_cli_positionals(RocketArray* arguments);
RocketAggregate* rocket_std_config_get(RocketString* text, RocketString* key);
RocketAggregate* rocket_std_config_load(RocketString* path, RocketString* key);
RocketAggregate* rocket_std_compression_xpress_compress(RocketAggregate* value);
RocketAggregate* rocket_std_compression_xpress_decompress(RocketAggregate* value);
RocketAggregate* rocket_std_archive_tar_create(RocketString* path,
                                                RocketArray* names,
                                                RocketArray* contents);
RocketAggregate* rocket_std_archive_tar_list(RocketString* path);
RocketAggregate* rocket_std_archive_tar_read(RocketString* path,
                                              RocketString* name);
RocketAggregate* rocket_std_sqlite_open(RocketString* path);
RocketAggregate* rocket_std_sqlite_execute(std::int64_t handle, RocketString* sql,
                                            RocketArray* parameters);
RocketAggregate* rocket_std_sqlite_query(std::int64_t handle, RocketString* sql,
                                          RocketArray* parameters);
RocketAggregate* rocket_std_sqlite_close(std::int64_t handle);
RocketAggregate* rocket_std_testing_assert(std::uint8_t condition,
                                            RocketString* message);
RocketAggregate* rocket_std_testing_equal_int(std::int64_t expected,
                                               std::int64_t actual,
                                               RocketString* message);
RocketAggregate* rocket_std_testing_equal_string(RocketString* expected,
                                                  RocketString* actual,
                                                  RocketString* message);
RocketAggregate* rocket_std_testing_temp_directory(RocketString* prefix);
RocketAggregate* rocket_std_testing_fixture_path(RocketString* root,
                                                  RocketString* relative);
RocketAggregate* rocket_std_testing_cleanup_temp(RocketString* path);
RocketAggregate* rocket_std_testing_coverage_hit(RocketString* name);
RocketAggregate* rocket_std_testing_coverage_write(RocketString* path);

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

RocketString* rocket_std_target_alias();
RocketString* rocket_std_target_triple();
RocketString* rocket_std_target_os();
RocketString* rocket_std_target_architecture();
RocketString* rocket_std_target_environment();
std::int64_t rocket_std_target_pointer_width();
RocketString* rocket_std_target_endianness();
std::uint8_t rocket_std_target_has_feature(RocketString* name);

std::int64_t rocket_std_time_unix_milliseconds();
std::int64_t rocket_std_time_monotonic_milliseconds();
void rocket_std_time_sleep_milliseconds(std::int64_t milliseconds);

double rocket_std_math_pi(); double rocket_std_math_tau(); double rocket_std_math_e();
double rocket_std_math_abs(double value); std::int64_t rocket_std_math_abs_int(std::int64_t value);
double rocket_std_math_min(double left, double right); double rocket_std_math_max(double left, double right);
std::int64_t rocket_std_math_min_int(std::int64_t left, std::int64_t right); std::int64_t rocket_std_math_max_int(std::int64_t left, std::int64_t right);
double rocket_std_math_clamp(double value, double minimum, double maximum); std::int64_t rocket_std_math_clamp_int(std::int64_t value, std::int64_t minimum, std::int64_t maximum);
double rocket_std_math_sign(double value); std::int64_t rocket_std_math_sign_int(std::int64_t value);
double rocket_std_math_floor(double value); double rocket_std_math_ceil(double value); double rocket_std_math_round(double value); double rocket_std_math_trunc(double value); double rocket_std_math_fract(double value);
double rocket_std_math_sqrt(double value); double rocket_std_math_pow(double base, double exponent); double rocket_std_math_exp(double value); double rocket_std_math_log(double value); double rocket_std_math_log10(double value);
double rocket_std_math_sin(double radians); double rocket_std_math_cos(double radians); double rocket_std_math_tan(double radians); double rocket_std_math_asin(double value); double rocket_std_math_acos(double value); double rocket_std_math_atan(double value); double rocket_std_math_atan2(double y, double x);
double rocket_std_math_radians(double degrees); double rocket_std_math_degrees(double radians);
double rocket_std_math_lerp(double start, double end, double progress); double rocket_std_math_inverse_lerp(double start, double end, double value); double rocket_std_math_remap(double input_start, double input_end, double output_start, double output_end, double value);
double rocket_std_math_smoothstep(double start, double end, double value); double rocket_std_math_smootherstep(double start, double end, double value); double rocket_std_math_approach(double current, double target, double maximum_delta); double rocket_std_math_move_towards(double current, double target, double maximum_delta);

} // extern "C"
