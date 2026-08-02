#pragma once

#include "platform_net.h"
#include "platform_crypto.h"
#include "platform_datetime.h"
#include "platform_compression.h"
#include "platform_sqlite.h"
#include "safe_regex.h"
#include "safe_archive.h"

// This header is included by C++ emitted from the permanently preserved Stage 0
// backend. It implements the public standard-library surface with the Stage 0
// RAII value representation; the production backend uses stdlib.cpp and ABI v1.

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cctype>
#include <condition_variable>
#include <deque>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>

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

inline RocketAggregate rocket_std_task_join(const RocketTask& task) {
  return rocket_await(task);
}

inline bool rocket_std_task_is_complete(const RocketTask& task) {
  return task->future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
}
inline bool rocket_std_task_cancel(const RocketTask& task) {
  if (rocket_std_task_is_complete(task)) return false;
  return !task->cancellation->cancelled.exchange(true, std::memory_order_acq_rel);
}

template <typename T> struct RocketTaskGroupState {
  std::vector<RocketTask> tasks;
  bool joined = false;
};
template <typename T> using RocketTaskGroup = std::shared_ptr<RocketTaskGroupState<T>>;
template <typename T> RocketTaskGroup<T> rocket_std_task_group(
    const RocketArray<RocketTask>& tasks) {
  auto group = std::make_shared<RocketTaskGroupState<T>>();
  group->tasks.assign(tasks->begin(), tasks->end()); return group;
}
template <typename T> RocketAggregate rocket_std_task_group_join(
    const RocketTaskGroup<T>& group) {
  if (group->joined) return rocket_stage0_error("TaskGroup was already joined");
  group->joined = true;
  auto values = std::make_shared<std::vector<T>>();
  for (const RocketTask& task : group->tasks) {
    RocketAggregate result = rocket_await(task);
    if (result->tag != 0) return result;
    values->push_back(rocket_field<T>(result, 0));
  }
  return rocket_stage0_ok(values);
}
template <typename T> bool rocket_std_task_group_cancel(const RocketTaskGroup<T>& group) {
  bool changed = false;
  for (const RocketTask& task : group->tasks)
    changed = rocket_std_task_cancel(task) || changed;
  return changed;
}

template <typename T> struct RocketThreadState {
  RocketTask task;
  std::thread worker;
  std::mutex mutex;
  std::condition_variable completed;
  RocketAggregate result;
  bool finished = false;
  bool consumed = false;
  ~RocketThreadState() {
    if (worker.joinable()) {
      if (worker.get_id() == std::this_thread::get_id()) worker.detach();
      else worker.join();
    }
  }
};
template <typename T> using RocketThread = std::shared_ptr<RocketThreadState<T>>;
template <typename T> RocketAggregate rocket_std_thread_spawn(const RocketTask& task) {
  auto thread = std::make_shared<RocketThreadState<T>>();
  thread->task = task;
  try {
    thread->worker = std::thread([thread] {
      RocketAggregate result = rocket_await(thread->task);
      {
        std::lock_guard lock(thread->mutex);
        thread->result = std::move(result);
        thread->finished = true;
      }
      thread->completed.notify_all();
    });
  } catch (const std::system_error& error) {
    return rocket_stage0_error(error.what());
  }
  return rocket_stage0_ok(thread);
}
template <typename T> RocketAggregate rocket_std_thread_join(const RocketThread<T>& thread) {
  {
    std::lock_guard lock(thread->mutex);
    if (thread->consumed) return rocket_stage0_error("Thread was already joined or detached");
    thread->consumed = true;
  }
  if (thread->worker.joinable()) thread->worker.join();
  std::lock_guard lock(thread->mutex);
  return thread->result;
}
template <typename T> RocketAggregate rocket_std_thread_detach(const RocketThread<T>& thread) {
  {
    std::lock_guard lock(thread->mutex);
    if (thread->consumed) return rocket_stage0_error("Thread was already joined or detached");
    thread->consumed = true;
  }
  if (thread->worker.joinable()) thread->worker.detach();
  return rocket_stage0_ok(true);
}
template <typename T> bool rocket_std_thread_is_complete(const RocketThread<T>& thread) {
  std::lock_guard lock(thread->mutex);
  return thread->finished;
}

template <typename T> RocketWeak<T> rocket_std_ownership_downgrade(const T& value) {
  return RocketWeak<T>{value};
}

template <typename T> RocketAggregate rocket_std_ownership_upgrade(
    const RocketWeak<T>& weak) {
  auto value = weak.value.lock();
  return value ? rocket_stage0_variant(0, {T{std::move(value)}})
               : rocket_stage0_variant(1);
}

template <typename T> bool rocket_std_ownership_expired(const RocketWeak<T>& weak) {
  return weak.value.expired();
}

template <typename T> RocketUniqueBuffer<T> rocket_std_buffer_thaw(
    const RocketArray<T>& values) {
  return std::make_shared<std::vector<T>>(*values);
}

template <typename T> std::int64_t rocket_std_buffer_length(
    const RocketUniqueBuffer<T>& buffer) {
  return static_cast<std::int64_t>(buffer->size());
}

template <typename T> std::int64_t rocket_std_buffer_capacity(
    const RocketUniqueBuffer<T>& buffer) {
  return static_cast<std::int64_t>(buffer->capacity());
}

template <typename T> T rocket_std_buffer_get(const RocketUniqueBuffer<T>& buffer,
                                               std::int64_t index) {
  if (index < 0 || index >= static_cast<std::int64_t>(buffer->size()))
    rocket_bounds_error();
  return (*buffer)[static_cast<std::size_t>(index)];
}

template <typename T> RocketUniqueBuffer<T> rocket_std_buffer_set(
    const RocketUniqueBuffer<T>& buffer, std::int64_t index, T value) {
  if (index < 0 || index >= static_cast<std::int64_t>(buffer->size()))
    rocket_bounds_error();
  (*buffer)[static_cast<std::size_t>(index)] = std::move(value);
  return buffer;
}

template <typename T> RocketUniqueBuffer<T> rocket_std_buffer_append(
    const RocketUniqueBuffer<T>& buffer, T value) {
  buffer->push_back(std::move(value));
  return buffer;
}

template <typename T> RocketUniqueBuffer<T> rocket_std_buffer_slice(
    const RocketUniqueBuffer<T>& buffer, std::int64_t start, std::int64_t end) {
  if (start < 0 || end < start || end > static_cast<std::int64_t>(buffer->size()))
    rocket_bounds_error();
  return std::make_shared<std::vector<T>>(buffer->begin() + start,
                                          buffer->begin() + end);
}

template <typename T> RocketArray<T> rocket_std_buffer_freeze(
    const RocketUniqueBuffer<T>& buffer) {
  return buffer;
}

inline RocketCancellation rocket_std_cancel_token() {
  return std::make_shared<RocketCancellationState>();
}
inline RocketCancellation rocket_std_cancel_child(const RocketCancellation& parent) {
  auto child = rocket_std_cancel_token(); child->parent = parent; return child;
}
inline RocketCancellation rocket_std_cancel_current() {
  return rocket_stage0_current_cancellation
      ? rocket_stage0_current_cancellation : rocket_std_cancel_token();
}
inline bool rocket_std_cancel_is_cancelled(const RocketCancellation& token) {
  return rocket_stage0_token_cancelled(token);
}
inline bool rocket_stage0_operation_cancelled(const RocketCancellation& token) {
  return rocket_stage0_token_cancelled(token) ||
      (rocket_stage0_current_cancellation != token &&
       rocket_stage0_token_cancelled(rocket_stage0_current_cancellation));
}
inline bool rocket_std_cancel_cancel(const RocketCancellation& token) {
  return !token->cancelled.exchange(true, std::memory_order_acq_rel);
}
inline RocketAggregate rocket_std_cancel_check(const RocketCancellation& token) {
  return rocket_std_cancel_is_cancelled(token)
      ? rocket_stage0_error("operation cancelled") : rocket_stage0_ok(true);
}
inline std::int64_t rocket_stage0_monotonic_milliseconds() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline RocketAggregate rocket_std_async_time_deadline_after(std::int64_t milliseconds) {
  if (milliseconds < 0) return rocket_stage0_error("deadline duration cannot be negative");
  return rocket_stage0_ok(rocket_stage0_monotonic_milliseconds() + milliseconds);
}
inline std::int64_t rocket_std_async_time_remaining(std::int64_t deadline) {
  return (std::max)(std::int64_t{0}, deadline - rocket_stage0_monotonic_milliseconds());
}
inline RocketTask rocket_std_async_time_sleep_until(
    std::int64_t deadline, const RocketCancellation& token) {
  return rocket_task([deadline, token] {
#ifdef _WIN32
    HANDLE timer = CreateWaitableTimerExW(
        nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!timer) timer = CreateWaitableTimerW(nullptr, TRUE, nullptr);
    if (!timer)
      return rocket_stage0_error("could not create Windows waitable timer");
    while (rocket_stage0_monotonic_milliseconds() < deadline) {
      if (rocket_stage0_operation_cancelled(token)) {
        CancelWaitableTimer(timer); CloseHandle(timer);
        return rocket_stage0_error("operation cancelled");
      }
      LARGE_INTEGER due{};
      due.QuadPart = -(std::max)(std::int64_t{1}, (std::min)(
          deadline - rocket_stage0_monotonic_milliseconds(),
          std::int64_t{86400000})) * 10000;
      if (!SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
        CloseHandle(timer);
        return rocket_stage0_error("could not arm Windows waitable timer");
      }
      while (WaitForSingleObject(timer, 2) == WAIT_TIMEOUT) {
        if (rocket_stage0_operation_cancelled(token)) {
          CancelWaitableTimer(timer); CloseHandle(timer);
          return rocket_stage0_error("operation cancelled");
        }
      }
    }
    CloseHandle(timer);
#else
    while (rocket_stage0_monotonic_milliseconds() < deadline) {
      if (rocket_stage0_operation_cancelled(token))
        return rocket_stage0_error("operation cancelled");
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
#endif
    return rocket_stage0_ok(true);
  });
}
inline RocketTask rocket_std_async_time_sleep(
    std::int64_t milliseconds, const RocketCancellation& token) {
  if (milliseconds < 0)
    return rocket_task([] { return rocket_stage0_error("sleep duration cannot be negative"); });
  return rocket_std_async_time_sleep_until(
      rocket_stage0_monotonic_milliseconds() + milliseconds, token);
}
inline RocketTask rocket_std_async_file_read(
    const std::string& path, std::int64_t maximum,
    const RocketCancellation& token) {
  return rocket_task([path, maximum, token] {
    if (maximum < 0 || maximum > 67108864)
      return rocket_stage0_error("asynchronous file read maximum must be from 0 through 67108864");
    if (rocket_stage0_operation_cancelled(token))
      return rocket_stage0_error("operation cancelled");
    std::ifstream input(std::filesystem::u8path(path), std::ios::binary | std::ios::ate);
    if (!input) return rocket_stage0_error("could not open file for asynchronous reading");
    const std::streamoff size = input.tellg();
    if (size < 0 || size > maximum)
      return rocket_stage0_error("asynchronous file read exceeds its maximum byte count");
    input.seekg(0, std::ios::beg);
    auto bytes = std::make_shared<std::vector<char>>(static_cast<std::size_t>(size));
    input.read(bytes->data(), size);
    if (!input && size != 0) return rocket_stage0_error("asynchronous file read failed before completion");
    if (rocket_stage0_operation_cancelled(token))
      return rocket_stage0_error("operation cancelled");
    return rocket_stage0_ok(bytes);
  });
}
inline RocketTask rocket_std_async_file_write(
    const std::string& path, const RocketUniqueBuffer<char>& buffer, bool append,
    const RocketCancellation& token) {
  return rocket_task([path, buffer, append, token] {
    if (buffer->size() > 67108864)
      return rocket_stage0_error("asynchronous file write exceeds 67108864 bytes");
    if (rocket_stage0_operation_cancelled(token))
      return rocket_stage0_error("operation cancelled");
    std::ofstream output(std::filesystem::u8path(path), std::ios::binary |
        (append ? std::ios::app : std::ios::trunc));
    if (!output) return rocket_stage0_error("could not open file for asynchronous writing");
    output.write(buffer->data(), static_cast<std::streamsize>(buffer->size()));
    if (!output) return rocket_stage0_error("asynchronous file write failed before completion");
    return rocket_stage0_operation_cancelled(token)
        ? rocket_stage0_error("operation cancelled") : rocket_stage0_ok(true);
  });
}

template <typename T> struct RocketMutexState {
  std::timed_mutex mutex;
  T value;
  explicit RocketMutexState(T initial) : value(std::move(initial)) {}
};
template <typename T> using RocketMutex = std::shared_ptr<RocketMutexState<T>>;
template <typename T> struct RocketLockGuardState {
  RocketMutex<T> owner;
  std::unique_lock<std::timed_mutex> lock;
  explicit RocketLockGuardState(RocketMutex<T> value)
      : owner(std::move(value)), lock(owner->mutex, std::adopt_lock) {}
};
template <typename T> using RocketLockGuard = std::shared_ptr<RocketLockGuardState<T>>;

template <typename T> RocketMutex<T> rocket_std_sync_mutex(T value) {
  return std::make_shared<RocketMutexState<T>>(std::move(value));
}
template <typename T> RocketAggregate rocket_std_sync_lock(
    const RocketMutex<T>& mutex, std::int64_t deadline,
    const RocketCancellation& token) {
  while (!mutex->mutex.try_lock_for(std::chrono::milliseconds(2))) {
    if (rocket_stage0_operation_cancelled(token))
      return rocket_stage0_error("operation cancelled");
    if (deadline >= 0 && rocket_stage0_monotonic_milliseconds() >= deadline)
      return rocket_stage0_error("mutex lock timed out");
  }
  return rocket_stage0_ok(std::make_shared<RocketLockGuardState<T>>(mutex));
}
template <typename T> T rocket_std_sync_guard_get(const RocketLockGuard<T>& guard) {
  return guard->owner->value;
}
template <typename T> bool rocket_std_sync_guard_set(
    const RocketLockGuard<T>& guard, T value) {
  guard->owner->value = std::move(value); return true;
}
template <typename T> RocketAggregate rocket_std_sync_unlock(
    const RocketLockGuard<T>& guard) {
  if (!guard->lock.owns_lock()) return rocket_stage0_ok(false);
  guard->lock.unlock(); return rocket_stage0_ok(true);
}

struct RocketEventState {
  std::mutex mutex; std::condition_variable changed;
  bool manual{}; bool set{};
};
using RocketEvent = std::shared_ptr<RocketEventState>;
inline RocketEvent rocket_std_sync_event(bool manual, bool initiallySet) {
  auto event = std::make_shared<RocketEventState>();
  event->manual = manual; event->set = initiallySet; return event;
}
inline bool rocket_std_sync_event_set(const RocketEvent& event) {
  { std::lock_guard lock(event->mutex); event->set = true; }
  if (event->manual) event->changed.notify_all(); else event->changed.notify_one();
  return true;
}
inline bool rocket_std_sync_event_reset(const RocketEvent& event) {
  std::lock_guard lock(event->mutex); const bool prior = event->set;
  event->set = false; return prior;
}
inline RocketAggregate rocket_std_sync_event_wait(
    const RocketEvent& event, std::int64_t deadline,
    const RocketCancellation& token) {
  std::unique_lock lock(event->mutex);
  while (!event->set) {
    if (rocket_stage0_operation_cancelled(token))
      return rocket_stage0_error("operation cancelled");
    if (deadline >= 0 && rocket_stage0_monotonic_milliseconds() >= deadline)
      return rocket_stage0_error("event wait timed out");
    event->changed.wait_for(lock, std::chrono::milliseconds(2));
  }
  if (!event->manual) event->set = false;
  return rocket_stage0_ok(true);
}

using RocketAtomicInt = std::shared_ptr<std::atomic<std::int64_t>>;
inline RocketAtomicInt rocket_std_sync_atomic_int(std::int64_t value) {
  return std::make_shared<std::atomic<std::int64_t>>(value);
}
inline std::int64_t rocket_std_sync_atomic_load(const RocketAtomicInt& value) {
  return value->load(std::memory_order_seq_cst);
}
inline RocketUnit rocket_std_sync_atomic_store(
    const RocketAtomicInt& value, std::int64_t replacement) {
  value->store(replacement, std::memory_order_seq_cst); return {};
}
inline std::int64_t rocket_std_sync_atomic_fetch_add(
    const RocketAtomicInt& value, std::int64_t delta) {
  return value->fetch_add(delta, std::memory_order_seq_cst);
}
inline bool rocket_std_sync_atomic_compare_exchange(
    const RocketAtomicInt& value, std::int64_t expected, std::int64_t replacement) {
  return value->compare_exchange_strong(expected, replacement, std::memory_order_seq_cst);
}

template <typename T> struct RocketOnceState { std::mutex mutex; std::optional<T> value; };
template <typename T> using RocketOnce = std::shared_ptr<RocketOnceState<T>>;
template <typename T> RocketOnce<T> rocket_std_sync_once(T value) {
  auto once = std::make_shared<RocketOnceState<T>>(); once->value = std::move(value); return once;
}
template <typename T> RocketOnce<T> rocket_std_sync_once_empty(const T&) {
  return std::make_shared<RocketOnceState<T>>();
}
template <typename T> RocketAggregate rocket_std_sync_once_set(
    const RocketOnce<T>& once, T value) {
  std::lock_guard lock(once->mutex);
  if (once->value) return rocket_stage0_ok(false);
  once->value = std::move(value); return rocket_stage0_ok(true);
}
template <typename T> RocketAggregate rocket_std_sync_once_get(const RocketOnce<T>& once) {
  std::lock_guard lock(once->mutex);
  return once->value ? rocket_stage0_variant(0, {*once->value}) : rocket_stage0_variant(1);
}

template <typename T> struct RocketChannelState {
  std::mutex mutex; std::condition_variable readable, writable;
  std::deque<T> values; std::size_t maximum{}; std::size_t senders{}; std::size_t receivers{};
};
template <typename T> struct RocketChannelEndpoint {
  std::shared_ptr<RocketChannelState<T>> state;
  std::shared_ptr<std::atomic<bool>> open;
  bool sender{};
  RocketChannelEndpoint() = default;
  RocketChannelEndpoint(std::shared_ptr<RocketChannelState<T>> value, bool sends)
      : state(std::move(value)), open(std::make_shared<std::atomic<bool>>(true)), sender(sends) {
    std::lock_guard lock(state->mutex); if (sender) ++state->senders; else ++state->receivers;
  }
  void close() const {
    if (!state || !open || !open->exchange(false)) return;
    { std::lock_guard lock(state->mutex); if (sender) --state->senders; else --state->receivers; }
    state->readable.notify_all(); state->writable.notify_all();
  }
  ~RocketChannelEndpoint() { if (open && open.use_count() == 1) close(); }
};
template <typename T> using RocketSender = RocketChannelEndpoint<T>;
template <typename T> using RocketReceiver = RocketChannelEndpoint<T>;
template <typename T> struct RocketChannel { RocketSender<T> sender; RocketReceiver<T> receiver; };

template <typename T> RocketAggregate rocket_stage0_channel(
    const RocketArray<T>& initial, std::size_t maximum) {
  if (initial->size() > maximum) return rocket_stage0_error("initial channel values exceed channel capacity");
  auto state = std::make_shared<RocketChannelState<T>>(); state->maximum = maximum;
  state->values.insert(state->values.end(), initial->begin(), initial->end());
  return rocket_stage0_ok(RocketChannel<T>{RocketSender<T>{state, true}, RocketReceiver<T>{state, false}});
}
template <typename T> RocketAggregate rocket_std_channel_bounded(
    const RocketArray<T>& initial, std::int64_t capacity) {
  if (capacity < 1 || capacity > 65536)
    return rocket_stage0_error("bounded channel capacity must be from 1 through 65536");
  return rocket_stage0_channel(initial, static_cast<std::size_t>(capacity));
}
template <typename T> RocketAggregate rocket_std_channel_unbounded(const RocketArray<T>& initial) {
  return rocket_stage0_channel(initial, 1048576);
}
template <typename T> RocketSender<T> rocket_std_channel_sender(const RocketChannel<T>& channel) {
  return channel.sender;
}
template <typename T> RocketReceiver<T> rocket_std_channel_receiver(const RocketChannel<T>& channel) {
  return channel.receiver;
}
template <typename T> RocketSender<T> rocket_std_channel_clone_sender(const RocketSender<T>& sender) {
  return RocketSender<T>{sender.state, true};
}
template <typename T> RocketReceiver<T> rocket_std_channel_clone_receiver(const RocketReceiver<T>& receiver) {
  return RocketReceiver<T>{receiver.state, false};
}
template <typename T> RocketAggregate rocket_std_channel_send(
    const RocketSender<T>& sender, T value, std::int64_t deadline,
    const RocketCancellation& token) {
  auto state = sender.state; std::unique_lock lock(state->mutex);
  while (state->values.size() >= state->maximum && state->receivers != 0) {
    if (rocket_stage0_operation_cancelled(token)) return rocket_stage0_error("operation cancelled");
    if (deadline >= 0 && rocket_stage0_monotonic_milliseconds() >= deadline)
      return rocket_stage0_error("channel send timed out");
    state->writable.wait_for(lock, std::chrono::milliseconds(2));
  }
  if (!sender.open->load() || state->receivers == 0)
    return rocket_stage0_error("channel receivers disconnected");
  state->values.push_back(std::move(value)); lock.unlock(); state->readable.notify_one();
  return rocket_stage0_ok(true);
}
template <typename T> RocketAggregate rocket_std_channel_receive(
    const RocketReceiver<T>& receiver, std::int64_t deadline,
    const RocketCancellation& token) {
  auto state = receiver.state; std::unique_lock lock(state->mutex);
  while (state->values.empty() && state->senders != 0) {
    if (rocket_stage0_operation_cancelled(token)) return rocket_stage0_error("operation cancelled");
    if (deadline >= 0 && rocket_stage0_monotonic_milliseconds() >= deadline)
      return rocket_stage0_error("channel receive timed out");
    state->readable.wait_for(lock, std::chrono::milliseconds(2));
  }
  RocketAggregate option;
  if (state->values.empty()) option = rocket_stage0_variant(1);
  else { T value = std::move(state->values.front()); state->values.pop_front();
         option = rocket_stage0_variant(0, {std::move(value)}); }
  lock.unlock(); state->writable.notify_one(); return rocket_stage0_ok(option);
}
template <typename T> RocketAggregate rocket_std_channel_close_sender(const RocketSender<T>& sender) {
  const bool prior = sender.open->load(); sender.close(); return rocket_stage0_ok(prior);
}
template <typename T> RocketAggregate rocket_std_channel_close_receiver(const RocketReceiver<T>& receiver) {
  const bool prior = receiver.open->load(); receiver.close(); return rocket_stage0_ok(prior);
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

inline RocketAggregate rocket_stage0_byte_buffer(RocketArray<char> bytes) {
  return rocket_stage0_variant(0, {std::move(bytes)});
}
inline RocketArray<char> rocket_stage0_buffer_bytes(const RocketAggregate& buffer) {
  return rocket_field<RocketArray<char>>(buffer, 0);
}
inline bool rocket_stage0_valid_utf8(std::string_view input) {
  std::size_t index = 0;
  while (index < input.size()) {
    const auto first = static_cast<unsigned char>(input[index]);
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
    } else return false;
    if (count > input.size() - index) return false;
    for (std::size_t continuation = 1; continuation < count; ++continuation) {
      const auto byte = static_cast<unsigned char>(input[index + continuation]);
      if ((byte & 0xc0) != 0x80) return false;
      codepoint = (codepoint << 6) | (byte & 0x3f);
    }
    if (codepoint < minimum || codepoint > 0x10ffff ||
        (codepoint >= 0xd800 && codepoint <= 0xdfff)) return false;
    index += count;
  }
  return true;
}
inline RocketAggregate rocket_std_binary_from_string(const std::string& value) {
  auto bytes = std::make_shared<std::vector<char>>(value.begin(), value.end());
  return rocket_stage0_byte_buffer(std::move(bytes));
}
inline RocketAggregate rocket_std_binary_to_string(const RocketAggregate& buffer) {
  const auto bytes = rocket_stage0_buffer_bytes(buffer);
  const std::string value(bytes->begin(), bytes->end());
  if (!rocket_stage0_valid_utf8(value))
    return rocket_stage0_error("buffer is not valid UTF-8");
  return rocket_stage0_ok(value);
}
inline std::int64_t rocket_std_binary_length(const RocketAggregate& buffer) {
  return static_cast<std::int64_t>(rocket_stage0_buffer_bytes(buffer)->size());
}
inline RocketAggregate rocket_std_binary_slice(const RocketAggregate& buffer,
                                                std::int64_t offset,
                                                std::int64_t length) {
  const auto bytes = rocket_stage0_buffer_bytes(buffer);
  if (offset < 0 || length < 0 || offset > static_cast<std::int64_t>(bytes->size()) ||
      length > static_cast<std::int64_t>(bytes->size()) - offset)
    return rocket_stage0_error("binary slice is outside the buffer");
  auto result = std::make_shared<std::vector<char>>(
      bytes->begin() + offset, bytes->begin() + offset + length);
  return rocket_stage0_ok(rocket_stage0_byte_buffer(std::move(result)));
}
inline RocketAggregate rocket_stage0_binary_read(const RocketAggregate& buffer,
                                                  std::int64_t offset,
                                                  std::size_t width) {
  const auto bytes = rocket_stage0_buffer_bytes(buffer);
  if (offset < 0 || offset > static_cast<std::int64_t>(bytes->size()) ||
      width > bytes->size() - static_cast<std::size_t>(offset))
    return rocket_stage0_error("binary read is outside the buffer");
  std::uint64_t value = 0;
  for (std::size_t index = 0; index < width; ++index)
    value |= static_cast<std::uint64_t>(static_cast<unsigned char>(
                 (*bytes)[static_cast<std::size_t>(offset) + index])) << (index * 8);
  return rocket_stage0_ok(static_cast<std::int64_t>(value));
}
inline RocketAggregate rocket_std_binary_read_u8(const RocketAggregate& buffer,
                                                  std::int64_t offset) {
  return rocket_stage0_binary_read(buffer, offset, 1);
}
inline RocketAggregate rocket_std_binary_read_u16_le(const RocketAggregate& buffer,
                                                      std::int64_t offset) {
  return rocket_stage0_binary_read(buffer, offset, 2);
}
inline RocketAggregate rocket_std_binary_read_u32_le(const RocketAggregate& buffer,
                                                      std::int64_t offset) {
  return rocket_stage0_binary_read(buffer, offset, 4);
}
inline RocketAggregate rocket_stage0_binary_write(std::int64_t value,
                                                   std::size_t width) {
  const std::uint64_t maximum = width == 1 ? 0xffULL
                               : width == 2 ? 0xffffULL
                                            : 0xffffffffULL;
  if (value < 0 || static_cast<std::uint64_t>(value) > maximum)
    return rocket_stage0_error("binary integer is outside the unsigned encoding range");
  auto bytes = std::make_shared<std::vector<char>>(width);
  for (std::size_t index = 0; index < width; ++index)
    (*bytes)[index] = static_cast<char>(static_cast<std::uint64_t>(value) >> (index * 8));
  return rocket_stage0_ok(rocket_stage0_byte_buffer(std::move(bytes)));
}
inline RocketAggregate rocket_std_binary_write_u8(std::int64_t value) {
  return rocket_stage0_binary_write(value, 1);
}
inline RocketAggregate rocket_std_binary_write_u16_le(std::int64_t value) {
  return rocket_stage0_binary_write(value, 2);
}
inline RocketAggregate rocket_std_binary_write_u32_le(std::int64_t value) {
  return rocket_stage0_binary_write(value, 4);
}

inline RocketAggregate rocket_std_binary_concat(const RocketAggregate& left,
                                                 const RocketAggregate& right) {
  const auto leftBytes = rocket_stage0_buffer_bytes(left);
  const auto rightBytes = rocket_stage0_buffer_bytes(right);
  auto bytes = std::make_shared<std::vector<char>>(*leftBytes);
  bytes->insert(bytes->end(), rightBytes->begin(), rightBytes->end());
  return rocket_stage0_byte_buffer(std::move(bytes));
}

inline RocketAggregate rocket_stage0_binary_read_be(const RocketAggregate& buffer,
                                                     std::int64_t offset,
                                                     std::size_t width) {
  const auto bytes = rocket_stage0_buffer_bytes(buffer);
  if (offset < 0 || offset > static_cast<std::int64_t>(bytes->size()) ||
      width > bytes->size() - static_cast<std::size_t>(offset))
    return rocket_stage0_error("binary read is outside the buffer");
  std::uint64_t value = 0;
  for (std::size_t index = 0; index < width; ++index)
    value = (value << 8) | static_cast<unsigned char>(
                               (*bytes)[static_cast<std::size_t>(offset) + index]);
  return rocket_stage0_ok(static_cast<std::int64_t>(value));
}

inline RocketAggregate rocket_std_binary_read_u16_be(const RocketAggregate& buffer,
                                                      std::int64_t offset) {
  return rocket_stage0_binary_read_be(buffer, offset, 2);
}

inline RocketAggregate rocket_std_binary_read_u32_be(const RocketAggregate& buffer,
                                                      std::int64_t offset) {
  return rocket_stage0_binary_read_be(buffer, offset, 4);
}

inline RocketAggregate rocket_stage0_binary_write_be(std::int64_t value,
                                                      std::size_t width) {
  const std::uint64_t maximum = width == 2 ? 0xffffULL : 0xffffffffULL;
  if (value < 0 || static_cast<std::uint64_t>(value) > maximum)
    return rocket_stage0_error("binary integer is outside the unsigned encoding range");
  auto bytes = std::make_shared<std::vector<char>>(width);
  for (std::size_t index = 0; index < width; ++index)
    (*bytes)[index] = static_cast<char>(static_cast<std::uint64_t>(value) >>
                                        ((width - index - 1) * 8));
  return rocket_stage0_ok(rocket_stage0_byte_buffer(std::move(bytes)));
}

inline RocketAggregate rocket_std_binary_write_u16_be(std::int64_t value) {
  return rocket_stage0_binary_write_be(value, 2);
}

inline RocketAggregate rocket_std_binary_write_u32_be(std::int64_t value) {
  return rocket_stage0_binary_write_be(value, 4);
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
inline RocketAggregate rocket_std_file_read_binary(const std::string& path) {
  try {
    std::ifstream input(rocket_stage0_path(path), std::ios::binary);
    if (!input) return rocket_stage0_error("could not open file for binary reading");
    std::ostringstream contents;
    contents << input.rdbuf();
    return rocket_stage0_ok(rocket_std_binary_from_string(contents.str()));
  } catch (const std::exception& error) { return rocket_stage0_error(error.what()); }
}
inline RocketAggregate rocket_stage0_write_binary_file(
    const std::string& path, const RocketAggregate& buffer, std::ios::openmode mode) {
  try {
    std::ofstream output(rocket_stage0_path(path), std::ios::binary | mode);
    if (!output) return rocket_stage0_error("could not open file for binary writing");
    const auto bytes = rocket_stage0_buffer_bytes(buffer);
    if (!bytes->empty())
      output.write(bytes->data(), static_cast<std::streamsize>(bytes->size()));
    if (!output) return rocket_stage0_error("could not write binary file contents");
    return rocket_stage0_ok(true);
  } catch (const std::exception& error) { return rocket_stage0_error(error.what()); }
}
inline RocketAggregate rocket_std_file_write_binary(const std::string& path,
                                                     const RocketAggregate& buffer) {
  return rocket_stage0_write_binary_file(path, buffer, std::ios::trunc);
}
inline RocketAggregate rocket_std_file_append_binary(const std::string& path,
                                                      const RocketAggregate& buffer) {
  return rocket_stage0_write_binary_file(path, buffer, std::ios::app);
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

struct RocketStage0BufferedReader {
  std::ifstream stream;
  std::vector<char> buffer;
};

struct RocketStage0BufferedWriter {
  std::ofstream stream;
  std::vector<char> buffer;
};

inline std::unordered_map<std::int64_t, std::unique_ptr<RocketStage0BufferedReader>>&
rocket_stage0_readers() {
  static std::unordered_map<std::int64_t,
                            std::unique_ptr<RocketStage0BufferedReader>> value;
  return value;
}

inline std::unordered_map<std::int64_t, std::unique_ptr<RocketStage0BufferedWriter>>&
rocket_stage0_writers() {
  static std::unordered_map<std::int64_t,
                            std::unique_ptr<RocketStage0BufferedWriter>> value;
  return value;
}

inline std::int64_t& rocket_stage0_next_stream_handle() {
  static std::int64_t value = 1;
  return value;
}

inline bool rocket_stage0_valid_stream_buffer_size(std::int64_t size) {
  return size >= 256 && size <= 16 * 1024 * 1024;
}

inline RocketAggregate rocket_std_stream_open_reader(const std::string& path,
                                                      std::int64_t buffer_size) {
  if (!rocket_stage0_valid_stream_buffer_size(buffer_size))
    return rocket_stage0_error("stream buffer size must be between 256 bytes and 16 MiB");
  auto state = std::make_unique<RocketStage0BufferedReader>();
  state->buffer.resize(static_cast<std::size_t>(buffer_size));
  state->stream.rdbuf()->pubsetbuf(state->buffer.data(),
                                   static_cast<std::streamsize>(state->buffer.size()));
  state->stream.open(rocket_stage0_path(path), std::ios::binary);
  if (!state->stream) return rocket_stage0_error("could not open buffered reader");
  const std::int64_t handle = rocket_stage0_next_stream_handle()++;
  rocket_stage0_readers().emplace(handle, std::move(state));
  return rocket_stage0_ok(handle);
}

inline RocketAggregate rocket_std_stream_read(std::int64_t handle,
                                              std::int64_t maximum_bytes) {
  const auto found = rocket_stage0_readers().find(handle);
  if (found == rocket_stage0_readers().end())
    return rocket_stage0_error("invalid or closed reader handle");
  if (maximum_bytes < 0 || maximum_bytes > 64 * 1024 * 1024)
    return rocket_stage0_error("stream read size must be between 0 bytes and 64 MiB");
  std::string bytes(static_cast<std::size_t>(maximum_bytes), '\0');
  found->second->stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  const std::streamsize count = found->second->stream.gcount();
  if (found->second->stream.bad()) return rocket_stage0_error("buffered reader failed");
  bytes.resize(static_cast<std::size_t>(count));
  return rocket_stage0_ok(rocket_std_binary_from_string(bytes));
}

inline RocketAggregate rocket_std_stream_close_reader(std::int64_t handle) {
  const auto found = rocket_stage0_readers().find(handle);
  if (found == rocket_stage0_readers().end())
    return rocket_stage0_error("invalid or closed reader handle");
  rocket_stage0_readers().erase(found);
  return rocket_stage0_ok(true);
}

inline RocketAggregate rocket_std_stream_open_writer(const std::string& path,
                                                      std::int64_t buffer_size,
                                                      bool append) {
  if (!rocket_stage0_valid_stream_buffer_size(buffer_size))
    return rocket_stage0_error("stream buffer size must be between 256 bytes and 16 MiB");
  auto state = std::make_unique<RocketStage0BufferedWriter>();
  state->buffer.resize(static_cast<std::size_t>(buffer_size));
  state->stream.rdbuf()->pubsetbuf(state->buffer.data(),
                                   static_cast<std::streamsize>(state->buffer.size()));
  state->stream.open(rocket_stage0_path(path),
                     std::ios::binary | (append ? std::ios::app : std::ios::trunc));
  if (!state->stream) return rocket_stage0_error("could not open buffered writer");
  const std::int64_t handle = rocket_stage0_next_stream_handle()++;
  rocket_stage0_writers().emplace(handle, std::move(state));
  return rocket_stage0_ok(handle);
}

inline RocketAggregate rocket_std_stream_write(std::int64_t handle,
                                                const RocketAggregate& buffer) {
  const auto found = rocket_stage0_writers().find(handle);
  if (found == rocket_stage0_writers().end())
    return rocket_stage0_error("invalid or closed writer handle");
  const auto bytes = rocket_stage0_buffer_bytes(buffer);
  found->second->stream.write(bytes->data(),
                              static_cast<std::streamsize>(bytes->size()));
  if (!found->second->stream) return rocket_stage0_error("buffered writer failed");
  return rocket_stage0_ok(true);
}

inline RocketAggregate rocket_std_stream_flush(std::int64_t handle) {
  const auto found = rocket_stage0_writers().find(handle);
  if (found == rocket_stage0_writers().end())
    return rocket_stage0_error("invalid or closed writer handle");
  found->second->stream.flush();
  if (!found->second->stream)
    return rocket_stage0_error("buffered writer flush failed");
  return rocket_stage0_ok(true);
}

inline RocketAggregate rocket_std_stream_close_writer(std::int64_t handle) {
  const auto found = rocket_stage0_writers().find(handle);
  if (found == rocket_stage0_writers().end())
    return rocket_stage0_error("invalid or closed writer handle");
  found->second->stream.flush();
  const bool succeeded = static_cast<bool>(found->second->stream);
  rocket_stage0_writers().erase(found);
  return succeeded ? rocket_stage0_ok(true)
                   : rocket_stage0_error("buffered writer close failed");
}

struct RocketStage0Utf8Scalar {
  std::uint32_t value;
  std::size_t start;
  std::size_t end;
};

inline std::vector<RocketStage0Utf8Scalar> rocket_stage0_utf8_scalars(
    std::string_view input) {
  std::vector<RocketStage0Utf8Scalar> result;
  for (std::size_t index = 0; index < input.size();) {
    const std::size_t start = index;
    const auto first = static_cast<unsigned char>(input[index++]);
    std::uint32_t scalar = first;
    std::size_t width = 1;
    if (first >= 0xc2 && first <= 0xdf) { scalar = first & 0x1f; width = 2; }
    else if (first >= 0xe0 && first <= 0xef) { scalar = first & 0x0f; width = 3; }
    else if (first >= 0xf0 && first <= 0xf4) { scalar = first & 0x07; width = 4; }
    for (std::size_t continuation = 1; continuation < width; ++continuation)
      scalar = (scalar << 6) |
               (static_cast<unsigned char>(input[index++]) & 0x3f);
    result.push_back({scalar, start, index});
  }
  return result;
}

inline void rocket_stage0_append_unicode_scalar(std::string& output,
                                                std::uint32_t scalar) {
  if (scalar <= 0x7f) output.push_back(static_cast<char>(scalar));
  else if (scalar <= 0x7ff) {
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

inline bool rocket_stage0_grapheme_extension(std::uint32_t scalar) {
  return (scalar >= 0x0300 && scalar <= 0x036f) ||
         (scalar >= 0x1ab0 && scalar <= 0x1aff) ||
         (scalar >= 0x1dc0 && scalar <= 0x1dff) ||
         (scalar >= 0x20d0 && scalar <= 0x20ff) ||
         (scalar >= 0xfe00 && scalar <= 0xfe0f) ||
         (scalar >= 0xfe20 && scalar <= 0xfe2f) ||
         (scalar >= 0x1f3fb && scalar <= 0x1f3ff) || scalar == 0x200d;
}

inline std::vector<std::pair<std::size_t, std::size_t>>
rocket_stage0_grapheme_ranges(std::string_view input) {
  const auto scalars = rocket_stage0_utf8_scalars(input);
  std::vector<std::pair<std::size_t, std::size_t>> ranges;
  std::size_t regional_count = 0;
  for (std::size_t index = 0; index < scalars.size(); ++index) {
    const auto scalar = scalars[index].value;
    const bool regional = scalar >= 0x1f1e6 && scalar <= 0x1f1ff;
    bool joins = index != 0 && rocket_stage0_grapheme_extension(scalar);
    if (index != 0 && scalars[index - 1].value == 0x200d) joins = true;
    if (regional) { joins = index != 0 && regional_count % 2 == 1; ++regional_count; }
    else regional_count = 0;
    if (!joins) ranges.push_back({scalars[index].start, scalars[index].end});
    else ranges.back().second = scalars[index].end;
  }
  return ranges;
}

inline std::int64_t rocket_std_unicode_scalar_count(const std::string& value) {
  return static_cast<std::int64_t>(rocket_stage0_utf8_scalars(value).size());
}

inline RocketAggregate rocket_std_unicode_scalar_at(const std::string& value,
                                                     std::int64_t index) {
  const auto scalars = rocket_stage0_utf8_scalars(value);
  if (index < 0 || static_cast<std::size_t>(index) >= scalars.size())
    return rocket_stage0_error("Unicode scalar index is outside the string");
  return rocket_stage0_ok(static_cast<std::int64_t>(
      scalars[static_cast<std::size_t>(index)].value));
}

inline RocketAggregate rocket_std_unicode_from_scalar(std::int64_t scalar) {
  if (scalar < 0 || scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff))
    return rocket_stage0_error("value is not a Unicode scalar");
  std::string result;
  rocket_stage0_append_unicode_scalar(result,
                                      static_cast<std::uint32_t>(scalar));
  return rocket_stage0_ok(result);
}

#ifdef _WIN32
inline std::wstring rocket_stage0_utf8_to_wide(const std::string& value) {
  if (value.empty()) return {};
  const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                         value.data(), static_cast<int>(value.size()),
                                         nullptr, 0);
  if (length <= 0) return {};
  std::wstring result(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                      static_cast<int>(value.size()), result.data(), length);
  return result;
}

inline std::string rocket_stage0_wide_to_utf8(const std::wstring& value) {
  if (value.empty()) return {};
  const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                         value.data(), static_cast<int>(value.size()),
                                         nullptr, 0, nullptr, nullptr);
  if (length <= 0) return {};
  std::string result(static_cast<std::size_t>(length), '\0');
  WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                      static_cast<int>(value.size()), result.data(), length,
                      nullptr, nullptr);
  return result;
}

inline RocketAggregate rocket_stage0_normalize_unicode(const std::string& value,
                                                        NORM_FORM form) {
  const std::wstring wide = rocket_stage0_utf8_to_wide(value);
  if (!value.empty() && wide.empty()) return rocket_stage0_error("invalid UTF-8 text");
  using NormalizeFunction = int(WINAPI*)(NORM_FORM, LPCWSTR, int, LPWSTR, int);
  const HMODULE library = LoadLibraryW(L"Normaliz.dll");
  if (!library) return rocket_stage0_error("Windows Unicode normalization is unavailable");
  const auto normalize = reinterpret_cast<NormalizeFunction>(
      GetProcAddress(library, "NormalizeString"));
  if (!normalize) { FreeLibrary(library); return rocket_stage0_error("Windows Unicode normalization is unavailable"); }
  const int required = normalize(form, wide.data(), static_cast<int>(wide.size()), nullptr, 0);
  if (required <= 0) { FreeLibrary(library); return rocket_stage0_error("Unicode normalization failed"); }
  std::wstring normalized(static_cast<std::size_t>(required), L'\0');
  const int written = normalize(form, wide.data(), static_cast<int>(wide.size()),
                                normalized.data(), required);
  FreeLibrary(library);
  if (written <= 0) return rocket_stage0_error("Unicode normalization failed");
  normalized.resize(static_cast<std::size_t>(written));
  return rocket_stage0_ok(rocket_stage0_wide_to_utf8(normalized));
}
#endif

inline RocketAggregate rocket_std_unicode_normalize_nfc(const std::string& value) {
#ifdef _WIN32
  return rocket_stage0_normalize_unicode(value, NormalizationC);
#else
  (void)value;
  return rocket_stage0_error("Unicode normalization is currently supported on Windows x64 only");
#endif
}

inline RocketAggregate rocket_std_unicode_normalize_nfd(const std::string& value) {
#ifdef _WIN32
  return rocket_stage0_normalize_unicode(value, NormalizationD);
#else
  (void)value;
  return rocket_stage0_error("Unicode normalization is currently supported on Windows x64 only");
#endif
}

inline std::int64_t rocket_std_unicode_grapheme_count(const std::string& value) {
  return static_cast<std::int64_t>(rocket_stage0_grapheme_ranges(value).size());
}

inline RocketAggregate rocket_std_unicode_grapheme_at(const std::string& value,
                                                       std::int64_t index) {
  const auto ranges = rocket_stage0_grapheme_ranges(value);
  if (index < 0 || static_cast<std::size_t>(index) >= ranges.size())
    return rocket_stage0_error("Unicode grapheme index is outside the string");
  const auto [start, end] = ranges[static_cast<std::size_t>(index)];
  return rocket_stage0_ok(value.substr(start, end - start));
}

inline RocketAggregate rocket_std_regex_is_match(const std::string& pattern,
                                                  const std::string& value) {
  const auto found = rocket::safe_regex::search(pattern, value);
  if (!found.valid) return rocket_stage0_error(found.error);
  return rocket_stage0_ok(!found.matches.empty());
}

inline RocketAggregate rocket_std_regex_find_all(const std::string& pattern,
                                                  const std::string& value) {
  const auto found = rocket::safe_regex::findAll(pattern, value);
  if (!found.valid) return rocket_stage0_error(found.error);
  auto matches = std::make_shared<std::vector<std::string>>();
  matches->reserve(found.matches.size());
  for (const auto match : found.matches)
    matches->push_back(value.substr(match.start, match.end - match.start));
  return rocket_stage0_ok(matches);
}

inline RocketAggregate rocket_std_regex_replace_all(const std::string& pattern,
                                                     const std::string& value,
                                                     const std::string& replacement) {
  std::string output;
  std::string error;
  if (!rocket::safe_regex::replaceAll(pattern, value, replacement, output, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(output);
}

inline RocketAggregate rocket_std_crypto_secure_bytes(std::int64_t length) {
  if (length < 0) return rocket_stage0_error("secure random length must not be negative");
  std::vector<std::uint8_t> random;
  std::string error;
  if (!rocket::platform_crypto::secureRandom(static_cast<std::size_t>(length), random,
                                              error))
    return rocket_stage0_error(error);
  auto bytes = std::make_shared<std::vector<char>>();
  bytes->reserve(random.size());
  for (std::uint8_t byte : random) bytes->push_back(static_cast<char>(byte));
  return rocket_stage0_ok(rocket_stage0_byte_buffer(std::move(bytes)));
}

inline RocketAggregate rocket_std_crypto_secure_int(std::int64_t minimum,
                                                      std::int64_t maximum) {
  std::int64_t value = 0;
  std::string error;
  if (!rocket::platform_crypto::secureInt(minimum, maximum, value, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(value);
}

inline RocketAggregate rocket_std_crypto_sha256(const RocketAggregate& value) {
  const auto bytes = rocket_stage0_buffer_bytes(value);
  std::string digest;
  std::string error;
  if (!rocket::platform_crypto::sha256(
          std::string_view(bytes->data(), bytes->size()), digest, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(digest);
}

inline RocketAggregate rocket_std_crypto_hmac_sha256(const RocketAggregate& key,
                                                       const RocketAggregate& value) {
  const auto keyBytes = rocket_stage0_buffer_bytes(key);
  const auto valueBytes = rocket_stage0_buffer_bytes(value);
  std::string digest;
  std::string error;
  if (!rocket::platform_crypto::hmacSha256(
          std::string_view(keyBytes->data(), keyBytes->size()),
          std::string_view(valueBytes->data(), valueBytes->size()), digest, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(digest);
}

inline bool rocket_std_crypto_constant_time_equal(const RocketAggregate& left,
                                                   const RocketAggregate& right) {
  const auto leftBytes = rocket_stage0_buffer_bytes(left);
  const auto rightBytes = rocket_stage0_buffer_bytes(right);
  return rocket::platform_crypto::constantTimeEqual(
      std::string_view(leftBytes->data(), leftBytes->size()),
      std::string_view(rightBytes->data(), rightBytes->size()));
}

inline RocketAggregate rocket_std_crypto_verify_signed_file(const std::string& path) {
#ifdef _WIN32
  const std::wstring wide = rocket_stage0_utf8_to_wide(path);
  if (!path.empty() && wide.empty())
    return rocket_stage0_error("signed-file path is not valid UTF-8");
#else
  const std::wstring wide;
#endif
  bool trusted = false;
  std::string error;
  if (!rocket::platform_crypto::verifySignedFile(wide, trusted, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(trusted);
}

struct RocketStage0NetworkSocket {
  rocket::platform_net::Socket socket = rocket::platform_net::invalidSocket;
  bool listener = false;
};

inline std::unordered_map<std::int64_t, RocketStage0NetworkSocket>&
rocket_stage0_network_sockets() {
  static std::unordered_map<std::int64_t, RocketStage0NetworkSocket> sockets;
  return sockets;
}

inline std::int64_t& rocket_stage0_next_network_handle() {
  static std::int64_t handle = 1;
  return handle;
}

inline RocketAggregate rocket_std_net_resolve(const std::string& host,
                                                const std::string& service) {
  std::vector<std::string> addresses;
  std::string error;
  if (!rocket::platform_net::resolve(host, service, addresses, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(std::make_shared<std::vector<std::string>>(
      addresses.begin(), addresses.end()));
}

inline RocketAggregate rocket_std_net_tcp_connect(const std::string& host,
                                                    std::int64_t port,
                                                    std::int64_t timeout) {
  rocket::platform_net::Socket socket = rocket::platform_net::invalidSocket;
  std::string error;
  if (!rocket::platform_net::connect(host, port, timeout, socket, error))
    return rocket_stage0_error(error);
  const std::int64_t token = rocket_stage0_next_network_handle()++;
  rocket_stage0_network_sockets().emplace(
      token, RocketStage0NetworkSocket{socket, false});
  return rocket_stage0_ok(token);
}

inline RocketAggregate rocket_std_net_tcp_listen(const std::string& address,
                                                   std::int64_t port,
                                                   std::int64_t backlog) {
  rocket::platform_net::Socket socket = rocket::platform_net::invalidSocket;
  std::string error;
  if (!rocket::platform_net::listen(address, port, backlog, socket, error))
    return rocket_stage0_error(error);
  const std::int64_t token = rocket_stage0_next_network_handle()++;
  rocket_stage0_network_sockets().emplace(
      token, RocketStage0NetworkSocket{socket, true});
  return rocket_stage0_ok(token);
}

inline RocketAggregate rocket_std_net_accept(std::int64_t listener,
                                               std::int64_t timeout) {
  const auto found = rocket_stage0_network_sockets().find(listener);
  if (found == rocket_stage0_network_sockets().end() || !found->second.listener)
    return rocket_stage0_error("network token is not an open TCP listener");
  rocket::platform_net::Socket client = rocket::platform_net::invalidSocket;
  std::string error;
  if (!rocket::platform_net::accept(found->second.socket, timeout, client, error))
    return rocket_stage0_error(error);
  const std::int64_t token = rocket_stage0_next_network_handle()++;
  rocket_stage0_network_sockets().emplace(
      token, RocketStage0NetworkSocket{client, false});
  return rocket_stage0_ok(token);
}

inline RocketAggregate rocket_std_net_send(std::int64_t handle,
                                             const RocketAggregate& buffer,
                                             std::int64_t timeout) {
  const auto found = rocket_stage0_network_sockets().find(handle);
  if (found == rocket_stage0_network_sockets().end() || found->second.listener)
    return rocket_stage0_error("network token is not an open TCP connection");
  const auto values = rocket_stage0_buffer_bytes(buffer);
  std::size_t sent = 0;
  std::string error;
  if (!rocket::platform_net::send(
          found->second.socket, std::string_view(values->data(), values->size()),
          timeout, sent, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(static_cast<std::int64_t>(sent));
}

inline RocketAggregate rocket_std_net_receive(std::int64_t handle,
                                                std::int64_t maximum,
                                                std::int64_t timeout) {
  const auto found = rocket_stage0_network_sockets().find(handle);
  if (found == rocket_stage0_network_sockets().end() || found->second.listener)
    return rocket_stage0_error("network token is not an open TCP connection");
  if (maximum < 0) return rocket_stage0_error("TCP receive maximum must not be negative");
  std::string bytes;
  std::string error;
  if (!rocket::platform_net::receive(found->second.socket,
                                      static_cast<std::size_t>(maximum), timeout,
                                      bytes, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(rocket_std_binary_from_string(bytes));
}

inline RocketAggregate rocket_std_net_close(std::int64_t handle) {
  const auto found = rocket_stage0_network_sockets().find(handle);
  if (found == rocket_stage0_network_sockets().end())
    return rocket_stage0_error("network token is not open");
  std::string error;
  const bool closed = rocket::platform_net::close(found->second.socket, error);
  rocket_stage0_network_sockets().erase(found);
  if (!closed) return rocket_stage0_error(error);
  return rocket_stage0_ok(true);
}

inline RocketAggregate rocket_std_net_cancel(std::int64_t handle) {
  return rocket_std_net_close(handle);
}

inline RocketAggregate rocket_std_net_local_port(std::int64_t handle) {
  const auto found = rocket_stage0_network_sockets().find(handle);
  if (found == rocket_stage0_network_sockets().end())
    return rocket_stage0_error("network token is not open");
  std::int64_t port = 0;
  std::string error;
  if (!rocket::platform_net::localPort(found->second.socket, port, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(port);
}

inline bool rocket_stage0_parse_http_request(
    rocket::platform_net::Socket socket, std::int64_t maximum, std::int64_t timeout,
    std::string& method, std::string& path, std::string& body, std::string& error) {
  if (maximum < 1 || maximum > 16 * 1024 * 1024) {
    error = "HTTP request limit must be between 1 byte and 16 MiB";
    return false;
  }
  std::string input;
  std::size_t header_end = std::string::npos;
  std::size_t content_length = 0;
  while (input.size() < static_cast<std::size_t>(maximum)) {
    std::string chunk;
    const std::size_t remaining = static_cast<std::size_t>(maximum) - input.size();
    if (!rocket::platform_net::receive(socket, (std::min)(remaining, std::size_t{8192}),
                                        timeout, chunk, error))
      return false;
    if (chunk.empty()) { error = "HTTP peer closed before the request completed"; return false; }
    input += chunk;
    if (header_end == std::string::npos) {
      header_end = input.find("\r\n\r\n");
      if (header_end == std::string::npos) continue;
      const std::size_t line_end = input.find("\r\n");
      if (line_end == std::string::npos || line_end > header_end) {
        error = "invalid HTTP request line";
        return false;
      }
      const std::string request_line = input.substr(0, line_end);
      const std::size_t first_space = request_line.find(' ');
      const std::size_t second_space = request_line.find(' ', first_space + 1);
      if (first_space == std::string::npos || second_space == std::string::npos ||
          !request_line.substr(second_space + 1).starts_with("HTTP/1.")) {
        error = "invalid HTTP request line";
        return false;
      }
      method = request_line.substr(0, first_space);
      path = request_line.substr(first_space + 1, second_space - first_space - 1);
      std::size_t cursor = line_end + 2;
      while (cursor < header_end) {
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
          if (converted.ec != std::errc{} || converted.ptr != text.data() + text.size() ||
              parsed > static_cast<std::uint64_t>(maximum)) {
            error = "invalid or excessive HTTP Content-Length";
            return false;
          }
          content_length = static_cast<std::size_t>(parsed);
        }
        cursor = end + 2;
      }
    }
    const std::size_t body_start = header_end + 4;
    if (input.size() >= body_start + content_length) {
      body = input.substr(body_start, content_length);
      return true;
    }
  }
  error = "HTTP request exceeds its configured byte limit";
  return false;
}

inline RocketAggregate rocket_std_http_request(const std::string& method,
                                                 const std::string& url,
                                                 const RocketAggregate& body,
                                                 std::int64_t timeout) {
  const auto body_bytes = rocket_stage0_buffer_bytes(body);
  rocket::platform_net::HttpResponse response;
  std::string error;
  if (!rocket::platform_net::httpRequest(
          method, url, std::string_view(body_bytes->data(), body_bytes->size()),
          timeout, response, error))
    return rocket_stage0_error(error);
  RocketAggregate value = rocket_stage0_variant(
      0, {response.status, rocket_std_binary_from_string(response.body)});
  return rocket_stage0_ok(std::move(value));
}

inline RocketAggregate rocket_std_http_read_request(std::int64_t handle,
                                                      std::int64_t maximum,
                                                      std::int64_t timeout) {
  const auto found = rocket_stage0_network_sockets().find(handle);
  if (found == rocket_stage0_network_sockets().end() || found->second.listener)
    return rocket_stage0_error("network token is not an open TCP connection");
  std::string method;
  std::string path;
  std::string body;
  std::string error;
  if (!rocket_stage0_parse_http_request(found->second.socket, maximum, timeout,
                                         method, path, body, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(rocket_stage0_variant(
      0, {method, path, rocket_std_binary_from_string(body)}));
}

inline RocketAggregate rocket_std_http_write_response(
    std::int64_t handle, std::int64_t status, const std::string& content_type,
    const RocketAggregate& body, std::int64_t timeout) {
  const auto found = rocket_stage0_network_sockets().find(handle);
  if (found == rocket_stage0_network_sockets().end() || found->second.listener)
    return rocket_stage0_error("network token is not an open TCP connection");
  const auto body_bytes = rocket_stage0_buffer_bytes(body);
  if (status < 100 || status > 599)
    return rocket_stage0_error("HTTP response status must be from 100 through 599");
  if (content_type.empty() || content_type.find('\r') != std::string::npos ||
      content_type.find('\n') != std::string::npos)
    return rocket_stage0_error("HTTP content type must be a non-empty single-line value");
  const std::string bytes(body_bytes->data(), body_bytes->size());
  const std::string response = "HTTP/1.1 " + std::to_string(status) +
      " Rocket\r\nContent-Type: " + content_type + "\r\nContent-Length: " +
      std::to_string(bytes.size()) + "\r\nConnection: close\r\n\r\n" + bytes;
  std::size_t sent = 0;
  std::string error;
  if (!rocket::platform_net::send(found->second.socket, response, timeout, sent, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(sent == response.size());
}

inline std::string rocket_stage0_trim_ascii(std::string_view value) {
  const std::size_t first = value.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) return {};
  const std::size_t last = value.find_last_not_of(" \t\r\n");
  return std::string(value.substr(first, last - first + 1));
}

inline RocketAggregate rocket_stage0_optional_string_result(bool found,
                                                              std::string value) {
  return rocket_stage0_ok(found ? rocket_stage0_variant(0, {std::move(value)})
                                : rocket_stage0_variant(1));
}

inline bool rocket_stage0_valid_log_level(std::string_view level) {
  return level == "trace" || level == "debug" || level == "info" ||
         level == "warn" || level == "error" || level == "fatal";
}

inline std::string rocket_stage0_log_line(std::string_view level,
                                           std::string_view message,
                                           std::string& error) {
  if (!rocket_stage0_valid_log_level(level)) {
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
  for (char character : message) {
    if (character == '\n') escaped += "\\n";
    else if (character == '\r') escaped += "\\r";
    else escaped.push_back(character);
  }
  return timestamp + " [" + std::string(level) + "] " + escaped + "\n";
}

inline bool rocket_stage0_config_value(
    std::string_view source, std::string_view requested, bool& found,
    std::string& value, std::string& error) {
  found = false;
  value.clear();
  if (source.size() > 1024 * 1024) {
    error = "configuration text exceeds the 1 MiB limit";
    return false;
  }
  const std::string key = rocket_stage0_trim_ascii(requested);
  if (key.empty() || key.size() > 256) {
    error = "configuration key must contain 1 through 256 bytes";
    return false;
  }
  std::unordered_map<std::string, std::string> values;
  std::string section;
  std::size_t cursor = 0;
  std::size_t line_number = 0;
  while (cursor <= source.size()) {
    const std::size_t end = source.find('\n', cursor);
    std::string line(source.substr(cursor, end == std::string_view::npos
                                              ? std::string_view::npos : end - cursor));
    ++line_number;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.size() > 65536) {
      error = "configuration line exceeds 64 KiB at line " +
              std::to_string(line_number);
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
    line = rocket_stage0_trim_ascii(line);
    if (!line.empty()) {
      if (line.front() == '[') {
        if (line.size() < 3 || line.back() != ']') {
          error = "invalid configuration section at line " +
                  std::to_string(line_number);
          return false;
        }
        section = rocket_stage0_trim_ascii(
            std::string_view(line).substr(1, line.size() - 2));
        if (section.empty()) {
          error = "empty configuration section at line " +
                  std::to_string(line_number);
          return false;
        }
      } else {
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
          error = "configuration entry is missing '=' at line " +
                  std::to_string(line_number);
          return false;
        }
        const std::string local = rocket_stage0_trim_ascii(
            std::string_view(line).substr(0, equals));
        std::string parsed = rocket_stage0_trim_ascii(
            std::string_view(line).substr(equals + 1));
        if (local.empty()) {
          error = "configuration entry has an empty key at line " +
                  std::to_string(line_number);
          return false;
        }
        if (!parsed.empty() && parsed.front() == '"') {
          if (parsed.size() < 2 || parsed.back() != '"') {
            error = "unterminated configuration string at line " +
                    std::to_string(line_number);
            return false;
          }
          std::string decoded;
          for (std::size_t index = 1; index + 1 < parsed.size(); ++index) {
            char character = parsed[index];
            if (character != '\\') { decoded.push_back(character); continue; }
            if (++index + 1 >= parsed.size()) {
              error = "unterminated configuration escape at line " +
                      std::to_string(line_number);
              return false;
            }
            character = parsed[index];
            if (character == 'n') decoded.push_back('\n');
            else if (character == 'r') decoded.push_back('\r');
            else if (character == 't') decoded.push_back('\t');
            else if (character == '"' || character == '\\') decoded.push_back(character);
            else {
              error = "unsupported configuration escape at line " +
                      std::to_string(line_number);
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

inline RocketAggregate rocket_std_datetime_format_utc(std::int64_t milliseconds) {
  std::string value;
  std::string error;
  if (!rocket::platform_datetime::formatUtc(milliseconds, value, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(value);
}

inline RocketAggregate rocket_std_datetime_parse_utc(const std::string& value) {
  std::int64_t milliseconds = 0;
  std::string error;
  if (!rocket::platform_datetime::parseUtc(value, milliseconds, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(milliseconds);
}

inline RocketAggregate rocket_std_datetime_days_in_month(std::int64_t year,
                                                           std::int64_t month) {
  std::int64_t days = 0;
  std::string error;
  if (!rocket::platform_datetime::daysInMonth(year, month, days, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(days);
}

inline RocketAggregate rocket_std_datetime_weekday(std::int64_t year,
                                                     std::int64_t month,
                                                     std::int64_t day) {
  std::int64_t weekday = 0;
  std::string error;
  if (!rocket::platform_datetime::weekday(year, month, day, weekday, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(weekday);
}

inline RocketAggregate rocket_std_datetime_local_offset_minutes(
    std::int64_t milliseconds) {
  std::int64_t offset = 0;
  std::string error;
  if (!rocket::platform_datetime::localOffsetMinutes(milliseconds, offset, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(offset);
}

inline RocketAggregate rocket_std_datetime_timezone_name() {
  std::string value;
  std::string error;
  if (!rocket::platform_datetime::timezoneName(value, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(value);
}

inline RocketAggregate rocket_std_log_write(const std::string& level,
                                              const std::string& message) {
  static std::mutex mutex;
  std::string error;
  const std::string line = rocket_stage0_log_line(level, message, error);
  if (!error.empty()) return rocket_stage0_error(error);
  std::lock_guard<std::mutex> lock(mutex);
  if (std::fwrite(line.data(), 1, line.size(), stderr) != line.size() ||
      std::fflush(stderr) != 0)
    return rocket_stage0_error("could not write the log message to standard error");
  return rocket_stage0_ok(true);
}

inline RocketAggregate rocket_std_log_append(const std::string& path,
                                               const std::string& level,
                                               const std::string& message) {
  static std::mutex mutex;
  std::string error;
  const std::string line = rocket_stage0_log_line(level, message, error);
  if (!error.empty()) return rocket_stage0_error(error);
  std::lock_guard<std::mutex> lock(mutex);
  std::ofstream output(rocket_stage0_path(path), std::ios::binary | std::ios::app);
  if (!output) return rocket_stage0_error("could not open the log file for append");
  output.write(line.data(), static_cast<std::streamsize>(line.size()));
  output.flush();
  if (!output) return rocket_stage0_error("could not append and flush the log message");
  return rocket_stage0_ok(true);
}

inline bool rocket_std_cli_has_flag(const RocketArray<std::string>& arguments,
                                     const std::string& name) {
  for (const std::string& argument : *arguments) {
    if (argument == "--") break;
    if (argument == name) return true;
  }
  return false;
}

inline RocketAggregate rocket_std_cli_option(const RocketArray<std::string>& arguments,
                                               const std::string& name) {
  if (!name.starts_with("--") || name.size() < 3 || name.find('=') != std::string::npos)
    return rocket_stage0_error("CLI option name must use --name without '='");
  const std::string prefix = name + "=";
  for (std::size_t index = 0; index < arguments->size(); ++index) {
    const std::string& argument = (*arguments)[index];
    if (argument == "--") break;
    if (argument.starts_with(prefix))
      return rocket_stage0_optional_string_result(true, argument.substr(prefix.size()));
    if (argument == name) {
      if (index + 1 >= arguments->size() || (*arguments)[index + 1] == "--")
        return rocket_stage0_error("CLI option is missing its value");
      return rocket_stage0_optional_string_result(true, (*arguments)[index + 1]);
    }
  }
  return rocket_stage0_optional_string_result(false, {});
}

inline RocketArray<std::string> rocket_std_cli_positionals(
    const RocketArray<std::string>& arguments) {
  auto result = std::make_shared<std::vector<std::string>>();
  bool after_separator = false;
  for (const std::string& argument : *arguments) {
    if (!after_separator && argument == "--") { after_separator = true; continue; }
    if (after_separator || argument.empty() || argument.front() != '-')
      result->push_back(argument);
  }
  return result;
}

inline RocketAggregate rocket_std_config_get(const std::string& text,
                                               const std::string& key) {
  bool found = false;
  std::string value;
  std::string error;
  if (!rocket_stage0_config_value(text, key, found, value, error))
    return rocket_stage0_error(error);
  return rocket_stage0_optional_string_result(found, std::move(value));
}

inline RocketAggregate rocket_std_config_load(const std::string& path,
                                                const std::string& key) {
  std::ifstream input(rocket_stage0_path(path), std::ios::binary);
  if (!input) return rocket_stage0_error("could not open configuration file");
  std::ostringstream contents;
  contents << input.rdbuf();
  if (!input.good() && !input.eof())
    return rocket_stage0_error("could not read configuration file");
  return rocket_std_config_get(contents.str(), key);
}

inline RocketAggregate rocket_std_compression_xpress_compress(
    const RocketAggregate& value) {
  const auto bytes = rocket_stage0_buffer_bytes(value);
  std::string output;
  std::string error;
  if (!rocket::platform_compression::compressXpress(
          std::string_view(bytes->data(), bytes->size()), output, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(rocket_std_binary_from_string(output));
}

inline RocketAggregate rocket_std_compression_xpress_decompress(
    const RocketAggregate& value) {
  const auto bytes = rocket_stage0_buffer_bytes(value);
  std::string output;
  std::string error;
  if (!rocket::platform_compression::decompressXpress(
          std::string_view(bytes->data(), bytes->size()), output, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(rocket_std_binary_from_string(output));
}

inline RocketAggregate rocket_std_archive_tar_create(
    const std::string& path, const RocketArray<std::string>& names,
    const RocketArray<RocketAggregate>& contents) {
  if (names->size() != contents->size())
    return rocket_stage0_error("TAR entry names and contents must have equal lengths");
  std::vector<rocket::safe_archive::Entry> entries;
  entries.reserve(names->size());
  for (std::size_t index = 0; index < names->size(); ++index) {
    const auto bytes = rocket_stage0_buffer_bytes((*contents)[index]);
    entries.push_back({(*names)[index], std::string(bytes->data(), bytes->size())});
  }
  std::string error;
  if (!rocket::safe_archive::create(path, entries, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(true);
}

inline RocketAggregate rocket_std_archive_tar_list(const std::string& path) {
  std::vector<std::string> names;
  std::string error;
  if (!rocket::safe_archive::list(path, names, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(std::make_shared<std::vector<std::string>>(
      names.begin(), names.end()));
}

inline RocketAggregate rocket_std_archive_tar_read(const std::string& path,
                                                     const std::string& name) {
  std::string contents;
  std::string error;
  if (!rocket::safe_archive::read(path, name, contents, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(rocket_std_binary_from_string(contents));
}

inline std::unordered_map<std::int64_t, sqlite3*>& rocket_stage0_sqlite_databases() {
  static std::unordered_map<std::int64_t, sqlite3*> databases;
  return databases;
}

inline std::int64_t& rocket_stage0_next_sqlite_handle() {
  static std::int64_t handle = 1;
  return handle;
}

inline RocketAggregate rocket_std_sqlite_open(const std::string& path) {
  sqlite3* database = nullptr;
  std::string error;
  if (!rocket::platform_sqlite::open(path, database, error))
    return rocket_stage0_error(error);
  const std::int64_t token = rocket_stage0_next_sqlite_handle()++;
  rocket_stage0_sqlite_databases().emplace(token, database);
  return rocket_stage0_ok(token);
}

inline RocketAggregate rocket_std_sqlite_execute(
    std::int64_t handle, const std::string& sql,
    const RocketArray<std::string>& parameters) {
  const auto found = rocket_stage0_sqlite_databases().find(handle);
  if (found == rocket_stage0_sqlite_databases().end())
    return rocket_stage0_error("SQLite token is not open");
  std::int64_t changes = 0;
  std::string error;
  if (!rocket::platform_sqlite::execute(found->second, sql, *parameters,
                                         changes, error))
    return rocket_stage0_error(error);
  return rocket_stage0_ok(changes);
}

inline RocketAggregate rocket_std_sqlite_query(
    std::int64_t handle, const std::string& sql,
    const RocketArray<std::string>& parameters) {
  const auto found = rocket_stage0_sqlite_databases().find(handle);
  if (found == rocket_stage0_sqlite_databases().end())
    return rocket_stage0_error("SQLite token is not open");
  std::vector<std::vector<std::string>> rows;
  std::string error;
  if (!rocket::platform_sqlite::query(found->second, sql, *parameters, rows, error))
    return rocket_stage0_error(error);
  auto result = std::make_shared<std::vector<RocketArray<std::string>>>();
  result->reserve(rows.size());
  for (auto& row : rows)
    result->push_back(std::make_shared<std::vector<std::string>>(std::move(row)));
  return rocket_stage0_ok(result);
}

inline RocketAggregate rocket_std_sqlite_close(std::int64_t handle) {
  const auto found = rocket_stage0_sqlite_databases().find(handle);
  if (found == rocket_stage0_sqlite_databases().end())
    return rocket_stage0_error("SQLite token is not open");
  std::string error;
  if (!rocket::platform_sqlite::close(found->second, error))
    return rocket_stage0_error(error);
  rocket_stage0_sqlite_databases().erase(found);
  return rocket_stage0_ok(true);
}

inline std::unordered_set<std::string>& rocket_stage0_testing_temporaries() {
  static std::unordered_set<std::string> paths;
  return paths;
}

inline std::unordered_map<std::string, std::int64_t>& rocket_stage0_testing_coverage() {
  static std::unordered_map<std::string, std::int64_t> points;
  return points;
}

inline std::mutex& rocket_stage0_testing_mutex() {
  static std::mutex mutex;
  return mutex;
}

inline bool rocket_stage0_safe_testing_relative(std::string_view value) {
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

inline RocketAggregate rocket_std_testing_assert(bool condition,
                                                   const std::string& message) {
  return condition ? rocket_stage0_ok(true)
                   : rocket_stage0_error(message.empty() ? "assertion failed" : message);
}

inline RocketAggregate rocket_std_testing_equal_int(std::int64_t expected,
                                                      std::int64_t actual,
                                                      const std::string& message) {
  if (expected == actual) return rocket_stage0_ok(true);
  return rocket_stage0_error((message.empty() ? "integer assertion failed" : message) +
                             ": expected " + std::to_string(expected) + ", actual " +
                             std::to_string(actual));
}

inline RocketAggregate rocket_std_testing_equal_string(const std::string& expected,
                                                         const std::string& actual,
                                                         const std::string& message) {
  if (expected == actual) return rocket_stage0_ok(true);
  return rocket_stage0_error((message.empty() ? "string assertion failed" : message) +
                             ": expected '" + expected + "', actual '" + actual + "'");
}

inline RocketAggregate rocket_std_testing_temp_directory(const std::string& prefix) {
  if (prefix.empty() || prefix.size() > 32 ||
      !std::all_of(prefix.begin(), prefix.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '-' || character == '_';
      }))
    return rocket_stage0_error("test temporary prefix must use 1 through 32 letters, digits, '-' or '_'");
  std::error_code filesystem_error;
  const auto temporary = std::filesystem::temp_directory_path(filesystem_error);
  if (filesystem_error)
    return rocket_stage0_error("could not locate the host temporary directory");
  for (int attempt = 0; attempt < 64; ++attempt) {
    std::vector<std::uint8_t> random;
    std::string error;
    if (!rocket::platform_crypto::secureRandom(8, random, error))
      return rocket_stage0_error(error);
    constexpr char digits[] = "0123456789abcdef";
    std::string suffix;
    for (std::uint8_t byte : random) {
      suffix.push_back(digits[byte >> 4]);
      suffix.push_back(digits[byte & 15]);
    }
    const auto candidate = temporary / (prefix + "-" + suffix);
    if (!std::filesystem::create_directory(candidate, filesystem_error)) {
      if (!filesystem_error) continue;
      return rocket_stage0_error("could not create a test temporary directory");
    }
    const std::string normalized = rocket_stage0_path_string(candidate.lexically_normal());
    {
      std::lock_guard<std::mutex> lock(rocket_stage0_testing_mutex());
      rocket_stage0_testing_temporaries().insert(normalized);
    }
    return rocket_stage0_ok(normalized);
  }
  return rocket_stage0_error("could not allocate a unique test temporary directory");
}

inline RocketAggregate rocket_std_testing_fixture_path(const std::string& root,
                                                         const std::string& relative) {
  const std::string normalized = rocket_stage0_path_string(
      rocket_stage0_path(root).lexically_normal());
  {
    std::lock_guard<std::mutex> lock(rocket_stage0_testing_mutex());
    if (!rocket_stage0_testing_temporaries().contains(normalized))
      return rocket_stage0_error("test fixture root was not created by testing.temp_directory");
  }
  if (!rocket_stage0_safe_testing_relative(relative))
    return rocket_stage0_error("test fixture path must be a safe relative path");
  return rocket_stage0_ok(rocket_stage0_path_string(
      (rocket_stage0_path(root) / rocket_stage0_path(relative)).lexically_normal()));
}

inline RocketAggregate rocket_std_testing_cleanup_temp(const std::string& path) {
  const std::string normalized = rocket_stage0_path_string(
      rocket_stage0_path(path).lexically_normal());
  {
    std::lock_guard<std::mutex> lock(rocket_stage0_testing_mutex());
    if (!rocket_stage0_testing_temporaries().contains(normalized))
      return rocket_stage0_error("test temporary path is unknown or already cleaned");
  }
  std::error_code error;
  std::filesystem::remove_all(rocket_stage0_path(path), error);
  if (error) return rocket_stage0_error("could not clean the test temporary directory");
  {
    std::lock_guard<std::mutex> lock(rocket_stage0_testing_mutex());
    rocket_stage0_testing_temporaries().erase(normalized);
  }
  return rocket_stage0_ok(true);
}

inline RocketAggregate rocket_std_testing_coverage_hit(const std::string& name) {
  if (name.empty() || name.size() > 256 ||
      !std::all_of(name.begin(), name.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '_' || character == '-' ||
               character == '.' || character == ':';
      }))
    return rocket_stage0_error("coverage point name contains unsupported bytes");
  std::lock_guard<std::mutex> lock(rocket_stage0_testing_mutex());
  auto& count = rocket_stage0_testing_coverage()[name];
  if (count == (std::numeric_limits<std::int64_t>::max)())
    return rocket_stage0_error("coverage point counter overflowed");
  ++count;
  return rocket_stage0_ok(true);
}

inline RocketAggregate rocket_std_testing_coverage_write(const std::string& path) {
  std::vector<std::pair<std::string, std::int64_t>> points;
  {
    std::lock_guard<std::mutex> lock(rocket_stage0_testing_mutex());
    points.assign(rocket_stage0_testing_coverage().begin(),
                  rocket_stage0_testing_coverage().end());
  }
  std::sort(points.begin(), points.end());
  std::ofstream output(rocket_stage0_path(path), std::ios::binary | std::ios::trunc);
  if (!output) return rocket_stage0_error("could not create coverage output file");
  output << "{\"version\":1,\"points\":[";
  for (std::size_t index = 0; index < points.size(); ++index) {
    if (index) output << ',';
    output << "{\"name\":\"" << points[index].first << "\",\"hits\":"
           << points[index].second << '}';
  }
  output << "]}\n";
  output.flush();
  if (!output) return rocket_stage0_error("could not write coverage output file");
  return rocket_stage0_ok(true);
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

inline RocketTask rocket_std_async_net_connect(
    const std::string& host, std::int64_t port, std::int64_t deadline,
    const RocketCancellation& token) {
  return rocket_task([host, port, deadline, token] {
    if (rocket_stage0_operation_cancelled(token)) return rocket_stage0_error("operation cancelled");
    return rocket_std_net_tcp_connect(host, port,
        (std::max)(std::int64_t{0}, deadline - rocket_stage0_monotonic_milliseconds()));
  });
}
inline RocketTask rocket_std_async_net_accept(
    std::int64_t listener, std::int64_t deadline, const RocketCancellation& token) {
  return rocket_task([listener, deadline, token] {
    if (rocket_stage0_operation_cancelled(token)) return rocket_stage0_error("operation cancelled");
    return rocket_std_net_accept(listener,
        (std::max)(std::int64_t{0}, deadline - rocket_stage0_monotonic_milliseconds()));
  });
}
inline RocketTask rocket_std_async_net_receive(
    std::int64_t socket, std::int64_t maximum, std::int64_t deadline,
    const RocketCancellation& token) {
  return rocket_task([socket, maximum, deadline, token] {
    if (rocket_stage0_operation_cancelled(token)) return rocket_stage0_error("operation cancelled");
    RocketAggregate result = rocket_std_net_receive(socket, maximum,
        (std::max)(std::int64_t{0}, deadline - rocket_stage0_monotonic_milliseconds()));
    if (result->tag != 0) return result;
    RocketAggregate buffer = rocket_field<RocketAggregate>(result, 0);
    return rocket_stage0_ok(rocket_field<RocketArray<char>>(buffer, 0));
  });
}
inline RocketTask rocket_std_async_net_send(
    std::int64_t socket, const RocketUniqueBuffer<char>& bytes,
    std::int64_t deadline, const RocketCancellation& token) {
  return rocket_task([socket, bytes, deadline, token] {
    if (rocket_stage0_operation_cancelled(token)) return rocket_stage0_error("operation cancelled");
    RocketAggregate buffer = rocket_aggregate(0, {bytes});
    return rocket_std_net_send(socket, buffer,
        (std::max)(std::int64_t{0}, deadline - rocket_stage0_monotonic_milliseconds()));
  });
}
inline RocketTask rocket_std_async_process_run(
    const std::string& program, const RocketArray<std::string>& arguments,
    std::int64_t deadline, const RocketCancellation& token) {
  return rocket_task([program, arguments, deadline, token] {
    if (rocket_stage0_operation_cancelled(token)) return rocket_stage0_error("operation cancelled");
    if (deadline < 0 || rocket_stage0_monotonic_milliseconds() >= deadline)
      return rocket_stage0_error("process deadline exceeded");
#ifdef _WIN32
    const std::wstring executable = rocket_stage0_wide(program);
    if (executable.empty())
      return rocket_stage0_error("process program is empty or invalid UTF-8");
    std::wstring command = rocket_stage0_quote(executable);
    for (const auto& argument : *arguments)
      command += L" " + rocket_stage0_quote(rocket_stage0_wide(argument));
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, 0,
                        nullptr, nullptr, &startup, &process))
      return rocket_stage0_error("could not start asynchronous process (Windows error " +
                                 std::to_string(GetLastError()) + ")");
    bool cancelled = false; bool timedOut = false; DWORD waited = WAIT_TIMEOUT;
    while (waited == WAIT_TIMEOUT) {
      if (rocket_stage0_operation_cancelled(token)) { cancelled = true; break; }
      const std::int64_t remaining = deadline - rocket_stage0_monotonic_milliseconds();
      if (remaining <= 0) { timedOut = true; break; }
      waited = WaitForSingleObject(process.hProcess,
          static_cast<DWORD>((std::min)(std::int64_t{10}, remaining)));
    }
    RocketAggregate result;
    if (cancelled || timedOut) {
      TerminateProcess(process.hProcess, 1); WaitForSingleObject(process.hProcess, 5000);
      result = rocket_stage0_error(cancelled ? "operation cancelled"
                                             : "process deadline exceeded");
    } else if (waited == WAIT_OBJECT_0) {
      DWORD code = 1;
      result = GetExitCodeProcess(process.hProcess, &code)
          ? rocket_stage0_ok(static_cast<std::int64_t>(code))
          : rocket_stage0_error("could not read asynchronous process exit code");
    } else result = rocket_stage0_error("asynchronous process wait failed");
    CloseHandle(process.hThread); CloseHandle(process.hProcess); return result;
#else
    return rocket_stage0_error("async_process.run is only implemented on Windows x64");
#endif
  });
}
