# Rocket Ownership, Concurrency, and Asynchronous I/O Contract 1.8

Rocket 1.8 adds safe concurrency without changing valid Rocket 1.0-1.7 source.
This document is normative for weak ownership, unique buffers, values crossing
threads, tasks, synchronization, cancellation, and the portable Phase 19
asynchronous surface. Recoverable failures remain `Result`; absence remains `Option`; safe
Rocket still has no null value and no exception path.

## Design and compatibility rules

Rocket uses three explicit mechanisms instead of hidden universal locking:

- `Weak[T]` expresses non-owning graph edges and prevents ARC cycles;
- `UniqueBuffer[T]` expresses one mutable owner and moves at consuming calls;
- structural `Send` and `Share` checks reject race-prone publication.

An allocation begins thread-confined with a plain strong count. Publication
through a task, thread, mutex, once cell, channel, or asynchronous request
recursively promotes the reachable strong graph to atomic ownership. Existing
`Array`, `Slice`, `ByteBuffer`, synchronous I/O, native ABI, no-null, and
deterministic MIR cleanup rules keep their prior meaning.

## Weak ownership and the cycle policy

`Weak[T]` is a managed non-owning reference to an identity-bearing `Share`
array, aggregate, or task. `String`, scalar, `Slice`, `UniqueBuffer`, pointer,
and opaque-native values are not valid weak targets.

```rocket
import std.ownership

let observer: Weak[Node] = ownership.downgrade(node)
match ownership.upgrade(observer):
    case Some(live):
        print(live.name)
    case None:
        print("expired")
```

| Function | Result | Contract |
| --- | --- | --- |
| `downgrade[T](value: T)` | `Weak[T]` | Creates a non-owning edge; `T` must be identity-bearing and `Share` |
| `upgrade[T](weak: Weak[T])` | `Option[T]` | Atomically acquires an owned value or returns `None` |
| `expired[T](weak: Weak[T])` | `Bool` | Snapshots whether upgrade is already impossible |

The payload is destroyed exactly once after the last strong release. Its
control storage remains through the last weak release. Upgrade racing with
destruction is all-or-nothing and never exposes a partially destroyed value.

Rocket uses cycle prevention, not tracing collection. Immutable construction
prevents a value from being rewired back into itself; recursive models use weak
parent, observer, cache, and owner links. Weak edges may appear in structs,
enums, arrays, closures, task inputs, and channel values. Unsafe native code is
responsible for cycles created outside this model. A deliberately strong cycle
is not collected automatically.

## Unique buffers

`UniqueBuffer[T]` is move-only contiguous mutable storage. It is intended for
construction and I/O transfer. `Array[T]` remains an immutable snapshot with
copy-on-write updates.

| Function | Result | Ownership |
| --- | --- | --- |
| `thaw[T](values: Array[T])` | `UniqueBuffer[T]` | Copies the input snapshot into unique storage |
| `length[T](buffer: UniqueBuffer[T])` | `Int` | Borrows |
| `capacity[T](buffer: UniqueBuffer[T])` | `Int` | Borrows |
| `get[T](buffer: UniqueBuffer[T], index: Int)` | `T` | Borrows and returns an owned element |
| `set[T](buffer: UniqueBuffer[T], index: Int, value: T)` | `UniqueBuffer[T]` | Consumes and returns the buffer |
| `append[T](buffer: UniqueBuffer[T], value: T)` | `UniqueBuffer[T]` | Consumes and returns the buffer |
| `slice[T](buffer: UniqueBuffer[T], start: Int, end: Int)` | `UniqueBuffer[T]` | Consumes and returns a copied unique range |
| `freeze[T](buffer: UniqueBuffer[T])` | `Array[T]` | Consumes mutation authority and returns a snapshot |

Bounds failures use the existing fatal collection-bounds rule. Assignment,
return, aggregate construction, and consuming parameters move a unique value;
use after move is `R4103`. A reusable closure cannot capture a move-only value.
`UniqueBuffer[T]` requires `T: Share`, is therefore `Send`, and is never
`Share`. The element constraint makes repeated `get` safe while still allowing
managed elements under ordinary ARC ownership. No writable slice aliases
another buffer or a frozen array.

## `Send`, `Share`, and scoped values

The compiler derives two closed structural properties. Source cannot add an
unsafe blanket implementation.

- `Send` permits ownership transfer to another thread or executor.
- `Share` permits concurrent immutable access or use of an internally
  synchronized handle. Every `Share` value is also `Send`.

Scalars and `String` are `Send + Share`. `Array[T]` and `Slice[T]` derive the
properties from `T`; structs, enums, and closure captures derive them from all
reachable fields and payloads. Move-only status is likewise derived through
arrays, slices, options, results, structs, and enums. `Weak[T]` requires an
identity-bearing `T: Share`. `Task[T]` is `Send` only when `T: Send`, is never
`Share`, and consequently cannot be a weak target. `Thread[T]` is likewise
never `Share` and cannot be weakly observed.

Cancellation tokens, mutexes, events, integer atomics, once cells, channels,
senders, and receivers are synchronized `Send + Share` handles. A
`UniqueBuffer`, `Task`, `LockGuard`, `TaskGroup`, and `Thread` is move-only.
`LockGuard` and `TaskGroup` cannot escape their function. Native pointers,
opaque native handles, native callbacks, and `std.string.Builder` are neither
`Send` nor `Share`. Race-prone foreign access therefore remains inside an
explicit `unsafe:` region and outside Rocket's guarantees. An `unsafe:` block
does not grant either property, so a native pointer still cannot be captured by
a task. `Thread[T]` is `Send` when `T` is `Send`, but is never `Share`.
Primitive payloads are valid at mutex, once, channel, task-group, and thread
boundaries; the runtime ABI preserves their exact type.

Stable diagnostics are:

- `R4101`: a task/thread/channel/synchronized-storage value does not satisfy
  its `Send` or `Share` constraint;
- `R4102`: a weak target is not an identity-bearing `Share` value;
- `R4103`: a move-only value is used after consumption;
- `R4104`: a scoped concurrency value escapes;
- `R4105`: `await` is outside an async body or its operand is not a task;
- `R4106`: an async signature or suspension-local violates the async contract.

## Atomic shared ARC

Promotion is idempotent and traverses strong edges only. The publication
boundary still owns the graph while it is promoted. Atomic retain is relaxed;
final release is a release decrement followed by an acquire fence; weak
upgrade uses acquire compare/exchange. Publication and synchronized
consumption form release/acquire edges. Promotion does not change value identity
or observable destruction order. Graphs that never cross a concurrency
boundary retain the non-atomic fast path.

## Async functions, tasks, and the default pool

`async` precedes `fn`. An async function must return `Result[T, String]`, and
`T` plus every argument must satisfy `Send`.

```rocket
async fn child(value: Int) -> Result[Int, String]:
    return Ok(value + 1)

async fn parent() -> Result[Int, String]:
    let result = await child(41)
    return result
```

Calling `child` evaluates and owns its arguments left to right, queues its body,
and returns `Task[Int]`. Prefix `await` is valid only in an async body and
produces `Result[Int, String]`. `?`, loops, matches, returns, and ordinary MIR
cleanup continue to apply inside an async body.

The implementation is a bounded stackful executor rather than a second
coroutine ownership system. It creates 1 through 64 workers from the reported
hardware concurrency and has a fixed 65,536-entry FIFO queue. The permanent
stage0 fallback uses the same bounded worker/queue policy. A worker awaiting
same-pool work pumps queued work, preventing child-wait pool deadlock. Managed
locals remain owning MIR locals on the worker stack and cleanup exactly once.

Task states are `queued -> running -> completed`. Cancellation before start
publishes one cancellation error. Running work observes cancellation
cooperatively at await and explicit wait/I/O points. Completion is immutable;
a late `task.cancel` returns `false`. `Task[T]` is an affine handle: `join` and
`await` consume it exactly once, while `is_complete` and `cancel` borrow it.
This prevents repeated extraction from aliasing a move-only result. At process exit the singleton executor stops
accepting work, drains its bounded queue, and joins every worker. Rocket 1.8
does not expose user-created executor pools; the required thread pool is the
bounded default executor used by all tasks and asynchronous operations.

| Function | Result |
| --- | --- |
| `task.join[T](task: Task[T])` | `Result[T, String]` |
| `task.is_complete[T](task: Task[T])` | `Bool` |
| `task.cancel[T](task: Task[T])` | `Bool` |

## Dedicated threads and structured groups

`thread.spawn` consumes an already-created `Task[T]`. It creates one dedicated
native coordination thread that owns and awaits that task. This keeps the
source boundary typed and prevents borrowed closure state from escaping.

| Function | Result |
| --- | --- |
| `thread.spawn[T](task: Task[T])` | `Result[Thread[T], String]` |
| `thread.join[T](thread: Thread[T])` | `Result[T, String]` |
| `thread.detach[T](thread: Thread[T])` | `Result[Bool, String]` |
| `thread.is_complete[T](thread: Thread[T])` | `Bool` |

Join and detach consume the handle and are accepted once. Dropping an unjoined
handle joins it; a detached worker owns its cleanup through completion.

A task group takes a finite array of already-created tasks and owns them in
array order:

| Function | Result |
| --- | --- |
| `task.group[T](tasks: Array[Task[T]])` | `TaskGroup[T]` |
| `task.group_cancel[T](group: TaskGroup[T])` | `Bool` |
| `task.group_join[T](group: TaskGroup[T])` | `Result[Array[T], String]` |

`group_cancel` borrows; `group_join` consumes. Join waits for every child and
returns successes in input order. The first error in input order is returned;
later unfinished children are cancelled and still joined. Dropping a group on
return, `?`, or another cleanup path cancels unfinished children and joins all
of them before destruction. Groups may be nested through ordinary function
scope, but Rocket 1.8 does not dynamically add children after construction.

## Synchronization

Deadlines below are absolute `Int` values from
`time.monotonic_milliseconds()` or `async_time.deadline_after`.

```text
mutex[T](value) -> Mutex[T]
lock[T](mutex, deadline, token) -> Result[LockGuard[T], String]
guard_get[T](guard) -> T
guard_set[T](guard, value) -> Bool
unlock[T](guard) -> Result[Bool, String]

event(manual_reset, initially_set) -> Event
event_set(event) -> Bool
event_reset(event) -> Bool
event_wait(event, deadline, token) -> Result[Bool, String]

atomic_int(value) -> AtomicInt
atomic_load(value) -> Int
atomic_store(value, replacement) -> Unit
atomic_fetch_add(value, delta) -> Int
atomic_compare_exchange(value, expected, replacement) -> Bool

once[T](value) -> Once[T]
once_empty[T](type_witness) -> Once[T]
once_set[T](cell, value) -> Result[Bool, String]
once_get[T](cell) -> Option[T]
```

Mutex and once values must be `Share`. Guard reads/writes borrow;
unlock consumes; dropping a live guard unlocks it. Rocket has no exception
poison state: recoverable errors do not poison, and a panic terminates. Event
waits use a locked predicate, preventing lost wakeups; auto-reset consumes one
set. Integer atomics are sequentially consistent. `once(value)` publishes its
seed immediately; `once_set` therefore returns `Ok(false)` for that initialized
cell and `once_get` returns an owned `Some(value)`. `once_empty(witness)` uses
its argument only to infer `T`. Concurrent `once_set` calls linearize under the
cell lock: exactly one returns `Ok(true)`, and every later getter observes that
immutable value.

## Channels

Channel construction takes initial values so `T` is always inferable:

```text
bounded[T](initial: Array[T], capacity: Int) -> Result[Channel[T], String]
unbounded[T](initial: Array[T]) -> Result[Channel[T], String]
sender[T](channel) -> Sender[T]
receiver[T](channel) -> Receiver[T]
clone_sender[T](sender) -> Sender[T]
clone_receiver[T](receiver) -> Receiver[T]
send[T](sender, value, deadline, token) -> Result[Bool, String]
receive[T](receiver, deadline, token) -> Result[Option[T], String]
close_sender[T](sender) -> Result[Bool, String]
close_receiver[T](receiver) -> Result[Bool, String]
```

`T` must be `Send`; scalar and managed payloads use the same typed contract. A
bounded capacity is 1 through 65,536 and
cannot be smaller than `initial.length`; a full send applies backpressure until
space, cancellation, deadline, or disconnection. The resource-limited
unbounded form permits 1,048,576 pending values, then returns `Err`. FIFO order
is commit order under the channel lock. Receive returns `Some(value)`, or
`None` after all senders close and the queue drains. Endpoint close and drop
wake all waiters. Close/send/receive races linearize under one channel lock.

## Cancellation, monotonic time, and timers

```text
cancel.token() -> CancellationToken
cancel.child(parent) -> CancellationToken
cancel.current() -> CancellationToken
cancel.cancel(token) -> Bool
cancel.is_cancelled(token) -> Bool
cancel.check(token) -> Result[Bool, String]

async_time.deadline_after(milliseconds) -> Result[Int, String]
async_time.remaining(deadline) -> Int
async_time.sleep(milliseconds, token) -> Task[Bool]
async_time.sleep_until(deadline, token) -> Task[Bool]
```

Cancellation is cooperative and idempotent; parent cancellation is observed by
children. `cancel.current()` returns the running task's token or a new detached
token outside a task. Observation points are task start, await, timer loops,
mutex/event/channel waits, file chunk boundaries, socket operation boundaries,
and process wait polling. An explicit operation token and its current task
token are both observed. Deadlines use the monotonic clock. Negative durations
are errors; an elapsed absolute deadline fails immediately. Timer waits use a
native waitable timer on Windows and a bounded monotonic condition wait on
Linux/macOS; all observe cancellation in bounded slices on the default executor,
so cancellation cannot be lost or double-complete a task.

## Native asynchronous files, sockets, and processes

The synchronous Rocket 1.5 APIs remain unchanged. Rocket 1.8 adds:

```text
async_file.read(path, maximum, token) -> Task[UniqueBuffer[Char]]
async_file.write(path, bytes, append, token) -> Task[Bool]

async_net.connect(host, port, deadline, token) -> Task[Int]
async_net.accept(listener, deadline, token) -> Task[Int]
async_net.receive(socket, maximum, deadline, token) -> Task[UniqueBuffer[Char]]
async_net.send(socket, bytes, deadline, token) -> Task[Int]
async_net.shutdown(socket) -> Result[Bool, String]

async_process.run(program, arguments, deadline, token) -> Task[Int]
```

The implementation uses the bounded default executor plus native platform
waits; it does not create an unbounded operating-system thread per I/O request.
Windows files use `FILE_FLAG_OVERLAPPED`, per-operation events,
`GetOverlappedResult`, and `CancelIoEx`; Linux/macOS file work uses bounded
native file operations. Work advances in 64 KiB chunks with a 64 MiB request
cap.
Read consumes the known file size up to `maximum`; an empty successful buffer
is EOF. Write continues through the full buffer or returns an error.

Socket calls use nonblocking Winsock readiness on Windows and POSIX socket
readiness on Linux/macOS with the remaining deadline.
`receive` may return fewer than `maximum` bytes, including an empty buffer for
orderly shutdown. `send` returns the actual byte count, so callers that require
a complete write must loop until their buffer is exhausted. A successful
connect or accept that loses a cancellation race is closed before cancellation
is returned. `shutdown` closes the runtime socket handle and prevents reuse.

Process execution uses `CreateProcessW` on Windows and `fork`/`exec` on POSIX,
with direct UTF-8 argument handling and bounded waits. Deadline or cancellation
terminates only the directly created child and waits for its handle before
completing. The result is the exit code. Rocket deliberately inherits the child's standard streams and
does not yet expose output capture; this is a documented limitation, not a
silent truncation behavior. Socket and process coordination remains
bounded-worker blocking rather than a general OS completion dispatcher; Windows
file transfer itself is overlapped.

All handles and buffers stay owned until the single task completion is
published. Timeouts, cancellation, close races, resource limits, and platform
failures are ordinary `Err(String)` outcomes.

## Compiler and bootstrap agreement

The C++ stage0 compiler and Rocket-written compiler use the same surface types,
`R4101`-`R4106` checks, explicit async-call/await MIR, move/scoped rules, and
runtime names. Stage0 emits RAII values plus its own bounded default executor;
the production LLVM backend emits opaque runtime ABI v1 calls. Bootstrap
determinism compares stage2 and stage3 compiler output, so concurrency support
cannot depend on generated artifacts or an unpinned external runtime.
