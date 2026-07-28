# Rocket Standard Library - Draft 0.7

The Phase 7 library is a set of built-in source modules. Import a module by its
stable name; no package file or downloaded dependency is required:

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
| `parse_int(value: String)` | `Result[Int, String]` | Parse an entire base-10 signed integer |
| `from_int(value: Int)` | `String` | Format a base-10 signed integer |

An empty split delimiter returns an array containing the original value.

## `std.collections`

`length[T](values: Array[T])` and `slice_length[T](values: Slice[T])` return an
`Int`. `reverse[T](values: Array[T])` returns a new array. `join` combines an
`Array[String]` with a separator. All operations preserve their inputs.

## `std.file` and `std.path`

`file.read_text`, `write_text`, and `append_text` use binary byte-preserving I/O
and return `Result`. `file.exists` returns `Bool`; `file.remove` returns
`Result[Bool, String]`; `file.list` returns a lexically sorted
`Result[Array[String], String]` containing entry names.

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
command shell or parses a command string. `process.environment(name)` returns
`Option[String]`; `process.working_directory()` returns `Result[String, String]`.

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
