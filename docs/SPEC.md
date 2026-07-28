# Rocket Language Specification - Draft 0.8

This document freezes the syntax implemented by the first compiler slice. Later incompatible changes require a recorded design decision.

## Layout and comments

Blocks begin after `:` and use four-space indentation. Tabs used for indentation are errors. Blank lines and comments beginning with `#` do not change indentation.

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

Precedence, from lowest to highest, is `or`, `and`, equality, comparison, addition/subtraction, multiplication/division, unary `not`/negation, and calls. `and`, `or`, and `not` require `Bool` operands; `and` and `or` short-circuit. `Int` and `Float` support arithmetic, ordering, and unary negation, but do not implicitly convert between each other. `Char` uses single quotes and supports `\n`, `\t`, `\\`, and `\'` escapes. Operators do not perform implicit conversions.

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
Arrays are immutable collections in this draft; a `var` may be assigned a new
whole Array, but individual elements cannot be assigned.

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

## Ownership and lifetime

`String`, `Array[T]`, `Slice[T]`, structs, and enums are managed values. Assignment and local
aliasing preserve the value through automatic reference counting; programmers
do not write retain or release operations. Destruction is deterministic when
the last owning reference is released. Arrays release managed elements, Slices
release their backing Array, and aggregate destructors release every managed
field. Reference-count cycles are a documented Rocket 1.0 limitation. Current
immutable aggregate construction does not provide a source-level operation that
can create a self-cycle.

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
modules rather than package files. Their function signatures are statically
checked and lower to typed MIR calls. The stable Phase 7 modules are
`std.string`, `std.collections`, `std.file`, `std.path`, `std.json`, `std.csv`,
`std.random`, `std.process`, and `std.time`.

Standard APIs use the same `Option` and `Result` enums as user code. File,
process, conversion, JSON, and CSV failures are recoverable values. No standard
function throws a language exception. Process execution receives one program
and an `Array[String]` of arguments and does not invoke a command shell.

`std.json.Json` and `std.json.JsonField` are nominal built-in declarations and
may appear in type annotations and exhaustive matches. Other library calls use
existing scalar, collection, `Option`, or `Result` types. Complete signatures
and deterministic behavior are specified in `STDLIB.md`.

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
