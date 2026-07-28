# Rocket Language Specification - Draft 0.1

This document freezes the syntax implemented by the first compiler slice. Later incompatible changes require a recorded design decision.

## Layout and comments

Blocks begin after `:` and use four-space indentation. Tabs used for indentation are errors. Blank lines and comments beginning with `#` do not change indentation.

## Functions and types

```rocket
fn add(left: Int, right: Int) -> Int:
    return left + right
```

Public function boundaries are explicit. Initial built-in types are `Int`, `Bool`, `String`, and `Unit`.

## Bindings

```rocket
let name = "Ada"   # immutable
var count = 0      # mutable storage; assignment arrives in the next slice
```

The initializer determines the local type. There is no `null` value.

## Control flow

```rocket
if score >= 50:
    print("pass")
else:
    print("fail")

while count < 10:
    print(count)
```

Conditions must have type `Bool`. Every non-`Unit` function must return a value on every path; complete control-flow proof is scheduled for semantic milestone 2.

## Expressions

Precedence, from lowest to highest, is equality, comparison, addition/subtraction, multiplication/division, unary negation, and calls. Operators do not perform implicit string/number conversions.

## Reserved future types

`Option[T]` represents possible absence and `Result[T, E]` represents recoverable failure. Their spelling and semantics are fixed, but parsing and exhaustive matching are scheduled after enums and generics.
