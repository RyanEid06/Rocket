# Rocket Language Specification - Draft 0.5

This document freezes the syntax implemented by the first compiler slice. Later incompatible changes require a recorded design decision.

## Layout and comments

Blocks begin after `:` and use four-space indentation. Tabs used for indentation are errors. Blank lines and comments beginning with `#` do not change indentation.

## Functions and types

```rocket
fn add(left: Int, right: Int) -> Int:
    return left + right
```

Public function boundaries are explicit. Built-in types are `Int`, `Float`,
`Bool`, `Char`, `String`, `Unit`, `Array[T]`, and `Slice[T]`. In this draft,
collection element type `T` may be `Int`, `Float`, `Bool`, `Char`, or `String`.
General nested types follow the generics milestone.

Every executable module defines `fn main() -> Int`. Its result becomes the
native process exit status. A `Unit` function returns with bare `return` or by
reaching the end of its body; the current syntax has no standalone `Unit`
literal.

## Bindings

```rocket
let name = "Ada"   # immutable
var count = 0       # mutable
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

Every element must have exactly the same type. Empty literals and nested
collection element types wait for general type inference and generics.
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

`String`, `Array[T]`, and `Slice[T]` are managed values. Assignment and local
aliasing preserve the value through automatic reference counting; programmers
do not write retain or release operations. Destruction is deterministic when
the last owning reference is released. Arrays release managed String elements,
and Slices release their backing Array. Reference-count cycles cannot currently
be formed with the implemented immutable collections, but cycles introduced by
future aggregate types are a documented Rocket 1.0 limitation.

## Reserved future types

`Option[T]` represents possible absence and `Result[T, E]` represents recoverable failure. Their spelling and semantics are fixed, but parsing and exhaustive matching are scheduled after enums and generics.
