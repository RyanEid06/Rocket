# Migrating to Rocket 1.8

Rocket 1.8 is source-compatible with valid Rocket 1.0-1.7 programs. Existing
synchronous file, socket, process, collection, `Array`, `Slice`, and
`ByteBuffer` APIs keep their previous behavior. Migration is opt-in unless code
adopts the new ownership or concurrency surface.

## Adopt weak ownership for cycles

ARC still gives deterministic destruction, but a graph made entirely of strong
edges remains a leak. Change at least one back edge in every potential cycle to
`Weak[T]` and handle expiration explicitly:

```rocket
let observer = ownership.downgrade(node)
match ownership.upgrade(observer):
    case Some(live):
        print(live.name)
    case None:
        print("expired")
```

Weak targets must be identity-bearing `Share` values. A rejected target reports
`R4102`; this makes accidental weak references to scalars or non-shareable
native resources deterministic at compile time.

## Use unique buffers for staged mutation

Keep `Array[T]` when persistent value semantics are useful. For repeated local
mutation, thaw an array into a move-only buffer and freeze it when publishing:

```rocket
let work = buffer.thaw([1, 2])
let grown = buffer.append(work, 3)
let immutable = buffer.freeze(grown)
```

Every consuming buffer operation moves its input. Reusing the old binding is an
`R4103` error. A frozen buffer becomes an ordinary immutable `Array[T]`; slices
remain retained immutable views and never expose aliases to mutable storage.

## Make concurrency boundaries explicit

`Send` means that ownership may cross a concurrency boundary. `Share` means a
value may be observed concurrently. Rocket derives both structurally and does
not allow user-written unsafe implementations. Pointers, opaque native handles,
unique buffers, guards, and task groups are intentionally rejected at the
wrong boundary with `R4101`-`R4104`.

The public scheduler is one bounded default pool. Calling an `async fn` queues
work and returns `Task[T]`; it does not create an OS thread per call. Use
`thread.spawn(task)` only when a dedicated coordination thread is required.

```rocket
async fn compute(value: Int) -> Result[Int, String]:
    return Ok(value + 1)

let pending = compute(41)
match task.join(pending):
    case Ok(value):
        print(value)
    case Err(message):
        print(message)
```

Async functions must return `Result[T, String]`, and `T` must be `Send`.
`await` is valid only inside an async function and also produces
`Result[T, String]`. Existing postfix `?`, loops, matches, and returns keep their
normal ownership cleanup behavior.

## Handle cancellation, deadlines, and closure

Cancellation is cooperative. Pass a token to waits and I/O, use absolute
deadlines from `time.monotonic_milliseconds()`, and continue to handle ordinary
`Result` errors. Cancelling a completed task returns `false`; cancellation does
not retroactively change its result.

Channels in 1.8 are bounded FIFO queues. Their constructors take an initial
array and a capacity; send applies backpressure, receive returns `None` after
the queue is drained and all senders close, and all waits accept a deadline and
cancellation token. Rocket 1.8 does not expose an unbounded channel.

Task groups take a finite array of already-created tasks. `group_join` returns
results in input order and the first input-order failure. Dropping an unjoined
group cancels and joins every child before destruction. There is no dynamic
group-spawn API in 1.8.

## Windows asynchronous I/O limits

The new async file, socket, process, and timer APIs use the bounded default
worker pool. File and socket operations are worker-blocking rather than IOCP
overlapped. Process tasks return an exit code and inherit standard streams;
they do not capture output. Synchronous Rocket 1.5 APIs remain available when
these limits are a better fit.

See `CONCURRENCY.md`, `STDLIB.md`, and the Rocket 1.8 syntax dictionary for the
normative signatures and memory-ordering rules.
