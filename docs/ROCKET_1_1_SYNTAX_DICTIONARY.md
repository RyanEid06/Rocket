# Rocket 1.1 Syntax Dictionary

Rocket 1.1 is an additive release. All syntax in the
[Rocket 1.0 syntax dictionary](ROCKET_1_0_SYNTAX_DICTIONARY.md) remains valid.
This dictionary records the additive Phase 11 features implemented by Rocket 1.1.

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

## Array growth

Array growth is expressed with ordinary `std.collections` calls and explicit
rebinding:

```rocket
import std.collections

var values: Array[String] = []
values = collections.reserve(values, 8)
values = collections.append(values, "new")
match collections.pop(values):
    case Some(removed):
        values = removed.values
        print(removed.value)
    case None:
        print("empty")
```

`collections.capacity(values)` reports the current capacity. `pop` returns
`Option[collections.Pop[T]]`; its `values` and `value` fields carry both results
without hidden mutation or exceptions.

`collections.insert(values, index, value)` inserts before a checked index,
where the current length is accepted as an append position.
`collections.remove(values, index)` returns a `Removal[T]` with `values` and
`value` fields. `collections.clear(values)` returns an empty Array while
preserving reserved capacity.

## Tuples, Map, and Set

```rocket
let pair = collections.Tuple2("answer", 42)
let table = collections.map_from_arrays(["b", "a"], [2, 1])
let unique = collections.set_from_array(["b", "a", "b"])
```

Tuples expose `first`, `second`, and optional `third` fields. Maps accept keys
and Sets accept elements of type `Int`, `Bool`, `Char`, or `String`. They
preserve the first occurrence and expose deterministic insertion-order Arrays
through `map_keys`, `map_values`, and `set_values`.

`contains`, `find`, `filter_equal`, the four scalar/String sort functions,
`map_hash`, and numeric sum folds provide the Phase 11 algorithms that do not
require callbacks. `Queue[T]`, `Stack[T]`, and `ByteBuffer` wrap ordinary Array
snapshots through public `values` and `bytes` fields.

General callback-based map/filter/fold syntax is part of Phase 12 function
values and closures.
