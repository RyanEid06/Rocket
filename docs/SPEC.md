# Rocket Language Specification - Draft 0.4

This document freezes the syntax implemented by the first compiler slice. Later incompatible changes require a recorded design decision.

## Layout and comments

Blocks begin after `:` and use four-space indentation. Tabs used for indentation are errors. Blank lines and comments beginning with `#` do not change indentation.

## Functions and types

```rocket
fn add(left: Int, right: Int) -> Int:
    return left + right
```

Public function boundaries are explicit. Initial built-in types are `Int`, `Float`, `Bool`, `Char`, `String`, and `Unit`.

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

`Int` is a signed 64-bit integer, `Float` is an IEEE 754 binary64 value, `Bool`
is `true` or `false`, and `Char` is one byte in the current scalar slice.
Equality on `String` compares contents rather than storage identity. Equality
on `Unit` is always true because `Unit` has one value. Out-of-range integer
literals, integer overflow, division by zero, and the final UTF-8 `String`
representation will be specified with the checked runtime in the next
milestone; programs that depend on those edge cases are not portable in this
draft.

The built-in `print(value)` accepts one scalar value, writes its textual form
followed by a newline, and returns `Unit`. `Bool` currently prints as `1` or
`0`; a stable formatting API is reserved for the standard-library milestone.

## Reserved future types

`Option[T]` represents possible absence and `Result[T, E]` represents recoverable failure. Their spelling and semantics are fixed, but parsing and exhaustive matching are scheduled after enums and generics.
