# Rocket Compiler Architecture

This document defines the stable frontend boundaries introduced in Phase 3. The
parsed AST represents source syntax only. Name resolution and type checking
produce HIR, and control-flow lowering produces MIR. Backends consume MIR and
must not inspect the AST or HIR.

## Resolved HIR invariants

- Every declaration receives a deterministic `SymbolId` in source traversal
  order. The built-in `print` declaration is registered first.
- Every parameter, local-name expression, assignment target, loop variable, and
  call target refers to its declaration by `SymbolId`; later compiler stages do
  not perform string-based name lookup.
- Function calls use the module function namespace. Local bindings use lexical
  block scopes and may shadow bindings in enclosing scopes.
- Every HIR expression stores its checked `Type`.
- Function signatures are registered before bodies are resolved, so forward and
  recursive calls are resolved identically.
- HIR is returned only when parsing, resolution, and type checking have produced
  no diagnostics.
- Phase 12 impl members receive qualified `Owner.member` declaration names.
  Associated calls resolve that name directly; instance dot calls are rewritten
  after receiver typing by inserting the receiver as argument zero. MIR and both
  backends therefore continue to contain ordinary direct calls and require no
  method tables or runtime ABI change.

## Typed MIR invariants

- A MIR function owns a numbered local table and a numbered basic-block table.
  Parameters, source bindings, loop variables, and compiler temporaries are all
  explicit typed locals.
- Each basic block ends in exactly one `goto`, conditional branch, or return
  terminator. Control flow is represented only by those terminators.
- Every instruction result type matches its destination local type. Every local
  operand carries the type declared by its referenced local.
- Calls retain their resolved `SymbolId`; backends do not resolve names.
- `and` and `or` become branches and join blocks, preserving short-circuit
  evaluation independently of backend behavior.
- `while` and integer-range `for` loops become explicit condition, body,
  increment, and exit edges. `break` and `continue` are lowered to `goto` edges.
- Range bounds are evaluated once, from left to right, before iteration. The end
  value is stored in a compiler temporary.
- MIR verification runs before backend emission. Invalid block targets, missing
  terminators, mismatched types, and invalid symbol/local references are compiler
  errors rather than backend assumptions.
- Managed copies, replacements, and function exits contain explicit MIR
  `retain` and `release` instructions. Parameters are borrowed, non-parameter
  managed locals own one reference, and managed returns transfer a +1 reference.
- Array construction, checked collection indexing, and exclusive slicing are
  explicit typed MIR rvalues rather than backend-recognized syntax.
- Copy-on-write Array element replacement is an explicit typed MIR rvalue. It
  consumes a borrowed Array, checked Int index, and exactly typed element, and
  produces an updated owned Array value before the source `var` is rebound.
- Rocket 1.8 async calls and awaits are explicit typed MIR rvalues. An async
  call owns a concrete argument frame and callback symbol; await borrows its
  task and returns the task's `Result[T, String]` outcome. Backends do not infer
  suspension from ordinary calls.
- Move-only and scoped-value validation completes in HIR. MIR never contains a
  read of a moved local or an escaping lock guard/task group.

Future aggregate and generic types must extend these invariants without
weakening them.

## Structural types and specialization

- `Type` is an immutable structural value with a kind, nominal declaration
  identity, and recursively nested arguments. Scalar constants remain canonical
  values; `Array[T]`, `Slice[T]`, structs, and enums no longer consume cases in
  a closed built-in enumeration.
- HIR contains a deterministic type-declaration table for structs and enums.
  Field and variant payload types may contain type-parameter nodes and are
  substituted with concrete nominal arguments at each use.
- Generic functions are inferred from concrete call operands and monomorphized
  before MIR. The specialization key is the function name plus canonical
  structural type spellings, so repeated and recursive calls reuse one symbol.
- HIR match cases store resolved enum tags and payload symbols. Exhaustiveness,
  duplicate variants, binding arity, wildcard position, and return paths are
  checked before MIR.
- MIR represents aggregate construction, field extraction, enum tag reads, and
  propagation explicitly. `?` becomes a success branch plus an early owned
  `None`/`Err` return; backends never reconstruct its semantics.

## Source module graph

The module loader resolves package-relative imports before HIR. It diagnoses
unreadable files, alias conflicts, private cross-module access, and DFS import
cycles. Public references are rewritten to deterministic fully qualified names.
Dependencies are traversed in stable postorder and the source graph is lowered
as one compilation unit in draft 0.6. This preserves deterministic symbol IDs
without introducing an unstable binary module format before packaging.

## Scalar LLVM backend

The production backend consumes only verified MIR and maps scalar values to the
following LLVM types on every Rocket 2.1 64-bit production target:

| Rocket type | LLVM value type | Function result ABI |
| --- | --- | --- |
| `Int` | `i64` | `i64` |
| `Float` | `double` | `double` |
| `Bool` | `i1` | `i1` |
| `Char` | `i8` | `i8` |
| `String` | opaque `ptr` | `ptr` (+1 owned) |
| `Array[T]` | opaque `ptr` | `ptr` (+1 owned) |
| `Slice[T]` | opaque `ptr` | `ptr` (+1 owned) |
| struct / enum | opaque `ptr` | `ptr` (+1 owned) |
| `Unit` | internal `i8` placeholder | `void` |

- MIR locals begin as entry-block stack slots initialized to the scalar zero or
  managed null value. The LLVM O2 pipeline promotes eligible slots and performs
  the production optimization pass sequence.
- MIR basic blocks and terminators lower directly to LLVM blocks, branches, and
  returns. Short-circuit behavior therefore remains encoded by MIR control flow.
- Rocket functions retain declaration-ID-qualified native names. A C-compatible
  `i32 @main()` wrapper invokes Rocket's required `fn main() -> Int` entry point.
- The explicit normalized target supplies the LLVM triple, data layout, object
  format, debug-information format, baseline CPU, and baseline features. x64
  uses the target's generic x86-64 baseline plus SSE2 and ARM64 uses the generic
  AArch64 baseline plus NEON; host CPU discovery never affects output.
- `emit-ir` prints verified unoptimized IR for inspection. `build`, `run`, and
  `emit-asm` run LLVM's O2 module pipeline; object and assembly emission use the
  LLVM target machine, and the pinned Clang driver performs native linking.
- Checked `Int` operations use LLVM signed-overflow intrinsics and explicit
  runtime-failure edges. Collection and String operations lower only through
  the Rocket runtime ABI; generated modules do not call `printf` or `strcmp`.

The C++ MIR transpiler remains buildable when `ROCKETC_ENABLE_LLVM=OFF` as the
reproducible stage0 fallback. LLVM-enabled builds never route normal production
commands through that transpiler. Stage0 `build`, `run`, and `emit-asm` invoke
the C++ compiler recorded by CMake, so the generated-code step uses the same
configured MSVC or compatible C++ toolchain instead of assuming `g++` is on
`PATH`.

## Runtime ABI v1 and ownership

The production linker combines each generated object with the statically built
Rocket runtime. `rocket_rt_abi_version()` reports version 1. ABI declarations
use `extern "C"`, fixed-width scalar arguments, and opaque pointers; C++ layout
is not exposed to generated code.

All managed runtime allocations begin with an internal strong-reference header
and a type-specific destructor. ABI v1 allocations begin thread-confined with a
plain reference count. Rocket 1.8 publication recursively promotes only the
reachable shared graph to atomic strong ownership; thread-confined
retain/release remains non-atomic. Null is accepted only by internal
retain/release operations to
support zero-initialized MIR storage; null is never a Rocket source value.

Ownership conventions are:

- A managed function parameter is borrowed at +0 for the duration of the call.
- A managed function result is returned owned at +1.
- Each non-parameter managed MIR local owns one reference. Copy assignment
  retains the new value before releasing the replaced value, making
  self-assignment safe.
- Managed temporaries from literals, calls, Array construction, String indexing,
  and slicing own their results. Function-exit cleanup releases all owning locals.
- FFI code must retain a borrowed value before storing it beyond the call and
  must release every +1 value exactly once.

`String` stores an owned valid-UTF-8 byte buffer plus an explicit 64-bit byte
length and trailing zero for C interoperability. The trailing zero is not part
of equality or length. Runtime printing uses the explicit length.

`Array[T]` owns contiguous zero-initialized element storage. Arrays of String
or another managed type retain every stored element and release them in deterministic order at
destruction. `Slice[T]` stores an owning reference to the backing Array plus an
offset and length; slicing a Slice flattens offsets while retaining the same
owner. Index and slice functions validate signed bounds before accessing data.
Phase 11 Array updates borrow the old allocation and return an updated Array at
+1. A reference count of one permits storage reuse; otherwise the runtime clones
the element buffer and retains every managed element before replacement. This
makes aliases and retained Slices stable snapshots while keeping the fast path
allocation-free. Both paths retain a managed replacement before releasing the
old element.

Runtime Arrays also carry an explicit capacity. Persistent `reserve` and
`append` return an owned Array at +1 and preserve their borrowed input. Storage
may be reused only when MIR proves a direct consuming rebind; otherwise the
runtime clones before changing capacity, length, or elements.
`pop` returns `None` for empty input or an owned `Some(Pop[T])` aggregate that
contains the updated Array and removed element. Managed elements are retained
before they become aggregate fields and are released exactly once when removed
from reused Array storage.

Phase 11 Maps and Sets are aggregate products containing insertion-ordered
Array snapshots. Canonical constructors remove later duplicates, and lookup
uses the existing scalar or length-aware String equality contract. Stable hash
entry points use the same FNV-1a bytes in the runtime and stage0 RAII library;
iteration never exposes an implementation-dependent lookup order.

Weak references keep an atomic control reference after the payload destructor
runs. Upgrade performs an acquire conditional strong increment, so it returns a
complete owner or reports expiration. Promoted retain is relaxed and final
release is release/acquire. Weak edges are not traversed during promotion.

The runtime reports bounds failures, invalid UTF-8, allocation failures,
reference-count corruption, integer overflow, and integer division by zero to
standard error and exits with status 101. Reference-count cycles remain a
historical Rocket 1.0 limitation; Rocket 1.8 safe construction prevents strong
cycles and exposes `Weak` for recursive back links.

Runtime aggregates store a deterministic enum tag, up to 64 typed field slots,
and a managed-field mask. Construction retains managed fields, managed field
reads return +1, and destruction releases fields in declaration order. This
opaque representation keeps generic specializations and nested aggregate types
on the same stable pointer ABI while scalar fields retain their native widths.

## Standard-library boundary

The module loader recognizes the reserved `std.*` namespace as virtual source
modules. HIR owns the public signatures and assigns an explicit `Intrinsic`
identity to each referenced library function. MIR verifies those calls by
resolved symbol and concrete parameter types; backends do not infer a library
operation from its source spelling.

LLVM lowers standard intrinsics to the `rocket_std_*` portion of runtime ABI v1.
It uses the opaque String, Array, and aggregate representations, so `Option`,
`Result`, JSON, and nested CSV data participate in ordinary MIR ARC. The Stage 0
C++ backend maps the same intrinsic identities to an isolated RAII compatibility
header, preserving no-LLVM bootstrap support for Phase 7 programs.

## Package and tooling front end

CLI target resolution happens before compilation. A standalone file supplies
its parent as the module root; a validated `rocket.toml` supplies one shared
root to the entry program and every discovered test. Manifest paths are lexical,
relative, and containment-checked. All artifacts for a package go to its ignored
`.rocketc` directory.

The formatter consumes lexer tokens so its spelling rules stay aligned with the
language, but retains source-line comments explicitly because comments are not
AST nodes. It is deterministic, idempotent, and stops on lexical diagnostics.
The test runner invokes the normal verified MIR/backend pipeline independently
for sorted test roots, so test execution has no alternate compiler semantics.

Diagnostics carry a stable enum identity from their originating layer. Printing
maps it to an `Rdddd` code consumed by golden tests and editor problem matchers;
the human message remains free to improve within that documented category.

## Self-hosted compiler

The production compiler implementation is `compiler/src/main.rocket`. The C++
implementation remains the permanent bootstrap stage0, while stage1 and later
perform every source, HIR, MIR, diagnostic, package, formatter, CLI, and LLVM
IR operation in Rocket. Generated compilers invoke pinned Clang/LLD only as an
object and link driver; no runtime callback can compile Rocket source.

Bootstrap MIR uses the same ownership contract as stage0 MIR: managed copies
contain explicit retain/release effects and each return block explicitly
transfers a managed result and releases every owning non-parameter local.
Canonical symbol allocation and textual IR make stage2/stage3 output directly
byte-comparable.

The Rocket compiler's internal generic `append` helper delegates to the public
`std.collections.append` intrinsic. That production use exercises the Phase 11
growth semantics throughout parsing and lowering during every bootstrap stage.

The self-hosted frontend recognizes the complete Phase 12 grammar: impls,
traits and constraints, expression lambdas, associated constants, and
user-defined iteration. Both compilers perform the same typed direct-call
rewrites, including lowering an immediately invoked lambda value to its generated
closure aggregate followed by the concrete `.call` symbol. Pending generated
callables retain enclosing generic substitutions so nested lambda annotations are
concrete when lowered. Canonical owner/trait-qualified symbols and structural
specialization keys keep selection deterministic across bootstrap stages.

Closures add compiler-generated immutable aggregate declarations and direct
`call` functions to HIR; captured managed values therefore use the existing ARC
MIR and backend paths. Lambda signatures inside generic functions are
substituted before their monomorphic closure declarations and `call` symbols are
queued. `for` over an iterable lowers to explicit cursor,
condition, value, body, advance, and exit blocks. Associated constants lower to
zero-argument functions. None of these features changes runtime ABI v1.

## Native ABI and library production

The parser records native declarations and unsafe blocks explicitly. Module
loading qualifies their Rocket names while preserving each unmangled C name.
HIR resolves `Pointer[T]`, native-layout, opaque, and callback types; validates
the closed ABI type set; expands native constants to typed literals; requires an
unsafe depth for imported calls; and turns compatible top-level function names
into typed callback references. MIR erases the lexical unsafe marker after HIR
validation and carries callback targets as resolved symbol constants. Native
pointer and handle values are excluded from managed retain/release insertion.

The LLVM backend declares imported C symbols, performs the `i1`/`i8` conversion
at Bool boundaries, materializes deterministic callback trampolines, and emits
`dllexport` wrappers for Rocket exports. Internal Rocket calling conventions and
runtime ABI v1 remain private and unchanged. The preserved C++ backend emits
equivalent `extern "C"` declarations/wrappers and uses the configured MSVC
compiler/librarian when LLVM is disabled.

Target resolution reads `[build]`, the selected `[target.<alias>]`, and only the
selected `[native.<alias>]` before compilation.
Executable linking consumes native libraries in manifest order; static output
archives the Rocket object; dynamic output links a DLL and import library.
Header and binding generation walk source declarations in canonical module
order and do not include timestamps, paths, hashes, or unordered-container
iteration. Stage0 and the Rocket compiler therefore produce byte-identical
native interface artifacts.

## Rocket 1.4 raylib validation architecture

Phase 14 validates the Phase 13 pipeline instead of adding a raylib-specific
compiler intrinsic. The pinned raylib 6.0 C target is statically linked behind
`rocket_raylib_adapter.lib`. That adapter owns all raylib by-value structures
and exposes a generated-header subset containing only `int64_t`, `double`,
`rocket_bool`, `uint8_t`, `void`, and one synchronous callback type. The package
manifest supplies the adapter, raylib, and Windows system libraries in
deterministic link order. Unresolved Windows `.lib` names are normalized to
Clang/LLD `-l` arguments by the production path while the LLVM-disabled stage0
path preserves MSVC library spelling.

The adapter registry gives each native object a positive token. Window and
audio devices are process singletons; frames are single-use; texture, font, and
sound maps own their corresponding raylib values. Every operation validates
state and token identity before reaching raylib. Registry counts, scripted
input, deterministic timing, and simulated devices are available only after
test mode is enabled, allowing lifecycle and stress tests on headless builders
while the same binary remains linked to the real raylib implementation.

`rocketc bind` generates the ignored low-level module before package analysis.
The handwritten Rocket wrapper is the safety boundary and contains every
`unsafe:` block. Stage0 and the self-hosted compiler resolve public generated
constants used as imported values and both test runners preserve package-native
link inputs. Bootstrap compares stage2/stage3 raylib binding output and builds
and tests the reference package without launching its interactive window.

## Rocket 1.5 production standard library

Phase 15 keeps `ByteBuffer` as the existing transparent,
immutable `Array[Char]` wrapper. `std.binary` is typed through ordinary virtual
standard signatures, lowers to explicit MIR calls, and uses runtime ABI entry
points that accept or return ARC aggregates. No byte-buffer-specific MIR type,
mutation rule, pointer exposure, or calling convention is introduced.

Binary reads and slices return `Result` for data-dependent bounds failures.
Unsigned integer writes also return `Result` for range failures. Little-endian
width is encoded in each function name and runtime entry point. String decoding
validates complete UTF-8, rejecting overlong forms, surrogate code points,
invalid continuations, and scalar values above U+10FFFF. File binary APIs move
the same buffer representation through synchronous byte-preserving streams and
translate host failures to `Err(String)`.

The same typed-intrinsic pattern carries bounded buffered streams, Unicode,
safe regular expressions, cryptography, networking/HTTP, calendars, operational
helpers, compression, validated archives, and SQLite. Stateful host resources
are stored behind kind-checked process-local tokens and cross HIR/MIR only as
`Int`. The runtime and stage0 use the same public signatures, bounds, close
rules, and `Result` layout.

`std.testing` demonstrates the ordinary-source path. Its public assertion and
fixture facade is loaded from `stdlib/std/testing.rocket`; only secure temporary
root creation/cleanup and coverage-counter storage remain in the private
`std.testing_core` host boundary. Installed compilers discover the sibling
`stdlib` tree, while repository builds use the configured source root. The C++
module loader and self-hosted loader apply identical resolution policy.

LLVM-disabled stage0 executables reserve an 8 MiB Windows stack. The fallback
backend materializes recursive compiler AST/HIR values as C++ RAII objects, so
the self-hosted compiler's full-source validation needs more than the PE default
1 MiB stack after the Rocket 1.5 library surface is registered. This is a link
property of generated stage0 executables, not a language ABI or heap limit.

## Rocket 1.6 package architecture

The package subsystem is a Stage 0 library beside the parser and backends, but
it never enters frontend semantics. It parses canonical manifests/lockfiles,
resolves a single version per name, verifies signed registry state, acquires
bounded HTTPS or immutable Git sources, and commits verified trees into
`<root>/.rocketc/cache/sha256/<digest>`. Registry and Git processes receive
separated arguments; dependency metadata never becomes a shell command.

Target resolution converts every locked package to a `PackageDependencyRoot`.
The module loader tracks the owning root of each loaded source and permits only
the direct edges recorded for that owner. Package names take precedence over
local paths, transitive modules retain an owner-qualified internal prefix, and
all source trees are rehashed before compilation. Approved native files are
resolved to regular files inside the verified tree before entering the existing
Phase 13 linker pipeline. Build scripts have no execution path.

The Rocket-written compiler implements the same graph boundary in its module
loader. Package-security CLI commands are forwarded, with inherited standard
input/output and an unchanged argument vector, to the fixed Stage 0 security
host selected by `ROCKET_STAGE0` or the installed relative layout. The host is
not invoked while stage1/stage2/stage3 compile the compiler itself. `/Brepro`
is passed to LLD by both compiler implementations so repeated locked builds do
not embed a varying PE timestamp.

## Rocket 1.7 tooling architecture

The language server is another consumer of the compiler pipeline, not a fork.
`ModuleLoader` accepts a normalized `SourceOverlays` map; package/lock roots and
standard declarations flow through the ordinary lexer, parser, semantic
analyzer, HIR symbols/types, and diagnostics. A bounded semantic snapshot adds
editor indexes (definitions, resolved occurrences, documentation, tokens) and a
small AST fallback only for declarations in incomplete code. Document versions
and graph generations control invalidation and stale-result suppression.

MIR instructions and terminators retain their originating `Location`. The LLVM
backend lowers those locations into CodeView line tables, functions and locals
into PDB records, and the driver writes `rocket-source-map-1`. Synthetic
`rocket:\source` directories prevent checkout paths entering native debug
records. Coverage and profile hooks are optional lowering flags calling the
runtime's bounded, process-local counters; normal MIR and binaries contain no
measurement hooks.

The compiler driver owns the versioned JSON reporting schemas, benchmark
orchestration, and source-map sidecars. The Rocket-written production compiler
forwards explicitly host-dependent Phase 17 commands (`coverage`, `profile`,
`benchmark`, `--debug`, and machine output) to its colocated reproducible stage0
host. This boundary is visible and tested; the self-hosted frontend/bootstrap
pipeline and ordinary optimized compilation remain unchanged.

## Rocket 1.8 concurrency and async architecture

HIR derives `Send`/`Share` recursively through concrete nominal declarations,
closure capture structs, collections, weak references, and runtime handles.
Recursive weak-backed models use a visited-type fixed point in both compilers.
Native pointers/handles/callbacks and mutable builders fail both properties.
The lowerer derives move-only status transitively through arrays, slices,
options, results, structs, and enums, then rejects copies/use-after-move. Tasks
are affine: join/await and thread/group transfer consume them, while status and
cancellation borrow. Reusable closures reject move-only captures. The lowerer also
rejects scoped guards/groups in return types, aggregate fields, escaping
captures, and cross-thread frames.

Each async function retains an ordinary MIR body returning `Result[T, String]`.
Its source-level call lowers to an async-call rvalue returning `Task[T]`. LLVM
emits a concrete frame plus one internal entry thunk that reads typed frame
fields, calls the ordinary body, and completes the runtime task. The stage0 C++
backend emits the equivalent typed closure. Await lowers to the versioned task
outcome ABI and returns the same Result representation used by `?`.

The runtime executor has a bounded queue and fixed workers. Its implementation
is directly stress-constructed and destroyed for 1,000 queued-work cycles in
the runtime suite. A worker awaiting
same-executor work pumps ready tasks, so nested awaits do not require an
unbounded worker supply. Ordinary MIR locals stay on the worker stack and retain
managed values across the logical suspension. Task groups own child tasks in
spawn order and their destructors cancel and join remaining children.

The Windows async surface submits timer and file/socket/process coordination to
the same bounded default executor used by async functions. File transfers use
overlapped handles, per-operation events, `GetOverlappedResult`, and
`CancelIoEx` in 64 KiB chunks. Socket calls reuse nonblocking Winsock readiness,
timers use waitable timers, and process work waits on a child handle so
cancellation/deadline cleanup owns the child through termination. Socket and
process coordination is bounded-worker blocking rather than a general IOCP
dispatcher, and no operation creates an unbounded I/O thread per request.
`CONCURRENCY.md` defines ordering, cancellation, partial-I/O, shutdown, the
deliberate process-capture limitation, and resource limits.

## Rocket 2.1 target architecture

CLI normalization creates distinct immutable host and target values before
manifest loading. The target value flows through source-overlay selection,
dependency loading, HIR target constants, MIR, LLVM target-machine creation,
native-input resolution, linker selection, artifact naming, package metadata,
and cache identity. No later layer re-detects a target from the host.

The module loader applies the selected target overlay before discovery. The HIR
standard-module table exposes `std.target` constants. LLVM initializes all
required target families, assigns the canonical triple and target-machine data
layout to every module, emits PE/COFF with CodeView on Windows, ELF with DWARF
on Linux, and Mach-O with DWARF on macOS. Export visibility, object/archive/
dynamic-library production, runtime selection, and link arguments are derived
from the target model rather than filename guesses.

Stage0 and `compiler/src/main.rocket` own matching normalization tables,
manifest rules, cache serialization, extensions, target diagnostics, and
canonical IR decisions. Native and cross toolchains are explicit SDK records;
missing SDKs and unsupported host/target operations fail before code generation.
The supported rows and acceptance authority are normative in `TARGETS.md` and
`PHASE_19_AUDIT.md`.
