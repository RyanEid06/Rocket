# Phase 18 Requirement-to-Evidence Audit

This audit records the final Rocket 1.8 ownership, concurrency, and asynchronous
I/O vertical slice. It supplements the normative contracts in `CONCURRENCY.md`,
`SPEC.md`, and `STDLIB.md`; it is evidence, not a second specification.

## Prerequisite and scope

Phase 16 and Phase 17 were already recorded as completed in
`PROJECT_CONTEXT.md` before Phase 18 began. Phase 18 preserves the C++20
compiler as stage0, implements the same checked surface in the Rocket-written
compiler, targets the pinned Windows x64 toolchain, and does not begin Phase 19
or Phase 20 work.

## Objective coverage

| Objective | Contract and implementation | Direct evidence |
| --- | --- | --- |
| Weak references and cycles | `CONCURRENCY.md` defines typed downgrade/upgrade/expiration, identity-bearing `Share` targets, atomic upgrade, exactly-once payload destruction, and weak-back-edge cycle prevention. `runtime.cpp`, `hir.cpp`, and `compiler/src/main.rocket` implement it. | Runtime tests cover 10,000 self-cycle allocations, a weak-broken multi-object cycle, expired upgrade, concurrent upgrade/destruction, and leak counters. `phase18_weak.rocket` covers recursive weak models; `phase18_weak_share_failure.rocket` proves R4102 parity. |
| Unique mutable buffers and COW | `UniqueBuffer[T]` is move-only, non-`Share`, requires `T: Share` for safe repeated reads, and has consuming mutation/slice/freeze operations without changing `Array` snapshots. | Runtime tests cover unaliased in-place mutation, source-array isolation, managed elements, transfer, slice/freeze, and lifetime. Bounds, nested move-only elements, reusable-closure capture, direct reuse, and transitive aggregate reuse have dedicated positive/negative fixtures. |
| Thread-safety type rules | Structural `Send`/`Share`, recursive fixed-point derivation, transitive move-only derivation, scoped values, and unsafe/native exclusions are specified in `CONCURRENCY.md` and checked by both compilers. Publication promotes reachable ARC graphs. | Positive scalar/managed/thread-transfer fixtures plus C++ and self-hosted negative fixtures for unsafe capture, async result, suspension, weak `Share`, native pointer transfer, task/buffer/aggregate moves, mutex payload sharing, and scoped escape exercise R4101-R4106. |
| Threads and synchronization | The runtime supplies joinable/detachable thread handles, the bounded default executor, mutex/guard, event, sequentially consistent `AtomicInt`, and seeded/empty `Once`. Guard/group moves and escape are checked statically. | Runtime contention covers 512 tasks, 1,000 real executor startup/drain/shutdown cycles, mutex deadlines, 8x10,000 atomic increments, event wake/cancel, 8,000 once reads, and 16-way exactly-one initialization. Thread fixtures cover scalar results and `Thread[Int]` transfer; guard-after-unlock is rejected. |
| Channels and tasks | Bounded and resource-limited unbounded channels define FIFO, backpressure, close/drain, clones, deadlines, cancellation, and disconnection. Tasks own immutable completion but expose it through one consuming join/await; status and cancellation borrow. The singleton pool drains on exit. | Runtime tests cover scalar and managed payloads, FIFO, blocked send release, receiver disconnect, final-sender close, drain-to-`None`, the 1,048,576-value cap, internal completion synchronization, task cancellation, compiler rejection of task reuse, and shutdown with outstanding work. |
| Structured concurrency | Finite `TaskGroup[T]` values own children in input order; join aggregates deterministically, and drop/return/`?` cancel then join. Groups cannot escape. | Managed and primitive group ordering, abandoned-group cleanup, scoped-escape R4104, and `phase18_structured_cleanup.rocket` nested `?` cleanup all pass natively and in compiler parity checks. |
| Cancellation and timers | Cooperative idempotent parent/child tokens, current-task observation, monotonic deadlines, timeout composition, bounded observation points, and single completion are normative. Windows timers use waitable timers with bounded 2 ms cancellation observation. | Runtime and native fixtures cover cancellation before start, during timer/event/channel/process waits, after completion, child propagation, deadlines, and contention. Every CTest native wait has a finite timeout. |
| `async`/`await` | Parser/AST, typed HIR, explicit MIR async-call/await operations, verifier, C++ stage0 emission, LLVM lowering, and Rocket-written LLVM emission agree. Owning MIR locals remain live across stackful suspension; unsafe locals/results are rejected. | Parser, HIR, MIR-verifier, C++ emitter, LLVM IR, nested-await, `?`, cancellation, positive native, R4105 context, and R4106 suspension/result fixtures run in the focused suite. |
| Windows asynchronous I/O | A bounded executor dispatches overlapped 64 KiB file transfers using `FILE_FLAG_OVERLAPPED`, events, `GetOverlappedResult`, and `CancelIoEx`; timers use waitable timers; sockets use nonblocking Winsock readiness; processes use bounded child-handle waits. | Native file read/write/cancel/resource-limit, socket connect/accept/send/receive, process exit/cancel, timer, partial-I/O, EOF, timeout, shutdown, and relocation runs pass. No API creates an unbounded thread per operation. |
| Atomic shared ownership | Thread-confined ARC retains its plain-count fast path. Checked publication recursively and idempotently promotes strong graphs; retain/release/weak-upgrade use the documented memory ordering. | High-contention retain/release and weak-upgrade races pass; leak/destructor counters prove exactly-once destruction. Bootstrap and both runtime backends use the same ownership boundary rules. |

## Vertical implementation coverage

- Specifications and compatibility: `CONCURRENCY.md`, `SPEC.md`, `STDLIB.md`,
  `COMPILER_ARCHITECTURE.md`, decision D032, `DIAGNOSTICS.md`, the 1.8 syntax
  dictionary, migration guide, release contract, README, roadmap, and example.
- C++ stage0: HIR constraint/move checks, MIR verification, bootstrap emission,
  typed stage0 standard-library handles, and the permanent LLVM-disabled build.
- Rocket-written compiler: matching intrinsic catalog, recursive `Send`/`Share`
  analysis, move checks, async typing, and suffixed typed runtime ABI emission.
- Production backend/runtime: typed scalar/managed mutex, once, channel, task,
  group, and thread values; ARC promotion; executor; Windows event-backed I/O;
  additive runtime ABI v1 exports.
- Tests: parser, HIR, MIR, bootstrap codegen, LLVM IR, runtime, positive and
  negative native fixtures, stage0/self-hosted parity, conformance,
  performance, package, and sanitized relocation.

## Acceptance and stress mapping

| Required category | Evidence |
| --- | --- |
| Safe data-race rejection / explicit unsafe | Structural `Send`/`Share` checks, `phase18_send_failure`, `phase18_pointer_send_failure`, and positive `phase18_unsafe_local`. An `unsafe` block does not grant transfer traits. |
| Cycles, leaks, exactly-once destruction | Weak self/multi-object/race tests, managed-element buffer/channel/group/thread cleanup, ARC leak counters, and high-contention destructor counter. |
| Deadlock-oriented bounded waits | Same-pool helping, finite mutex/event/channel/process deadlines, CTest `TIMEOUT` properties, and bounded executor shutdown tests. |
| Channel contention and races | Backpressure, FIFO, close/drain, sender/receiver disconnect races, cancellation, scalar/managed ownership, and hard-cap exhaustion. |
| Task lifetime / structured scope | Owned async captures, affine task R4103, scoped-group R4104, abandoned/nested group cancellation+join, early `?`, and thread join/detach/drop cleanup. |
| Cancellation state transitions | Before start, active waits/I/O, parent-child propagation, after immutable completion, and contended event/channel/process paths. |
| Async files/sockets/processes/timers | Native fixtures plus conformance and relocated-package execution cover every shipped Windows async family, partial results, close/shutdown, deadlines, cancellation, and limits. |
| Scheduler startup/shutdown | 512-task contention and 1,000 constructions of the real one-worker executor, each with queued outstanding work followed by drain and join. |
| Compiler parity and determinism | Fifteen negative and fourteen positive Phase 18 fixtures pass through stage0, stage1, stage2, and stage3. The final-source bootstrap produced byte-identical Release stage2/stage3 LLVM IR. |

## Observed validation

All commands ran from the repository root against the final source with the
pinned Windows x64 toolchain.

| Command | Observed result |
| --- | --- |
| `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\dependencies\verify.ps1` | Passed: Git 2.54.0.windows.1, CMake 4.3.2, Ninja 1.13.1, Clang/LLVM 22.1.6, MSVC 19.51.36252 x64, raylib 6.0. The fixed verifier requires every native version command to exit zero. |
| `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Debug` | Passed 210/210 in 481.59 seconds CTest time; 528.3 seconds end-to-end. |
| `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Release` | Passed 210/210 in 133.59 seconds CTest time; 176.0 seconds end-to-end. |
| `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-stage0.ps1 -Configuration Debug` | Passed 165/165 in 392.80 seconds CTest time; 410.2 seconds end-to-end. |
| `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-stage0.ps1 -Configuration Release` | Passed 165/165 in 349.03 seconds CTest time; 387.8 seconds end-to-end. |
| Pinned-environment `ctest --test-dir out/build/windows-release -C Release -L phase18 --output-on-failure` | Passed 78/78 twice in 24.35 and 24.20 seconds CTest time (26.2 and 26.0 seconds end-to-end). The selection is 77 labelled tests plus its self-host compiler fixture dependency. |
| `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\bootstrap.ps1 -Configuration Release` | Passed in 489.4 seconds. Stage0-stage3 agreed on all 14 positive and 15 negative Phase 18 fixtures. Stage2/stage3 IR is byte-identical at SHA-256 `d2bb814269f3c05fd823b21e6ab0e6b908fcca99f37d28d5fd1130a50d01ba23`; checksum-file SHA-256 is `c1a42db7de91fb2812e0ae8fd5a9b60e17ddc912035de2e00c5fddfa1d2b72f8`. |
| `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\conformance.ps1 -Configuration Release` | Passed 90/90 in 29.3 seconds; report SHA-256 `0e98a67c23332df7311059828172288f1f8bb61af0c426dcaa9d69da30b04d8c`. |
| `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\performance.ps1 -Configuration Release` | Passed all 11 budgets in 98.4 seconds end-to-end. Measurements were 0.009, 0.181, 43.937, 50.155, 0.133, 0.351, 0.082, 0.534, 0.020, 0.186, and 0.172 seconds; report SHA-256 `22007e4ca181623b360c1b9f2d90a5e9a9b648933d2880b74d15827dce610780`. |
| `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package-compiler.ps1 -Configuration Release` | Passed in 496.4 seconds: final Release rebuild, bootstrap, self-contained packaging, checksums, and sanitized relocation, including relocated Phase 18 compilation/execution. `rocket-1.8.0-windows-x64.zip` is 268,864,815 bytes at SHA-256 `0311bb7f305d6e2d3c23eb18ba34040cb9267261fd5a52fe31e9dba3c3cb4576`; its 913-entry checksum file is `79ca17067d58585421d4867b1f2af01d65783895bad1eb55375a0bf9c71423d6`, and packaged bootstrap checksum-file SHA-256 is `f089af554078ebd0b558773036701b92626628fefdf76a2d9cd9f849b5792d93`. |

Generated `out`, dependency, compiler `.rocketc`, fixture `.rocketc`, and
example `.rocketc` trees remain ignored.

Validation found no compiler/runtime defect. Three genuine validation bugs were
fixed: tool version commands could fail without failing dependency verification;
bootstrap did not explicitly compare the full Phase 18 positive/negative set
across all four stages; and sanitized relocation did not compile and execute a
Phase 18 program.

## Deliberate limitations

- Windows x64 is the only async backend in 1.8.
- Files use overlapped I/O, but socket/process coordination is bounded-worker
  blocking rather than a general IOCP dispatcher.
- Child processes inherit standard streams; output capture is not exposed.
- Only the singleton bounded executor is public, and task groups accept a
  finite existing task array rather than dynamic spawning.
- Strong cycles require an explicit weak back edge; Rocket 1.8 has no tracing
  collector.
- The Release compiler HIR self-check retains the bounded 135-second ceiling;
  the final observed measurement was 43.937 seconds.
