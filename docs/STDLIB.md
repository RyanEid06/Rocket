# Rocket Standard Library 2.1 + Rocket 3.0 Wave B

The stable library combines typed host-backed modules with bundled ordinary
Rocket source modules, including the Rocket 1.2 dot-call additions, the
unchanged Rocket 1.3 native boundary, the completed Rocket 1.5 production
library, and Rocket 1.8 concurrency and asynchronous I/O. Rocket 2.0 freezes
these signatures and their runtime ABI v1 mappings. Import a module by
its stable name; no downloaded dependency is
required:

Rocket 3 Wave B adds the callable ergonomics and the bundled graphics/UI
modules described below without changing runtime ABI v1. Wave C themes,
controls, containers, and cache calibration are not yet accepted. The current
copyable index is `ROCKET_3_0_SYNTAX_DICTIONARY.md`; the complete file
disposition is in `DOCUMENTATION_STATUS.md`.

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

Every parameter name shown in this inventory is a stable source-compatibility
commitment and may be used in named calls. Module-function and dot-call forms
share the same compiler-owned names; a dot-call receiver supplies the first
listed parameter before any written named operands. Stage0, the self-hosted
compiler, signature help, and generated documentation use one matching metadata
inventory. Changing a public standard-library parameter name is therefore a
source-versioning event even though runtime ABI v1 remains unchanged.

## `std.math` (Rocket 3 Wave B)

`std.math` is a compiler-owned module with no graphics policy. The accepted
surface includes `pi()`, `tau()`, `e()`; Float `abs`, `min`, `max`, `clamp`, and
`sign`; Int `abs_int`, `min_int`, `max_int`, `clamp_int`, and `sign_int`;
`floor`, `ceil`, `round`, `trunc`, `fract`, `sqrt`, `pow`, `exp`, `log`,
`log10`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, and `atan2`; conversion
and interpolation helpers `radians`, `degrees`, `lerp`, `inverse_lerp`,
`remap`, `smoothstep`, and `smootherstep`; and scalar movement helpers
`approach` and `move_towards`. Float operations follow the documented
IEEE-754 NaN, infinity, and signed-zero behavior; interpolation does not
implicitly clamp intermediate progress.

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
Rocket 1.2 adds dot-call convenience without changing
representation. Examples include `values.length()`, `values.append(value)`,
`map.get(key)`, `set.contains(value)`, and `text.trim()`. `Slice[T]` currently
supports `length()`; Array-only operations remain unavailable on Slice. Static
String construction may use `String.from_int(value)`, while
`std.string.from_int(value)` remains valid. These aliases resolve to the same
standard intrinsics and do not add runtime entry points.

## `std.binary` (Rocket 1.5)

`std.collections.ByteBuffer` is an immutable wrapper around `Array[Char]` and
stores arbitrary bytes, including zero and invalid UTF-8. Binary operations
never panic for data-dependent bounds, encoding, or numeric-range failures;
they return `Result[..., String]`.

| Function | Result | Meaning |
| --- | --- | --- |
| `from_string(value: String)` | `ByteBuffer` | Copy the complete UTF-8 byte sequence, including embedded zero bytes |
| `to_string(buffer: ByteBuffer)` | `Result[String, String]` | Validate UTF-8 and copy it into an immutable String |
| `length(buffer: ByteBuffer)` | `Int` | Return the byte count |
| `concat(left: ByteBuffer, right: ByteBuffer)` | `ByteBuffer` | Return the bytes from both inputs in order |
| `slice(buffer: ByteBuffer, offset: Int, length: Int)` | `Result[ByteBuffer, String]` | Copy a checked byte range |
| `read_u8(buffer: ByteBuffer, offset: Int)` | `Result[Int, String]` | Read one unsigned byte |
| `read_u16_le(buffer: ByteBuffer, offset: Int)` | `Result[Int, String]` | Read a little-endian unsigned 16-bit integer |
| `read_u32_le(buffer: ByteBuffer, offset: Int)` | `Result[Int, String]` | Read a little-endian unsigned 32-bit integer |
| `read_u16_be(buffer: ByteBuffer, offset: Int)` | `Result[Int, String]` | Read a big-endian unsigned 16-bit integer |
| `read_u32_be(buffer: ByteBuffer, offset: Int)` | `Result[Int, String]` | Read a big-endian unsigned 32-bit integer |
| `write_u8(value: Int)` | `Result[ByteBuffer, String]` | Encode an unsigned 8-bit integer |
| `write_u16_le(value: Int)` | `Result[ByteBuffer, String]` | Encode an unsigned 16-bit integer |
| `write_u32_le(value: Int)` | `Result[ByteBuffer, String]` | Encode an unsigned 32-bit integer |
| `write_u16_be(value: Int)` | `Result[ByteBuffer, String]` | Encode an unsigned 16-bit integer in network byte order |
| `write_u32_be(value: Int)` | `Result[ByteBuffer, String]` | Encode an unsigned 32-bit integer in network byte order |

Offsets are zero-based. Reads reject negative or truncated ranges. Writes
reject negative values and values outside the selected unsigned width. The
byte-order spelling is explicit so serialized data is independent of the host
CPU. `concat` and every encoder return new immutable snapshots. These functions
are synchronous CPU operations and do not retain hidden mutable cursor state.

## `std.stream`

| Function | Result | Meaning |
| --- | --- | --- |
| `open_reader(path: String, buffer_size: Int)` | `Result[Int, String]` | Open a buffered binary reader and return an opaque process-local token |
| `read(handle: Int, maximum: Int)` | `Result[ByteBuffer, String]` | Read up to `maximum` bytes; an empty buffer is end-of-file |
| `close_reader(handle: Int)` | `Result[Bool, String]` | Close a reader token exactly once |
| `open_writer(path: String, buffer_size: Int, append: Bool)` | `Result[Int, String]` | Open a buffered binary writer in replace or append mode |
| `write(handle: Int, buffer: ByteBuffer)` | `Result[Bool, String]` | Write all bytes or return an operating-system error |
| `flush(handle: Int)` | `Result[Bool, String]` | Flush buffered writer bytes to the host |
| `close_writer(handle: Int)` | `Result[Bool, String]` | Flush and close a writer token exactly once |

Handles are opaque positive integers scoped to the current process. A token of
the wrong kind, or any use after close, returns `Err`. Buffer sizes must be from
256 bytes through 16 MiB, and one `read` request is capped at 64 MiB. Stream
operations are synchronous and may block the calling thread; they do not claim
asynchronous I/O or cancellation support.

## `std.unicode`

| Function | Result | Meaning |
| --- | --- | --- |
| `scalar_count(value: String)` | `Int` | Count Unicode scalar values in valid UTF-8 text |
| `scalar_at(value: String, index: Int)` | `Result[Int, String]` | Return a scalar value by scalar index |
| `from_scalar(value: Int)` | `Result[String, String]` | Encode a Unicode scalar as UTF-8 |
| `normalize_nfc(value: String)` | `Result[String, String]` | Normalize with the host Unicode NFC tables |
| `normalize_nfd(value: String)` | `Result[String, String]` | Normalize with the host Unicode NFD tables |
| `grapheme_count(value: String)` | `Int` | Count practical extended grapheme clusters |
| `grapheme_at(value: String, index: Int)` | `Result[String, String]` | Copy one practical grapheme cluster |

Strings remain UTF-8 and byte-indexed by default. Scalar APIs reject surrogate
code points and values above `U+10FFFF`. Normalization is explicit rather than
silently changing equality, hashing, paths, or identifiers. The practical
grapheme boundary layer keeps combining marks, variation selectors, emoji skin
tones, zero-width-joiner sequences, and regional-indicator pairs together. It
does not promise locale-sensitive segmentation or a permanently frozen Unicode
version.

## `std.regex`

`is_match(pattern, value)`, `find_all(pattern, value)`, and
`replace_all(pattern, value, replacement)` use Rocket's bounded Thompson-NFA
engine. The documented grammar supports literals, `.`, `^`, `$`, grouping,
alternation, `*`, `+`, `?`, byte classes and ranges, negated classes, and
`\\d`/`\\w`/`\\s` plus escaped literal bytes. Replacement text is literal and
does not expand captures. Matching uses a bounded tagged Thompson simulation
with at most one earliest start per compiled state; it does not use recursive
backtracking. Group nesting is capped at 256. Patterns are capped at 4096 bytes,
inputs and replacements at 16 MiB, compiled programs at 16384 states, matches at
one million, and replacement output at 64 MiB. Every limit returns `Err`.

Matching operates on UTF-8 bytes, so callers should normalize first when
canonical equivalence is required. Quantifiers return the leftmost-longest
whole match. Calls are synchronous and safe for untrusted patterns within the
documented resource bounds.

## `std.crypto`

| Function | Result | Meaning |
| --- | --- | --- |
| `secure_bytes(length: Int)` | `Result[ByteBuffer, String]` | Read bytes from the target system cryptographic provider (Windows CNG or OpenSSL-backed POSIX provider) |
| `secure_int(minimum: Int, maximum: Int)` | `Result[Int, String]` | Uniformly sample the inclusive range with rejection sampling |
| `sha256(value: ByteBuffer)` | `Result[String, String]` | Return a lowercase 64-digit SHA-256 digest |
| `hmac_sha256(key: ByteBuffer, value: ByteBuffer)` | `Result[String, String]` | Return a lowercase HMAC-SHA-256 digest |
| `constant_time_equal(left: ByteBuffer, right: ByteBuffer)` | `Bool` | Compare secret byte strings without data-dependent early exit |
| `verify_signed_file(path: String)` | `Result[Bool, String]` | Validate a target-native signing policy where available; Windows uses Authenticode |

Random byte requests and hash inputs are capped at 64 MiB; HMAC keys are capped
at 1 MiB and must not be empty. These cryptographic APIs use the target system
provider rather than the deterministic `std.random` generator. On Windows,
`verify_signed_file` delegates signature, certificate-chain, and revocation
policy to WinVerifyTrust with no UI and cache-only URL retrieval. POSIX hosts
return an explicit `Err` when no equivalent platform verification policy is
available. `Ok(false)` means a file was inspected but is not trusted; malformed
paths and unavailable platform services are `Err`.

## `std.net` and `std.http`

`net.resolve(host, service)` performs DNS/service resolution and returns unique
numeric addresses. `tcp_connect(host, port, timeout_ms)` and
`tcp_listen(address, port, backlog)` return opaque process-local tokens;
`accept(listener, timeout_ms)` returns a connection token. Port zero requests an
ephemeral listener, whose assigned port is available through `local_port`.
`send(handle, bytes, timeout_ms)` writes the whole buffer, while
`receive(handle, maximum, timeout_ms)` reads up to the requested bound. An empty
buffer means orderly peer shutdown. `close` and `cancel` both invalidate a token
exactly once; cancellation is implemented by closing the socket so any host I/O
waiting on that socket is interrupted.

Every socket timeout is explicit, from zero through one hour. Individual sends
and receives are capped at 64 MiB; a whole-buffer send shares one deadline
across its partial writes. Calls are synchronous. DNS may invoke the host
resolver outside the socket deadline and therefore may use configured network
services; no name-resolution result is cached by Rocket.

`http.request(method, url, body, timeout_ms)` returns
`Result[std.http.Response, String]`, where `Response` contains `status: Int` and
`body: ByteBuffer`. It accepts absolute HTTP and HTTPS URLs, uses WinHTTP on
Windows and the pinned/libcurl provider on Linux/macOS, retains normal target
certificate validation, and exposes no insecure certificate bypass. Both
request and response bodies are capped at 64 MiB. Each provider applies the
documented timeout to its resolution, connection, send, and receive phases.

For servers, `http.read_request(connection, maximum, timeout_ms)` returns a
bounded `std.http.Request` containing `method`, `path`, and binary `body`.
`write_response(connection, status, content_type, body, timeout_ms)` writes a
complete HTTP/1.1 response with `Content-Length` and `Connection: close`.
The intentionally small server foundation accepts HTTP/1.x requests with a
single `Content-Length`; it rejects chunked bodies, malformed request lines,
header injection, and over-limit bodies. Applications close the connection and
listener explicitly.

## `std.datetime`

`format_utc(unix_ms)` and `parse_utc(text)` round-trip the fixed, locale-neutral
`YYYY-MM-DDTHH:MM:SS.mmmZ` form. `days_in_month(year, month)` and
`weekday(year, month, day)` validate Gregorian dates from year 1 through 9999;
weekday zero is Sunday. `local_offset_minutes(unix_ms)` applies the target
historical daylight/time-zone rules for that instant, and `timezone_name()`
returns the configured target time-zone identifier. Calendar and time-zone
failures are `Err`, never silently clamped. These calls do not change process
locale or time-zone settings.

## `std.log`, `std.cli`, and `std.config`

`log.write(level, message)` writes one timestamped UTC record to standard error;
`log.append(path, level, message)` appends and flushes one record to a file.
Levels are `trace`, `debug`, `info`, `warn`, `error`, and `fatal`. Embedded CR/LF
bytes are escaped to prevent record injection, messages are capped at 1 MiB,
and same-process writers are serialized. File logging is synchronous and may
block.

`cli.has_flag(arguments, "--name")` stops at `--`. `cli.option` accepts either
`--name=value` or `--name value` and returns `Result[Option[String], String]` so
a missing value differs from an absent option. `cli.positionals` includes
non-option arguments and every argument following `--`; callers should use the
separator when positional values begin with `-`.

`config.get(text, dotted_key)` and `config.load(path, dotted_key)` parse a
bounded deterministic configuration subset: `key = value`, quoted strings with
basic escapes, `#` comments, and `[section]` names that qualify keys. Values are
returned as text through `Result[Option[String], String]`; typed interpretation
is explicit at the call site. Duplicate keys, malformed syntax, lines above 64
KiB, and documents above 1 MiB are rejected. `load` performs synchronous file
I/O.

## `std.compression` and `std.archive`

`compression.xpress_compress(bytes)` and `xpress_decompress(bytes)` use the
portable deterministic XPRESS Huffman implementation (compatible with the
Windows format). Both directions return
`Result[ByteBuffer, String]`, reject invalid streams, and cap uncompressed data
at 64 MiB. XPRESS bytes are a Windows interoperability format, not ZIP or gzip;
callers must label stored data accordingly.

`archive.tar_create(path, names, contents)` writes deterministic ustar archives:
regular files only, mode `0644`, zero owner/time metadata, insertion order, and
two end blocks. `tar_list` and `tar_read` validate every header and checksum
before returning data. Archives allow at most 1024 entries and 64 MiB of content
within a 128 MiB file. Absolute paths, drive names, backslashes, empty segments,
`.`/`..`, duplicate names, links, devices, and non-ustar input are rejected, so
the API never extracts host paths as a side effect. All archive I/O is
synchronous.

## `std.sqlite`

`sqlite.open(path)` opens the target SQLite provider in read/write, create,
full-mutex mode and applies a five-second busy timeout. `:memory:` is
supported for isolated tests. `execute(handle, sql, parameters)` returns the
changed-row count; `query` returns rows as `Array[Array[String]]`; `close`
invalidates the opaque token exactly once.

Every call prepares exactly one statement. All values are bound through `?`
parameters as copied UTF-8 text; callers must not concatenate untrusted values
into SQL. SQL text is capped at 1 MiB, parameter count at 1024, individual
parameters at 16 MiB, results at 256 columns, 100000 rows, and 64 MiB of text.
SQLite `NULL` is currently returned as an empty String, so schemas that need to
distinguish the two should encode nullability explicitly. Query values that
cannot be represented as valid UTF-8 Rocket Strings are rejected. Database
calls are synchronous, can wait up to the configured busy timeout, and return
all engine errors through `Result`.

## `std.testing`

`std.testing` is shipped as an ordinary Rocket source module over the private
`std.testing_core` host boundary. It provides `assert`, `equal_int`, and
`equal_string`, each returning `Result[Bool, String]` so a test can propagate a
precise failure. `temp_directory(prefix)` creates an unpredictable directory in
the host temporary root; `fixture_path(root, relative)` only accepts safe paths
under a directory created by that process; and `cleanup_temp` recursively
removes only a registered temporary root exactly once.

`coverage_hit(name)` increments a bounded named hook, and
`coverage_write(path)` writes deterministic versioned JSON in lexical point
order. These hooks support libraries and the future automatic instrumentation
pass without hard-coding a vendor report format into source semantics.

The package test runner accepts `--filter text`. Test files ending
`.xfail.rocket` are expected failures: a nonzero exit is `XFAIL`, while a zero
exit is the suite-failing `XPASS`. Every other `.rocket` file must exit zero.
Temporary resources, filters, assertions, fixtures, expected failures, and
coverage hooks therefore share the native and self-hosted toolchains.

## `rocket.motion`

`rocket.motion` is an ordinary bundled Rocket source module at
`stdlib/rocket/motion.rocket`; import it with `import rocket.motion`. It keeps
motion descriptions and returned samples value-owned, so application code owns
the state it advances and no arbitrary object mutation or reflection is used.

`Vec2`, `Color`, `Easing`, and `MotionPolicy` are public value types. The
module exposes `linear`, plus `in_`, `out_`, and `in_out_` variants for Quad,
Cubic, Quart, Sine, Back, Bounce, and Elastic. Every easing preserves exact
`0.0` and `1.0` endpoints without clamping other progress values; Back and
Elastic intentionally overshoot, and Bounce uses deterministic piecewise
segments. Use the corresponding `*_easing()` constructor with
`apply_easing` when storing an easing choice.

`FloatTween`, `Vec2Tween`, and `ColorTween` are created by `float_tween`,
`vec2_tween`, and `color_tween`, then sampled with `sample_float`,
`sample_vec2`, or `sample_color`. Zero or negative duration samples the final
value. A reduced policy snaps nonessential work to its final sample; essential
work may continue only when `allow_essential` is true. The sample result
records value, progress, completion, and whether it snapped.

`delay`, `sequence`, `parallel`, `repeat`, `yoyo`, `cancel`, and
`sample_timeline` provide immutable timeline descriptions. Nonpositive repeat
counts and nonpositive total duration complete immediately; a large elapsed
delta completes at the final state; cancellation returns a completed,
cancelled sample. `fade`, `move`, `slide`, `scale`, `rotate`, `pulse`, and
`color_transition` are convenience constructors over those tween types.

## `rocket.raylib.safe`

`rocket.raylib.safe` is the narrow bundled Rocket 3 native boundary over the
reviewed raylib adapter. Application code should normally use `rocket.graphics`
instead. The safe module owns the `Window` and `Frame` token wrappers, validates
primitive geometry and color arguments before an `unsafe` adapter call, and
translates adapter status values to `Result`. It exposes no native pointer or
raylib structure. Pointer queries use framebuffer coordinates and provide
pressed, down, and released states. `key_pressed` and `key_down` expose reviewed
integer key queries for higher-level UI input snapshots. F17 also uses this boundary
for checked render-target/scissor scopes, framebuffer/DPI/display-revision queries,
resize/fullscreen/borderless transitions, and render-target PNG export; those
primitives remain backend plumbing rather than the preferred application API.

## `rocket.graphics`

`rocket.graphics` is an ordinary bundled Rocket source module at
`stdlib/rocket/graphics.rocket`; import it with `import rocket.graphics`. It
defines public graphics-facing value types without exposing renderer handles:
`Vec2`, `Size`, `Rect`, `Transform2D`, `Color`, `TextStyle`, `TextMetrics`,
`TextLayout`, `WindowConfig`, and `VirtualCanvas`.

Constructors are ordinary functions with stable parameter names:
`vec2(x, y)`, `size(width, height)`, `rect(x, y, width, height)`,
`transform2d(translation, rotation, scale, pivot)`, `text_style(font_name,
size, letter_spacing = 0.0, line_height = 1.0)`, `text_metrics(width, height,
baseline, line_height)`, `window_config(width, height, title, high_dpi = true,
msaa4x = true, resizable = true)`, and the raw
`virtual_canvas(logical_size, viewport, scale)` value constructor. Prefer
`fit_virtual_canvas(logical_size, framebuffer_size)` when establishing a real
mapping: it validates positive finite dimensions, preserves aspect ratio,
centers the viewport, and returns a recoverable `Err` instead of dividing by
zero for invalid dimensions.

Typography uses immutable `TextStyle` values and actual selected-font metrics
from the safe raylib boundary. `TextLayout` combines `TextMetrics`, measured
bounds, line count, and explicit clipped/ellipsized flags. Horizontal alignment
is left, center, or right; vertical alignment is top, middle, baseline, or
bottom. `text_overflow_clip()` and `text_overflow_ellipsis()` select the bounded
container policy. Font selection is explicit: applications load or select a
`Font` token and choose another font resource for another weight; Rocket never
synthesizes guessed bold metrics. `TextStyle.font_name` is the stable
application/asset identity paired with that token.

The safe raylib package provides `default_font`, `measure_text`,
`draw_text_layout`, and `invalidate_font_measurements`. Measurement includes
requested size, letter spacing, line-height multiplier, explicit newlines,
optional word wrapping, maximum width/height, clipping, and last-visible-line
ellipsis. Zero maximum width or height means unbounded measurement; draw bounds
must be positive. Baseline alignment treats the bounds' `y` coordinate as the
baseline anchor. A 256-entry deterministic LRU measurement cache keys the font
token, complete UTF-8 text, and every style/layout input. Style changes cannot
return stale metrics; explicit invalidation and font unload remove only that
font's cached entries.

Geometry helpers include `vec2_add`, `vec2_subtract`, `vec2_scale`,
`vec2_is_finite`, `size_is_valid`, `rect_is_finite`,
`rect_has_nonnegative_size`, `rect_normalized`, `rect_translate`,
`rect_scale`, `rect_contains`, `rect_intersects`, `rect_intersection`, and
`transform_point`. Rectangle containment includes the boundary; invalid or
non-finite rectangles and points return false from predicate helpers.
`transform_point` scales around the pivot, rotates counterclockwise in radians,
then translates. `virtual_canvas_is_valid` validates a mapping before native
integration. `virtual_canvas_logical_to_physical` and
`virtual_canvas_physical_to_logical` use the same viewport/scale contract;
physical-to-logical conversion returns `None` outside the viewport and treats
right/bottom edges as exclusive. `virtual_canvas_rect_to_physical` applies that
same transform to logical clipping rectangles.

Color helpers include `color_from_rgb`, `color_from_rgba(alpha = 1.0)`,
`color_from_hex`, `color_from_hsv(alpha = 1.0)`, `color_with_alpha`,
`color_mix`, `color_lerp`, `color_lighten`, `color_darken`,
`color_saturate`, and `color_desaturate`. RGB byte inputs and HSV saturation,
value, and alpha channels clamp deterministically to `0.0..1.0`; hue wraps;
malformed hex returns `Result.Err` with a stable message. Hex accepts `RRGGBB`
and `RRGGBBAA`, each with an optional leading `#`; six-digit input defaults
alpha to `1.0`.

`rocket.graphics.shapes` owns Rocket-facing drawing. Its functions take typed
`rocket.graphics.Vec2`, `Rect`, and `Color` values and delegate through
`rocket.raylib.safe`: `draw_rect`, `draw_rect_outline`,
`draw_rounded_rect`, `draw_rounded_rect_outline`, `draw_circle`,
`draw_circle_outline`, `draw_ellipse`, `draw_ring`, `draw_ring_sector`,
`draw_sector`, `draw_line`, `draw_thick_line`, `draw_triangle`,
`draw_triangle_outline`, `draw_polygon`, `draw_polygon_outline`, `draw_bezier`,
`draw_gradient_rect`, and `draw_gradient_circle`. Outline/thick-line/Bezier
operations default to thickness `1.0`; rounded rectangles default to roundness
`0.25`; polygons default to rotation `0.0`; rectangle gradients default to
vertical. Invalid or non-finite public geometry returns `Err` before crossing
the native boundary.

The pure `rocket.graphics` helpers `point_in_rect(point, bounds)` and
`point_in_circle(point, center, radius)` provide public hit testing. Rectangle
bounds are half-open: left/top are included and right/bottom are excluded;
zero-area rectangles never contain a point. Circle containment includes the
circumference and rejects negative or non-finite radii.

`rocket.graphics.input` owns native pointer mapping. `pointer_position(window)`
reports physical framebuffer coordinates, while `pointer_down`,
`pointer_pressed`, and `pointer_released` default to button `0`.
`pointer_position_in_canvas` converts
a physical pointer through a `VirtualCanvas` viewport and scale. It returns
`None` for invalid canvas data, letterbox/pillarbox space, and the exclusive
right/bottom viewport edges, so outside input cannot be mistaken for logical UI
input.

## `rocket.graphics.canvas`

`rocket.graphics.canvas` integrates the pure `VirtualCanvas` mapping with live
rendering and display state through `rocket.raylib.safe`. `framebuffer_size`,
`from_window`, and `refresh` fit the logical design size to the current physical
framebuffer, while `dpi_scale` and `display_revision` expose the values needed
to detect DPI/monitor/display changes without inventing a second mapping rule.

`create_target`, `begin_target`, `end_target`, and `unload_target` manage the
logical-size render target. `present(frame, target, mapping)` requires the
render-target size to match the logical canvas size, vertically corrects the
raylib render texture, and composites it into exactly the fitted viewport.
`begin_clip` maps logical clip bounds through the same transform, intersects
them with the viewport, and returns a checked scissor scope; `end_clip` closes
that scope. `save_logical_screenshot` exports the render target itself, so the
PNG contains logical content rather than framebuffer letterbox/pillarbox bars.

`resize`, `set_fullscreen`, and `set_borderless` first request the checked
window transition and then recompute the mapping from the resulting framebuffer.
Unsupported transitions return the adapter's stable recoverable error. Callers
should also use `refresh` whenever `display_revision` changes because of a DPI,
monitor, or external resize event. Across pointer mapping, clipping,
presentation, screenshots, resize/fullscreen, and DPI changes, the fitted
viewport and its half-open outside rule are the single coordinate contract.

## `rocket.ui`

`rocket.ui` is the Rocket 3 immediate-mode context foundation. `new_context`
creates a bounded, namespaced state store; `begin_frame(context, window, canvas)`
opens exactly one `UiFrame` and snapshots pointer and common keyboard state once
through `rocket.graphics.input` and `rocket.raylib.safe`. `end_frame` closes the
frame, evicts widget IDs not seen in that frame, and clears stale active, focused,
or modal state. Nested frames, closed-frame use, duplicate IDs, over-capacity
registration, invalid bounds, and unclosed modal scopes return recoverable
contract errors. `Context` and `UiFrame` are single-thread-confined UI state;
their atomic lease rejects stale copied values but is not a cross-thread
synchronization contract.

`widget_id` and `child_id` use deterministic length-prefixed composition and keep
both the composed path and hash, so equal hashes do not collapse distinct widget
identities. `register_widget` records an ID once per frame. `interact` centralizes
half-open logical-canvas hit testing, pointer press/hold/release activation,
focused Space/Enter activation, disabled behavior, and modal capture.
`request_focus`, `activate_modal`, `enter_modal`, `exit_modal`, and `clear_modal`
make focus and modal ownership explicit. `Response` records hover/active/click/
focus/disabled/modal and activation facts for the frame that produced it;
`response_is_current` and `response_is_current_context` reject stale responses.
Keyboard helpers expose activation, cancel, focus-next, and directional snapshots
without rereading native input during widget evaluation. Themes and styles are
provided by `rocket.ui.theme` and `rocket.ui.styles`; concrete controls remain a
later `rocket.ui` layer. Layout is provided by `rocket.ui.layout`.

## `rocket.ui.layout`

`rocket.ui.layout` is the pure logical-coordinate layout layer for Rocket 3 UI.
It consumes `rocket.graphics.Rect`/`Size` values and returns rectangles without
owning renderer, window, font, or asset state. `row`, `column`, `grid`, `stack`,
and `anchor_rect` cover the public Row/Column/Grid/Stack/Anchor model. Every
`LayoutItem` explicitly supplies horizontal and vertical `Sizing`, measured
content size, margin `Insets`, and an `Anchor`, so an item cannot silently inherit
an unspecified sizing policy.

Sizing constructors are `fixed(value)`, `fill()`, `content()`, and
`percent(fraction)`. Percentages are relative to the containing logical extent
and must lie in `[0, 1]`; content sizing uses the caller-provided measured content
size. `horizontal_start`/`horizontal_center`/`horizontal_end` and their vertical
counterparts make alignment explicit. The nine anchor constructors are
`top_left`, `top_center`, `top_right`, `center_left`, `center`, `center_right`,
`bottom_left`, `bottom_center`, and `bottom_right`.

`Insets` are used for padding and per-item margins. `SafeArea` is explicit:
`no_safe_area()` applies no platform inset, while `safe_area(insets)` composes the
provided safe-area inset before ordinary padding. Row and Column gaps are charged
between children; Grid has independent column and row gaps. Fill children divide
the primary-axis remainder deterministically after fixed/content/percentage
sizes, margins, and gaps have been accounted for. Stack and Anchor place each
item within its margin-reduced logical region using its anchor.

All public layout entry points return `Result`. Non-finite or negative bounds,
insets, gaps, fixed/content sizes, invalid percentage or alignment/sizing modes,
excessive grid dimensions/item counts, cross-axis overflow, grid-capacity
overflow, and overfull primary axes return stable `Err(String)` diagnostics.
Padding/safe-area/margin insets that consume more space than their containing
rectangle are errors rather than silently clamped. Sparse/empty containers are
well-defined: empty Row/Column/Stack return an empty rectangle array, and Grid
may contain fewer items than cells. The API therefore makes ordinary
underspecified child sizing unrepresentable while still diagnosing malformed raw
policy values deterministically.

## `rocket.ui.theme`

`rocket.ui.theme` centralizes immutable semantic tokens without owning renderer,
widget, or native state. `ColorTokens` covers background, surface, raised
surface, table treatment, action states, primary/muted text, success, warning,
error, border, and focus colors. `SpacingTokens`, `RadiusTokens`,
`TypographyTokens`, and `MotionTokens` provide the corresponding numeric scales;
`Theme` groups all five token families.

`color_tokens` creates an explicit semantic palette. `spacing_tokens`,
`radius_tokens`, `typography_tokens`, and `motion_tokens` expose source-stable
named parameters with practical defaults. `theme` groups explicit token sets and
`dark_theme` supplies the bundled neutral dark/table palette. `theme_is_valid`
rejects non-finite, negative, out-of-range color, and non-positive typography
values even when callers construct public structs directly.

## `rocket.ui.styles`

`rocket.ui.styles` provides value-owned `TextStyle`, `BorderStyle`,
`ShadowStyle`, `ImageStyle`, `PanelStyle`, and `ButtonStyle` data. `StyleSet`
groups those objects for a single visual state and `StyleStates` stores normal,
hovered, pressed, disabled, and focused sets. The constructors expose explicit
data plus defaults for optional width, radius, blur, opacity, and padding; the
validation predicates reject invalid colors, non-finite or negative dimensions,
non-positive text sizes, and opacity outside `[0, 1]`.

`default_styles(theme)` maps the selected semantic tokens into a complete state
set. `control_state` describes current interaction flags and `resolve` selects a
style deterministically with priority `disabled > pressed > hovered > focused >
normal`. This priority prevents a disabled control from inheriting hover or
press visuals and keeps drawing behavior out of the style layer. Controls,
containers, renderer conversion, retained caches, and application inheritance
remain outside these modules.

## Typed asset-store reference package

The accepted Wave B typed asset store is currently an ordinary package module
at `examples/raylib_showcase/src/rocket_assets.rocket`, imported as
`src.rocket_assets`. It exposes `AssetStore`, `TextureRef`, `FontRef`,
`SoundRef`, `MusicRef`, and `ShaderRef`, with typed load, lookup, borrow, and
idempotent `cleanup` operations. Paths are rooted to the package, traversal and
symlink escapes are rejected, duplicate names and wrong-type lookups return
recoverable errors, and cleanup invalidates borrowed references. The planned
`rocket.assets` / `stdlib/rocket/assets` namespace in the Rocket 3 design is not
an available standard-library module in this checkout.

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

## `std.target` (Rocket 2.1)

`target.alias()`, `target.triple()`, `target.os()`,
`target.architecture()`, `target.environment()`, and `target.endianness()`
return immutable compiler-provided strings for the selected compilation target.
`target.pointer_width()` returns `64` for every Rocket 2.1 production target.
`target.has_feature(name)` recognizes the stable baseline feature vocabulary in
`TARGETS.md` and returns `false` for an unknown well-formed name. These calls do
not inspect the running host or environment and are constant-foldable.

## Ownership and compatibility

Returned strings, arrays, JSON values, and errors are ordinary ARC-managed
Rocket values. LLVM calls versioned C ABI entry points; the permanent C++ Stage
0 backend provides equivalent RAII implementations. The same Phase 7 program
is compiled and run by both backends in the test workflow.

## Rocket 1.8 ownership and concurrency modules

The detailed type, ordering, cancellation, publication, and memory-ordering
rules are normative in `CONCURRENCY.md`. Every deadline below is an absolute
monotonic `Int`; every cancellation argument is
`std.cancel.CancellationToken`.

### `std.ownership` and `std.buffer`

| Function | Result |
| --- | --- |
| `downgrade[T](value: T)` | `Weak[T]` |
| `upgrade[T](value: Weak[T])` | `Option[T]` |
| `expired[T](value: Weak[T])` | `Bool` |
| `thaw[T](values: Array[T])` | `UniqueBuffer[T]` |
| `length[T](buffer: UniqueBuffer[T])` | `Int` |
| `capacity[T](buffer: UniqueBuffer[T])` | `Int` |
| `get[T](buffer: UniqueBuffer[T], index: Int)` | `T` |
| `set[T](buffer: UniqueBuffer[T], index: Int, value: T)` | `UniqueBuffer[T]` |
| `append[T](buffer: UniqueBuffer[T], value: T)` | `UniqueBuffer[T]` |
| `slice[T](buffer: UniqueBuffer[T], start: Int, end: Int)` | `UniqueBuffer[T]` |
| `freeze[T](buffer: UniqueBuffer[T])` | `Array[T]` |

Buffer `length`, `capacity`, and `get` borrow. `set`, `append`, `slice`, and
`freeze` consume their buffer argument. No writable slice aliases the storage.

### `std.cancel` and `std.async_time`

| Function | Result |
| --- | --- |
| `token()` | `CancellationToken` |
| `child(parent: CancellationToken)` | `CancellationToken` |
| `current()` | `CancellationToken` |
| `cancel(token: CancellationToken)` | `Bool` |
| `is_cancelled(token: CancellationToken)` | `Bool` |
| `check(token: CancellationToken)` | `Result[Bool, String]` |
| `deadline_after(milliseconds: Int)` | `Result[Int, String]` |
| `remaining(deadline: Int)` | `Int` |
| `sleep(milliseconds: Int, token: CancellationToken)` | `Task[Bool]` |
| `sleep_until(deadline: Int, token: CancellationToken)` | `Task[Bool]` |

Deadlines are monotonic `Int` values. Timer tasks use target-native waits with
bounded cancellation observation on the default executor and observe both their
explicit token and task token.

### `std.thread` and `std.task`

| Function | Result |
| --- | --- |
| `thread.spawn[T](task: Task[T])` | `Result[Thread[T], String]` |
| `thread.join[T](thread: Thread[T])` | `Result[T, String]` |
| `thread.detach[T](thread: Thread[T])` | `Result[Bool, String]` |
| `thread.is_complete[T](thread: Thread[T])` | `Bool` |
| `task.join[T](task: Task[T])` | `Result[T, String]` |
| `task.cancel[T](task: Task[T])` | `Bool` |
| `task.is_complete[T](task: Task[T])` | `Bool` |
| `task.group[T](tasks: Array[Task[T]])` | `TaskGroup[T]` |
| `task.group_cancel[T](group: TaskGroup[T])` | `Bool` |
| `task.group_join[T](group: TaskGroup[T])` | `Result[Array[T], String]` |

Task join/await, thread spawn/join/detach, task-group construction, and group
join consume their move-only input; task/thread `is_complete`, task `cancel`,
and `group_cancel` borrow. Async calls use one default pool with
1-64 workers and a 65,536-entry queue. Group results and the selected first
error use input-array order.

### `std.sync`

| Function | Result |
| --- | --- |
| `mutex[T](value: T)` | `Mutex[T]` |
| `lock[T](mutex: Mutex[T], deadline: Int, token: CancellationToken)` | `Result[LockGuard[T], String]` |
| `guard_get[T](guard: LockGuard[T])` | `T` |
| `guard_set[T](guard: LockGuard[T], value: T)` | `Bool` |
| `unlock[T](guard: LockGuard[T])` | `Result[Bool, String]` |
| `event(manual_reset: Bool, initially_set: Bool)` | `Event` |
| `event_set(event: Event)` | `Bool` |
| `event_reset(event: Event)` | `Bool` |
| `event_wait(event: Event, deadline: Int, token: CancellationToken)` | `Result[Bool, String]` |
| `atomic_int(value: Int)` | `AtomicInt` |
| `atomic_load(value: AtomicInt)` | `Int` |
| `atomic_store(value: AtomicInt, replacement: Int)` | `Unit` |
| `atomic_fetch_add(value: AtomicInt, delta: Int)` | `Int` |
| `atomic_compare_exchange(value: AtomicInt, expected: Int, replacement: Int)` | `Bool` |
| `once[T](value: T)` | `Once[T]` |
| `once_empty[T](type_witness: T)` | `Once[T]` |
| `once_set[T](cell: Once[T], value: T)` | `Result[Bool, String]` |
| `once_get[T](cell: Once[T])` | `Option[T]` |

Guard reads/writes borrow; unlock consumes. Mutex and once values must be
`Share`. Events use predicate-based waits, atomics are
sequentially consistent, and mutexes do not have exception poisoning.
`once(value)` publishes its seed immediately. `once_empty(witness)` uses its
argument only to infer `T`; exactly one concurrent `once_set` succeeds.

### `std.channel`

| Function | Result |
| --- | --- |
| `bounded[T](initial: Array[T], capacity: Int)` | `Result[Channel[T], String]` |
| `unbounded[T](initial: Array[T])` | `Result[Channel[T], String]` |
| `sender[T](channel: Channel[T])` | `Sender[T]` |
| `receiver[T](channel: Channel[T])` | `Receiver[T]` |
| `clone_sender[T](sender: Sender[T])` | `Sender[T]` |
| `clone_receiver[T](receiver: Receiver[T])` | `Receiver[T]` |
| `send[T](sender: Sender[T], value: T, deadline: Int, token: CancellationToken)` | `Result[Bool, String]` |
| `receive[T](receiver: Receiver[T], deadline: Int, token: CancellationToken)` | `Result[Option[T], String]` |
| `close_sender[T](sender: Sender[T])` | `Result[Bool, String]` |
| `close_receiver[T](receiver: Receiver[T])` | `Result[Bool, String]` |

Send consumes `value`. Endpoints are explicitly cloneable. Bounded channels
apply backpressure; unbounded channels return a resource error at the process
safety ceiling. Dropping the last endpoint closes its direction and wakes every
waiter. Any structurally `Send` payload is accepted, including primitives.

### Asynchronous files, sockets, and processes

| Function | Result |
| --- | --- |
| `async_file.read(path: String, maximum: Int, token: CancellationToken)` | `Task[UniqueBuffer[Char]]` |
| `async_file.write(path: String, bytes: UniqueBuffer[Char], append: Bool, token: CancellationToken)` | `Task[Bool]` |
| `async_net.connect(host: String, port: Int, deadline: Int, token: CancellationToken)` | `Task[Int]` |
| `async_net.accept(listener: Int, deadline: Int, token: CancellationToken)` | `Task[Int]` |
| `async_net.receive(socket: Int, maximum: Int, deadline: Int, token: CancellationToken)` | `Task[UniqueBuffer[Char]]` |
| `async_net.send(socket: Int, bytes: UniqueBuffer[Char], deadline: Int, token: CancellationToken)` | `Task[Int]` |
| `async_net.shutdown(socket: Int)` | `Result[Bool, String]` |
| `async_process.run(program: String, arguments: Array[String], deadline: Int, token: CancellationToken)` | `Task[Int]` |

Partial I/O, EOF, close, cancellation races, and resource limits are defined
in `CONCURRENCY.md`. Async process execution returns the child exit code and
inherits standard streams; 1.8 does not expose output capture. The Rocket 1.5
synchronous functions remain unchanged. The target backend combines bounded
executor work with native file, socket, timer, and process waits; Windows uses
overlapped file events while Linux/macOS use POSIX providers.
