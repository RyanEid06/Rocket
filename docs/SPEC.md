# Rocket Language Specification 1.3

This document freezes Rocket 1.0 syntax and semantics. Compatible 1.x releases
may clarify wording or add APIs without changing valid 1.0 program behavior;
incompatible language changes require a recorded decision and a new major version.

## Layout and comments

Blocks begin after `:` and use four-space indentation. Tabs used for indentation
are errors. Source files may use LF or CRLF line endings; both produce identical
tokens and locations. Blank lines and comments beginning with `#` do not change
indentation.

## Functions and types

```rocket
fn add(left: Int, right: Int) -> Int:
    return left + right
```

Public function boundaries are explicit. Built-in types are `Int`, `Float`,
`Bool`, `Char`, `String`, `Unit`, `Array[T]`, `Slice[T]`, `Option[T]`, and
`Result[T, E]`. Type arguments nest without a fixed depth. User structs,
enums, and functions may declare type parameters in brackets. Generic function
calls infer every type argument from value arguments and are specialized to
concrete functions before MIR lowering.

Every executable module defines `fn main() -> Int`. Its result becomes the
native process exit status. A `Unit` function returns with bare `return` or by
reaching the end of its body; the current syntax has no standalone `Unit`
literal.

## Bindings

```rocket
let name = "Ada"   # immutable
var count = 0       # mutable
let answer: Option[Int] = None()
count = 1
```

The initializer determines the local type. Assignment targets must be existing `var` bindings and the assigned value must have the binding's type. There is no `null` value.

## Control flow

```rocket
if score >= 50:
    print("pass")
else:
    print("fail")

while count < 10:
    print(count)

for index in 0..10:
    if index == 5:
        continue
    print(index)
```

Conditions must have type `Bool`. A range uses integer bounds and excludes its end (`0..10` produces 0 through 9). Both bounds are evaluated once, from left to right, before iteration. The loop variable is an immutable `Int`; `break` and `continue` are valid only inside loop bodies. Every non-`Unit` function must return a value on every path; complete control-flow proof is scheduled for semantic milestone 2.

## Expressions

Precedence, from lowest to highest, is `or`, `and`, equality, comparison, addition/subtraction, multiplication/division, unary `not`/negation, and calls. `and`, `or`, and `not` require `Bool` operands; `and` and `or` short-circuit. `Int` and `Float` support arithmetic, ordering, and unary negation, but do not implicitly convert between each other. `Char` uses single quotes and supports `\n`, `\r`, `\t`, `\\`, and `\'` escapes. String literals support the corresponding newline, carriage-return, tab, backslash, and double-quote escapes. Operators do not perform implicit conversions.

`Int` is a signed 64-bit integer. Out-of-range literals are compile-time errors.
Addition, subtraction, multiplication, unary negation, and division terminate
with a runtime diagnostic instead of wrapping when the mathematical result is
outside the signed 64-bit range. Integer division by zero is also a runtime
error. `Float` is an IEEE 754 binary64 value, `Bool` is `true` or `false`, and
`Char` is one byte in the current implementation.

`String` is an owned, immutable sequence of valid UTF-8 bytes. Its byte length
is stored separately, so embedded zero bytes do not terminate it. Equality on
`String` compares byte length and contents rather than storage identity.
Equality on `Unit` is always true because `Unit` has one value. Array and Slice
equality is reserved for a later collection API.

The built-in `print(value)` accepts one scalar value, writes its textual form
followed by a newline, and returns `Unit`. `Bool` currently prints as `1` or
`0`; a stable formatting API is reserved for the standard-library milestone.

## Arrays and slices

An Array literal is non-empty and infers one element type:

```rocket
let scores = [10, 20, 30, 40]       # Array[Int]
let names = ["Ada", "Grace"]       # Array[String]
```

Every element must have exactly the same type. Nested managed values are
supported. An empty literal is valid when an `Array[T]` type is supplied by a
binding, argument, or return context.
Array values use copy-on-write value semantics. A direct `var` Array binding may
replace an element with another value of exactly the same element type:

```rocket
var scores = [10, 20, 30]
scores[1] = 99
```

The index and replacement expression are each evaluated once, from left to
right. The index must be `Int`, and normal bounds checks apply. Indexed
assignment through a `let`, a `Slice`, a field, or another temporary expression
is rejected. If the Array is shared by another binding or retained by a Slice,
the mutated binding first receives a private copy. Existing aliases and Slices
therefore continue to observe the pre-mutation value. A uniquely owned Array
may reuse its storage as an implementation optimization without changing these
semantics.

Growing Array operations are exposed by `std.collections` and return updated
values, so rebinding remains visible in source:

```rocket
import std.collections

fn main() -> Int:
    var values: Array[Int] = []
    values = collections.reserve(values, 16)
    values = collections.append(values, 10)
    match collections.pop(values):
        case Some(removed):
            values = removed.values
            print(removed.value)
        case None:
            print(0)
    return 0
```

`capacity` reports the number of elements that can be held without another
allocation. `reserve` never shrinks and rejects a negative requested capacity.
`append` grows geometrically when needed. `pop` returns `None` for an empty
Array; otherwise it returns `Some(std.collections.Pop[T])`, whose `values`
field is the updated Array and whose `value` field is the removed last element.
These persistent operations preserve their input even when their result is
ignored. An implementation may reuse storage only when MIR proves the input is
consumed by a direct rebind. Aliases and Slices retain their old length,
capacity, and values.

Indexing works on Array and Slice and returns the element value. A slice uses an
exclusive end bound and retains its backing Array, so it remains valid after
the original Array binding is replaced or leaves use:

```rocket
let middle = scores[1..3]  # Slice[Int], containing 20 and 30
print(middle[0])           # 20
```

Both index and slice bounds are checked at runtime. Negative indices,
`index >= length`, `start < 0`, `end < start`, and `end > length` terminate the
program with a deterministic runtime diagnostic. Slicing a Slice produces a
view over the same retained backing Array.

## Tuples, maps, and sets

`std.collections.Tuple2[A, B]` and `Tuple3[A, B, C]` are immutable
heterogeneous product values with `first`, `second`, and (for `Tuple3`) `third`
fields. They use ordinary aggregate ownership and can carry managed values.

`std.collections.Map[K, V]` stores unique keys in deterministic insertion
order; `Set[T]` stores unique values in the same order. Eligible key/element
types are `Int`, `Bool`, `Char`, and `String`. `Float` is excluded so NaN and
signed-zero behavior cannot undermine equality. String equality and hashing use
the complete length-aware UTF-8 byte sequence. `map_from_arrays` and
`set_from_array` preserve the first occurrence and discard later duplicates.
Iteration is obtained from the snapshot Arrays returned by `map_keys`,
`map_values`, and `set_values`; no internal lookup-storage order is exposed.

Stable hashing uses 64-bit FNV-1a over the canonical value bytes and clears the
sign bit so the result fits non-negative `Int`. The byte order for `Int` is the
Windows x64 little-endian target order in Rocket 1.1. Hash output is stable
across runs of the same target and release line.

Collection searching, equality filtering, scalar/String sorting, stable hash
mapping, and numeric sum folds are ordinary deterministic standard-library
operations. General callback-driven mapping, filtering, and folding require the
function-value and closure rules introduced in Rocket 1.2; Rocket 1.1 does not
create a privileged callback ABI ahead of that language design.

`Queue[T]`, `Stack[T]`, and `ByteBuffer` are immutable library product values
wrapping `Array[T]`, `Array[T]`, and `Array[Char]`. Their exposed snapshots use
the same copy-on-write/persistent operations and ownership rules as Arrays.

## Ownership and lifetime

`String`, `Array[T]`, `Slice[T]`, structs, and enums are managed values. Assignment and local
aliasing preserve the value through automatic reference counting; programmers
do not write retain or release operations. Destruction is deterministic when
the last owning reference is released. Arrays release managed elements, Slices
release their backing Array, and aggregate destructors release every managed
field. Reference-count cycles are a documented Rocket 1.0 limitation. Current
immutable aggregate construction does not provide a source-level operation that
can create a self-cycle. Array mutation uses copy-on-write: cloning retains each
managed element, replacement retains the new element before releasing the old
one, and rebinding releases the prior Array after the updated value is owned.

## Structs and generics

Structs are immutable product values with positional construction and named
field access:

```rocket
pub struct Pair[T]:
    first: T
    second: T

fn identity[T](value: T) -> T:
    return value

let pair = Pair(identity(10), 20)
print(pair.first)
```

Every constructor argument must match its declared field. Type arguments are
inferred from constructor values or supplied by an expected type. Fields cannot
be assigned after construction. Aggregate equality is intentionally not
implicit; programs match enums or compare individual fields.

## Methods, traits, and functional values (Rocket 1.2)

An `impl` block groups functions with a struct or enum declared in the same
module. Generic impl parameters precede the owner type. Instance methods name
an explicit first parameter `self`; associated functions omit it:

```rocket
struct Box[T]:
    value: T

impl[T] Box[T]:
    pub fn make(value: T) -> Box[T]:
        return Box(value)

    pub fn get(self: Box[T]) -> T:
        return self.value

let value = Box.make(42).get()
```

Dot calls are statically rewritten after receiver typing. `box.get()` is
equivalent to the resolved function call `Box.get(box)` and evaluates `box`
exactly once. There is no implicit receiver mutation, virtual dispatch,
inheritance, or runtime method table. `self` must be first and its type must
exactly match the impl owner. Individual methods use `pub`; an impl block itself
cannot be public. Method overloads are namespaced by owner type, while duplicate
members on the same owner are rejected.

Standard String, Array, Slice, Map, and Set operations also support documented
dot-call spellings. Their module-function spellings remain source compatible
and both forms resolve to the same intrinsic and runtime ABI entry point.

Traits declare statically dispatched behavior. Every trait method has an
explicit first parameter `self: Self`; an implementation uses
`impl Trait for Owner`. Generic functions constrain type parameters with a
`where` clause:

```rocket
trait Readable:
    fn read(self: Self) -> Int

impl Readable for Counter:
    fn read(self: Counter) -> Int:
        return self.value

fn twice[T](value: T) -> Int where T: Readable:
    return value.read() * 2
```

Selection is deterministic and entirely compile-time. Inherent members take
precedence; otherwise exactly one matching trait implementation is required.
Missing, incomplete, duplicate, and ambiguous implementations are errors.
There are no trait objects, virtual calls, or runtime method tables.

An expression lambda has typed parameters, an explicit result type, and one
expression body: `fn(value: Int) -> Int => value + offset`. Its value is a
compiler-generated immutable struct containing the captured local values and a
direct `call` method. Captures use normal ARC ownership and may escape. Generic
functions accept closure values and specialize to their concrete closure type.
Lambda parameter and result annotations may reference the enclosing generic
function's type parameters and are substituted in each specialization;
lambda values may be invoked immediately without first assigning them to a local;
Rocket 1.2 does not expose a dynamically erased function type.

`for item in source:` uses the statically resolved persistent iterator protocol:
`source.iterator() -> Cursor`, `Cursor.has_next() -> Bool`,
`Cursor.value() -> T`, and `Cursor.advance() -> Cursor`. The source and cursor
are evaluated once, `continue` advances the cursor, and iterator operations are
ordinary inherent or trait methods. Integer `start..end` loops retain their
existing exclusive-end meaning.

An impl may declare a non-generic associated constant with
`pub const NAME: Type = expression`. `Owner.NAME` lowers to an ordinary
zero-argument direct function, so initialization is deterministic and adds no
global storage or runtime ABI. Trait and generic-impl associated constants are
reserved.

Rocket 1.2 parameters remain positional, required, and fixed-arity. Default,
named, and variadic arguments are intentionally reserved until their evaluation
order and C-ABI interaction can be specified without ambiguity. User generic
specialization is deterministic and capped at 4,096 generated instances per
compilation.

## Native interoperability (Rocket 1.3)

Native operations are declarations, not inferred behavior. The supported forms
are `extern fn`, `extern const`, `extern struct`, `extern opaque`,
`extern callback`, and `export fn`. Calling an `extern fn` is permitted only
inside an explicitly indented `unsafe:` block. An unsafe block changes no type,
ownership, or control-flow rule; it records that the programmer is accepting
the foreign contract. A safe Rocket wrapper keeps its own `unsafe:` block small
and exposes ordinary checked functions to callers.
The five Phase 13 spellings are contextual rather than globally reserved, so
older code may continue to use them as ordinary identifiers elsewhere.

The stable `C` ABI is currently defined only for Windows x64. `Int` is
`int64_t`, `Float` is `double`, `Bool` is an unsigned 8-bit `rocket_bool` whose
canonical values are zero and one, `Char` is `uint8_t`, and `Unit` is `void`.
`Pointer[T]` is a non-owning C pointer. `Pointer[Unit]` is `void*`. Rocket has no
pointer arithmetic, pointer dereference, pointer construction, or universal
null value; a pointer can only cross another native call. Managed `String`,
collections, Rocket structs/enums, closures, `Option`, and `Result` never cross
this ABI.

`extern opaque Handle` represents a C pointer to an incomplete named type.
`extern struct S:` records a deterministic C field layout for header/binding
generation, but structs are passed only as `Pointer[S]`, never by value.
Native-struct fields may be primitive values, pointers, or opaque handles.
`extern callback C(...) -> R` is a C function pointer. A callback argument must
name a top-level, non-capturing Rocket function with an exactly matching
signature. The C callee may invoke it synchronously only for the duration of the
extern call and may not store it. Callback parameters/results use primitive,
pointer, or opaque types; callbacks cannot return callbacks.

Opaque handles and pointers are borrowed unless the C API documents an acquire
or release function. Rocket ARC never retains or releases them. A handle
returned as owned by C must be released exactly once through its documented
extern destructor and must not be used afterward. Rocket does not diagnose
foreign use-after-free, double-free, null, or alias violations. C errors cross
the boundary as primitive status/result values and safe wrappers translate
those values to `Option` or `Result`. Strings require an API-specific pointer
plus explicit byte-length convention; direct Rocket `String` ABI exposure is
not supported.

`extern const NAME: Primitive = literal` imports a deterministic compile-time
literal rather than native storage. `export fn` emits a C-visible unmangled
wrapper and is restricted to the same non-managed ABI values. Native imports,
exports, types, and callbacks cannot be generic. A library module may omit
`main`; executable modules retain the required `fn main() -> Int` entry point.

## Validated raylib boundary (Rocket 1.4)

Rocket 1.4 adds no calling convention and does not expose raylib's by-value C
structures. The supported raylib 6.0 integration is an ordinary native package
built on the Rocket 1.3 ABI. A reviewed C adapter converts windows, frames,
textures, fonts, audio devices, sounds, and temporary UTF-8 buffers into positive
64-bit resource tokens. Coordinates, sizes, colors, input codes, time, status
values, and callback arguments cross only as frozen primitive values.

The safe Rocket module is the only production module that calls the generated
low-level bindings. It translates every nonzero adapter status to `Result`,
copies a Rocket string into a temporary native byte buffer, calls the native API
synchronously, and releases that buffer before returning. Embedded NUL is
rejected by the adapter. No raylib pointer, structure, callback pointer, or
borrowed C string becomes a Rocket value.

One `Window` may be live. `begin_frame` creates a single-use `Frame` token;
drawing requires that token and `end_frame` consumes it. Textures and fonts are
owned by the window context and must be unloaded before `close_window` succeeds.
One `AudioDevice` may be live, and every owned `Sound` must be unloaded before
`close_audio`. Reusing or forging a released token returns a stable error and
never dereferences foreign state. Rocket 1.4 does not add linear types or
destructors, so application code still performs explicit cleanup.

Callbacks remain top-level, noncapturing, synchronous, and non-storing. The
safe raylib module exposes reviewed callback-backed operations rather than a
general storable callback. The deterministic test mode implements the same C ABI
without opening a real window or audio device; it is test-only and must be
enabled before resources are created.

## Enums and pattern matching

Enums are tagged alternatives. A variant may carry zero or more positional
payload values:

```rocket
enum Message:
    Number(Int)
    Text(String)

match message:
    case Number(value):
        print(value)
    case Text(text):
        print(text)
```

`match` accepts enum values, binds payloads immutably, rejects duplicate cases,
and must cover every variant unless its final case is `_`. A wildcard cannot
bind payload values. Match arms are statement blocks; a match is considered to
return when every exhaustive arm returns.

## Option, Result, and propagation

`Option[T]` has `Some(T)` and `None`. `Result[T, E]` has `Ok(T)` and `Err(E)`.
They are ordinary built-in generic enums and use the same exhaustive matching
rules as user enums. Null and exceptions do not exist.

Postfix `?` unwraps `Some` or `Ok`. On `None`, it returns `None` from an
`Option[...]` function. On `Err(error)`, it returns a newly typed `Err(error)`
from a `Result[..., E]` function; the error type must match exactly.

## Modules and visibility

Each `.rocket` file is a module. The root file imports package-relative module
paths, where dots map to directories:

```rocket
import utilities.math

fn main() -> Int:
    return math.doubled(21)
```

The final path component is the local module alias; the complete import path is
also accepted. Cross-file functions, structs, enums, and enum variants are
accessible only when their declaration is marked `pub`. Imports are loaded
recursively, cycles are errors, and declarations receive deterministic fully
qualified identities. A command compiles the complete source graph into one
native artifact; independent binary module artifacts are not part of draft 0.6.

## Standard modules

Imports whose complete path starts with `std.` resolve to compiler-provided
modules rather than package files, except for explicitly bundled source modules
such as `std.testing`. Their function signatures are statically checked and
lower to typed MIR calls. The stable foundational modules are `std.string`,
`std.collections`, `std.file`, `std.path`, `std.json`, `std.csv`, `std.random`,
`std.process`, and `std.time`. Rocket 1.5 adds `std.binary`, `std.stream`,
`std.unicode`, `std.regex`, `std.crypto`, `std.net`, `std.http`, `std.datetime`,
`std.log`, `std.cli`, `std.config`, `std.compression`, `std.archive`,
`std.sqlite`, and `std.testing`.

Standard APIs use the same `Option` and `Result` enums as user code. File,
process, conversion, JSON, and CSV failures are recoverable values. No standard
function throws a language exception. Process execution receives one program
and an `Array[String]` of arguments and does not invoke a command shell.

`std.json.Json`, `std.json.JsonField`, `std.http.Request`, and
`std.http.Response` are nominal standard declarations and may appear in type
annotations and exhaustive matches. Other library calls use existing scalar,
collection, `Option`, or `Result` types. Streams, sockets, listeners, and
databases use kind-checked process-local integer tokens. Close or cancellation
invalidates a token, and later use is a recoverable error. Tokens are not native
pointers and cannot be dereferenced by Rocket code.

Security-sensitive and blocking functions have normative input, output, and
timeout bounds in `STDLIB.md`. HTTPS uses platform certificate validation;
regular expressions use a bounded non-backtracking engine; archives are
validated data and never implicitly extract files; SQLite inputs use bound
parameters. `std.testing` resolves from the bundled ordinary Rocket source tree
and calls a narrow private `std.testing_core` host boundary for temporary roots
and coverage storage. Complete signatures and deterministic behavior are
specified in `STDLIB.md` and `RELEASE_1_5.md`.

The self-hosted compiler uses ordinary public APIs rather than privileged
syntax. `string.byte_at` and `string.slice` traverse immutable UTF-8 source by
checked byte offsets, `collections.append` grows compiler work lists,
`collections.concat` composes persistent arrays, and `process.arguments`
exposes command-line arguments after the executable name.
These APIs have identical LLVM/runtime and preserved Stage 0 behavior.

## Package compilation model

A package is rooted by `rocket.toml`; its entry and test directories are
relative paths contained by that root. Source imports resolve from the package
root. Standalone-file compilation instead uses the file's parent directory as
its import root. In both modes, generated artifacts are outside the source graph
and cannot become implicit modules.

Formatting is not semantically observable. The canonical formatter preserves
tokens, literals, and line comments while normalizing whitespace and newlines.
Each test-runner input is an ordinary independent program with the same required
`fn main() -> Int` entry signature; zero is success and nonzero is failure.

Rocket 1.6 package metadata is an additive tooling contract. Package versions
are exact Semantic Versioning 2.0.0 values. `[dependencies]` maps a package name
to a registry constraint, `path:` source, or immutable-revision `git:` source.
Resolution is deterministic, permits only one selected version per package
name, and records exact source checksums and graph edges in committed
`rocket.lock`. Registry identities additionally carry an owned namespace and
signed publisher provenance. A dependency import is rooted only in the exact
SHA-256-verified cached package named by that manifest dependency and cannot be
shadowed by a package-local path. Transitive imports must be edges in the same
lock graph. A locked offline operation consumes no network or source checkout.

Registry metadata is authenticated by a manifest-pinned ECDSA P-256 key,
archives are bounded deterministic regular-file-only ustar data, and HTTPS/Git
transport never disables certificate or immutable-object verification. Package
metadata grants no code execution. Build scripts are unsupported and denied;
dependency native inputs require an exact root capability allow-list. These
rules add no language syntax or runtime ABI feature. The complete import,
transport, credential, publishing, audit, documentation, and governance
contract is in `PACKAGES.md`.

Rocket 1.7 tooling is also additive and does not change program semantics. An
open editor document is an in-memory replacement for the same normalized module
path; it has no authority to execute a build or package hook. Semantic tooling
must consume the compiler's tokens, AST, HIR symbols, and types, distinguish
resolved references from text, and preserve stable diagnostic codes. LSP
positions and edits use UTF-16 code units and increasing document versions.

Native debug locations originate on MIR instructions/terminators and map to
CodeView/PDB plus `rocket-source-map-1`. Coverage/profiling instrumentation is
opt-in and cannot affect ordinary builds. Machine compiler/test/build output is
newline-delimited `rocket-message-1`; coverage, profile, benchmark, debug-map,
and REPL-evaluation records have independent versioned schemas. None of these
formats is a language/runtime ABI promise, but incompatible schema changes
require a new major schema name.
