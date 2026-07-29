# Rocket 1.1 Syntax Dictionary (Draft)

Rocket 1.1 is an additive release. All syntax in the
[Rocket 1.0 syntax dictionary](ROCKET_1_0_SYNTAX_DICTIONARY.md) remains valid.
This draft records Phase 11 additions as they become implemented.

## Mutable Array elements

Declare the Array binding with `var`, then assign through one checked index:

```rocket
var scores = [10, 20, 30]
scores[1] = 99
print(scores[1])  # 99
```

The replacement must have the Array's element type and the index must be `Int`.
Mutation through `let`, `Slice[T]`, fields, calls, or other temporary expressions
is not allowed.

Arrays have copy-on-write value semantics:

```rocket
var scores = [10, 20, 30]
let original = scores
let middle = scores[1..3]
scores[1] = 99
print(original[1])  # 20
print(middle[0])    # 20
```

The mutation changes only `scores`. Existing aliases and Slices retain the old
value. Bounds failures use the same deterministic runtime diagnostics as reads.

Further Phase 11 operations—capacity management, `append`, `pop`, `insert`,
`remove`, `clear`, maps, sets, tuples, and iteration—remain under development.
