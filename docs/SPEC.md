# Rocket Language Specification - Draft 0.3

This document freezes the syntax implemented by the first compiler slice. Later incompatible changes require a recorded design decision.

## Layout and comments

Blocks begin after `:` and use four-space indentation. Tabs used for indentation are errors. Blank lines and comments beginning with `#` do not change indentation.

## Functions and types

```rocket
fn add(left: Int, right: Int) -> Int:
    return left + right
```

Public function boundaries are explicit. Initial built-in types are `Int`, `Float`, `Bool`, `Char`, `String`, and `Unit`.

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

## Reserved future types

`Option[T]` represents possible absence and `Result[T, E]` represents recoverable failure. Their spelling and semantics are fixed, but parsing and exhaustive matching are scheduled after enums and generics.
