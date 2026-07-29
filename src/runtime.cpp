#include "runtime.h"

#include <atomic>
#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {

inline constexpr std::uint32_t RuntimeAbiVersion = 1;

struct AllocationHeader {
  std::uint64_t references;
  void (*destroy)(AllocationHeader*);
  std::uint32_t objectKind;
  std::uint32_t reserved;
};

inline constexpr std::uint32_t ObjectString = 1;
inline constexpr std::uint32_t ObjectArray = 2;
inline constexpr std::uint32_t ObjectSlice = 3;
inline constexpr std::uint32_t ObjectAggregate = 4;
inline constexpr std::uint32_t ObjectStringBuilder = 5;

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

struct CollectionView {
  const RuntimeArray* owner;
  std::uint64_t offset;
  std::uint64_t length;
};

std::atomic<std::uint64_t> liveAllocations = 0;

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
  std::free(header);
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

void destroyArray(AllocationHeader* header) {
  auto* array = reinterpret_cast<RuntimeArray*>(header);
  if (array->elementKind == ROCKET_ELEMENT_STRING ||
      array->elementKind == ROCKET_ELEMENT_MANAGED) {
    auto** strings = static_cast<void**>(array->elements);
    for (std::uint64_t index = 0; index < array->length; ++index)
      rocket_rt_release(strings[index]);
  }
  std::free(array->elements);
  std::free(array);
  liveAllocations.fetch_sub(1, std::memory_order_relaxed);
}

void destroyAggregate(AllocationHeader* header) {
  auto* aggregate = reinterpret_cast<RuntimeAggregate*>(header);
  for (std::uint32_t index = 0; index < aggregate->fieldCount; ++index)
    if ((aggregate->managedMask & (std::uint64_t{1} << index)) != 0)
      rocket_rt_release(reinterpret_cast<void*>(aggregate->fields[index]));
  std::free(aggregate->fields);
  std::free(aggregate);
  liveAllocations.fetch_sub(1, std::memory_order_relaxed);
}

void destroySlice(AllocationHeader* header) {
  auto* slice = reinterpret_cast<RuntimeSlice*>(header);
  rocket_rt_release(slice->owner);
  std::free(slice);
  liveAllocations.fetch_sub(1, std::memory_order_relaxed);
}

void destroyStringBuilder(AllocationHeader* header) {
  auto* builder = reinterpret_cast<RuntimeStringBuilder*>(header);
  std::free(builder->bytes);
  std::free(builder);
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

RuntimeArray* copyArrayForUpdate(RocketArray* array, std::int64_t index,
                                 std::uint32_t expectedKind) {
  RuntimeArray* source = checkedArray(array, index, expectedKind);
  if (source->header.references == 1) {
    rocket_rt_retain(array);
    return source;
  }

  RuntimeArray* copy = arrayFor(rocket_rt_array_new(source->elementKind, source->length));
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

} // namespace

extern "C" {

std::uint32_t rocket_rt_abi_version() { return RuntimeAbiVersion; }

RocketStringBuilder* rocket_std_string_builder() {
  auto* builder = static_cast<RuntimeStringBuilder*>(std::malloc(sizeof(RuntimeStringBuilder)));
  if (!builder) runtimeFailure("out of memory while allocating String Builder");
  builder->header = {1, destroyStringBuilder, ObjectStringBuilder, 0};
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
  if (header->references == std::numeric_limits<std::uint64_t>::max())
    runtimeFailure("reference count overflow");
  ++header->references;
}

void rocket_rt_release(void* object) {
  if (!object) return;
  AllocationHeader* header = headerFor(object);
  if (header->references == 0) runtimeFailure("reference count underflow");
  --header->references;
  if (header->references == 0) header->destroy(header);
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
  string->header = {1, destroyString, ObjectString, 0};
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
  const std::size_t size = elementSize(elementKind);
  if (length > std::numeric_limits<std::size_t>::max() / size)
    runtimeFailure("Array allocation is too large");
  auto* array = static_cast<RuntimeArray*>(std::malloc(sizeof(RuntimeArray)));
  if (!array) runtimeFailure("out of memory while allocating Array");
  void* elements = nullptr;
  if (length != 0) {
    elements = std::calloc(static_cast<std::size_t>(length), size);
    if (!elements) {
      std::free(array);
      runtimeFailure("out of memory while allocating Array elements");
    }
  }
  array->header = {1, destroyArray, ObjectArray, 0};
  array->elementKind = elementKind;
  array->reserved = 0;
  array->length = length;
  array->elements = elements;
  liveAllocations.fetch_add(1, std::memory_order_relaxed);
  return reinterpret_cast<RocketArray*>(array);
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

RocketSlice* rocket_rt_slice_new(void* collection, std::int64_t start, std::int64_t end) {
  const CollectionView view = collectionView(collection);
  if (start < 0 || end < start || static_cast<std::uint64_t>(end) > view.length)
    sliceFailure(start, end, view.length);
  auto* slice = static_cast<RuntimeSlice*>(std::malloc(sizeof(RuntimeSlice)));
  if (!slice) runtimeFailure("out of memory while allocating Slice");
  rocket_rt_retain(const_cast<RuntimeArray*>(view.owner));
  slice->header = {1, destroySlice, ObjectSlice, 0};
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
  aggregate->header = {1, destroyAggregate, ObjectAggregate, 0};
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

std::uint64_t rocket_rt_debug_live_allocations() {
  return liveAllocations.load(std::memory_order_relaxed);
}

} // extern "C"
