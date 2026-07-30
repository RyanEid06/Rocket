# Rocket Standard Library through the Rocket 1.5 foundation

The stable library is a set of built-in source modules, including the Rocket 1.2
dot-call additions, the unchanged Rocket 1.3 native boundary, and the first
Rocket 1.5 production-library APIs. Import a module by its stable name; no
package file or downloaded dependency is required:

```rocket
import std.file
import std.json

fn load(path: String) -> Result[std.json.Json, String]:
    let text = file.read_text(path)?
    return json.parse(text)
```

Recoverable operating-system, parsing, and conversion failures return
`Result[..., String]`. Optional environment values return `Option[String]`.
Only violated programmer contracts, such as an invalid random range or a
negative sleep duration, terminate through the runtime error path.

## `std.string`

| Function | Result | Meaning |
| --- | --- | --- |
| `byte_length(value: String)` | `Int` | UTF-8 byte count |
| `concat(left: String, right: String)` | `String` | Concatenate strings |
| `contains(value: String, needle: String)` | `Bool` | Byte-sequence containment |
| `starts_with(value: String, prefix: String)` | `Bool` | Prefix test |
| `ends_with(value: String, suffix: String)` | `Bool` | Suffix test |
| `trim(value: String)` | `String` | Remove leading/trailing ASCII whitespace |
| `split(value: String, delimiter: String)` | `Array[String]` | Split on a literal delimiter |
| `byte_at(value: String, index: Int)` | `Char` | Read one checked UTF-8 byte |
| `byte_value_at(value: String, index: Int)` | `Int` | Read one checked byte as 0 through 255 |
| `slice(value: String, start: Int, end: Int)` | `String` | Copy an exclusive checked byte range |
| `parse_int(value: String)` | `Result[Int, String]` | Parse an entire base-10 signed integer |
| `from_int(value: Int)` | `String` | Format a base-10 signed integer |
| `builder()` | `Builder` | Create an empty mutable text builder |
| `builder_append(builder: Builder, value: String)` | `Unit` | Append text to a builder |
| `builder_finish(builder: Builder)` | `String` | Copy the builder contents to an immutable String |

An empty split delimiter returns an array containing the original value.
`byte_at` and `byte_value_at` use a byte index, not a Unicode scalar index. `slice` requires valid
exclusive bounds and its result must also be valid UTF-8; violated bounds or a
range that splits a UTF-8 sequence are programmer-contract runtime failures.
`Builder` is an explicitly mutable standard-library construction type intended
for compilers, formatters, and encoders. Aliases observe appended text; only the
immutable `String` returned by `builder_finish` crosses ordinary text APIs.

## `std.collections`

`length[T](values: Array[T])` and `slice_length[T](values: Slice[T])` return an
`Int`. `reverse[T](values: Array[T])` returns a new array.
`concat[T](left: Array[T], right: Array[T])` returns a new array containing both
inputs in order. `join` combines an `Array[String]` with a separator. All
operations preserve their inputs.

Rocket 1.1 adds copy-on-write growth operations:

| Function | Result | Meaning |
| --- | --- | --- |
| `capacity[T](values: Array[T])` | `Int` | Current element capacity |
| `reserve[T](values: Array[T], minimum: Int)` | `Array[T]` | Ensure at least `minimum` capacity |
| `append[T](values: Array[T], value: T)` | `Array[T]` | Return the Array with `value` added |
| `pop[T](values: Array[T])` | `Option[Pop[T]]` | Return the shortened Array and removed last value |
| `insert[T](values: Array[T], index: Int, value: T)` | `Array[T]` | Insert before `index`, including at the end |
| `remove[T](values: Array[T], index: Int)` | `Removal[T]` | Return the shortened Array and indexed value |
| `clear[T](values: Array[T])` | `Array[T]` | Return an empty Array retaining its capacity |

`Pop[T]` is a public standard-library struct with `values: Array[T]` and
`value: T`. Empty `pop` returns `None`. A negative reserve request is a
programmer-contract runtime failure. Existing aliases and Slices remain stable
snapshots; capacity is part of that observable snapshot.
`Removal[T]` has the same two fields as `Pop[T]`. Invalid insert/remove indices
use the ordinary deterministic collection-bounds failure path.

`Tuple2[A, B]` and `Tuple3[A, B, C]` provide `first`, `second`, and `third`
fields. Map keys and Set elements are limited to `Int`, `Bool`, `Char`, and
`String`. Construction preserves the first duplicate and all returned iteration
Arrays use insertion order.

| Function | Result |
| --- | --- |
| `map_from_arrays[K, V](keys: Array[K], values: Array[V])` | `Map[K, V]` |
| `map_length[K, V](map: Map[K, V])` | `Int` |
| `map_find[K, V](map: Map[K, V], key: K)` | `Option[Int]` |
| `map_get[K, V](map: Map[K, V], key: K)` | `Option[V]` |
| `map_keys[K, V](map: Map[K, V])` | `Array[K]` |
| `map_values[K, V](map: Map[K, V])` | `Array[V]` |
| `set_from_array[T](values: Array[T])` | `Set[T]` |
| `set_contains[T](set: Set[T], value: T)` | `Bool` |
| `set_values[T](set: Set[T])` | `Array[T]` |
| `hash[T](value: T)` | `Int` |

`contains` and `find` search scalar/String Arrays; `filter_equal` keeps values
equal to a supplied scalar/String. `sort_int`, `sort_float`, `sort_char`, and
`sort_string` return stable ascending snapshots (Float NaNs retain relative
order after all ordered values). `map_hash` maps eligible values to stable
hashes. `fold_sum_int` and `fold_sum_float` provide checked-Int and IEEE Float
numeric folds. General callback-based `map`, `filter`, and `fold` arrive with
first-class function values in Rocket 1.2 rather than using a privileged
callback convention in 1.1.

`Queue[T]`, `Stack[T]`, and `ByteBuffer` are ordinary immutable wrappers around
an insertion-ordered `Array[T]`, `Array[T]`, and `Array[Char]` respectively.
Their public `values`/`bytes` snapshots compose with the Array operations above;
Rocket 1.2 development adds dot-call convenience without changing
representation. Examples include `values.length()`, `values.append(value)`,
`map.get(key)`, `set.contains(value)`, and `text.trim()`. `Slice[T]` currently
supports `length()`; Array-only operations remain unavailable on Slice. Static
String construction may use `String.from_int(value)`, while
`std.string.from_int(value)` remains valid. These aliases resolve to the same
standard intrinsics and do not add runtime entry points.

## `std.binary` (Rocket 1.5 foundation)

`std.collections.ByteBuffer` is an immutable wrapper around `Array[Char]` and
stores arbitrary bytes, including zero and invalid UTF-8. Binary operations
never panic for data-dependent bounds, encoding, or numeric-range failures;
they return `Result[..., String]`.

| Function | Result | Meaning |
| --- | --- | --- |
| `from_string(value: String)` | `ByteBuffer` | Copy the complete UTF-8 byte sequence, including embedded zero bytes |
| `to_string(buffer: ByteBuffer)` | `Result[String, String]` | Validate UTF-8 and copy it into an immutable String |
| `length(buffer: ByteBuffer)` | `Int` | Return the byte count |
| `slice(buffer: ByteBuffer, offset: Int, length: Int)` | `Result[ByteBuffer, String]` | Copy a checked byte range |
| `read_u8(buffer: ByteBuffer, offset: Int)` | `Result[Int, String]` | Read one unsigned byte |
| `read_u16_le(buffer: ByteBuffer, offset: Int)` | `Result[Int, String]` | Read a little-endian unsigned 16-bit integer |
| `read_u32_le(buffer: ByteBuffer, offset: Int)` | `Result[Int, String]` | Read a little-endian unsigned 32-bit integer |
| `write_u8(value: Int)` | `Result[ByteBuffer, String]` | Encode an unsigned 8-bit integer |
| `write_u16_le(value: Int)` | `Result[ByteBuffer, String]` | Encode an unsigned 16-bit integer |
| `write_u32_le(value: Int)` | `Result[ByteBuffer, String]` | Encode an unsigned 32-bit integer |

Offsets are zero-based. Reads reject negative or truncated ranges. Writes
reject negative values and values outside the selected unsigned width. The
little-endian spelling is explicit so serialized data is independent of the
host CPU. These functions are synchronous CPU operations and do not retain
hidden mutable cursor state. Buffered stream objects and additional encodings
remain later Phase 15 work.

## `std.file` and `std.path`

`file.read_text`, `write_text`, and `append_text` use binary byte-preserving I/O
and return `Result`. `file.exists` returns `Bool`; `file.create_directory`
creates missing parent directories and returns `Result[Bool, String]`;
`file.remove` returns `Result[Bool, String]`; `file.list` returns a lexically sorted
`Result[Array[String], String]` containing entry names.

`file.read_binary(path)` returns `Result[ByteBuffer, String]` without text
decoding. `file.write_binary(path, buffer)` replaces a file and
`file.append_binary(path, buffer)` appends to it; both return
`Result[Bool, String]`. All file functions are synchronous and may block the
calling thread. Operating-system errors are recoverable `Err(String)` values.

Paths are UTF-8 at the Rocket boundary. `path.join`, `basename`, `extension`,
and `normalize` perform lexical platform-native operations. They do not touch
the filesystem or resolve symbolic links.

## `std.json`

`json.parse(text)` returns `Result[std.json.Json, String]`. `Json` is a public
built-in enum, so ordinary exhaustive `match` can inspect its values:

```rocket
enum Json:
    Null
    Boolean(Bool)
    Integer(Int)
    Decimal(Float)
    Text(String)
    List(Array[Json])
    Object(Array[JsonField])

struct JsonField:
    key: String
    value: Json
```

The parser accepts RFC 8259 strings, escapes, surrogate pairs, numbers, arrays,
and objects up to 256 nested levels. Integers that fit Rocket `Int` remain
`Integer`; other finite JSON numbers become `Decimal`. Object field order and
duplicates are preserved. `json.stringify(value)` emits compact JSON.

## `std.csv`

`csv.parse(text)` returns `Result[Array[Array[String]], String]` and supports
quoted fields, escaped double quotes, commas, and CRLF/LF records.
`csv.encode(rows)` applies quoting when needed and emits CRLF records.

## `std.random`

`random.seed(value)` starts a reproducible process-local sequence.
`random.int(minimum, maximum)` samples uniformly from the half-open range
`[minimum, maximum)` using rejection sampling. `random.float()` returns a value
in `[0.0, 1.0)`. Phase 7 randomness is deterministic and not cryptographic.

## `std.process`

`process.run(program, arguments)` starts the program directly, waits for it,
and returns `Result[Int, String]` containing the exit code. It never invokes a
command shell or parses a command string. `process.arguments()` returns an
`Array[String]` containing arguments after the executable name in original
order. `process.executable_path()` returns the normalized executable path as
`Result[String, String]`. `process.environment(name)` returns `Option[String]`;
`process.working_directory()` returns `Result[String, String]`.

## `std.time`

`time.unix_milliseconds()` reads wall-clock milliseconds since the Unix epoch.
`time.monotonic_milliseconds()` reads a non-decreasing process clock suitable
for durations. `time.sleep_milliseconds(value)` blocks for a non-negative
duration and returns `Unit`.

## Ownership and compatibility

Returned strings, arrays, JSON values, and errors are ordinary ARC-managed
Rocket values. LLVM calls versioned C ABI entry points; the permanent C++ Stage
0 backend provides equivalent RAII implementations. The same Phase 7 program
is compiled and run by both backends in the test workflow.
