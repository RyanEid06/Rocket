# Rocket 1.8 Release Contract

Rocket 1.8 completes robust ownership, safe concurrency, and asynchronous I/O
on the self-hosted Rocket 1.7 foundation. It is additive for Rocket 1.0-1.7
source, retains the C++20 compiler permanently as stage0, targets the pinned
Windows x64 toolchain, and preserves runtime ABI v1 through additive entry
points and opaque control-block changes.

## Contract

- typed weak references and safe strong-cycle prevention;
- move-only unique mutable buffers with Array/ByteBuffer compatibility;
- structurally derived `Send`/`Share` rules and scoped guard/group enforcement;
- atomic ARC promotion only at checked publication boundaries;
- dedicated thread handles, a bounded default pool, tasks, structured groups, channels,
  mutexes, events, integer atomics, and once initialization;
- cooperative cancellation, monotonic deadlines, timers, `async fn`, and
  prefix `await`;
- bounded-executor asynchronous files, sockets, cancellable child processes,
  and timers without an unbounded thread-per-operation implementation.

All expected scheduling, cancellation, timeout, disconnect, close, and host I/O
failures are `Result` values. The exact syntax, typing, ordering, ownership,
resource bounds, memory ordering, shutdown, and race behavior are normative in
`CONCURRENCY.md`; the public function table is normative in `STDLIB.md`.

## Compatibility and deliberate limits

Existing synchronous APIs remain available and retain their blocking behavior.
Rocket 1.8 does not add general borrowed references, shared mutable fields,
implicit thread detachment, exception unwinding, a tracing garbage collector,
native-pointer thread safety, work-stealing order guarantees, or a portable
non-Windows async backend. File/socket operations are worker-blocking rather
than IOCP-overlapped, process tasks inherit standard streams rather than
capturing them, only the bounded default pool is public, and task groups are
constructed from an existing finite task array. Multi-platform event systems
remain Phase 19 work.

## Release gate

A Rocket 1.8 artifact is releasable only after dependency verification; Debug
and Release LLVM builds; Debug and Release LLVM-disabled stage0 builds; focused
weak/unique/send-share/MIR/LLVM/native concurrency suites; repeatable bounded
contention, cycle, exactly-once destruction, cancellation, deadline, channel,
pool, scheduler-shutdown, and asynchronous-I/O stress; stage0/self-hosted parity; deterministic
stage0 through stage3 bootstrap; conformance and performance gates; packaging;
and sanitized relocation all pass. Stage2 and stage3 compiler IR must be byte
identical. Exact commands, counts, timings, hashes, and remaining limitations
belong in `PROJECT_CONTEXT.md`; this document must not claim unobserved results.
