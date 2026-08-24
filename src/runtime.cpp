#include "runtime.h"

#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <chrono>
#include <cstddef>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <limits>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

inline constexpr std::uint32_t RuntimeAbiVersion = 1;

struct AllocationHeader {
  std::uint64_t references;
  std::uint64_t sharedReferences;
  std::uint64_t weakReferences;
  void (*destroy)(AllocationHeader*);
  std::uint32_t objectKind;
  std::uint32_t flags;
};

inline constexpr std::uint32_t AllocationShared = 1U;
inline constexpr std::uint32_t AllocationDestroyed = 2U;
inline constexpr std::uint32_t AllocationPromoting = 4U;

inline constexpr std::uint32_t ObjectString = 1;
inline constexpr std::uint32_t ObjectArray = 2;
inline constexpr std::uint32_t ObjectSlice = 3;
inline constexpr std::uint32_t ObjectAggregate = 4;
inline constexpr std::uint32_t ObjectStringBuilder = 5;
inline constexpr std::uint32_t ObjectWeak = 6;
inline constexpr std::uint32_t ObjectTask = 7;
inline constexpr std::uint32_t ObjectCancellation = 8;
inline constexpr std::uint32_t ObjectMutex = 9;
inline constexpr std::uint32_t ObjectGuard = 10;
inline constexpr std::uint32_t ObjectEvent = 11;
inline constexpr std::uint32_t ObjectAtomicInt = 12;
inline constexpr std::uint32_t ObjectOnce = 13;
inline constexpr std::uint32_t ObjectChannel = 14;
inline constexpr std::uint32_t ObjectChannelEndpoint = 15;
inline constexpr std::uint32_t ObjectTaskGroup = 16;
inline constexpr std::uint32_t ObjectThread = 17;

struct RuntimeString {
  AllocationHeader header;
  std::uint64_t byteLength;
  std::uint8_t bytes[1];
};

struct RuntimeArray {
  AllocationHeader header;
  std::uint32_t elementKind;
  std::uint32_t reserved;
  std::uint64_t length;
  std::uint64_t capacity;
  void* elements;
};

struct RuntimeSlice {
  AllocationHeader header;
  RuntimeArray* owner;
  std::uint64_t offset;
  std::uint64_t length;
};

struct RuntimeAggregate {
  AllocationHeader header;
  std::uint32_t tag;
  std::uint32_t fieldCount;
  std::uint64_t managedMask;
  std::uint64_t* fields;
};

struct RuntimeStringBuilder {
  AllocationHeader header;
  std::uint64_t length;
  std::uint64_t capacity;
  std::uint8_t* bytes;
};

struct RuntimeWeak {
  AllocationHeader header;
  AllocationHeader* target;
};

using TaskEntry = void* (*)(void*);
struct RuntimeCancellation;

enum class TaskState : std::uint8_t { Queued, Running, Completed };

struct RuntimeTask {
  AllocationHeader header;
  TaskEntry entry;
  void* context;
  void* result;
  std::mutex mutex;
  std::condition_variable completed;
  TaskState state;
  bool executorReleased = false;
  RuntimeCancellation* cancellation = nullptr;
};

struct RuntimeCancellation {
  AllocationHeader header;
  std::atomic<std::uint8_t> cancelled{0};
  RuntimeCancellation* parent = nullptr;
};

thread_local RuntimeCancellation* currentTaskCancellation = nullptr;

struct RuntimeStoredValue {
  std::uint64_t bits = 0;
};

RocketAggregate* runtimeError(std::string_view message);
RocketAggregate* runtimeOkBool(bool value);
bool cancellationObserved(RuntimeCancellation* token);
bool operationCancellationObserved(RuntimeCancellation* token);

struct RuntimeMutex {
  AllocationHeader header;
  std::timed_mutex mutex;
  RuntimeStoredValue value;
  std::uint32_t elementKind = ROCKET_ELEMENT_MANAGED;
};

struct RuntimeGuard {
  AllocationHeader header;
  RuntimeMutex* owner = nullptr;
  bool locked = false;
};

struct RuntimeEvent {
  AllocationHeader header;
  std::mutex mutex;
  std::condition_variable changed;
  bool manualReset = false;
  bool set = false;
  std::uint64_t generation = 0;
};

struct RuntimeAtomicInt {
  AllocationHeader header;
  std::atomic<std::int64_t> value{0};
};

struct RuntimeOnce {
  AllocationHeader header;
  std::mutex mutex;
  RuntimeStoredValue value;
  std::uint32_t elementKind = ROCKET_ELEMENT_MANAGED;
  bool initialized = false;
};

struct RuntimeChannel {
  AllocationHeader header;
  std::mutex mutex;
  std::condition_variable readable;
  std::condition_variable writable;
  std::deque<RuntimeStoredValue> values;
  std::uint32_t elementKind = ROCKET_ELEMENT_MANAGED;
  std::uint64_t capacity = 0;
  std::uint64_t maximum = 0;
  std::uint64_t senders = 0;
  std::uint64_t receivers = 0;
};

struct RuntimeChannelEndpoint {
  AllocationHeader header;
  RuntimeChannel* channel = nullptr;
  bool sender = false;
  std::atomic<std::uint8_t> open{1};
};

struct RuntimeTaskGroup {
  AllocationHeader header;
  std::mutex mutex;
  std::vector<RuntimeTask*> tasks;
  bool joined = false;
  std::uint32_t elementKind = ROCKET_ELEMENT_MANAGED;
};

struct RuntimeThread {
  AllocationHeader header;
  RuntimeTask* task = nullptr;
  std::thread worker;
  std::mutex mutex;
  std::condition_variable completed;
  void* result = nullptr;
  bool finished = false;
  bool consumed = false;
  bool detached = false;
};

std::mutex toolingMutex;
std::map<std::pair<std::string, std::int64_t>, std::uint64_t> coverageHits;
std::map<std::string, std::uint64_t> profileHits;
std::once_flag toolingExitRegistration;

std::string toolingJson(const std::string& value) {
  std::string result;
  for (const unsigned char character : value) {
    if (character == '"' || character == '\\') {
      result.push_back('\\');
      result.push_back(static_cast<char>(character));
    } else if (character == '\n') result += "\\n";
    else if (character == '\r') result += "\\r";
    else if (character == '\t') result += "\\t";
    else if (character >= 0x20) result.push_back(static_cast<char>(character));
  }
  return result;
}

void toolingWriteReports() {
  std::lock_guard lock(toolingMutex);
  if (const char* path = std::getenv("ROCKET_COVERAGE_FILE")) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << "{\n  \"schema\": \"rocket-coverage-1\",\n  \"points\": [";
    bool first = true;
    for (const auto& [point, hits] : coverageHits) {
      if (!first) output << ',';
      first = false;
      output << "\n    {\"source\":\"" << toolingJson(point.first)
             << "\",\"line\":" << point.second << ",\"hits\":" << hits << '}';
    }
    output << "\n  ]\n}\n";
  }
  if (const char* path = std::getenv("ROCKET_PROFILE_FILE")) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << "{\n  \"schema\": \"rocket-profile-1\",\n  \"symbols\": [";
    bool first = true;
    for (const auto& [symbol, hits] : profileHits) {
      if (!first) output << ',';
      first = false;
      output << "\n    {\"symbol\":\"" << toolingJson(symbol)
             << "\",\"samples\":" << hits << '}';
    }
    output << "\n  ]\n}\n";
  }
}

struct CollectionView {
  const RuntimeArray* owner;
  std::uint64_t offset;
  std::uint64_t length;
};

std::atomic<std::uint64_t> liveAllocations = 0;

std::uint64_t strongReferenceCount(const AllocationHeader* header);

[[noreturn]] void runtimeFailure(const char* message) {
  std::fputs("rocket runtime error: ", stderr);
  std::fputs(message, stderr);
  std::fputc('\n', stderr);
  std::exit(101);
}

bool continuation(std::uint8_t byte) { return (byte & 0xc0U) == 0x80U; }

bool validUtf8(const std::uint8_t* bytes, std::uint64_t length) {
  std::uint64_t index = 0;
  while (index < length) {
    const std::uint8_t first = bytes[index++];
    if (first <= 0x7fU) continue;
    if (first >= 0xc2U && first <= 0xdfU) {
      if (index >= length || !continuation(bytes[index])) return false;
      ++index;
      continue;
    }
    if (first >= 0xe0U && first <= 0xefU) {
      if (index + 1 >= length || !continuation(bytes[index]) ||
          !continuation(bytes[index + 1]))
        return false;
      if (first == 0xe0U && bytes[index] < 0xa0U) return false;
      if (first == 0xedU && bytes[index] >= 0xa0U) return false;
      index += 2;
      continue;
    }
    if (first >= 0xf0U && first <= 0xf4U) {
      if (index + 2 >= length || !continuation(bytes[index]) ||
          !continuation(bytes[index + 1]) || !continuation(bytes[index + 2]))
        return false;
      if (first == 0xf0U && bytes[index] < 0x90U) return false;
      if (first == 0xf4U && bytes[index] >= 0x90U) return false;
      index += 3;
      continue;
    }
    return false;
  }
  return true;
}

void destroyString(AllocationHeader* header) {
  (void)header;
  liveAllocations.fetch_sub(1, std::memory_order_relaxed);
}

std::size_t elementSize(std::uint32_t elementKind) {
  switch (elementKind) {
  case ROCKET_ELEMENT_INT: return sizeof(std::int64_t);
  case ROCKET_ELEMENT_FLOAT: return sizeof(double);
  case ROCKET_ELEMENT_BOOL:
  case ROCKET_ELEMENT_CHAR: return sizeof(std::uint8_t);
  case ROCKET_ELEMENT_STRING: return sizeof(RocketString*);
  case ROCKET_ELEMENT_MANAGED: return sizeof(void*);
  default: runtimeFailure("invalid Array element kind");
  }
}

void destroyArray(AllocationHeader* header);

RuntimeArray* allocateArray(std::uint32_t elementKind, std::uint64_t length,
                            std::uint64_t capacity) {
  if (capacity < length) runtimeFailure("Array capacity is smaller than its length");
  const std::size_t size = elementSize(elementKind);
  if (capacity > std::numeric_limits<std::size_t>::max() / size)
    runtimeFailure("Array allocation is too large");
  auto* array = static_cast<RuntimeArray*>(std::malloc(sizeof(RuntimeArray)));
  if (!array) runtimeFailure("out of memory while allocating Array");
  void* elements = nullptr;
  if (capacity != 0) {
    elements = std::calloc(static_cast<std::size_t>(capacity), size);
    if (!elements) {
      std::free(array);
      runtimeFailure("out of memory while allocating Array elements");
    }
  }
  array->header = {1, 0, 1, destroyArray, ObjectArray, 0};
  array->elementKind = elementKind;
  array->reserved = 0;
  array->length = length;
  array->capacity = capacity;
  array->elements = elements;
  liveAllocations.fetch_add(1, std::memory_order_relaxed);
  return array;
}

void destroyArray(AllocationHeader* header) {
  auto* array = reinterpret_cast<RuntimeArray*>(header);
  if (array->elementKind == ROCKET_ELEMENT_STRING ||
      array->elementKind == ROCKET_ELEMENT_MANAGED) {
    auto** strings = static_cast<void**>(array->elements);
    for (std::uint64_t index = 0; index < array->length; ++index)
      rocket_rt_release(strings[index]);
  }
  std::free(array->elements);
  array->elements = nullptr;
  liveAllocations.fetch_sub(1, std::memory_order_relaxed);
}

void destroyAggregate(AllocationHeader* header) {
  auto* aggregate = reinterpret_cast<RuntimeAggregate*>(header);
  for (std::uint32_t index = 0; index < aggregate->fieldCount; ++index)
    if ((aggregate->managedMask & (std::uint64_t{1} << index)) != 0)
      rocket_rt_release(reinterpret_cast<void*>(aggregate->fields[index]));
  std::free(aggregate->fields);
  aggregate->fields = nullptr;
  liveAllocations.fetch_sub(1, std::memory_order_relaxed);
}

void destroySlice(AllocationHeader* header) {
  auto* slice = reinterpret_cast<RuntimeSlice*>(header);
  rocket_rt_release(slice->owner);
  slice->owner = nullptr;
  liveAllocations.fetch_sub(1, std::memory_order_relaxed);
}

void destroyStringBuilder(AllocationHeader* header) {
  auto* builder = reinterpret_cast<RuntimeStringBuilder*>(header);
  std::free(builder->bytes);
  builder->bytes = nullptr;
  liveAllocations.fetch_sub(1, std::memory_order_relaxed);
}

AllocationHeader* headerFor(void* object) {
  return static_cast<AllocationHeader*>(object);
}

const RuntimeString* stringFor(const RocketString* string) {
  return reinterpret_cast<const RuntimeString*>(string);
}

RuntimeArray* arrayFor(RocketArray* array) {
  return reinterpret_cast<RuntimeArray*>(array);
}

CollectionView collectionView(const void* collection) {
  if (!collection) runtimeFailure("null collection");
  const auto* header = static_cast<const AllocationHeader*>(collection);
  if (header->objectKind == ObjectArray) {
    const auto* array = static_cast<const RuntimeArray*>(collection);
    return {array, 0, array->length};
  }
  if (header->objectKind == ObjectSlice) {
    const auto* slice = static_cast<const RuntimeSlice*>(collection);
    return {slice->owner, slice->offset, slice->length};
  }
  runtimeFailure("value is not an Array or Slice");
}

[[noreturn]] void indexFailure(std::int64_t index, std::uint64_t length) {
  std::fprintf(stderr, "rocket runtime error: index %" PRId64
                       " out of bounds for length %" PRIu64 "\n",
               index, length);
  std::exit(101);
}

[[noreturn]] void sliceFailure(std::int64_t start, std::int64_t end,
                               std::uint64_t length) {
  std::fprintf(stderr, "rocket runtime error: slice %" PRId64 "..%" PRId64
                       " out of bounds for length %" PRIu64 "\n",
               start, end, length);
  std::exit(101);
}

std::uint64_t checkedIndex(const CollectionView& view, std::int64_t index,
                           std::uint32_t expectedKind) {
  if (view.owner->elementKind != expectedKind)
    runtimeFailure("collection element kind does not match generated code");
  if (index < 0 || static_cast<std::uint64_t>(index) >= view.length)
    indexFailure(index, view.length);
  return view.offset + static_cast<std::uint64_t>(index);
}

RuntimeArray* checkedArray(RocketArray* array, std::int64_t index,
                           std::uint32_t expectedKind) {
  RuntimeArray* runtimeArray = arrayFor(array);
  if (!runtimeArray || runtimeArray->header.objectKind != ObjectArray)
    runtimeFailure("array initializer received an invalid Array");
  if (runtimeArray->elementKind != expectedKind)
    runtimeFailure("array initializer element kind mismatch");
  if (index < 0 || static_cast<std::uint64_t>(index) >= runtimeArray->length)
    indexFailure(index, runtimeArray->length);
  return runtimeArray;
}

RuntimeArray* checkedArray(RocketArray* array) {
  RuntimeArray* runtimeArray = arrayFor(array);
  if (!runtimeArray || runtimeArray->header.objectKind != ObjectArray)
    runtimeFailure("Array operation received an invalid Array");
  return runtimeArray;
}

RuntimeArray* cloneArray(const RuntimeArray* source, std::uint64_t capacity) {
  RuntimeArray* copy = allocateArray(source->elementKind, source->length, capacity);
  const std::size_t bytes = static_cast<std::size_t>(source->length) *
                            elementSize(source->elementKind);
  if (bytes != 0) std::memcpy(copy->elements, source->elements, bytes);
  if (source->elementKind == ROCKET_ELEMENT_STRING ||
      source->elementKind == ROCKET_ELEMENT_MANAGED) {
    auto** elements = static_cast<void**>(copy->elements);
    for (std::uint64_t element = 0; element < copy->length; ++element)
      rocket_rt_retain(elements[element]);
  }
  return copy;
}

RuntimeArray* copyArrayForUpdate(RocketArray* array, std::int64_t index,
                                 std::uint32_t expectedKind) {
  RuntimeArray* source = checkedArray(array, index, expectedKind);
  if (strongReferenceCount(&source->header) == 1) {
    rocket_rt_retain(array);
    return source;
  }

  return cloneArray(source, source->capacity);
}

std::uint64_t grownCapacity(std::uint64_t current, std::uint64_t minimum) {
  std::uint64_t capacity = current == 0 ? 4 : current;
  while (capacity < minimum) {
    if (capacity > (std::numeric_limits<std::uint64_t>::max)() / 2) {
      capacity = minimum;
      break;
    }
    capacity *= 2;
  }
  return capacity;
}

RuntimeAggregate* checkedAggregate(RocketAggregate* aggregate, std::uint32_t field) {
  auto* runtime = reinterpret_cast<RuntimeAggregate*>(aggregate);
  if (!runtime || runtime->header.objectKind != ObjectAggregate)
    runtimeFailure("aggregate operation received an invalid value");
  if (field >= runtime->fieldCount) runtimeFailure("aggregate field index out of bounds");
  return runtime;
}

const RuntimeAggregate* checkedAggregate(const RocketAggregate* aggregate,
                                         std::uint32_t field) {
  return checkedAggregate(const_cast<RocketAggregate*>(aggregate), field);
}

template <typename Value, typename Setter>
RocketArray* appendArray(RocketArray* array, Value value, std::uint32_t expectedKind,
                         Setter setter) {
  RuntimeArray* source = checkedArray(array);
  if (source->elementKind != expectedKind)
    runtimeFailure("Array append element kind mismatch");
  if (source->length == (std::numeric_limits<std::uint64_t>::max)())
    runtimeFailure("Array length overflow");
  const std::uint64_t index = source->length;
  RuntimeArray* result = cloneArray(source, grownCapacity(source->capacity, index + 1));
  result->length = index + 1;
  setter(reinterpret_cast<RocketArray*>(result), static_cast<std::int64_t>(index), value);
  return reinterpret_cast<RocketArray*>(result);
}

template <typename Value, typename Setter>
RocketArray* insertArray(RocketArray* array, std::int64_t index, Value value,
                         std::uint32_t expectedKind, Setter setter) {
  RuntimeArray* source = checkedArray(array);
  if (source->elementKind != expectedKind)
    runtimeFailure("Array insert element kind mismatch");
  if (index < 0 || static_cast<std::uint64_t>(index) > source->length)
    indexFailure(index, source->length);
  if (source->length == (std::numeric_limits<std::uint64_t>::max)())
    runtimeFailure("Array length overflow");
  const std::uint64_t offset = static_cast<std::uint64_t>(index);
  RuntimeArray* result = cloneArray(
      source, grownCapacity(source->capacity, source->length + 1));
  const std::size_t size = elementSize(result->elementKind);
  const std::size_t trailing = static_cast<std::size_t>(result->length - offset) * size;
  auto* bytes = static_cast<std::uint8_t*>(result->elements);
  if (trailing != 0)
    std::memmove(bytes + (offset + 1) * size, bytes + offset * size, trailing);
  std::memset(bytes + offset * size, 0, size);
  ++result->length;
  setter(reinterpret_cast<RocketArray*>(result), index, value);
  return reinterpret_cast<RocketArray*>(result);
}

template <typename Value, typename Setter>
RocketArray* appendUniqueBuffer(RocketArray* buffer, Value value,
                                std::uint32_t expectedKind, Setter setter) {
  RuntimeArray* array = checkedArray(buffer);
  if (array->elementKind != expectedKind)
    runtimeFailure("UniqueBuffer append element kind mismatch");
  if (array->length == (std::numeric_limits<std::uint64_t>::max)())
    runtimeFailure("UniqueBuffer length overflow");
  const std::uint64_t index = array->length;
  if (index == array->capacity) {
    const std::uint64_t capacity = grownCapacity(array->capacity, index + 1);
    const std::size_t elementBytes = elementSize(array->elementKind);
    if (capacity > (std::numeric_limits<std::size_t>::max)() / elementBytes)
      runtimeFailure("UniqueBuffer allocation is too large");
    void* resized = std::realloc(array->elements,
                                 static_cast<std::size_t>(capacity) * elementBytes);
    if (!resized) runtimeFailure("out of memory while growing UniqueBuffer");
    array->elements = resized;
    std::memset(static_cast<std::uint8_t*>(array->elements) +
                    static_cast<std::size_t>(array->capacity) * elementBytes,
                0, static_cast<std::size_t>(capacity - array->capacity) * elementBytes);
    array->capacity = capacity;
  }
  ++array->length;
  setter(buffer, static_cast<std::int64_t>(index), value);
  rocket_rt_retain(buffer);
  return buffer;
}

std::uint64_t strongReferenceCount(const AllocationHeader* header) {
  auto* mutableHeader = const_cast<AllocationHeader*>(header);
  const auto flags = std::atomic_ref<std::uint32_t>(mutableHeader->flags)
                         .load(std::memory_order_acquire);
  if ((flags & AllocationShared) == 0) return header->references;
  return std::atomic_ref<std::uint64_t>(mutableHeader->sharedReferences)
      .load(std::memory_order_relaxed);
}

void freeAllocationStorage(AllocationHeader* header) {
  if (header->objectKind == ObjectTask)
    delete reinterpret_cast<RuntimeTask*>(header);
  else if (header->objectKind == ObjectCancellation)
    delete reinterpret_cast<RuntimeCancellation*>(header);
  else if (header->objectKind == ObjectMutex)
    delete reinterpret_cast<RuntimeMutex*>(header);
  else if (header->objectKind == ObjectGuard)
    delete reinterpret_cast<RuntimeGuard*>(header);
  else if (header->objectKind == ObjectEvent)
    delete reinterpret_cast<RuntimeEvent*>(header);
  else if (header->objectKind == ObjectAtomicInt)
    delete reinterpret_cast<RuntimeAtomicInt*>(header);
  else if (header->objectKind == ObjectOnce)
    delete reinterpret_cast<RuntimeOnce*>(header);
  else if (header->objectKind == ObjectChannel)
    delete reinterpret_cast<RuntimeChannel*>(header);
  else if (header->objectKind == ObjectChannelEndpoint)
    delete reinterpret_cast<RuntimeChannelEndpoint*>(header);
  else if (header->objectKind == ObjectTaskGroup)
    delete reinterpret_cast<RuntimeTaskGroup*>(header);
  else if (header->objectKind == ObjectThread)
    delete reinterpret_cast<RuntimeThread*>(header);
  else
    std::free(header);
}

void releaseWeakReference(AllocationHeader* header) {
  auto weak = std::atomic_ref<std::uint64_t>(header->weakReferences);
  const std::uint64_t previous = weak.fetch_sub(1, std::memory_order_release);
  if (previous == 0) runtimeFailure("weak reference count underflow");
  if (previous == 1) {
    std::atomic_thread_fence(std::memory_order_acquire);
    freeAllocationStorage(header);
  }
}

bool managedElementKind(std::uint32_t elementKind) {
  return elementKind == ROCKET_ELEMENT_STRING ||
         elementKind == ROCKET_ELEMENT_MANAGED;
}

RuntimeStoredValue storedPointer(void* value) {
  return {static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(value))};
}

void* storedPointer(RuntimeStoredValue value) {
  return reinterpret_cast<void*>(static_cast<std::uintptr_t>(value.bits));
}

RuntimeStoredValue storedFloat(double value) {
  RuntimeStoredValue stored;
  static_assert(sizeof(stored.bits) == sizeof(value));
  std::memcpy(&stored.bits, &value, sizeof(value));
  return stored;
}

double storedFloat(RuntimeStoredValue value) {
  double result = 0;
  std::memcpy(&result, &value.bits, sizeof(result));
  return result;
}

void promoteStored(std::uint32_t elementKind, RuntimeStoredValue value) {
  if (managedElementKind(elementKind)) rocket_rt_promote(storedPointer(value));
}

void retainStored(std::uint32_t elementKind, RuntimeStoredValue value) {
  if (managedElementKind(elementKind)) rocket_rt_retain(storedPointer(value));
}

void releaseStored(std::uint32_t elementKind, RuntimeStoredValue value) {
  if (managedElementKind(elementKind)) rocket_rt_release(storedPointer(value));
}

void promoteObject(void* object);

void promoteChildren(AllocationHeader* header) {
  switch (header->objectKind) {
  case ObjectArray: {
    auto* array = reinterpret_cast<RuntimeArray*>(header);
    if (array->elementKind == ROCKET_ELEMENT_STRING ||
        array->elementKind == ROCKET_ELEMENT_MANAGED) {
      auto** values = static_cast<void**>(array->elements);
      for (std::uint64_t index = 0; index < array->length; ++index)
        promoteObject(values[index]);
    }
    break;
  }
  case ObjectSlice:
    promoteObject(reinterpret_cast<RuntimeSlice*>(header)->owner);
    break;
  case ObjectAggregate: {
    auto* aggregate = reinterpret_cast<RuntimeAggregate*>(header);
    for (std::uint32_t index = 0; index < aggregate->fieldCount; ++index)
      if ((aggregate->managedMask & (std::uint64_t{1} << index)) != 0)
        promoteObject(reinterpret_cast<void*>(aggregate->fields[index]));
    break;
  }
  case ObjectTask: {
    auto* task = reinterpret_cast<RuntimeTask*>(header);
    std::lock_guard lock(task->mutex);
    promoteObject(task->context);
    promoteObject(task->result);
    promoteObject(task->cancellation);
    break;
  }
  case ObjectCancellation:
    promoteObject(reinterpret_cast<RuntimeCancellation*>(header)->parent);
    break;
  case ObjectMutex: {
    auto* mutex = reinterpret_cast<RuntimeMutex*>(header);
    std::lock_guard lock(mutex->mutex);
    promoteStored(mutex->elementKind, mutex->value);
    break;
  }
  case ObjectGuard:
    promoteObject(reinterpret_cast<RuntimeGuard*>(header)->owner);
    break;
  case ObjectOnce: {
    auto* once = reinterpret_cast<RuntimeOnce*>(header);
    std::lock_guard lock(once->mutex);
    if (once->initialized) promoteStored(once->elementKind, once->value);
    break;
  }
  case ObjectChannel: {
    auto* channel = reinterpret_cast<RuntimeChannel*>(header);
    std::lock_guard lock(channel->mutex);
    for (RuntimeStoredValue value : channel->values)
      promoteStored(channel->elementKind, value);
    break;
  }
  case ObjectChannelEndpoint:
    promoteObject(reinterpret_cast<RuntimeChannelEndpoint*>(header)->channel);
    break;
  case ObjectTaskGroup: {
    auto* group = reinterpret_cast<RuntimeTaskGroup*>(header);
    std::lock_guard lock(group->mutex);
    for (RuntimeTask* task : group->tasks) promoteObject(task);
    break;
  }
  case ObjectThread: {
    auto* thread = reinterpret_cast<RuntimeThread*>(header);
    std::lock_guard lock(thread->mutex);
    promoteObject(thread->task);
    promoteObject(thread->result);
    break;
  }
  case ObjectString:
  case ObjectStringBuilder:
  case ObjectWeak:
  case ObjectEvent:
  case ObjectAtomicInt:
    break;
  default:
    runtimeFailure("invalid managed object during atomic promotion");
  }
}

void promoteObject(void* object) {
  if (!object) return;
  AllocationHeader* header = headerFor(object);
  auto flags = std::atomic_ref<std::uint32_t>(header->flags);
  std::uint32_t observed = flags.load(std::memory_order_acquire);
  while (true) {
    if ((observed & AllocationShared) != 0) return;
    if ((observed & AllocationDestroyed) != 0)
      runtimeFailure("attempted to publish a destroyed managed object");
    if ((observed & AllocationPromoting) != 0) {
      std::this_thread::yield();
      observed = flags.load(std::memory_order_acquire);
      continue;
    }
    if (flags.compare_exchange_weak(observed, observed | AllocationPromoting,
                                    std::memory_order_acq_rel,
                                    std::memory_order_acquire))
      break;
  }
  promoteChildren(header);
  std::atomic_ref<std::uint64_t>(header->sharedReferences)
      .store(header->references, std::memory_order_relaxed);
  flags.store(AllocationShared, std::memory_order_release);
}

void finishStrongLifetime(AllocationHeader* header) {
  header->destroy(header);
  std::atomic_ref<std::uint32_t>(header->flags)
      .fetch_or(AllocationDestroyed, std::memory_order_release);
  releaseWeakReference(header);
}

void destroyWeak(AllocationHeader* header) {
  auto* weak = reinterpret_cast<RuntimeWeak*>(header);
  if (weak->target) releaseWeakReference(weak->target);
  weak->target = nullptr;
  liveAllocations.fetch_sub(1, std::memory_order_relaxed);
}

void destroyTask(AllocationHeader* header) {
  auto* task = reinterpret_cast<RuntimeTask*>(header);
  rocket_rt_release(task->context);
  rocket_rt_release(task->result);
  rocket_rt_release(task->cancellation);
  task->context = nullptr;
  task->result = nullptr;
  task->cancellation = nullptr;
  liveAllocations.fetch_sub(1, std::memory_order_relaxed);
}

void destroyCancellation(AllocationHeader* header) {
  auto* token = reinterpret_cast<RuntimeCancellation*>(header);
  rocket_rt_release(token->parent);
  token->parent = nullptr;
  liveAllocations.fetch_sub(1, std::memory_order_relaxed);
}

void destroyMutex(AllocationHeader* header) {
  auto* mutex = reinterpret_cast<RuntimeMutex*>(header);
  releaseStored(mutex->elementKind, mutex->value);
  mutex->value = {};
  liveAllocations.fetch_sub(1, std::memory_order_relaxed);
}

void destroyGuard(AllocationHeader* header) {
  auto* guard = reinterpret_cast<RuntimeGuard*>(header);
  if (guard->locked && guard->owner) {
    guard->locked = false;
    guard->owner->mutex.unlock();
  }
  rocket_rt_release(guard->owner);
  guard->owner = nullptr;
  liveAllocations.fetch_sub(1, std::memory_order_relaxed);
}

void destroyEvent(AllocationHeader*) {
  liveAllocations.fetch_sub(1, std::memory_order_relaxed);
}

void destroyAtomicInt(AllocationHeader*) {
  liveAllocations.fetch_sub(1, std::memory_order_relaxed);
}

void destroyOnce(AllocationHeader* header) {
  auto* once = reinterpret_cast<RuntimeOnce*>(header);
  if (once->initialized) releaseStored(once->elementKind, once->value);
  once->value = {};
  once->initialized = false;
  liveAllocations.fetch_sub(1, std::memory_order_relaxed);
}

void destroyChannel(AllocationHeader* header) {
  auto* channel = reinterpret_cast<RuntimeChannel*>(header);
  for (RuntimeStoredValue value : channel->values)
    releaseStored(channel->elementKind, value);
  channel->values.clear();
  liveAllocations.fetch_sub(1, std::memory_order_relaxed);
}

void closeEndpoint(RuntimeChannelEndpoint* endpoint) {
  if (!endpoint || endpoint->open.exchange(0, std::memory_order_acq_rel) == 0)
    return;
  RuntimeChannel* channel = endpoint->channel;
  {
    std::lock_guard lock(channel->mutex);
    std::uint64_t& count = endpoint->sender ? channel->senders : channel->receivers;
    if (count == 0) runtimeFailure("channel endpoint count underflow");
    --count;
  }
  channel->readable.notify_all();
  channel->writable.notify_all();
}

void destroyChannelEndpoint(AllocationHeader* header) {
  auto* endpoint = reinterpret_cast<RuntimeChannelEndpoint*>(header);
  closeEndpoint(endpoint);
  rocket_rt_release(endpoint->channel);
  endpoint->channel = nullptr;
  liveAllocations.fetch_sub(1, std::memory_order_relaxed);
}

void destroyTaskGroup(AllocationHeader* header) {
  auto* group = reinterpret_cast<RuntimeTaskGroup*>(header);
  if (!group->joined) {
    for (RuntimeTask* task : group->tasks)
      rocket_std_task_cancel(reinterpret_cast<RocketTask*>(task));
    for (RuntimeTask* task : group->tasks) {
      void* result = rocket_rt_task_await(reinterpret_cast<RocketTask*>(task));
      rocket_rt_release(result);
    }
  }
  for (RuntimeTask* task : group->tasks) rocket_rt_release(task);
  group->tasks.clear();
  group->joined = true;
  liveAllocations.fetch_sub(1, std::memory_order_relaxed);
}

void destroyThread(AllocationHeader* header) {
  auto* thread = reinterpret_cast<RuntimeThread*>(header);
  if (thread->worker.joinable()) {
    if (thread->worker.get_id() == std::this_thread::get_id()) thread->worker.detach();
    else thread->worker.join();
  }
  rocket_rt_release(thread->result);
  rocket_rt_release(thread->task);
  thread->result = nullptr;
  thread->task = nullptr;
  liveAllocations.fetch_sub(1, std::memory_order_relaxed);
}

class TaskExecutor {
public:
  explicit TaskExecutor(unsigned requestedWorkers = 0) {
    const unsigned detected = std::thread::hardware_concurrency();
    const unsigned available = detected == 0 ? 4U : detected;
    const unsigned requested = requestedWorkers == 0 ? available : requestedWorkers;
    const unsigned count = (std::max)(1U, (std::min)(requested, 64U));
    workers_.reserve(count);
    for (unsigned index = 0; index < count; ++index)
      workers_.emplace_back([this] { workerLoop(); });
  }

  ~TaskExecutor() {
    {
      std::lock_guard lock(mutex_);
      stopping_ = true;
    }
    ready_.notify_all();
    space_.notify_all();
    for (auto& worker : workers_)
      if (worker.joinable()) worker.join();
  }

  void enqueue(RuntimeTask* task) {
    std::unique_lock lock(mutex_);
    space_.wait(lock, [this] { return stopping_ || queue_.size() < QueueLimit; });
    if (stopping_) runtimeFailure("task executor is shutting down");
    queue_.push_back(task);
    lock.unlock();
    ready_.notify_one();
  }

  bool helpOne() {
    RuntimeTask* task = nullptr;
    {
      std::lock_guard lock(mutex_);
      if (queue_.empty()) return false;
      task = queue_.front();
      queue_.pop_front();
    }
    space_.notify_one();
    execute(task);
    return true;
  }

  static bool isWorker() { return worker_; }

private:
  static constexpr std::size_t QueueLimit = 65536;

  void workerLoop() {
    worker_ = true;
    while (true) {
      RuntimeTask* task = nullptr;
      {
        std::unique_lock lock(mutex_);
        ready_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
        if (stopping_ && queue_.empty()) break;
        task = queue_.front();
        queue_.pop_front();
      }
      space_.notify_one();
      execute(task);
    }
    worker_ = false;
  }

  static void execute(RuntimeTask* task) {
    rocket_rt_retain(task); // completion-publication ownership
    {
      std::lock_guard lock(task->mutex);
      task->state = TaskState::Running;
    }
    RuntimeCancellation* previousCancellation = currentTaskCancellation;
    currentTaskCancellation = task->cancellation;
    void* result = cancellationObserved(task->cancellation)
        ? static_cast<void*>(runtimeError("operation cancelled"))
        : task->entry(task->context);
    currentTaskCancellation = previousCancellation;
    promoteObject(result);
    {
      std::lock_guard lock(task->mutex);
      task->result = result;
      task->state = TaskState::Completed;
    }
    task->completed.notify_all();
    rocket_rt_release(task); // executor ownership
    {
      std::lock_guard lock(task->mutex);
      task->executorReleased = true;
    }
    task->completed.notify_all();
    rocket_rt_release(task); // completion-publication ownership
  }

  std::mutex mutex_;
  std::condition_variable ready_;
  std::condition_variable space_;
  std::deque<RuntimeTask*> queue_;
  std::vector<std::thread> workers_;
  bool stopping_ = false;
  static thread_local bool worker_;
};

thread_local bool TaskExecutor::worker_ = false;

TaskExecutor& taskExecutor() {
  static TaskExecutor executor;
  return executor;
}

RuntimeTask* spawnOnExecutor(void* opaqueEntry, void* context,
                             TaskExecutor& executor) {
  if (!opaqueEntry || !context)
    runtimeFailure("task spawn received an invalid entry or context");
  auto* task = new (std::nothrow) RuntimeTask;
  if (!task) runtimeFailure("out of memory while allocating Task");
  task->header = {1, 0, 1, destroyTask, ObjectTask, 0};
  task->entry = reinterpret_cast<TaskEntry>(opaqueEntry);
  task->context = context;
  task->result = nullptr;
  task->state = TaskState::Queued;
  task->executorReleased = false;
  task->cancellation = reinterpret_cast<RuntimeCancellation*>(rocket_std_cancel_token());
  liveAllocations.fetch_add(1, std::memory_order_relaxed);
  promoteObject(task);
  rocket_rt_retain(task); // executor ownership
  executor.enqueue(task);
  return task;
}

void* executorLifecycleEntry(void*) {
  return runtimeOkBool(true);
}

RocketString* runtimeString(std::string_view value) {
  return rocket_rt_string_new(reinterpret_cast<const std::uint8_t*>(value.data()),
                              static_cast<std::uint64_t>(value.size()));
}

RocketAggregate* runtimeManagedVariant(std::uint32_t tag, void* value) {
  RocketAggregate* result = rocket_rt_aggregate_new(tag, value ? 1 : 0, value ? 1 : 0);
  if (value) rocket_rt_aggregate_set_managed(result, 0, value);
  return result;
}

RocketAggregate* runtimeStoredVariant(std::uint32_t tag,
                                      std::uint32_t elementKind,
                                      RuntimeStoredValue value) {
  RocketAggregate* result = rocket_rt_aggregate_new(
      tag, 1, managedElementKind(elementKind) ? 1 : 0);
  switch (elementKind) {
  case ROCKET_ELEMENT_INT:
    rocket_rt_aggregate_set_int(result, 0, static_cast<std::int64_t>(value.bits));
    break;
  case ROCKET_ELEMENT_FLOAT:
    rocket_rt_aggregate_set_float(result, 0, storedFloat(value));
    break;
  case ROCKET_ELEMENT_BOOL:
    rocket_rt_aggregate_set_bool(result, 0, value.bits != 0 ? 1 : 0);
    break;
  case ROCKET_ELEMENT_CHAR:
    rocket_rt_aggregate_set_char(result, 0, static_cast<std::uint8_t>(value.bits));
    break;
  case ROCKET_ELEMENT_STRING:
  case ROCKET_ELEMENT_MANAGED:
    rocket_rt_aggregate_set_managed(result, 0, storedPointer(value));
    break;
  default:
    runtimeFailure("invalid synchronized value kind");
  }
  return result;
}

RuntimeStoredValue arrayStoredValue(RuntimeArray* array, std::uint64_t index) {
  switch (array->elementKind) {
  case ROCKET_ELEMENT_INT:
    return {static_cast<std::uint64_t>(
        static_cast<std::int64_t*>(array->elements)[index])};
  case ROCKET_ELEMENT_FLOAT:
    return storedFloat(static_cast<double*>(array->elements)[index]);
  case ROCKET_ELEMENT_BOOL:
  case ROCKET_ELEMENT_CHAR:
    return {static_cast<std::uint8_t*>(array->elements)[index]};
  case ROCKET_ELEMENT_STRING:
  case ROCKET_ELEMENT_MANAGED:
    return storedPointer(static_cast<void**>(array->elements)[index]);
  default:
    runtimeFailure("invalid synchronized Array element kind");
  }
}

void setArrayStored(RocketArray* array, std::int64_t index,
                    std::uint32_t elementKind, RuntimeStoredValue value) {
  switch (elementKind) {
  case ROCKET_ELEMENT_INT:
    rocket_rt_array_set_int(array, index, static_cast<std::int64_t>(value.bits));
    break;
  case ROCKET_ELEMENT_FLOAT:
    rocket_rt_array_set_float(array, index, storedFloat(value));
    break;
  case ROCKET_ELEMENT_BOOL:
    rocket_rt_array_set_bool(array, index, value.bits != 0 ? 1 : 0);
    break;
  case ROCKET_ELEMENT_CHAR:
    rocket_rt_array_set_char(array, index, static_cast<std::uint8_t>(value.bits));
    break;
  case ROCKET_ELEMENT_STRING:
    rocket_rt_array_set_string(array, index,
        reinterpret_cast<RocketString*>(storedPointer(value)));
    break;
  case ROCKET_ELEMENT_MANAGED:
    rocket_rt_array_set_managed(array, index, storedPointer(value));
    break;
  default:
    runtimeFailure("invalid synchronized Array element kind");
  }
}

RocketAggregate* runtimeBoolVariant(std::uint32_t tag, bool value) {
  RocketAggregate* result = rocket_rt_aggregate_new(tag, 1, 0);
  rocket_rt_aggregate_set_bool(result, 0, value ? 1 : 0);
  return result;
}

RocketAggregate* runtimeError(std::string_view message) {
  RocketString* text = runtimeString(message);
  RocketAggregate* result = runtimeManagedVariant(1, text);
  rocket_rt_release(text);
  return result;
}

RocketAggregate* runtimeOkBool(bool value) { return runtimeBoolVariant(0, value); }

bool cancellationObserved(RuntimeCancellation* token) {
  for (RuntimeCancellation* current = token; current; current = current->parent)
    if (current->cancelled.load(std::memory_order_acquire) != 0) return true;
  return false;
}

bool operationCancellationObserved(RuntimeCancellation* token) {
  return cancellationObserved(token) ||
      (currentTaskCancellation != token &&
       cancellationObserved(currentTaskCancellation));
}

std::int64_t monotonicMilliseconds() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

#ifdef _WIN32
std::wstring runtimeWide(std::string_view value) {
  if (value.empty()) return {};
  const int length = MultiByteToWideChar(
      CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
      nullptr, 0);
  if (length <= 0) return {};
  std::wstring result(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                      static_cast<int>(value.size()), result.data(), length);
  return result;
}

std::wstring runtimeQuoteWindowsArgument(const std::wstring& argument) {
  if (argument.empty()) return L"\"\"";
  if (argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) return argument;
  std::wstring result = L"\"";
  std::size_t backslashes = 0;
  for (const wchar_t character : argument) {
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

bool waitOverlappedFile(HANDLE file, OVERLAPPED& overlapped,
                        RuntimeCancellation* token, DWORD& transferred,
                        std::string& error) {
  while (true) {
    if (operationCancellationObserved(token)) {
      CancelIoEx(file, &overlapped);
      WaitForSingleObject(overlapped.hEvent, INFINITE);
      DWORD ignored = 0;
      GetOverlappedResult(file, &overlapped, &ignored, FALSE);
      error = "operation cancelled";
      return false;
    }
    const DWORD waited = WaitForSingleObject(overlapped.hEvent, 2);
    if (waited == WAIT_TIMEOUT) continue;
    if (waited != WAIT_OBJECT_0) {
      error = "asynchronous file event wait failed (Windows error " +
              std::to_string(GetLastError()) + ")";
      return false;
    }
    if (!GetOverlappedResult(file, &overlapped, &transferred, FALSE)) {
      const DWORD status = GetLastError();
      if (status == ERROR_OPERATION_ABORTED && operationCancellationObserved(token))
        error = "operation cancelled";
      else
        error = "asynchronous file operation failed (Windows error " +
                std::to_string(status) + ")";
      return false;
    }
    return true;
  }
}
#endif

template <typename Predicate>
bool waitUntil(std::condition_variable& changed, std::unique_lock<std::mutex>& lock,
               std::int64_t deadline, RuntimeCancellation* token,
               Predicate predicate) {
  while (!predicate()) {
    if (operationCancellationObserved(token)) return false;
    const std::int64_t now = monotonicMilliseconds();
    if (deadline >= 0 && now >= deadline) return false;
    const std::int64_t remaining = deadline < 0 ? 5 : (std::min)(std::int64_t{5}, deadline - now);
    changed.wait_for(lock, std::chrono::milliseconds((std::max)(std::int64_t{1}, remaining)));
  }
  return true;
}

RuntimeCancellation* checkedCancellation(void* opaque) {
  auto* token = reinterpret_cast<RuntimeCancellation*>(opaque);
  if (!token || token->header.objectKind != ObjectCancellation)
    runtimeFailure("operation received an invalid CancellationToken");
  return token;
}

RuntimeChannelEndpoint* checkedEndpoint(void* opaque, bool sender) {
  auto* endpoint = reinterpret_cast<RuntimeChannelEndpoint*>(opaque);
  if (!endpoint || endpoint->header.objectKind != ObjectChannelEndpoint ||
      endpoint->sender != sender)
    runtimeFailure(sender ? "operation received an invalid Sender"
                          : "operation received an invalid Receiver");
  return endpoint;
}

RuntimeChannelEndpoint* newEndpoint(RuntimeChannel* channel, bool sender) {
  auto* endpoint = new (std::nothrow) RuntimeChannelEndpoint;
  if (!endpoint) runtimeFailure("out of memory while allocating channel endpoint");
  endpoint->header = {1, 0, 1, destroyChannelEndpoint, ObjectChannelEndpoint, 0};
  endpoint->channel = channel;
  endpoint->sender = sender;
  endpoint->open.store(1, std::memory_order_relaxed);
  rocket_rt_retain(channel);
  {
    std::lock_guard lock(channel->mutex);
    if (sender) ++channel->senders;
    else ++channel->receivers;
  }
  liveAllocations.fetch_add(1, std::memory_order_relaxed);
  return endpoint;
}

void* timerTaskEntry(void* opaque) {
  auto* context = reinterpret_cast<RocketAggregate*>(opaque);
  const std::int64_t deadline = rocket_rt_aggregate_get_int(context, 0);
  auto* token = checkedCancellation(rocket_rt_aggregate_get_managed(context, 1));
  RocketAggregate* result = nullptr;
#ifdef _WIN32
  HANDLE timer = CreateWaitableTimerExW(
      nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
  if (!timer) timer = CreateWaitableTimerW(nullptr, TRUE, nullptr);
  if (!timer) {
    result = runtimeError("could not create Windows waitable timer (Windows error " +
                          std::to_string(GetLastError()) + ")");
  }
  while (timer && !result) {
    if (operationCancellationObserved(token)) {
      CancelWaitableTimer(timer);
      result = runtimeError("operation cancelled");
      break;
    }
    const std::int64_t now = monotonicMilliseconds();
    if (now >= deadline) {
      result = runtimeOkBool(true);
      break;
    }
    LARGE_INTEGER due{};
    const std::int64_t remaining = (std::min)(deadline - now,
                                              std::int64_t{86400000});
    due.QuadPart = -remaining * 10000;
    if (!SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
      result = runtimeError("could not arm Windows waitable timer (Windows error " +
                            std::to_string(GetLastError()) + ")");
      break;
    }
    while (!result) {
      if (operationCancellationObserved(token)) {
        CancelWaitableTimer(timer);
        result = runtimeError("operation cancelled");
        break;
      }
      const DWORD waited = WaitForSingleObject(timer, 2);
      if (waited == WAIT_OBJECT_0) break;
      if (waited != WAIT_TIMEOUT) {
        result = runtimeError("Windows waitable timer failed (Windows error " +
                              std::to_string(GetLastError()) + ")");
        break;
      }
    }
  }
  if (timer) CloseHandle(timer);
#else
  while (true) {
    if (operationCancellationObserved(token)) {
      result = runtimeError("operation cancelled");
      break;
    }
    const std::int64_t now = monotonicMilliseconds();
    if (now >= deadline) {
      result = runtimeOkBool(true);
      break;
    }
    std::this_thread::sleep_for(
        std::chrono::milliseconds((std::min)(std::int64_t{2}, deadline - now)));
  }
#endif
  rocket_rt_release(token);
  return result;
}

void* asyncFileReadEntry(void* opaque) {
  auto* context = reinterpret_cast<RocketAggregate*>(opaque);
  auto* path = reinterpret_cast<RocketString*>(rocket_rt_aggregate_get_managed(context, 0));
  const std::int64_t maximum = rocket_rt_aggregate_get_int(context, 1);
  auto* token = checkedCancellation(rocket_rt_aggregate_get_managed(context, 2));
  RocketAggregate* result = nullptr;
  if (operationCancellationObserved(token)) {
    result = runtimeError("operation cancelled");
  } else {
#ifdef _WIN32
    const std::wstring nativePath = runtimeWide(std::string_view(
        reinterpret_cast<const char*>(rocket_rt_string_bytes(path)),
        static_cast<std::size_t>(rocket_rt_string_byte_length(path))));
    HANDLE file = nativePath.empty() ? INVALID_HANDLE_VALUE : CreateFileW(
        nativePath.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
      result = runtimeError("could not open file for asynchronous reading (Windows error " +
                            std::to_string(GetLastError()) + ")");
    } else {
      LARGE_INTEGER fileSize{};
      if (!GetFileSizeEx(file, &fileSize) || fileSize.QuadPart < 0) {
        result = runtimeError("could not determine asynchronous file size (Windows error " +
                              std::to_string(GetLastError()) + ")");
      } else if (fileSize.QuadPart > maximum) {
        result = runtimeError("asynchronous file read exceeds its maximum byte count");
      } else {
        const std::uint64_t size = static_cast<std::uint64_t>(fileSize.QuadPart);
        RocketArray* array = rocket_rt_array_new(ROCKET_ELEMENT_CHAR, size);
        auto* runtimeArray = reinterpret_cast<RuntimeArray*>(array);
        HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!event) {
          result = runtimeError("could not create asynchronous file event (Windows error " +
                                std::to_string(GetLastError()) + ")");
        } else {
          std::uint64_t offset = 0;
          std::string operationError;
          while (offset < size && operationError.empty()) {
            if (operationCancellationObserved(token)) {
              operationError = "operation cancelled";
              break;
            }
            const DWORD requested = static_cast<DWORD>((std::min)(
                std::uint64_t{65536}, size - offset));
            OVERLAPPED overlapped{};
            overlapped.Offset = static_cast<DWORD>(offset & 0xffffffffU);
            overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32U);
            overlapped.hEvent = event;
            ResetEvent(event);
            DWORD transferred = 0;
            const BOOL immediate = ReadFile(
                file, static_cast<std::uint8_t*>(runtimeArray->elements) + offset,
                requested, &transferred, &overlapped);
            if (!immediate) {
              const DWORD status = GetLastError();
              if (status != ERROR_IO_PENDING) {
                operationError = "asynchronous file read failed (Windows error " +
                                 std::to_string(status) + ")";
                break;
              }
              if (!waitOverlappedFile(file, overlapped, token, transferred,
                                      operationError))
                break;
            }
            if (transferred == 0 || transferred > requested) {
              operationError = "asynchronous file read reached EOF before completion";
              break;
            }
            offset += transferred;
          }
          if (!operationError.empty()) result = runtimeError(operationError);
          else result = runtimeManagedVariant(0, array);
          CloseHandle(event);
        }
        rocket_rt_release(array);
      }
      CloseHandle(file);
    }
#else
    try {
      const auto* bytes = rocket_rt_string_bytes(path);
      const auto length = static_cast<std::size_t>(rocket_rt_string_byte_length(path));
      const std::filesystem::path nativePath = std::filesystem::u8path(
          reinterpret_cast<const char*>(bytes), reinterpret_cast<const char*>(bytes) + length);
      std::ifstream input(nativePath, std::ios::binary | std::ios::ate);
      if (!input) {
        result = runtimeError("could not open file for asynchronous reading");
      } else {
        const std::streamoff size = input.tellg();
        if (size < 0 || size > maximum) {
          result = runtimeError("asynchronous file read exceeds its maximum byte count");
        } else {
          input.seekg(0, std::ios::beg);
          RocketArray* array = rocket_rt_array_new(ROCKET_ELEMENT_CHAR,
                                                   static_cast<std::uint64_t>(size));
          auto* runtimeArray = reinterpret_cast<RuntimeArray*>(array);
          std::size_t offset = 0;
          while (offset < static_cast<std::size_t>(size)) {
            if (operationCancellationObserved(token)) break;
            const std::size_t count = (std::min)(std::size_t{65536},
                static_cast<std::size_t>(size) - offset);
            input.read(reinterpret_cast<char*>(runtimeArray->elements) + offset,
                       static_cast<std::streamsize>(count));
            if (input.gcount() != static_cast<std::streamsize>(count)) break;
            offset += count;
          }
          if (operationCancellationObserved(token)) {
            result = runtimeError("operation cancelled");
          } else if (offset != static_cast<std::size_t>(size)) {
            result = runtimeError("asynchronous file read failed before completion");
          } else {
            result = runtimeManagedVariant(0, array);
          }
          rocket_rt_release(array);
        }
      }
    } catch (const std::exception& error) {
      result = runtimeError(error.what());
    }
#endif
  }
  rocket_rt_release(token);
  rocket_rt_release(path);
  return result;
}

void* asyncFileWriteEntry(void* opaque) {
  auto* context = reinterpret_cast<RocketAggregate*>(opaque);
  auto* path = reinterpret_cast<RocketString*>(rocket_rt_aggregate_get_managed(context, 0));
  auto* buffer = reinterpret_cast<RocketArray*>(rocket_rt_aggregate_get_managed(context, 1));
  const bool append = rocket_rt_aggregate_get_bool(context, 2) != 0;
  auto* token = checkedCancellation(rocket_rt_aggregate_get_managed(context, 3));
  RocketAggregate* result = nullptr;
  if (operationCancellationObserved(token)) {
    result = runtimeError("operation cancelled");
  } else {
#ifdef _WIN32
    RuntimeArray* bytes = checkedArray(buffer);
    if (bytes->elementKind != ROCKET_ELEMENT_CHAR) {
      result = runtimeError("asynchronous file writes require UniqueBuffer[Char]");
    } else {
      const std::wstring nativePath = runtimeWide(std::string_view(
          reinterpret_cast<const char*>(rocket_rt_string_bytes(path)),
          static_cast<std::size_t>(rocket_rt_string_byte_length(path))));
      HANDLE file = nativePath.empty() ? INVALID_HANDLE_VALUE : CreateFileW(
          nativePath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
          append ? OPEN_ALWAYS : CREATE_ALWAYS,
          FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
      if (file == INVALID_HANDLE_VALUE) {
        result = runtimeError("could not open file for asynchronous writing (Windows error " +
                              std::to_string(GetLastError()) + ")");
      } else {
        LARGE_INTEGER initialOffset{};
        if (append && !GetFileSizeEx(file, &initialOffset)) {
          result = runtimeError("could not determine asynchronous append offset (Windows error " +
                                std::to_string(GetLastError()) + ")");
        } else {
          HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
          if (!event) {
            result = runtimeError("could not create asynchronous file event (Windows error " +
                                  std::to_string(GetLastError()) + ")");
          } else {
            std::uint64_t offset = 0;
            std::string operationError;
            while (offset < bytes->length && operationError.empty()) {
              if (operationCancellationObserved(token)) {
                operationError = "operation cancelled";
                break;
              }
              const DWORD requested = static_cast<DWORD>((std::min)(
                  std::uint64_t{65536}, bytes->length - offset));
              const std::uint64_t fileOffset =
                  static_cast<std::uint64_t>(initialOffset.QuadPart) + offset;
              OVERLAPPED overlapped{};
              overlapped.Offset = static_cast<DWORD>(fileOffset & 0xffffffffU);
              overlapped.OffsetHigh = static_cast<DWORD>(fileOffset >> 32U);
              overlapped.hEvent = event;
              ResetEvent(event);
              DWORD transferred = 0;
              const BOOL immediate = WriteFile(
                  file, static_cast<const std::uint8_t*>(bytes->elements) + offset,
                  requested, &transferred, &overlapped);
              if (!immediate) {
                const DWORD status = GetLastError();
                if (status != ERROR_IO_PENDING) {
                  operationError = "asynchronous file write failed (Windows error " +
                                   std::to_string(status) + ")";
                  break;
                }
                if (!waitOverlappedFile(file, overlapped, token, transferred,
                                        operationError))
                  break;
              }
              if (transferred == 0 || transferred > requested) {
                operationError = "asynchronous file write made no progress";
                break;
              }
              offset += transferred;
            }
            result = operationError.empty() ? runtimeOkBool(true)
                                            : runtimeError(operationError);
            CloseHandle(event);
          }
        }
        CloseHandle(file);
      }
    }
#else
    try {
      RuntimeArray* bytes = checkedArray(buffer);
      if (bytes->elementKind != ROCKET_ELEMENT_CHAR) {
        result = runtimeError("asynchronous file writes require UniqueBuffer[Char]");
      } else {
        const auto* pathBytes = rocket_rt_string_bytes(path);
        const auto pathLength = static_cast<std::size_t>(rocket_rt_string_byte_length(path));
        const std::filesystem::path nativePath = std::filesystem::u8path(
            reinterpret_cast<const char*>(pathBytes),
            reinterpret_cast<const char*>(pathBytes) + pathLength);
        std::ofstream output(nativePath, std::ios::binary |
            (append ? std::ios::app : std::ios::trunc));
        if (!output) {
          result = runtimeError("could not open file for asynchronous writing");
        } else {
          std::size_t offset = 0;
          while (offset < bytes->length) {
            if (operationCancellationObserved(token)) break;
            const std::size_t count = (std::min)(std::size_t{65536},
                static_cast<std::size_t>(bytes->length) - offset);
            output.write(reinterpret_cast<const char*>(bytes->elements) + offset,
                         static_cast<std::streamsize>(count));
            if (!output) break;
            offset += count;
          }
          if (operationCancellationObserved(token)) result = runtimeError("operation cancelled");
          else if (offset != bytes->length)
            result = runtimeError("asynchronous file write failed before completion");
          else result = runtimeOkBool(true);
        }
      }
    } catch (const std::exception& error) {
      result = runtimeError(error.what());
    }
#endif
  }
  rocket_rt_release(token);
  rocket_rt_release(buffer);
  rocket_rt_release(path);
  return result;
}

RocketAggregate* uniqueBufferResult(RocketAggregate* binaryResult) {
  if (rocket_rt_aggregate_tag(binaryResult) != 0) return binaryResult;
  auto* byteBuffer = reinterpret_cast<RocketAggregate*>(
      rocket_rt_aggregate_get_managed(binaryResult, 0));
  auto* bytes = reinterpret_cast<RocketArray*>(
      rocket_rt_aggregate_get_managed(byteBuffer, 0));
  RocketAggregate* result = runtimeManagedVariant(0, bytes);
  rocket_rt_release(bytes);
  rocket_rt_release(byteBuffer);
  rocket_rt_release(binaryResult);
  return result;
}

void* asyncNetConnectEntry(void* opaque) {
  auto* context = reinterpret_cast<RocketAggregate*>(opaque);
  auto* host = reinterpret_cast<RocketString*>(rocket_rt_aggregate_get_managed(context, 0));
  const std::int64_t port = rocket_rt_aggregate_get_int(context, 1);
  const std::int64_t deadline = rocket_rt_aggregate_get_int(context, 2);
  auto* token = checkedCancellation(rocket_rt_aggregate_get_managed(context, 3));
  RocketAggregate* result = operationCancellationObserved(token)
      ? runtimeError("operation cancelled")
      : rocket_std_net_tcp_connect(host, port,
            (std::max)(std::int64_t{0}, deadline - monotonicMilliseconds()));
  if (operationCancellationObserved(token) && rocket_rt_aggregate_tag(result) == 0) {
    const std::int64_t handle = rocket_rt_aggregate_get_int(result, 0);
    RocketAggregate* closed = rocket_std_net_close(handle);
    rocket_rt_release(closed);
    rocket_rt_release(result);
    result = runtimeError("operation cancelled");
  }
  rocket_rt_release(token);
  rocket_rt_release(host);
  return result;
}

void* asyncNetAcceptEntry(void* opaque) {
  auto* context = reinterpret_cast<RocketAggregate*>(opaque);
  const std::int64_t listener = rocket_rt_aggregate_get_int(context, 0);
  const std::int64_t deadline = rocket_rt_aggregate_get_int(context, 1);
  auto* token = checkedCancellation(rocket_rt_aggregate_get_managed(context, 2));
  RocketAggregate* result = operationCancellationObserved(token)
      ? runtimeError("operation cancelled")
      : rocket_std_net_accept(listener,
            (std::max)(std::int64_t{0}, deadline - monotonicMilliseconds()));
  if (operationCancellationObserved(token) && rocket_rt_aggregate_tag(result) == 0) {
    const std::int64_t handle = rocket_rt_aggregate_get_int(result, 0);
    RocketAggregate* closed = rocket_std_net_close(handle);
    rocket_rt_release(closed);
    rocket_rt_release(result);
    result = runtimeError("operation cancelled");
  }
  rocket_rt_release(token);
  return result;
}

void* asyncNetReceiveEntry(void* opaque) {
  auto* context = reinterpret_cast<RocketAggregate*>(opaque);
  const std::int64_t socket = rocket_rt_aggregate_get_int(context, 0);
  const std::int64_t maximum = rocket_rt_aggregate_get_int(context, 1);
  const std::int64_t deadline = rocket_rt_aggregate_get_int(context, 2);
  auto* token = checkedCancellation(rocket_rt_aggregate_get_managed(context, 3));
  RocketAggregate* result = operationCancellationObserved(token)
      ? runtimeError("operation cancelled")
      : uniqueBufferResult(rocket_std_net_receive(
            socket, maximum,
            (std::max)(std::int64_t{0}, deadline - monotonicMilliseconds())));
  if (operationCancellationObserved(token)) {
    rocket_rt_release(result);
    result = runtimeError("operation cancelled");
  }
  rocket_rt_release(token);
  return result;
}

void* asyncNetSendEntry(void* opaque) {
  auto* context = reinterpret_cast<RocketAggregate*>(opaque);
  const std::int64_t socket = rocket_rt_aggregate_get_int(context, 0);
  auto* bytes = reinterpret_cast<RocketArray*>(rocket_rt_aggregate_get_managed(context, 1));
  const std::int64_t deadline = rocket_rt_aggregate_get_int(context, 2);
  auto* token = checkedCancellation(rocket_rt_aggregate_get_managed(context, 3));
  RocketAggregate* result = nullptr;
  if (operationCancellationObserved(token)) {
    result = runtimeError("operation cancelled");
  } else {
    RocketAggregate* byteBuffer = rocket_rt_aggregate_new(0, 1, 1);
    rocket_rt_aggregate_set_managed(byteBuffer, 0, bytes);
    result = rocket_std_net_send(
        socket, byteBuffer,
        (std::max)(std::int64_t{0}, deadline - monotonicMilliseconds()));
    rocket_rt_release(byteBuffer);
    if (operationCancellationObserved(token)) {
      rocket_rt_release(result);
      result = runtimeError("operation cancelled");
    }
  }
  rocket_rt_release(token);
  rocket_rt_release(bytes);
  return result;
}

void* asyncProcessRunEntry(void* opaque) {
  auto* context = reinterpret_cast<RocketAggregate*>(opaque);
  auto* program = reinterpret_cast<RocketString*>(rocket_rt_aggregate_get_managed(context, 0));
  auto* arguments = reinterpret_cast<RocketArray*>(rocket_rt_aggregate_get_managed(context, 1));
  const std::int64_t deadline = rocket_rt_aggregate_get_int(context, 2);
  auto* token = checkedCancellation(rocket_rt_aggregate_get_managed(context, 3));
  RocketAggregate* result = nullptr;
  if (operationCancellationObserved(token)) {
    result = runtimeError("operation cancelled");
  } else if (deadline < 0 || monotonicMilliseconds() >= deadline) {
    result = runtimeError("process deadline exceeded");
  } else {
#ifdef _WIN32
    const auto* programBytes = rocket_rt_string_bytes(program);
    const auto programLength = static_cast<std::size_t>(
        rocket_rt_string_byte_length(program));
    const std::wstring executable = runtimeWide(std::string_view(
        reinterpret_cast<const char*>(programBytes), programLength));
    if (executable.empty()) {
      result = runtimeError("process program is empty or invalid UTF-8");
    } else {
      std::wstring command = runtimeQuoteWindowsArgument(executable);
      const std::uint64_t count = rocket_rt_collection_length(arguments);
      bool validArguments = true;
      for (std::uint64_t index = 0; index < count; ++index) {
        RocketString* argument = rocket_rt_index_string(
            arguments, static_cast<std::int64_t>(index));
        const std::wstring wide = runtimeWide(std::string_view(
            reinterpret_cast<const char*>(rocket_rt_string_bytes(argument)),
            static_cast<std::size_t>(rocket_rt_string_byte_length(argument))));
        if (wide.empty() && rocket_rt_string_byte_length(argument) != 0)
          validArguments = false;
        command += L" " + runtimeQuoteWindowsArgument(wide);
        rocket_rt_release(argument);
      }
      if (!validArguments) {
        result = runtimeError("process argument is invalid UTF-8");
      } else {
        std::vector<wchar_t> mutableCommand(command.begin(), command.end());
        mutableCommand.push_back(L'\0');
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr,
                            FALSE, 0, nullptr, nullptr, &startup, &process)) {
          result = runtimeError("could not start asynchronous process (Windows error " +
                                std::to_string(GetLastError()) + ")");
        } else {
          bool cancelled = false;
          bool timedOut = false;
          DWORD waitResult = WAIT_TIMEOUT;
          while (waitResult == WAIT_TIMEOUT) {
            if (operationCancellationObserved(token)) {
              cancelled = true;
              break;
            }
            const std::int64_t remaining = deadline - monotonicMilliseconds();
            if (remaining <= 0) {
              timedOut = true;
              break;
            }
            waitResult = WaitForSingleObject(
                process.hProcess,
                static_cast<DWORD>((std::min)(std::int64_t{10}, remaining)));
          }
          if (cancelled || timedOut) {
            TerminateProcess(process.hProcess, 1);
            WaitForSingleObject(process.hProcess, 5000);
            result = runtimeError(cancelled ? "operation cancelled"
                                            : "process deadline exceeded");
          } else if (waitResult == WAIT_OBJECT_0) {
            DWORD exitCode = 1;
            if (GetExitCodeProcess(process.hProcess, &exitCode)) {
              RocketAggregate* completed = rocket_rt_aggregate_new(0, 1, 0);
              rocket_rt_aggregate_set_int(completed, 0,
                                          static_cast<std::int64_t>(exitCode));
              result = completed;
            } else {
              result = runtimeError("could not read asynchronous process exit code");
            }
          } else {
            result = runtimeError("asynchronous process wait failed");
          }
          CloseHandle(process.hThread);
          CloseHandle(process.hProcess);
        }
      }
    }
#else
    std::vector<std::string> ownedArguments;
    ownedArguments.emplace_back(
        reinterpret_cast<const char*>(rocket_rt_string_bytes(program)),
        static_cast<std::size_t>(rocket_rt_string_byte_length(program)));
    if (ownedArguments.front().empty()) {
      result = runtimeError("process program is empty or invalid UTF-8");
    } else {
      const std::uint64_t count = rocket_rt_collection_length(arguments);
      ownedArguments.reserve(static_cast<std::size_t>(count) + 1);
      for (std::uint64_t index = 0; index < count; ++index) {
        RocketString* argument = rocket_rt_index_string(
            arguments, static_cast<std::int64_t>(index));
        ownedArguments.emplace_back(
            reinterpret_cast<const char*>(rocket_rt_string_bytes(argument)),
            static_cast<std::size_t>(rocket_rt_string_byte_length(argument)));
        rocket_rt_release(argument);
      }
      std::vector<char*> nativeArguments;
      nativeArguments.reserve(ownedArguments.size() + 1);
      for (auto& argument : ownedArguments)
        nativeArguments.push_back(argument.data());
      nativeArguments.push_back(nullptr);
      const pid_t child = ::fork();
      if (child < 0) {
        result = runtimeError("could not fork asynchronous process: " +
                              std::string(std::strerror(errno)));
      } else if (child == 0) {
        ::setpgid(0, 0);
        ::execvp(nativeArguments[0], nativeArguments.data());
        _exit(errno == ENOENT ? 127 : 126);
      } else {
        ::setpgid(child, child);
        int status = 0;
        bool cancelled = false;
        bool timedOut = false;
        bool waitFailed = false;
        while (true) {
          const pid_t waited = ::waitpid(child, &status, WNOHANG);
          if (waited == child) break;
          if (waited < 0 && errno != EINTR) {
            waitFailed = true;
            break;
          }
          if (operationCancellationObserved(token)) {
            cancelled = true;
            break;
          }
          if (monotonicMilliseconds() >= deadline) {
            timedOut = true;
            break;
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        if (cancelled || timedOut || waitFailed) {
          if (!waitFailed) ::kill(-child, SIGTERM);
          const std::int64_t stopDeadline = monotonicMilliseconds() + 500;
          while (::waitpid(child, &status, WNOHANG) == 0 &&
                 monotonicMilliseconds() < stopDeadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
          if (::waitpid(child, &status, WNOHANG) == 0) {
            ::kill(-child, SIGKILL);
            while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {}
          }
          result = runtimeError(
              cancelled ? "operation cancelled"
                        : timedOut ? "process deadline exceeded"
                                   : "asynchronous process wait failed");
        } else if (WIFEXITED(status)) {
          RocketAggregate* completed = rocket_rt_aggregate_new(0, 1, 0);
          rocket_rt_aggregate_set_int(completed, 0, WEXITSTATUS(status));
          result = completed;
        } else if (WIFSIGNALED(status)) {
          RocketAggregate* completed = rocket_rt_aggregate_new(0, 1, 0);
          rocket_rt_aggregate_set_int(completed, 0, 128 + WTERMSIG(status));
          result = completed;
        } else {
          result = runtimeError("asynchronous process ended without an exit status");
        }
      }
    }
#endif
  }
  rocket_rt_release(token);
  rocket_rt_release(arguments);
  rocket_rt_release(program);
  return result;
}

} // namespace

extern "C" {

std::uint32_t rocket_rt_abi_version() { return RuntimeAbiVersion; }

RocketStringBuilder* rocket_std_string_builder() {
  auto* builder = static_cast<RuntimeStringBuilder*>(std::malloc(sizeof(RuntimeStringBuilder)));
  if (!builder) runtimeFailure("out of memory while allocating String Builder");
  builder->header = {1, 0, 1, destroyStringBuilder, ObjectStringBuilder, 0};
  builder->length = 0;
  builder->capacity = 0;
  builder->bytes = nullptr;
  liveAllocations.fetch_add(1, std::memory_order_relaxed);
  return reinterpret_cast<RocketStringBuilder*>(builder);
}

void rocket_std_string_builder_append(RocketStringBuilder* opaque, RocketString* value) {
  auto* builder = reinterpret_cast<RuntimeStringBuilder*>(opaque);
  if (!builder || builder->header.objectKind != ObjectStringBuilder)
    runtimeFailure("string.builder_append received an invalid Builder");
  const std::uint64_t appended = rocket_rt_string_byte_length(value);
  if (appended > (std::numeric_limits<std::uint64_t>::max)() - builder->length)
    runtimeFailure("String Builder is too large");
  const std::uint64_t required = builder->length + appended;
  if (required > builder->capacity) {
    std::uint64_t capacity = builder->capacity == 0 ? 256 : builder->capacity;
    while (capacity < required) {
      if (capacity > (std::numeric_limits<std::uint64_t>::max)() / 2) {
        capacity = required;
        break;
      }
      capacity *= 2;
    }
    if (capacity > (std::numeric_limits<std::size_t>::max)())
      runtimeFailure("String Builder allocation is too large");
    void* resized = std::realloc(builder->bytes, static_cast<std::size_t>(capacity));
    if (!resized) runtimeFailure("out of memory while growing String Builder");
    builder->bytes = static_cast<std::uint8_t*>(resized);
    builder->capacity = capacity;
  }
  if (appended != 0)
    std::memcpy(builder->bytes + builder->length, rocket_rt_string_bytes(value),
                static_cast<std::size_t>(appended));
  builder->length = required;
}

RocketString* rocket_std_string_builder_finish(RocketStringBuilder* opaque) {
  auto* builder = reinterpret_cast<RuntimeStringBuilder*>(opaque);
  if (!builder || builder->header.objectKind != ObjectStringBuilder)
    runtimeFailure("string.builder_finish received an invalid Builder");
  return rocket_rt_string_new(builder->bytes, builder->length);
}

void rocket_rt_retain(void* object) {
  if (!object) return;
  AllocationHeader* header = headerFor(object);
  const std::uint32_t flags = std::atomic_ref<std::uint32_t>(header->flags)
                                  .load(std::memory_order_acquire);
  if ((flags & AllocationShared) != 0) {
    auto references = std::atomic_ref<std::uint64_t>(header->sharedReferences);
    const std::uint64_t previous = references.fetch_add(1, std::memory_order_relaxed);
    if (previous == 0 || previous == (std::numeric_limits<std::uint64_t>::max)())
      runtimeFailure(previous == 0 ? "retain of destroyed object" :
                                     "reference count overflow");
    return;
  }
  if ((flags & AllocationDestroyed) != 0 || header->references == 0)
    runtimeFailure("retain of destroyed object");
  if (header->references == (std::numeric_limits<std::uint64_t>::max)())
    runtimeFailure("reference count overflow");
  ++header->references;
}

void rocket_rt_release(void* object) {
  if (!object) return;
  AllocationHeader* header = headerFor(object);
  const std::uint32_t flags = std::atomic_ref<std::uint32_t>(header->flags)
                                  .load(std::memory_order_acquire);
  if ((flags & AllocationShared) != 0) {
    auto references = std::atomic_ref<std::uint64_t>(header->sharedReferences);
    const std::uint64_t previous = references.fetch_sub(1, std::memory_order_release);
    if (previous == 0) runtimeFailure("reference count underflow");
    if (previous == 1) {
      std::atomic_thread_fence(std::memory_order_acquire);
      finishStrongLifetime(header);
    }
    return;
  }
  if (header->references == 0) runtimeFailure("reference count underflow");
  --header->references;
  if (header->references == 0) finishStrongLifetime(header);
}

void rocket_rt_promote(void* object) { promoteObject(object); }

RocketWeak* rocket_rt_weak_new(void* object) {
  if (!object) runtimeFailure("cannot create Weak from a null object");
  promoteObject(object);
  AllocationHeader* target = headerFor(object);
  auto weakReferences = std::atomic_ref<std::uint64_t>(target->weakReferences);
  const std::uint64_t previous =
      weakReferences.fetch_add(1, std::memory_order_relaxed);
  if (previous == (std::numeric_limits<std::uint64_t>::max)())
    runtimeFailure("weak reference count overflow");
  auto* weak = static_cast<RuntimeWeak*>(std::malloc(sizeof(RuntimeWeak)));
  if (!weak) {
    releaseWeakReference(target);
    runtimeFailure("out of memory while allocating Weak");
  }
  weak->header = {1, 0, 1, destroyWeak, ObjectWeak, 0};
  weak->target = target;
  liveAllocations.fetch_add(1, std::memory_order_relaxed);
  return reinterpret_cast<RocketWeak*>(weak);
}

void* rocket_rt_weak_upgrade(RocketWeak* opaque) {
  auto* weak = reinterpret_cast<RuntimeWeak*>(opaque);
  if (!weak || weak->header.objectKind != ObjectWeak)
    runtimeFailure("Weak upgrade received an invalid value");
  AllocationHeader* target = weak->target;
  if (!target) return nullptr;
  auto references = std::atomic_ref<std::uint64_t>(target->sharedReferences);
  std::uint64_t observed = references.load(std::memory_order_acquire);
  while (observed != 0) {
    if (observed == (std::numeric_limits<std::uint64_t>::max)())
      runtimeFailure("reference count overflow during Weak upgrade");
    if (references.compare_exchange_weak(observed, observed + 1,
                                         std::memory_order_acquire,
                                         std::memory_order_relaxed))
      return target;
  }
  return nullptr;
}

std::uint8_t rocket_rt_weak_expired(RocketWeak* opaque) {
  auto* weak = reinterpret_cast<RuntimeWeak*>(opaque);
  if (!weak || weak->header.objectKind != ObjectWeak)
    runtimeFailure("Weak expiration check received an invalid value");
  if (!weak->target) return 1;
  return std::atomic_ref<std::uint64_t>(weak->target->sharedReferences)
                     .load(std::memory_order_acquire) == 0
             ? 1
             : 0;
}

RocketTask* rocket_rt_task_spawn(void* opaqueEntry, void* context) {
  return reinterpret_cast<RocketTask*>(
      spawnOnExecutor(opaqueEntry, context, taskExecutor()));
}

std::uint8_t rocket_rt_debug_executor_cycles(std::int64_t cycles) {
  if (cycles < 0 || cycles > 10000) return 0;
  for (std::int64_t index = 0; index < cycles; ++index) {
    RuntimeTask* task = nullptr;
    {
      TaskExecutor executor(1);
      RocketAggregate* context = rocket_rt_aggregate_new(0, 0, 0);
      task = spawnOnExecutor(reinterpret_cast<void*>(&executorLifecycleEntry),
                             context, executor);
    }
    auto* result = reinterpret_cast<RocketAggregate*>(
        rocket_rt_task_await(reinterpret_cast<RocketTask*>(task)));
    const bool completed = rocket_rt_aggregate_tag(result) == 0 &&
                           rocket_rt_aggregate_get_bool(result, 0) != 0;
    rocket_rt_release(result);
    rocket_rt_release(task);
    if (!completed) return 0;
  }
  return 1;
}

RocketTask* rocket_rt_task_ready(void* result) {
  if (!result) runtimeFailure("ready task received an invalid result");
  auto* task = new (std::nothrow) RuntimeTask;
  if (!task) runtimeFailure("out of memory while allocating Task");
  task->header = {1, 0, 1, destroyTask, ObjectTask, 0};
  task->entry = nullptr;
  task->context = nullptr;
  task->result = result;
  task->state = TaskState::Completed;
  task->executorReleased = true;
  task->cancellation = reinterpret_cast<RuntimeCancellation*>(rocket_std_cancel_token());
  liveAllocations.fetch_add(1, std::memory_order_relaxed);
  promoteObject(task);
  return reinterpret_cast<RocketTask*>(task);
}

void* rocket_rt_task_await(RocketTask* opaque) {
  auto* task = reinterpret_cast<RuntimeTask*>(opaque);
  if (!task || task->header.objectKind != ObjectTask)
    runtimeFailure("await received an invalid Task");
  std::unique_lock lock(task->mutex);
  while (task->state != TaskState::Completed || !task->executorReleased) {
    if (currentTaskCancellation && cancellationObserved(currentTaskCancellation)) {
      lock.unlock();
      rocket_std_task_cancel(opaque);
      return runtimeError("operation cancelled");
    }
    if (TaskExecutor::isWorker()) {
      lock.unlock();
      if (!taskExecutor().helpOne()) std::this_thread::yield();
      lock.lock();
    } else {
      task->completed.wait(lock);
    }
  }
  void* result = task->result;
  rocket_rt_retain(result);
  return result;
}

RocketAggregate* rocket_std_task_join(RocketTask* task) {
  return reinterpret_cast<RocketAggregate*>(rocket_rt_task_await(task));
}

std::uint8_t rocket_std_task_is_complete(RocketTask* opaque) {
  auto* task = reinterpret_cast<RuntimeTask*>(opaque);
  if (!task || task->header.objectKind != ObjectTask)
    runtimeFailure("task.is_complete received an invalid Task");
  std::lock_guard lock(task->mutex);
  return task->state == TaskState::Completed ? 1 : 0;
}

std::uint8_t rocket_std_task_cancel(RocketTask* opaque) {
  auto* task = reinterpret_cast<RuntimeTask*>(opaque);
  if (!task || task->header.objectKind != ObjectTask)
    runtimeFailure("task.cancel received an invalid Task");
  {
    std::lock_guard lock(task->mutex);
    if (task->state == TaskState::Completed) return 0;
  }
  return rocket_std_cancel_cancel(
      reinterpret_cast<RocketCancellation*>(task->cancellation));
}

RocketTaskGroup* taskGroupWithKind(RocketArray* opaqueTasks, std::uint32_t elementKind) {
  RuntimeArray* tasks = checkedArray(opaqueTasks);
  if (tasks->elementKind != ROCKET_ELEMENT_MANAGED)
    runtimeFailure("task.group requires Array[Task[T]]");
  auto* group = new (std::nothrow) RuntimeTaskGroup;
  if (!group) runtimeFailure("out of memory while allocating TaskGroup");
  group->header = {1, 0, 1, destroyTaskGroup, ObjectTaskGroup, 0};
  group->joined = false;
  group->elementKind = elementKind;
  auto** values = static_cast<void**>(tasks->elements);
  group->tasks.reserve(static_cast<std::size_t>(tasks->length));
  for (std::uint64_t index = 0; index < tasks->length; ++index) {
    auto* task = reinterpret_cast<RuntimeTask*>(values[index]);
    if (!task || task->header.objectKind != ObjectTask) {
      for (RuntimeTask* retained : group->tasks) rocket_rt_release(retained);
      delete group;
      runtimeFailure("task.group received a non-Task element");
    }
    rocket_rt_retain(task);
    group->tasks.push_back(task);
  }
  liveAllocations.fetch_add(1, std::memory_order_relaxed);
  promoteObject(group);
  return reinterpret_cast<RocketTaskGroup*>(group);
}

RocketTaskGroup* rocket_std_task_group(RocketArray* tasks) {
  return taskGroupWithKind(tasks, ROCKET_ELEMENT_MANAGED);
}

RocketTaskGroup* rocket_std_task_group_string(RocketArray* tasks) {
  return taskGroupWithKind(tasks, ROCKET_ELEMENT_STRING);
}

RocketTaskGroup* rocket_std_task_group_managed(RocketArray* tasks) {
  return taskGroupWithKind(tasks, ROCKET_ELEMENT_MANAGED);
}
RocketTaskGroup* rocket_std_task_group_int(RocketArray* tasks) {
  return taskGroupWithKind(tasks, ROCKET_ELEMENT_INT);
}
RocketTaskGroup* rocket_std_task_group_float(RocketArray* tasks) {
  return taskGroupWithKind(tasks, ROCKET_ELEMENT_FLOAT);
}
RocketTaskGroup* rocket_std_task_group_bool(RocketArray* tasks) {
  return taskGroupWithKind(tasks, ROCKET_ELEMENT_BOOL);
}
RocketTaskGroup* rocket_std_task_group_char(RocketArray* tasks) {
  return taskGroupWithKind(tasks, ROCKET_ELEMENT_CHAR);
}

RocketAggregate* rocket_std_task_group_join(RocketTaskGroup* opaque) {
  auto* group = reinterpret_cast<RuntimeTaskGroup*>(opaque);
  if (!group || group->header.objectKind != ObjectTaskGroup)
    runtimeFailure("task.group_join received an invalid TaskGroup");
  std::lock_guard lock(group->mutex);
  if (group->joined) return runtimeError("TaskGroup was already joined");
  group->joined = true;
  std::vector<RuntimeStoredValue> successes;
  RocketAggregate* firstError = nullptr;
  for (RuntimeTask* task : group->tasks) {
    if (firstError)
      rocket_std_task_cancel(reinterpret_cast<RocketTask*>(task));
    auto* result = reinterpret_cast<RocketAggregate*>(
        rocket_rt_task_await(reinterpret_cast<RocketTask*>(task)));
    if (rocket_rt_aggregate_tag(result) == 0 && !firstError) {
      RuntimeStoredValue value;
      switch (group->elementKind) {
      case ROCKET_ELEMENT_INT:
        value.bits = static_cast<std::uint64_t>(rocket_rt_aggregate_get_int(result, 0));
        break;
      case ROCKET_ELEMENT_FLOAT:
        value = storedFloat(rocket_rt_aggregate_get_float(result, 0));
        break;
      case ROCKET_ELEMENT_BOOL:
        value.bits = rocket_rt_aggregate_get_bool(result, 0);
        break;
      case ROCKET_ELEMENT_CHAR:
        value.bits = rocket_rt_aggregate_get_char(result, 0);
        break;
      case ROCKET_ELEMENT_STRING:
      case ROCKET_ELEMENT_MANAGED:
        value = storedPointer(rocket_rt_aggregate_get_managed(result, 0));
        break;
      default:
        runtimeFailure("TaskGroup result kind is invalid");
      }
      successes.push_back(value);
    } else if (!firstError) {
      firstError = result;
      rocket_rt_retain(firstError);
    }
    rocket_rt_release(result);
  }
  if (firstError) {
    for (RuntimeStoredValue value : successes)
      releaseStored(group->elementKind, value);
    return firstError;
  }
  RocketArray* values = rocket_rt_array_new(group->elementKind, successes.size());
  for (std::size_t index = 0; index < successes.size(); ++index) {
    setArrayStored(values, static_cast<std::int64_t>(index), group->elementKind,
                   successes[index]);
    releaseStored(group->elementKind, successes[index]);
  }
  RocketAggregate* result = runtimeManagedVariant(0, values);
  rocket_rt_release(values);
  return result;
}

std::uint8_t rocket_std_task_group_cancel(RocketTaskGroup* opaque) {
  auto* group = reinterpret_cast<RuntimeTaskGroup*>(opaque);
  if (!group || group->header.objectKind != ObjectTaskGroup)
    runtimeFailure("task.group_cancel received an invalid TaskGroup");
  std::lock_guard lock(group->mutex);
  bool changed = false;
  for (RuntimeTask* task : group->tasks)
    changed = rocket_std_task_cancel(reinterpret_cast<RocketTask*>(task)) != 0 || changed;
  return changed ? 1 : 0;
}

RocketAggregate* rocket_std_thread_spawn(RocketTask* opaqueTask) {
  auto* task = reinterpret_cast<RuntimeTask*>(opaqueTask);
  if (!task || task->header.objectKind != ObjectTask)
    runtimeFailure("thread.spawn requires a Task[T]");
  auto* thread = new (std::nothrow) RuntimeThread;
  if (!thread) return runtimeError("out of memory while allocating Thread");
  thread->header = {1, 0, 1, destroyThread, ObjectThread, 0};
  thread->task = task;
  rocket_rt_retain(task);
  liveAllocations.fetch_add(1, std::memory_order_relaxed);
  promoteObject(thread);
  rocket_rt_retain(thread); // worker ownership
  try {
    thread->worker = std::thread([thread] {
      void* result = rocket_rt_task_await(reinterpret_cast<RocketTask*>(thread->task));
      {
        std::lock_guard lock(thread->mutex);
        thread->result = result;
        thread->finished = true;
      }
      thread->completed.notify_all();
      rocket_rt_release(thread);
    });
  } catch (const std::system_error& error) {
    rocket_rt_release(thread); // worker ownership
    rocket_rt_release(thread); // caller ownership
    return runtimeError(error.what());
  }
  RocketAggregate* result = runtimeManagedVariant(0, thread);
  rocket_rt_release(thread);
  return result;
}

RocketAggregate* rocket_std_thread_join(RocketThread* opaque) {
  auto* thread = reinterpret_cast<RuntimeThread*>(opaque);
  if (!thread || thread->header.objectKind != ObjectThread)
    runtimeFailure("thread.join received an invalid Thread");
  std::unique_lock lock(thread->mutex);
  if (thread->consumed) return runtimeError("Thread was already joined or detached");
  thread->consumed = true;
  thread->completed.wait(lock, [&] { return thread->finished; });
  void* result = thread->result;
  rocket_rt_retain(result);
  lock.unlock();
  if (thread->worker.joinable()) thread->worker.join();
  return reinterpret_cast<RocketAggregate*>(result);
}

RocketAggregate* rocket_std_thread_detach(RocketThread* opaque) {
  auto* thread = reinterpret_cast<RuntimeThread*>(opaque);
  if (!thread || thread->header.objectKind != ObjectThread)
    runtimeFailure("thread.detach received an invalid Thread");
  std::lock_guard lock(thread->mutex);
  if (thread->consumed) return runtimeError("Thread was already joined or detached");
  thread->consumed = true;
  thread->detached = true;
  if (thread->worker.joinable()) thread->worker.detach();
  return runtimeOkBool(true);
}

std::uint8_t rocket_std_thread_is_complete(RocketThread* opaque) {
  auto* thread = reinterpret_cast<RuntimeThread*>(opaque);
  if (!thread || thread->header.objectKind != ObjectThread)
    runtimeFailure("thread.is_complete received an invalid Thread");
  std::lock_guard lock(thread->mutex);
  return thread->finished ? 1 : 0;
}

RocketWeak* rocket_std_ownership_downgrade(void* object) {
  return rocket_rt_weak_new(object);
}

RocketAggregate* rocket_std_ownership_upgrade(RocketWeak* weak) {
  void* value = rocket_rt_weak_upgrade(weak);
  if (!value) return rocket_rt_aggregate_new(1, 0, 0);
  RocketAggregate* some = rocket_rt_aggregate_new(0, 1, 1);
  rocket_rt_aggregate_set_managed(some, 0, value);
  rocket_rt_release(value);
  return some;
}

std::uint8_t rocket_std_ownership_expired(RocketWeak* weak) {
  return rocket_rt_weak_expired(weak);
}

RocketArray* rocket_std_buffer_thaw(RocketArray* values) {
  RuntimeArray* source = checkedArray(values);
  return reinterpret_cast<RocketArray*>(cloneArray(source, source->capacity));
}

std::int64_t rocket_std_buffer_length(RocketArray* buffer) {
  return static_cast<std::int64_t>(checkedArray(buffer)->length);
}

std::int64_t rocket_std_buffer_capacity(RocketArray* buffer) {
  return static_cast<std::int64_t>(checkedArray(buffer)->capacity);
}

std::int64_t rocket_std_buffer_get_int(RocketArray* buffer, std::int64_t index) {
  return rocket_rt_index_int(buffer, index);
}
double rocket_std_buffer_get_float(RocketArray* buffer, std::int64_t index) {
  return rocket_rt_index_float(buffer, index);
}
std::uint8_t rocket_std_buffer_get_bool(RocketArray* buffer, std::int64_t index) {
  return rocket_rt_index_bool(buffer, index);
}
std::uint8_t rocket_std_buffer_get_char(RocketArray* buffer, std::int64_t index) {
  return rocket_rt_index_char(buffer, index);
}
RocketString* rocket_std_buffer_get_string(RocketArray* buffer, std::int64_t index) {
  return rocket_rt_index_string(buffer, index);
}
void* rocket_std_buffer_get_managed(RocketArray* buffer, std::int64_t index) {
  return rocket_rt_index_managed(buffer, index);
}

#define ROCKET_BUFFER_SET(name, type, setter)                                      \
  RocketArray* name(RocketArray* buffer, std::int64_t index, type value) {         \
    setter(buffer, index, value);                                                   \
    rocket_rt_retain(buffer);                                                       \
    return buffer;                                                                  \
  }
ROCKET_BUFFER_SET(rocket_std_buffer_set_int, std::int64_t, rocket_rt_array_set_int)
ROCKET_BUFFER_SET(rocket_std_buffer_set_float, double, rocket_rt_array_set_float)
ROCKET_BUFFER_SET(rocket_std_buffer_set_bool, std::uint8_t, rocket_rt_array_set_bool)
ROCKET_BUFFER_SET(rocket_std_buffer_set_char, std::uint8_t, rocket_rt_array_set_char)
ROCKET_BUFFER_SET(rocket_std_buffer_set_string, RocketString*, rocket_rt_array_set_string)
ROCKET_BUFFER_SET(rocket_std_buffer_set_managed, void*, rocket_rt_array_set_managed)
#undef ROCKET_BUFFER_SET

RocketArray* rocket_std_buffer_append_int(RocketArray* buffer, std::int64_t value) {
  return appendUniqueBuffer(buffer, value, ROCKET_ELEMENT_INT, rocket_rt_array_set_int);
}
RocketArray* rocket_std_buffer_append_float(RocketArray* buffer, double value) {
  return appendUniqueBuffer(buffer, value, ROCKET_ELEMENT_FLOAT, rocket_rt_array_set_float);
}
RocketArray* rocket_std_buffer_append_bool(RocketArray* buffer, std::uint8_t value) {
  return appendUniqueBuffer(buffer, value, ROCKET_ELEMENT_BOOL, rocket_rt_array_set_bool);
}
RocketArray* rocket_std_buffer_append_char(RocketArray* buffer, std::uint8_t value) {
  return appendUniqueBuffer(buffer, value, ROCKET_ELEMENT_CHAR, rocket_rt_array_set_char);
}
RocketArray* rocket_std_buffer_append_string(RocketArray* buffer, RocketString* value) {
  return appendUniqueBuffer(buffer, value, ROCKET_ELEMENT_STRING,
                            rocket_rt_array_set_string);
}
RocketArray* rocket_std_buffer_append_managed(RocketArray* buffer, void* value) {
  return appendUniqueBuffer(buffer, value, ROCKET_ELEMENT_MANAGED,
                            rocket_rt_array_set_managed);
}

RocketArray* rocket_std_buffer_slice(RocketArray* buffer, std::int64_t start,
                                     std::int64_t end) {
  RuntimeArray* source = checkedArray(buffer);
  if (start < 0 || end < start || static_cast<std::uint64_t>(end) > source->length)
    sliceFailure(start, end, source->length);
  const std::uint64_t length = static_cast<std::uint64_t>(end - start);
  RuntimeArray* result = allocateArray(source->elementKind, length, length);
  const std::size_t size = elementSize(source->elementKind);
  if (length != 0)
    std::memcpy(result->elements,
                static_cast<std::uint8_t*>(source->elements) +
                    static_cast<std::size_t>(start) * size,
                static_cast<std::size_t>(length) * size);
  if (source->elementKind == ROCKET_ELEMENT_STRING ||
      source->elementKind == ROCKET_ELEMENT_MANAGED) {
    auto** elements = static_cast<void**>(result->elements);
    for (std::uint64_t index = 0; index < length; ++index)
      rocket_rt_retain(elements[index]);
  }
  return reinterpret_cast<RocketArray*>(result);
}

RocketArray* rocket_std_buffer_freeze(RocketArray* buffer) {
  checkedArray(buffer);
  rocket_rt_retain(buffer);
  return buffer;
}

RocketCancellation* rocket_std_cancel_token() {
  auto* token = new (std::nothrow) RuntimeCancellation;
  if (!token) runtimeFailure("out of memory while allocating CancellationToken");
  token->header = {1, 0, 1, destroyCancellation, ObjectCancellation, 0};
  token->cancelled.store(0, std::memory_order_relaxed);
  token->parent = nullptr;
  liveAllocations.fetch_add(1, std::memory_order_relaxed);
  promoteObject(token);
  return reinterpret_cast<RocketCancellation*>(token);
}

RocketCancellation* rocket_std_cancel_child(RocketCancellation* opaqueParent) {
  RuntimeCancellation* parent = checkedCancellation(opaqueParent);
  auto* token = new (std::nothrow) RuntimeCancellation;
  if (!token) runtimeFailure("out of memory while allocating child CancellationToken");
  token->header = {1, 0, 1, destroyCancellation, ObjectCancellation, 0};
  token->cancelled.store(0, std::memory_order_relaxed);
  token->parent = parent;
  rocket_rt_retain(parent);
  liveAllocations.fetch_add(1, std::memory_order_relaxed);
  promoteObject(token);
  return reinterpret_cast<RocketCancellation*>(token);
}

RocketCancellation* rocket_std_cancel_current() {
  if (!currentTaskCancellation) return rocket_std_cancel_token();
  rocket_rt_retain(currentTaskCancellation);
  return reinterpret_cast<RocketCancellation*>(currentTaskCancellation);
}

std::uint8_t rocket_std_cancel_cancel(RocketCancellation* opaque) {
  RuntimeCancellation* token = checkedCancellation(opaque);
  return token->cancelled.exchange(1, std::memory_order_acq_rel) == 0 ? 1 : 0;
}

std::uint8_t rocket_std_cancel_is_cancelled(RocketCancellation* opaque) {
  return cancellationObserved(checkedCancellation(opaque)) ? 1 : 0;
}

RocketAggregate* rocket_std_cancel_check(RocketCancellation* opaque) {
  return cancellationObserved(checkedCancellation(opaque))
             ? runtimeError("operation cancelled")
             : runtimeOkBool(true);
}

RocketAggregate* rocket_std_async_time_deadline_after(std::int64_t milliseconds) {
  if (milliseconds < 0) return runtimeError("deadline duration cannot be negative");
  const std::int64_t now = monotonicMilliseconds();
  if (milliseconds > (std::numeric_limits<std::int64_t>::max)() - now)
    return runtimeError("deadline duration is too large");
  RocketAggregate* result = rocket_rt_aggregate_new(0, 1, 0);
  rocket_rt_aggregate_set_int(result, 0, now + milliseconds);
  return result;
}

std::int64_t rocket_std_async_time_remaining(std::int64_t deadline) {
  return (std::max)(std::int64_t{0}, deadline - monotonicMilliseconds());
}

RocketTask* rocket_std_async_time_sleep_until(std::int64_t deadline,
                                              RocketCancellation* opaqueToken) {
  RuntimeCancellation* token = checkedCancellation(opaqueToken);
  if (deadline < 0)
    return rocket_rt_task_ready(runtimeError("deadline cannot be negative"));
  RocketAggregate* context = rocket_rt_aggregate_new(0, 2, 2);
  rocket_rt_aggregate_set_int(context, 0, deadline);
  rocket_rt_aggregate_set_managed(context, 1, token);
  return rocket_rt_task_spawn(reinterpret_cast<void*>(&timerTaskEntry), context);
}

RocketTask* rocket_std_async_time_sleep(std::int64_t milliseconds,
                                        RocketCancellation* token) {
  if (milliseconds < 0)
    return rocket_rt_task_ready(runtimeError("sleep duration cannot be negative"));
  const std::int64_t now = monotonicMilliseconds();
  if (milliseconds > (std::numeric_limits<std::int64_t>::max)() - now)
    return rocket_rt_task_ready(runtimeError("sleep duration is too large"));
  return rocket_std_async_time_sleep_until(now + milliseconds, token);
}

RocketTask* rocket_std_async_file_read(RocketString* path, std::int64_t maximum,
                                       RocketCancellation* token) {
  if (!path || !token)
    runtimeFailure("async_file.read received an invalid path or token");
  if (maximum < 0 || maximum > 67108864)
    return rocket_rt_task_ready(runtimeError(
        "asynchronous file read maximum must be from 0 through 67108864"));
  RocketAggregate* context = rocket_rt_aggregate_new(0, 3, 5);
  rocket_rt_aggregate_set_managed(context, 0, path);
  rocket_rt_aggregate_set_int(context, 1, maximum);
  rocket_rt_aggregate_set_managed(context, 2, token);
  return rocket_rt_task_spawn(reinterpret_cast<void*>(&asyncFileReadEntry), context);
}

RocketTask* rocket_std_async_file_write(RocketString* path, RocketArray* buffer,
                                        std::uint8_t append,
                                        RocketCancellation* token) {
  if (!path || !buffer || !token)
    runtimeFailure("async_file.write received an invalid path, buffer, or token");
  RuntimeArray* bytes = checkedArray(buffer);
  if (bytes->elementKind != ROCKET_ELEMENT_CHAR)
    return rocket_rt_task_ready(runtimeError(
        "asynchronous file writes require UniqueBuffer[Char]"));
  if (bytes->length > 67108864)
    return rocket_rt_task_ready(runtimeError(
        "asynchronous file write exceeds 67108864 bytes"));
  RocketAggregate* context = rocket_rt_aggregate_new(0, 4, 11);
  rocket_rt_aggregate_set_managed(context, 0, path);
  rocket_rt_aggregate_set_managed(context, 1, buffer);
  rocket_rt_aggregate_set_bool(context, 2, append);
  rocket_rt_aggregate_set_managed(context, 3, token);
  return rocket_rt_task_spawn(reinterpret_cast<void*>(&asyncFileWriteEntry), context);
}

RocketTask* rocket_std_async_net_connect(RocketString* host, std::int64_t port,
                                         std::int64_t deadline,
                                         RocketCancellation* token) {
  if (!host || !token) runtimeFailure("async_net.connect received an invalid value");
  RocketAggregate* context = rocket_rt_aggregate_new(0, 4, 9);
  rocket_rt_aggregate_set_managed(context, 0, host);
  rocket_rt_aggregate_set_int(context, 1, port);
  rocket_rt_aggregate_set_int(context, 2, deadline);
  rocket_rt_aggregate_set_managed(context, 3, token);
  return rocket_rt_task_spawn(reinterpret_cast<void*>(&asyncNetConnectEntry), context);
}

RocketTask* rocket_std_async_net_accept(std::int64_t listener,
                                        std::int64_t deadline,
                                        RocketCancellation* token) {
  if (!token) runtimeFailure("async_net.accept received an invalid token");
  RocketAggregate* context = rocket_rt_aggregate_new(0, 3, 4);
  rocket_rt_aggregate_set_int(context, 0, listener);
  rocket_rt_aggregate_set_int(context, 1, deadline);
  rocket_rt_aggregate_set_managed(context, 2, token);
  return rocket_rt_task_spawn(reinterpret_cast<void*>(&asyncNetAcceptEntry), context);
}

RocketTask* rocket_std_async_net_receive(std::int64_t socket,
                                         std::int64_t maximum,
                                         std::int64_t deadline,
                                         RocketCancellation* token) {
  if (!token) runtimeFailure("async_net.receive received an invalid token");
  if (maximum < 0 || maximum > 67108864)
    return rocket_rt_task_ready(runtimeError(
        "asynchronous socket receive maximum must be from 0 through 67108864"));
  RocketAggregate* context = rocket_rt_aggregate_new(0, 4, 8);
  rocket_rt_aggregate_set_int(context, 0, socket);
  rocket_rt_aggregate_set_int(context, 1, maximum);
  rocket_rt_aggregate_set_int(context, 2, deadline);
  rocket_rt_aggregate_set_managed(context, 3, token);
  return rocket_rt_task_spawn(reinterpret_cast<void*>(&asyncNetReceiveEntry), context);
}

RocketTask* rocket_std_async_net_send(std::int64_t socket, RocketArray* bytes,
                                      std::int64_t deadline,
                                      RocketCancellation* token) {
  if (!bytes || !token) runtimeFailure("async_net.send received an invalid value");
  RuntimeArray* checked = checkedArray(bytes);
  if (checked->elementKind != ROCKET_ELEMENT_CHAR)
    return rocket_rt_task_ready(runtimeError("async_net.send requires UniqueBuffer[Char]"));
  RocketAggregate* context = rocket_rt_aggregate_new(0, 4, 10);
  rocket_rt_aggregate_set_int(context, 0, socket);
  rocket_rt_aggregate_set_managed(context, 1, bytes);
  rocket_rt_aggregate_set_int(context, 2, deadline);
  rocket_rt_aggregate_set_managed(context, 3, token);
  return rocket_rt_task_spawn(reinterpret_cast<void*>(&asyncNetSendEntry), context);
}

RocketTask* rocket_std_async_process_run(RocketString* program,
                                         RocketArray* arguments,
                                         std::int64_t deadline,
                                         RocketCancellation* token) {
  if (!program || !arguments || !token)
    runtimeFailure("async_process.run received an invalid value");
  RocketAggregate* context = rocket_rt_aggregate_new(0, 4, 11);
  rocket_rt_aggregate_set_managed(context, 0, program);
  rocket_rt_aggregate_set_managed(context, 1, arguments);
  rocket_rt_aggregate_set_int(context, 2, deadline);
  rocket_rt_aggregate_set_managed(context, 3, token);
  return rocket_rt_task_spawn(reinterpret_cast<void*>(&asyncProcessRunEntry), context);
}

RocketMutex* syncMutexStored(std::uint32_t elementKind, RuntimeStoredValue value) {
  if (managedElementKind(elementKind) && !storedPointer(value))
    runtimeFailure("sync.mutex requires a non-null managed value");
  auto* mutex = new (std::nothrow) RuntimeMutex;
  if (!mutex) runtimeFailure("out of memory while allocating Mutex");
  mutex->header = {1, 0, 1, destroyMutex, ObjectMutex, 0};
  mutex->value = value;
  mutex->elementKind = elementKind;
  promoteStored(elementKind, value);
  retainStored(elementKind, value);
  liveAllocations.fetch_add(1, std::memory_order_relaxed);
  promoteObject(mutex);
  return reinterpret_cast<RocketMutex*>(mutex);
}

RocketMutex* rocket_std_sync_mutex(void* value) {
  return syncMutexStored(ROCKET_ELEMENT_MANAGED, storedPointer(value));
}
RocketMutex* rocket_std_sync_mutex_managed(void* value) {
  return rocket_std_sync_mutex(value);
}
RocketMutex* rocket_std_sync_mutex_string(RocketString* value) {
  return syncMutexStored(ROCKET_ELEMENT_STRING, storedPointer(value));
}
RocketMutex* rocket_std_sync_mutex_int(std::int64_t value) {
  return syncMutexStored(ROCKET_ELEMENT_INT,
                         {static_cast<std::uint64_t>(value)});
}
RocketMutex* rocket_std_sync_mutex_float(double value) {
  return syncMutexStored(ROCKET_ELEMENT_FLOAT, storedFloat(value));
}
RocketMutex* rocket_std_sync_mutex_bool(std::uint8_t value) {
  return syncMutexStored(ROCKET_ELEMENT_BOOL, {value != 0 ? 1U : 0U});
}
RocketMutex* rocket_std_sync_mutex_char(std::uint8_t value) {
  return syncMutexStored(ROCKET_ELEMENT_CHAR, {value});
}

RocketAggregate* rocket_std_sync_lock(RocketMutex* opaque, std::int64_t deadline,
                                      RocketCancellation* opaqueToken) {
  auto* mutex = reinterpret_cast<RuntimeMutex*>(opaque);
  if (!mutex || mutex->header.objectKind != ObjectMutex)
    runtimeFailure("sync.lock received an invalid Mutex");
  RuntimeCancellation* token = checkedCancellation(opaqueToken);
  while (!mutex->mutex.try_lock_for(std::chrono::milliseconds(2))) {
    if (operationCancellationObserved(token)) return runtimeError("operation cancelled");
    if (deadline >= 0 && monotonicMilliseconds() >= deadline)
      return runtimeError("mutex lock timed out");
  }
  if (operationCancellationObserved(token)) {
    mutex->mutex.unlock();
    return runtimeError("operation cancelled");
  }
  auto* guard = new (std::nothrow) RuntimeGuard;
  if (!guard) {
    mutex->mutex.unlock();
    runtimeFailure("out of memory while allocating LockGuard");
  }
  guard->header = {1, 0, 1, destroyGuard, ObjectGuard, 0};
  guard->owner = mutex;
  guard->locked = true;
  rocket_rt_retain(mutex);
  liveAllocations.fetch_add(1, std::memory_order_relaxed);
  promoteObject(guard);
  RocketAggregate* result = runtimeManagedVariant(0, guard);
  rocket_rt_release(guard);
  return result;
}

RuntimeGuard* checkedLiveGuard(RocketGuard* opaque) {
  auto* guard = reinterpret_cast<RuntimeGuard*>(opaque);
  if (!guard || guard->header.objectKind != ObjectGuard || !guard->locked)
    runtimeFailure("sync.guard_get received an inactive LockGuard");
  return guard;
}

RuntimeStoredValue guardStored(RocketGuard* opaque, std::uint32_t elementKind) {
  RuntimeGuard* guard = checkedLiveGuard(opaque);
  if (guard->owner->elementKind != elementKind)
    runtimeFailure("sync.guard_get value kind mismatch");
  retainStored(elementKind, guard->owner->value);
  return guard->owner->value;
}

void* rocket_std_sync_guard_get(RocketGuard* opaque) {
  return storedPointer(guardStored(opaque, ROCKET_ELEMENT_MANAGED));
}
void* rocket_std_sync_guard_get_managed(RocketGuard* opaque) {
  return rocket_std_sync_guard_get(opaque);
}
RocketString* rocket_std_sync_guard_get_string(RocketGuard* opaque) {
  return reinterpret_cast<RocketString*>(
      storedPointer(guardStored(opaque, ROCKET_ELEMENT_STRING)));
}
std::int64_t rocket_std_sync_guard_get_int(RocketGuard* opaque) {
  return static_cast<std::int64_t>(guardStored(opaque, ROCKET_ELEMENT_INT).bits);
}
double rocket_std_sync_guard_get_float(RocketGuard* opaque) {
  return storedFloat(guardStored(opaque, ROCKET_ELEMENT_FLOAT));
}
std::uint8_t rocket_std_sync_guard_get_bool(RocketGuard* opaque) {
  return guardStored(opaque, ROCKET_ELEMENT_BOOL).bits != 0 ? 1 : 0;
}
std::uint8_t rocket_std_sync_guard_get_char(RocketGuard* opaque) {
  return static_cast<std::uint8_t>(guardStored(opaque, ROCKET_ELEMENT_CHAR).bits);
}

std::uint8_t guardSetStored(RocketGuard* opaque, std::uint32_t elementKind,
                            RuntimeStoredValue value) {
  RuntimeGuard* guard = checkedLiveGuard(opaque);
  if (guard->owner->elementKind != elementKind ||
      (managedElementKind(elementKind) && !storedPointer(value)))
    runtimeFailure("sync.guard_set value kind mismatch");
  promoteStored(elementKind, value);
  retainStored(elementKind, value);
  releaseStored(elementKind, guard->owner->value);
  guard->owner->value = value;
  return 1;
}

std::uint8_t rocket_std_sync_guard_set(RocketGuard* opaque, void* value) {
  return guardSetStored(opaque, ROCKET_ELEMENT_MANAGED, storedPointer(value));
}
std::uint8_t rocket_std_sync_guard_set_managed(RocketGuard* opaque, void* value) {
  return rocket_std_sync_guard_set(opaque, value);
}
std::uint8_t rocket_std_sync_guard_set_string(RocketGuard* opaque,
                                              RocketString* value) {
  return guardSetStored(opaque, ROCKET_ELEMENT_STRING, storedPointer(value));
}
std::uint8_t rocket_std_sync_guard_set_int(RocketGuard* opaque, std::int64_t value) {
  return guardSetStored(opaque, ROCKET_ELEMENT_INT,
                        {static_cast<std::uint64_t>(value)});
}
std::uint8_t rocket_std_sync_guard_set_float(RocketGuard* opaque, double value) {
  return guardSetStored(opaque, ROCKET_ELEMENT_FLOAT, storedFloat(value));
}
std::uint8_t rocket_std_sync_guard_set_bool(RocketGuard* opaque, std::uint8_t value) {
  return guardSetStored(opaque, ROCKET_ELEMENT_BOOL, {value != 0 ? 1U : 0U});
}
std::uint8_t rocket_std_sync_guard_set_char(RocketGuard* opaque, std::uint8_t value) {
  return guardSetStored(opaque, ROCKET_ELEMENT_CHAR, {value});
}

RocketAggregate* rocket_std_sync_unlock(RocketGuard* opaque) {
  auto* guard = reinterpret_cast<RuntimeGuard*>(opaque);
  if (!guard || guard->header.objectKind != ObjectGuard)
    runtimeFailure("sync.unlock received an invalid LockGuard");
  if (!guard->locked) return runtimeOkBool(false);
  guard->locked = false;
  guard->owner->mutex.unlock();
  return runtimeOkBool(true);
}

RocketEvent* rocket_std_sync_event(std::uint8_t manualReset,
                                   std::uint8_t initiallySet) {
  auto* event = new (std::nothrow) RuntimeEvent;
  if (!event) runtimeFailure("out of memory while allocating Event");
  event->header = {1, 0, 1, destroyEvent, ObjectEvent, 0};
  event->manualReset = manualReset != 0;
  event->set = initiallySet != 0;
  event->generation = event->set ? 1 : 0;
  liveAllocations.fetch_add(1, std::memory_order_relaxed);
  promoteObject(event);
  return reinterpret_cast<RocketEvent*>(event);
}

std::uint8_t rocket_std_sync_event_set(RocketEvent* opaque) {
  auto* event = reinterpret_cast<RuntimeEvent*>(opaque);
  if (!event || event->header.objectKind != ObjectEvent)
    runtimeFailure("sync.event_set received an invalid Event");
  {
    std::lock_guard lock(event->mutex);
    event->set = true;
    ++event->generation;
  }
  if (event->manualReset) event->changed.notify_all();
  else event->changed.notify_one();
  return 1;
}

std::uint8_t rocket_std_sync_event_reset(RocketEvent* opaque) {
  auto* event = reinterpret_cast<RuntimeEvent*>(opaque);
  if (!event || event->header.objectKind != ObjectEvent)
    runtimeFailure("sync.event_reset received an invalid Event");
  std::lock_guard lock(event->mutex);
  const bool changed = event->set;
  event->set = false;
  return changed ? 1 : 0;
}

RocketAggregate* rocket_std_sync_event_wait(RocketEvent* opaque,
                                            std::int64_t deadline,
                                            RocketCancellation* opaqueToken) {
  auto* event = reinterpret_cast<RuntimeEvent*>(opaque);
  if (!event || event->header.objectKind != ObjectEvent)
    runtimeFailure("sync.event_wait received an invalid Event");
  RuntimeCancellation* token = checkedCancellation(opaqueToken);
  std::unique_lock lock(event->mutex);
  if (!waitUntil(event->changed, lock, deadline, token, [&] { return event->set; }))
    return operationCancellationObserved(token) ? runtimeError("operation cancelled")
                                       : runtimeError("event wait timed out");
  if (!event->manualReset) event->set = false;
  return runtimeOkBool(true);
}

RocketAtomicInt* rocket_std_sync_atomic_int(std::int64_t value) {
  auto* atomic = new (std::nothrow) RuntimeAtomicInt;
  if (!atomic) runtimeFailure("out of memory while allocating AtomicInt");
  atomic->header = {1, 0, 1, destroyAtomicInt, ObjectAtomicInt, 0};
  atomic->value.store(value, std::memory_order_seq_cst);
  liveAllocations.fetch_add(1, std::memory_order_relaxed);
  promoteObject(atomic);
  return reinterpret_cast<RocketAtomicInt*>(atomic);
}

RuntimeAtomicInt* checkedAtomic(RocketAtomicInt* opaque) {
  auto* atomic = reinterpret_cast<RuntimeAtomicInt*>(opaque);
  if (!atomic || atomic->header.objectKind != ObjectAtomicInt)
    runtimeFailure("atomic operation received an invalid AtomicInt");
  return atomic;
}

std::int64_t rocket_std_sync_atomic_load(RocketAtomicInt* value) {
  return checkedAtomic(value)->value.load(std::memory_order_seq_cst);
}

void rocket_std_sync_atomic_store(RocketAtomicInt* value, std::int64_t replacement) {
  checkedAtomic(value)->value.store(replacement, std::memory_order_seq_cst);
}

std::int64_t rocket_std_sync_atomic_fetch_add(RocketAtomicInt* value,
                                              std::int64_t delta) {
  return checkedAtomic(value)->value.fetch_add(delta, std::memory_order_seq_cst);
}

std::uint8_t rocket_std_sync_atomic_compare_exchange(RocketAtomicInt* value,
                                                     std::int64_t expected,
                                                     std::int64_t replacement) {
  return checkedAtomic(value)->value.compare_exchange_strong(
             expected, replacement, std::memory_order_seq_cst)
             ? 1
             : 0;
}

RocketOnce* syncOnceStored(std::uint32_t elementKind, RuntimeStoredValue value,
                           bool initialized) {
  if (initialized && managedElementKind(elementKind) && !storedPointer(value))
    runtimeFailure("sync.once requires a non-null managed initial value");
  auto* once = new (std::nothrow) RuntimeOnce;
  if (!once) runtimeFailure("out of memory while allocating Once");
  once->header = {1, 0, 1, destroyOnce, ObjectOnce, 0};
  once->value = value;
  once->elementKind = elementKind;
  once->initialized = initialized;
  if (initialized) {
    promoteStored(elementKind, value);
    retainStored(elementKind, value);
  }
  liveAllocations.fetch_add(1, std::memory_order_relaxed);
  promoteObject(once);
  return reinterpret_cast<RocketOnce*>(once);
}

RocketOnce* rocket_std_sync_once(void* value) {
  return syncOnceStored(ROCKET_ELEMENT_MANAGED, storedPointer(value), true);
}
RocketOnce* rocket_std_sync_once_managed(void* value) {
  return rocket_std_sync_once(value);
}
RocketOnce* rocket_std_sync_once_string(RocketString* value) {
  return syncOnceStored(ROCKET_ELEMENT_STRING, storedPointer(value), true);
}
RocketOnce* rocket_std_sync_once_int(std::int64_t value) {
  return syncOnceStored(ROCKET_ELEMENT_INT, {static_cast<std::uint64_t>(value)}, true);
}
RocketOnce* rocket_std_sync_once_float(double value) {
  return syncOnceStored(ROCKET_ELEMENT_FLOAT, storedFloat(value), true);
}
RocketOnce* rocket_std_sync_once_bool(std::uint8_t value) {
  return syncOnceStored(ROCKET_ELEMENT_BOOL, {value != 0 ? 1U : 0U}, true);
}
RocketOnce* rocket_std_sync_once_char(std::uint8_t value) {
  return syncOnceStored(ROCKET_ELEMENT_CHAR, {value}, true);
}

RocketOnce* rocket_std_sync_once_empty(void*) {
  return syncOnceStored(ROCKET_ELEMENT_MANAGED, {}, false);
}
RocketOnce* rocket_std_sync_once_empty_managed(void* witness) {
  return rocket_std_sync_once_empty(witness);
}
RocketOnce* rocket_std_sync_once_empty_string(RocketString*) {
  return syncOnceStored(ROCKET_ELEMENT_STRING, {}, false);
}
RocketOnce* rocket_std_sync_once_empty_int(std::int64_t) {
  return syncOnceStored(ROCKET_ELEMENT_INT, {}, false);
}
RocketOnce* rocket_std_sync_once_empty_float(double) {
  return syncOnceStored(ROCKET_ELEMENT_FLOAT, {}, false);
}
RocketOnce* rocket_std_sync_once_empty_bool(std::uint8_t) {
  return syncOnceStored(ROCKET_ELEMENT_BOOL, {}, false);
}
RocketOnce* rocket_std_sync_once_empty_char(std::uint8_t) {
  return syncOnceStored(ROCKET_ELEMENT_CHAR, {}, false);
}

RocketAggregate* onceSetStored(RocketOnce* opaque, std::uint32_t elementKind,
                               RuntimeStoredValue value) {
  auto* once = reinterpret_cast<RuntimeOnce*>(opaque);
  if (!once || once->header.objectKind != ObjectOnce ||
      once->elementKind != elementKind ||
      (managedElementKind(elementKind) && !storedPointer(value)))
    runtimeFailure("sync.once_set received an invalid Once or value");
  std::lock_guard lock(once->mutex);
  if (once->initialized) return runtimeOkBool(false);
  promoteStored(elementKind, value);
  retainStored(elementKind, value);
  once->value = value;
  once->initialized = true;
  return runtimeOkBool(true);
}

RocketAggregate* rocket_std_sync_once_set(RocketOnce* opaque, void* value) {
  return onceSetStored(opaque, ROCKET_ELEMENT_MANAGED, storedPointer(value));
}
RocketAggregate* rocket_std_sync_once_set_managed(RocketOnce* opaque, void* value) {
  return rocket_std_sync_once_set(opaque, value);
}
RocketAggregate* rocket_std_sync_once_set_string(RocketOnce* opaque,
                                                 RocketString* value) {
  return onceSetStored(opaque, ROCKET_ELEMENT_STRING, storedPointer(value));
}
RocketAggregate* rocket_std_sync_once_set_int(RocketOnce* opaque, std::int64_t value) {
  return onceSetStored(opaque, ROCKET_ELEMENT_INT,
                       {static_cast<std::uint64_t>(value)});
}
RocketAggregate* rocket_std_sync_once_set_float(RocketOnce* opaque, double value) {
  return onceSetStored(opaque, ROCKET_ELEMENT_FLOAT, storedFloat(value));
}
RocketAggregate* rocket_std_sync_once_set_bool(RocketOnce* opaque, std::uint8_t value) {
  return onceSetStored(opaque, ROCKET_ELEMENT_BOOL, {value != 0 ? 1U : 0U});
}
RocketAggregate* rocket_std_sync_once_set_char(RocketOnce* opaque, std::uint8_t value) {
  return onceSetStored(opaque, ROCKET_ELEMENT_CHAR, {value});
}

RocketAggregate* onceGetStored(RocketOnce* opaque, std::uint32_t elementKind) {
  auto* once = reinterpret_cast<RuntimeOnce*>(opaque);
  if (!once || once->header.objectKind != ObjectOnce ||
      once->elementKind != elementKind)
    runtimeFailure("sync.once_get received an invalid Once");
  std::lock_guard lock(once->mutex);
  return once->initialized ? runtimeStoredVariant(0, elementKind, once->value)
                           : rocket_rt_aggregate_new(1, 0, 0);
}

RocketAggregate* rocket_std_sync_once_get(RocketOnce* opaque) {
  return onceGetStored(opaque, ROCKET_ELEMENT_MANAGED);
}
RocketAggregate* rocket_std_sync_once_get_managed(RocketOnce* opaque) {
  return rocket_std_sync_once_get(opaque);
}
RocketAggregate* rocket_std_sync_once_get_string(RocketOnce* opaque) {
  return onceGetStored(opaque, ROCKET_ELEMENT_STRING);
}
RocketAggregate* rocket_std_sync_once_get_int(RocketOnce* opaque) {
  return onceGetStored(opaque, ROCKET_ELEMENT_INT);
}
RocketAggregate* rocket_std_sync_once_get_float(RocketOnce* opaque) {
  return onceGetStored(opaque, ROCKET_ELEMENT_FLOAT);
}
RocketAggregate* rocket_std_sync_once_get_bool(RocketOnce* opaque) {
  return onceGetStored(opaque, ROCKET_ELEMENT_BOOL);
}
RocketAggregate* rocket_std_sync_once_get_char(RocketOnce* opaque) {
  return onceGetStored(opaque, ROCKET_ELEMENT_CHAR);
}

RocketAggregate* makeChannel(RocketArray* initial, std::uint64_t capacity,
                             std::uint64_t maximum) {
  RuntimeArray* initialValues = checkedArray(initial);
  if (initialValues->length > maximum)
    return runtimeError("initial channel values exceed channel capacity");
  auto* channel = new (std::nothrow) RuntimeChannel;
  if (!channel) return runtimeError("out of memory while allocating Channel");
  channel->header = {1, 0, 1, destroyChannel, ObjectChannel, 0};
  channel->capacity = capacity;
  channel->maximum = maximum;
  channel->elementKind = initialValues->elementKind;
  channel->senders = 0;
  channel->receivers = 0;
  for (std::uint64_t index = 0; index < initialValues->length; ++index) {
    RuntimeStoredValue value = arrayStoredValue(initialValues, index);
    promoteStored(channel->elementKind, value);
    retainStored(channel->elementKind, value);
    channel->values.push_back(value);
  }
  liveAllocations.fetch_add(1, std::memory_order_relaxed);
  auto* sender = newEndpoint(channel, true);
  auto* receiver = newEndpoint(channel, false);
  RocketAggregate* pair = rocket_rt_aggregate_new(0, 2, 3);
  rocket_rt_aggregate_set_managed(pair, 0, sender);
  rocket_rt_aggregate_set_managed(pair, 1, receiver);
  rocket_rt_release(sender);
  rocket_rt_release(receiver);
  rocket_rt_release(channel);
  RocketAggregate* result = runtimeManagedVariant(0, pair);
  rocket_rt_release(pair);
  return result;
}

RocketAggregate* rocket_std_channel_bounded(RocketArray* initial,
                                            std::int64_t capacity) {
  if (capacity < 1 || capacity > 65536)
    return runtimeError("bounded channel capacity must be from 1 through 65536");
  return makeChannel(initial, static_cast<std::uint64_t>(capacity),
                     static_cast<std::uint64_t>(capacity));
}

RocketAggregate* rocket_std_channel_unbounded(RocketArray* initial) {
  return makeChannel(initial, 0, 1048576);
}

RocketSender* rocket_std_channel_sender(RocketAggregate* channel) {
  return reinterpret_cast<RocketSender*>(rocket_rt_aggregate_get_managed(channel, 0));
}

RocketReceiver* rocket_std_channel_receiver(RocketAggregate* channel) {
  return reinterpret_cast<RocketReceiver*>(rocket_rt_aggregate_get_managed(channel, 1));
}

RocketSender* rocket_std_channel_clone_sender(RocketSender* opaque) {
  RuntimeChannelEndpoint* endpoint = checkedEndpoint(opaque, true);
  if (endpoint->open.load(std::memory_order_acquire) == 0)
    runtimeFailure("cannot clone a closed Sender");
  return reinterpret_cast<RocketSender*>(newEndpoint(endpoint->channel, true));
}

RocketReceiver* rocket_std_channel_clone_receiver(RocketReceiver* opaque) {
  RuntimeChannelEndpoint* endpoint = checkedEndpoint(opaque, false);
  if (endpoint->open.load(std::memory_order_acquire) == 0)
    runtimeFailure("cannot clone a closed Receiver");
  return reinterpret_cast<RocketReceiver*>(newEndpoint(endpoint->channel, false));
}

RocketAggregate* channelSendStored(RocketSender* opaque,
                                   std::uint32_t elementKind,
                                   RuntimeStoredValue value,
                                   std::int64_t deadline,
                                   RocketCancellation* opaqueToken) {
  RuntimeChannelEndpoint* endpoint = checkedEndpoint(opaque, true);
  RuntimeCancellation* token = checkedCancellation(opaqueToken);
  if (managedElementKind(elementKind) && !storedPointer(value))
    runtimeFailure("channel.send requires a non-null managed value");
  RuntimeChannel* channel = endpoint->channel;
  if (channel->elementKind != elementKind)
    runtimeFailure("channel.send value kind mismatch");
  std::unique_lock lock(channel->mutex);
  const auto ready = [&] {
    return endpoint->open.load(std::memory_order_acquire) == 0 ||
           channel->receivers == 0 || channel->values.size() < channel->maximum;
  };
  if (!waitUntil(channel->writable, lock, deadline, token, ready))
    return operationCancellationObserved(token) ? runtimeError("operation cancelled")
                                       : runtimeError("channel send timed out");
  if (endpoint->open.load(std::memory_order_acquire) == 0)
    return runtimeError("Sender is closed");
  if (channel->receivers == 0) return runtimeError("channel receivers disconnected");
  promoteStored(elementKind, value);
  retainStored(elementKind, value);
  channel->values.push_back(value);
  lock.unlock();
  channel->readable.notify_one();
  return runtimeOkBool(true);
}

RocketAggregate* rocket_std_channel_send(RocketSender* sender, void* value,
                                         std::int64_t deadline,
                                         RocketCancellation* token) {
  RuntimeChannelEndpoint* endpoint = checkedEndpoint(sender, true);
  if (!managedElementKind(endpoint->channel->elementKind))
    runtimeFailure("legacy channel.send requires a managed channel value");
  return channelSendStored(sender, endpoint->channel->elementKind,
                           storedPointer(value), deadline, token);
}
RocketAggregate* rocket_std_channel_send_managed(RocketSender* sender, void* value,
                                                 std::int64_t deadline,
                                                 RocketCancellation* token) {
  return rocket_std_channel_send(sender, value, deadline, token);
}
RocketAggregate* rocket_std_channel_send_string(RocketSender* sender,
                                                RocketString* value,
                                                std::int64_t deadline,
                                                RocketCancellation* token) {
  return channelSendStored(sender, ROCKET_ELEMENT_STRING, storedPointer(value),
                           deadline, token);
}
RocketAggregate* rocket_std_channel_send_int(RocketSender* sender,
                                             std::int64_t value,
                                             std::int64_t deadline,
                                             RocketCancellation* token) {
  return channelSendStored(sender, ROCKET_ELEMENT_INT,
                           {static_cast<std::uint64_t>(value)}, deadline, token);
}
RocketAggregate* rocket_std_channel_send_float(RocketSender* sender, double value,
                                               std::int64_t deadline,
                                               RocketCancellation* token) {
  return channelSendStored(sender, ROCKET_ELEMENT_FLOAT, storedFloat(value),
                           deadline, token);
}
RocketAggregate* rocket_std_channel_send_bool(RocketSender* sender,
                                              std::uint8_t value,
                                              std::int64_t deadline,
                                              RocketCancellation* token) {
  return channelSendStored(sender, ROCKET_ELEMENT_BOOL, {value != 0 ? 1U : 0U},
                           deadline, token);
}
RocketAggregate* rocket_std_channel_send_char(RocketSender* sender,
                                              std::uint8_t value,
                                              std::int64_t deadline,
                                              RocketCancellation* token) {
  return channelSendStored(sender, ROCKET_ELEMENT_CHAR, {value}, deadline, token);
}

RocketAggregate* rocket_std_channel_receive(RocketReceiver* opaque,
                                            std::int64_t deadline,
                                            RocketCancellation* opaqueToken) {
  RuntimeChannelEndpoint* endpoint = checkedEndpoint(opaque, false);
  RuntimeCancellation* token = checkedCancellation(opaqueToken);
  RuntimeChannel* channel = endpoint->channel;
  std::unique_lock lock(channel->mutex);
  const auto ready = [&] {
    return endpoint->open.load(std::memory_order_acquire) == 0 ||
           !channel->values.empty() || channel->senders == 0;
  };
  if (!waitUntil(channel->readable, lock, deadline, token, ready))
    return operationCancellationObserved(token) ? runtimeError("operation cancelled")
                                       : runtimeError("channel receive timed out");
  if (endpoint->open.load(std::memory_order_acquire) == 0)
    return runtimeError("Receiver is closed");
  RocketAggregate* option = nullptr;
  if (channel->values.empty()) {
    option = runtimeManagedVariant(1, nullptr);
  } else {
    RuntimeStoredValue value = channel->values.front();
    channel->values.pop_front();
    option = runtimeStoredVariant(0, channel->elementKind, value);
    releaseStored(channel->elementKind, value);
  }
  lock.unlock();
  channel->writable.notify_one();
  RocketAggregate* result = runtimeManagedVariant(0, option);
  rocket_rt_release(option);
  return result;
}

RocketAggregate* rocket_std_channel_close_sender(RocketSender* opaque) {
  RuntimeChannelEndpoint* endpoint = checkedEndpoint(opaque, true);
  const bool wasOpen = endpoint->open.load(std::memory_order_acquire) != 0;
  closeEndpoint(endpoint);
  return runtimeOkBool(wasOpen);
}

RocketAggregate* rocket_std_channel_close_receiver(RocketReceiver* opaque) {
  RuntimeChannelEndpoint* endpoint = checkedEndpoint(opaque, false);
  const bool wasOpen = endpoint->open.load(std::memory_order_acquire) != 0;
  closeEndpoint(endpoint);
  return runtimeOkBool(wasOpen);
}

RocketString* rocket_rt_string_new(const std::uint8_t* bytes, std::uint64_t length) {
  if (length != 0 && !bytes) runtimeFailure("null UTF-8 input");
  if (length != 0 && !validUtf8(bytes, length)) runtimeFailure("invalid UTF-8 string");
  if (length > std::numeric_limits<std::size_t>::max() -
                   offsetof(RuntimeString, bytes) - 1)
    runtimeFailure("string allocation is too large");

  const std::size_t allocationSize =
      offsetof(RuntimeString, bytes) + static_cast<std::size_t>(length) + 1;
  auto* string = static_cast<RuntimeString*>(std::malloc(allocationSize));
  if (!string) runtimeFailure("out of memory while allocating String");
  string->header = {1, 0, 1, destroyString, ObjectString, 0};
  string->byteLength = length;
  if (length != 0) std::memcpy(string->bytes, bytes, static_cast<std::size_t>(length));
  string->bytes[length] = 0;
  liveAllocations.fetch_add(1, std::memory_order_relaxed);
  return reinterpret_cast<RocketString*>(string);
}

std::uint8_t rocket_rt_string_equal(const RocketString* left, const RocketString* right) {
  if (left == right) return 1;
  if (!left || !right) return 0;
  const RuntimeString* leftString = stringFor(left);
  const RuntimeString* rightString = stringFor(right);
  if (leftString->byteLength != rightString->byteLength) return 0;
  return static_cast<std::uint8_t>(
      leftString->byteLength == 0 ||
      std::memcmp(leftString->bytes, rightString->bytes,
                  static_cast<std::size_t>(leftString->byteLength)) == 0);
}

std::uint64_t rocket_rt_string_byte_length(const RocketString* string) {
  return string ? stringFor(string)->byteLength : 0;
}

const std::uint8_t* rocket_rt_string_bytes(const RocketString* string) {
  return string ? stringFor(string)->bytes : nullptr;
}

RocketArray* rocket_rt_array_new(std::uint32_t elementKind, std::uint64_t length) {
  return reinterpret_cast<RocketArray*>(allocateArray(elementKind, length, length));
}

std::uint64_t rocket_rt_array_capacity(const RocketArray* array) {
  return checkedArray(const_cast<RocketArray*>(array))->capacity;
}

RocketArray* rocket_rt_array_reserve(RocketArray* array, std::int64_t minimumCapacity) {
  if (minimumCapacity < 0) runtimeFailure("Array reserve capacity cannot be negative");
  RuntimeArray* source = checkedArray(array);
  const auto minimum = static_cast<std::uint64_t>(minimumCapacity);
  if (minimum <= source->capacity) {
    rocket_rt_retain(array);
    return array;
  }
  return reinterpret_cast<RocketArray*>(
      cloneArray(source, grownCapacity(source->capacity, minimum)));
}

void rocket_rt_array_set_int(RocketArray* array, std::int64_t index, std::int64_t value) {
  RuntimeArray* checked = checkedArray(array, index, ROCKET_ELEMENT_INT);
  static_cast<std::int64_t*>(checked->elements)[index] = value;
}

void rocket_rt_array_set_float(RocketArray* array, std::int64_t index, double value) {
  RuntimeArray* checked = checkedArray(array, index, ROCKET_ELEMENT_FLOAT);
  static_cast<double*>(checked->elements)[index] = value;
}

void rocket_rt_array_set_bool(RocketArray* array, std::int64_t index, std::uint8_t value) {
  RuntimeArray* checked = checkedArray(array, index, ROCKET_ELEMENT_BOOL);
  static_cast<std::uint8_t*>(checked->elements)[index] = value ? 1 : 0;
}

void rocket_rt_array_set_char(RocketArray* array, std::int64_t index, std::uint8_t value) {
  RuntimeArray* checked = checkedArray(array, index, ROCKET_ELEMENT_CHAR);
  static_cast<std::uint8_t*>(checked->elements)[index] = value;
}

void rocket_rt_array_set_string(RocketArray* array, std::int64_t index, RocketString* value) {
  RuntimeArray* checked = checkedArray(array, index, ROCKET_ELEMENT_STRING);
  auto** elements = static_cast<RocketString**>(checked->elements);
  rocket_rt_retain(value);
  rocket_rt_release(elements[index]);
  elements[index] = value;
}

void rocket_rt_array_set_managed(RocketArray* array, std::int64_t index, void* value) {
  RuntimeArray* checked = checkedArray(array, index, ROCKET_ELEMENT_MANAGED);
  auto** elements = static_cast<void**>(checked->elements);
  rocket_rt_retain(value);
  rocket_rt_release(elements[index]);
  elements[index] = value;
}

RocketArray* rocket_rt_array_update_int(RocketArray* array, std::int64_t index,
                                        std::int64_t value) {
  RuntimeArray* result = copyArrayForUpdate(array, index, ROCKET_ELEMENT_INT);
  rocket_rt_array_set_int(reinterpret_cast<RocketArray*>(result), index, value);
  return reinterpret_cast<RocketArray*>(result);
}

RocketArray* rocket_rt_array_update_float(RocketArray* array, std::int64_t index,
                                          double value) {
  RuntimeArray* result = copyArrayForUpdate(array, index, ROCKET_ELEMENT_FLOAT);
  rocket_rt_array_set_float(reinterpret_cast<RocketArray*>(result), index, value);
  return reinterpret_cast<RocketArray*>(result);
}

RocketArray* rocket_rt_array_update_bool(RocketArray* array, std::int64_t index,
                                         std::uint8_t value) {
  RuntimeArray* result = copyArrayForUpdate(array, index, ROCKET_ELEMENT_BOOL);
  rocket_rt_array_set_bool(reinterpret_cast<RocketArray*>(result), index, value);
  return reinterpret_cast<RocketArray*>(result);
}

RocketArray* rocket_rt_array_update_char(RocketArray* array, std::int64_t index,
                                         std::uint8_t value) {
  RuntimeArray* result = copyArrayForUpdate(array, index, ROCKET_ELEMENT_CHAR);
  rocket_rt_array_set_char(reinterpret_cast<RocketArray*>(result), index, value);
  return reinterpret_cast<RocketArray*>(result);
}

RocketArray* rocket_rt_array_update_string(RocketArray* array, std::int64_t index,
                                           RocketString* value) {
  RuntimeArray* result = copyArrayForUpdate(array, index, ROCKET_ELEMENT_STRING);
  rocket_rt_array_set_string(reinterpret_cast<RocketArray*>(result), index, value);
  return reinterpret_cast<RocketArray*>(result);
}

RocketArray* rocket_rt_array_update_managed(RocketArray* array, std::int64_t index,
                                            void* value) {
  RuntimeArray* result = copyArrayForUpdate(array, index, ROCKET_ELEMENT_MANAGED);
  rocket_rt_array_set_managed(reinterpret_cast<RocketArray*>(result), index, value);
  return reinterpret_cast<RocketArray*>(result);
}

RocketArray* rocket_rt_array_append_int(RocketArray* array, std::int64_t value) {
  return appendArray(array, value, ROCKET_ELEMENT_INT, rocket_rt_array_set_int);
}

RocketArray* rocket_rt_array_append_float(RocketArray* array, double value) {
  return appendArray(array, value, ROCKET_ELEMENT_FLOAT, rocket_rt_array_set_float);
}

RocketArray* rocket_rt_array_append_bool(RocketArray* array, std::uint8_t value) {
  return appendArray(array, value, ROCKET_ELEMENT_BOOL, rocket_rt_array_set_bool);
}

RocketArray* rocket_rt_array_append_char(RocketArray* array, std::uint8_t value) {
  return appendArray(array, value, ROCKET_ELEMENT_CHAR, rocket_rt_array_set_char);
}

RocketArray* rocket_rt_array_append_string(RocketArray* array, RocketString* value) {
  return appendArray(array, value, ROCKET_ELEMENT_STRING, rocket_rt_array_set_string);
}

RocketArray* rocket_rt_array_append_managed(RocketArray* array, void* value) {
  return appendArray(array, value, ROCKET_ELEMENT_MANAGED, rocket_rt_array_set_managed);
}

RocketArray* rocket_rt_array_insert_int(RocketArray* array, std::int64_t index,
                                        std::int64_t value) {
  return insertArray(array, index, value, ROCKET_ELEMENT_INT, rocket_rt_array_set_int);
}

RocketArray* rocket_rt_array_insert_float(RocketArray* array, std::int64_t index,
                                          double value) {
  return insertArray(array, index, value, ROCKET_ELEMENT_FLOAT, rocket_rt_array_set_float);
}

RocketArray* rocket_rt_array_insert_bool(RocketArray* array, std::int64_t index,
                                         std::uint8_t value) {
  return insertArray(array, index, value, ROCKET_ELEMENT_BOOL, rocket_rt_array_set_bool);
}

RocketArray* rocket_rt_array_insert_char(RocketArray* array, std::int64_t index,
                                         std::uint8_t value) {
  return insertArray(array, index, value, ROCKET_ELEMENT_CHAR, rocket_rt_array_set_char);
}

RocketArray* rocket_rt_array_insert_string(RocketArray* array, std::int64_t index,
                                           RocketString* value) {
  return insertArray(array, index, value, ROCKET_ELEMENT_STRING,
                     rocket_rt_array_set_string);
}

RocketArray* rocket_rt_array_insert_managed(RocketArray* array, std::int64_t index,
                                            void* value) {
  return insertArray(array, index, value, ROCKET_ELEMENT_MANAGED,
                     rocket_rt_array_set_managed);
}

RocketAggregate* rocket_rt_array_pop(RocketArray* array) {
  RuntimeArray* source = checkedArray(array);
  if (source->length == 0)
    return rocket_rt_aggregate_new(1, 0, 0);

  RuntimeArray* result = cloneArray(source, source->capacity);
  const std::uint64_t index = result->length - 1;
  const bool managed = result->elementKind == ROCKET_ELEMENT_STRING ||
                       result->elementKind == ROCKET_ELEMENT_MANAGED;
  RocketAggregate* popped = rocket_rt_aggregate_new(0, 2, managed ? 3 : 1);
  rocket_rt_aggregate_set_managed(popped, 0, result);
  switch (result->elementKind) {
  case ROCKET_ELEMENT_INT:
    rocket_rt_aggregate_set_int(popped, 1, static_cast<std::int64_t*>(result->elements)[index]);
    break;
  case ROCKET_ELEMENT_FLOAT:
    rocket_rt_aggregate_set_float(popped, 1, static_cast<double*>(result->elements)[index]);
    break;
  case ROCKET_ELEMENT_BOOL:
    rocket_rt_aggregate_set_bool(popped, 1, static_cast<std::uint8_t*>(result->elements)[index]);
    break;
  case ROCKET_ELEMENT_CHAR:
    rocket_rt_aggregate_set_char(popped, 1, static_cast<std::uint8_t*>(result->elements)[index]);
    break;
  case ROCKET_ELEMENT_STRING:
  case ROCKET_ELEMENT_MANAGED: {
    auto** elements = static_cast<void**>(result->elements);
    void* removed = elements[index];
    rocket_rt_aggregate_set_managed(popped, 1, removed);
    elements[index] = nullptr;
    rocket_rt_release(removed);
    break;
  }
  default: runtimeFailure("invalid Array element kind during pop");
  }
  result->length = index;
  rocket_rt_release(result);

  RocketAggregate* some = rocket_rt_aggregate_new(0, 1, 1);
  rocket_rt_aggregate_set_managed(some, 0, popped);
  rocket_rt_release(popped);
  return some;
}

RocketAggregate* rocket_rt_array_remove(RocketArray* array, std::int64_t index) {
  RuntimeArray* source = checkedArray(array);
  if (index < 0 || static_cast<std::uint64_t>(index) >= source->length)
    indexFailure(index, source->length);
  RuntimeArray* result = cloneArray(source, source->capacity);
  const std::uint64_t offset = static_cast<std::uint64_t>(index);
  const bool managed = result->elementKind == ROCKET_ELEMENT_STRING ||
                       result->elementKind == ROCKET_ELEMENT_MANAGED;
  RocketAggregate* removed = rocket_rt_aggregate_new(0, 2, managed ? 3 : 1);
  rocket_rt_aggregate_set_managed(removed, 0, result);
  switch (result->elementKind) {
  case ROCKET_ELEMENT_INT:
    rocket_rt_aggregate_set_int(removed, 1,
                                static_cast<std::int64_t*>(result->elements)[offset]);
    break;
  case ROCKET_ELEMENT_FLOAT:
    rocket_rt_aggregate_set_float(removed, 1,
                                  static_cast<double*>(result->elements)[offset]);
    break;
  case ROCKET_ELEMENT_BOOL:
    rocket_rt_aggregate_set_bool(removed, 1,
                                 static_cast<std::uint8_t*>(result->elements)[offset]);
    break;
  case ROCKET_ELEMENT_CHAR:
    rocket_rt_aggregate_set_char(removed, 1,
                                 static_cast<std::uint8_t*>(result->elements)[offset]);
    break;
  case ROCKET_ELEMENT_STRING:
  case ROCKET_ELEMENT_MANAGED:
    rocket_rt_aggregate_set_managed(
        removed, 1, static_cast<void**>(result->elements)[offset]);
    break;
  default: runtimeFailure("invalid Array element kind during remove");
  }

  const std::size_t size = elementSize(result->elementKind);
  auto* bytes = static_cast<std::uint8_t*>(result->elements);
  const std::size_t trailing =
      static_cast<std::size_t>(result->length - offset - 1) * size;
  void* removedManaged = managed ? static_cast<void**>(result->elements)[offset] : nullptr;
  if (trailing != 0)
    std::memmove(bytes + offset * size, bytes + (offset + 1) * size, trailing);
  std::memset(bytes + (result->length - 1) * size, 0, size);
  --result->length;
  if (removedManaged) rocket_rt_release(removedManaged);
  rocket_rt_release(result);
  return removed;
}

RocketArray* rocket_rt_array_clear(RocketArray* array) {
  RuntimeArray* source = checkedArray(array);
  return reinterpret_cast<RocketArray*>(
      allocateArray(source->elementKind, 0, source->capacity));
}

RocketSlice* rocket_rt_slice_new(void* collection, std::int64_t start, std::int64_t end) {
  const CollectionView view = collectionView(collection);
  if (start < 0 || end < start || static_cast<std::uint64_t>(end) > view.length)
    sliceFailure(start, end, view.length);
  auto* slice = static_cast<RuntimeSlice*>(std::malloc(sizeof(RuntimeSlice)));
  if (!slice) runtimeFailure("out of memory while allocating Slice");
  rocket_rt_retain(const_cast<RuntimeArray*>(view.owner));
  slice->header = {1, 0, 1, destroySlice, ObjectSlice, 0};
  slice->owner = const_cast<RuntimeArray*>(view.owner);
  slice->offset = view.offset + static_cast<std::uint64_t>(start);
  slice->length = static_cast<std::uint64_t>(end - start);
  liveAllocations.fetch_add(1, std::memory_order_relaxed);
  return reinterpret_cast<RocketSlice*>(slice);
}

std::uint64_t rocket_rt_collection_length(const void* collection) {
  return collectionView(collection).length;
}

std::uint32_t rocket_rt_collection_element_kind(const void* collection) {
  return collectionView(collection).owner->elementKind;
}

std::int64_t rocket_rt_index_int(const void* collection, std::int64_t index) {
  const CollectionView view = collectionView(collection);
  return static_cast<const std::int64_t*>(view.owner->elements)[
      checkedIndex(view, index, ROCKET_ELEMENT_INT)];
}

double rocket_rt_index_float(const void* collection, std::int64_t index) {
  const CollectionView view = collectionView(collection);
  return static_cast<const double*>(view.owner->elements)[
      checkedIndex(view, index, ROCKET_ELEMENT_FLOAT)];
}

std::uint8_t rocket_rt_index_bool(const void* collection, std::int64_t index) {
  const CollectionView view = collectionView(collection);
  return static_cast<const std::uint8_t*>(view.owner->elements)[
      checkedIndex(view, index, ROCKET_ELEMENT_BOOL)];
}

std::uint8_t rocket_rt_index_char(const void* collection, std::int64_t index) {
  const CollectionView view = collectionView(collection);
  return static_cast<const std::uint8_t*>(view.owner->elements)[
      checkedIndex(view, index, ROCKET_ELEMENT_CHAR)];
}

RocketString* rocket_rt_index_string(const void* collection, std::int64_t index) {
  const CollectionView view = collectionView(collection);
  RocketString* value = static_cast<RocketString**>(view.owner->elements)[
      checkedIndex(view, index, ROCKET_ELEMENT_STRING)];
  rocket_rt_retain(value);
  return value;
}

void* rocket_rt_index_managed(const void* collection, std::int64_t index) {
  const CollectionView view = collectionView(collection);
  void* value = static_cast<void**>(view.owner->elements)[
      checkedIndex(view, index, ROCKET_ELEMENT_MANAGED)];
  rocket_rt_retain(value);
  return value;
}

RocketAggregate* rocket_rt_aggregate_new(std::uint32_t tag, std::uint32_t fieldCount,
                                         std::uint64_t managedMask) {
  if (fieldCount > 64) runtimeFailure("aggregates are limited to 64 fields in ABI v1");
  auto* aggregate = static_cast<RuntimeAggregate*>(std::malloc(sizeof(RuntimeAggregate)));
  if (!aggregate) runtimeFailure("out of memory while allocating aggregate");
  std::uint64_t* fields = nullptr;
  if (fieldCount != 0) {
    fields = static_cast<std::uint64_t*>(std::calloc(fieldCount, sizeof(std::uint64_t)));
    if (!fields) {
      std::free(aggregate);
      runtimeFailure("out of memory while allocating aggregate fields");
    }
  }
  aggregate->header = {1, 0, 1, destroyAggregate, ObjectAggregate, 0};
  aggregate->tag = tag;
  aggregate->fieldCount = fieldCount;
  aggregate->managedMask = managedMask;
  aggregate->fields = fields;
  liveAllocations.fetch_add(1, std::memory_order_relaxed);
  return reinterpret_cast<RocketAggregate*>(aggregate);
}

std::uint32_t rocket_rt_aggregate_tag(const RocketAggregate* aggregate) {
  const auto* runtime = reinterpret_cast<const RuntimeAggregate*>(aggregate);
  if (!runtime || runtime->header.objectKind != ObjectAggregate)
    runtimeFailure("tag operation received an invalid enum");
  return runtime->tag;
}

void rocket_rt_aggregate_set_int(RocketAggregate* aggregate, std::uint32_t field,
                                 std::int64_t value) {
  checkedAggregate(aggregate, field)->fields[field] = static_cast<std::uint64_t>(value);
}

void rocket_rt_aggregate_set_float(RocketAggregate* aggregate, std::uint32_t field,
                                   double value) {
  auto* checked = checkedAggregate(aggregate, field);
  std::memcpy(&checked->fields[field], &value, sizeof(value));
}

void rocket_rt_aggregate_set_bool(RocketAggregate* aggregate, std::uint32_t field,
                                  std::uint8_t value) {
  checkedAggregate(aggregate, field)->fields[field] = value ? 1 : 0;
}

void rocket_rt_aggregate_set_char(RocketAggregate* aggregate, std::uint32_t field,
                                  std::uint8_t value) {
  checkedAggregate(aggregate, field)->fields[field] = value;
}

void rocket_rt_aggregate_set_managed(RocketAggregate* aggregate, std::uint32_t field,
                                     void* value) {
  auto* checked = checkedAggregate(aggregate, field);
  if ((checked->managedMask & (std::uint64_t{1} << field)) == 0)
    runtimeFailure("aggregate managed-field mask mismatch");
  void* previous = reinterpret_cast<void*>(checked->fields[field]);
  rocket_rt_retain(value);
  rocket_rt_release(previous);
  checked->fields[field] = reinterpret_cast<std::uint64_t>(value);
}

std::int64_t rocket_rt_aggregate_get_int(const RocketAggregate* aggregate,
                                         std::uint32_t field) {
  return static_cast<std::int64_t>(checkedAggregate(aggregate, field)->fields[field]);
}

double rocket_rt_aggregate_get_float(const RocketAggregate* aggregate,
                                     std::uint32_t field) {
  double value = 0.0;
  const auto* checked = checkedAggregate(aggregate, field);
  std::memcpy(&value, &checked->fields[field], sizeof(value));
  return value;
}

std::uint8_t rocket_rt_aggregate_get_bool(const RocketAggregate* aggregate,
                                          std::uint32_t field) {
  return checkedAggregate(aggregate, field)->fields[field] != 0 ? 1 : 0;
}

std::uint8_t rocket_rt_aggregate_get_char(const RocketAggregate* aggregate,
                                          std::uint32_t field) {
  return static_cast<std::uint8_t>(checkedAggregate(aggregate, field)->fields[field]);
}

void* rocket_rt_aggregate_get_managed(const RocketAggregate* aggregate,
                                      std::uint32_t field) {
  const auto* checked = checkedAggregate(aggregate, field);
  if ((checked->managedMask & (std::uint64_t{1} << field)) == 0)
    runtimeFailure("aggregate managed-field mask mismatch");
  void* value = reinterpret_cast<void*>(checked->fields[field]);
  rocket_rt_retain(value);
  return value;
}

void rocket_rt_panic_integer_overflow() { runtimeFailure("Int arithmetic overflow"); }

void rocket_rt_panic_division_by_zero() { runtimeFailure("Int division by zero"); }

void rocket_rt_print_int(std::int64_t value) { std::printf("%" PRId64 "\n", value); }

void rocket_rt_print_float(double value) { std::printf("%g\n", value); }

void rocket_rt_print_bool(std::uint8_t value) { std::printf("%u\n", value ? 1U : 0U); }

void rocket_rt_print_char(std::uint8_t value) {
  std::fwrite(&value, 1, 1, stdout);
  std::fputc('\n', stdout);
}

void rocket_rt_print_string(const RocketString* value) {
  if (!value) runtimeFailure("attempted to print an invalid String");
  const RuntimeString* string = stringFor(value);
  if (string->byteLength != 0)
    std::fwrite(string->bytes, 1, static_cast<std::size_t>(string->byteLength), stdout);
  std::fputc('\n', stdout);
}

void rocket_rt_print_unit() { std::fputs("()\n", stdout); }

void rocket_rt_tooling_hit(const char* source, std::int64_t line,
                           const char* symbol, std::uint32_t kind) {
  std::call_once(toolingExitRegistration, [] { std::atexit(toolingWriteReports); });
  std::lock_guard lock(toolingMutex);
  if (kind == 1 && source != nullptr && line > 0)
    ++coverageHits[{source, line}];
  else if (kind == 2 && symbol != nullptr)
    ++profileHits[symbol];
}

std::uint64_t rocket_rt_debug_live_allocations() {
  return liveAllocations.load(std::memory_order_relaxed);
}

} // extern "C"
