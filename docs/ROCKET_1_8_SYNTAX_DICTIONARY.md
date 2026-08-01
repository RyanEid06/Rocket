# Rocket 1.8 Ownership and Async Syntax Dictionary

Rocket 1.8 adds two contextual keywords and additive generic types. Existing
identifiers named `async` or `await` remain valid outside the positions below.

## Async functions

```rocket
async fn fetch(path: String) -> Result[UniqueBuffer[Char], String]:
    return await async_file.read(path, 67108864, cancel.current())
```

- `async` immediately before `fn` declares an asynchronous function.
- Its written result must be `Result[T, String]` with `T: Send`.
- Calling it returns `Task[T]` after evaluating arguments left to right.
- `await task` is a prefix expression valid only in an async body and has type
  `Result[T, String]` for an operand of type `Task[T]`.
- `(await task)?` uses the existing postfix propagation rule.

## Ownership types

```rocket
let parent: Weak[Node] = ownership.downgrade(node)
var bytes: UniqueBuffer[Char] = buffer.thaw(['a', 'b'])
bytes = buffer.append(bytes, 'c')
let frozen: Array[Char] = buffer.freeze(bytes)
```

`Weak[T]`, `UniqueBuffer[T]`, and `Task[T]` are built-in structural generic type
spellings. `Weak` is non-owning; `UniqueBuffer` is move-only; `Task` is an
internally synchronized owned handle. Standard concurrency handle types live in
`std.thread`, `std.task`, `std.sync`, `std.channel`, `std.cancel`, and the
`std.async_*` modules. Their exact signatures and lifetime rules are defined in
`CONCURRENCY.md` and `STDLIB.md`.

Rocket 1.8 adds no null, exceptions, shared mutable fields, implicit detach,
unchecked native sharing, or user-written unsafe `Send`/`Share` implementation.
