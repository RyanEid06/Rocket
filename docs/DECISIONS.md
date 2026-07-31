# Design Decision Journal

## D001 - LLVM ahead-of-time backend

**Accepted.** LLVM provides optimized native code while keeping the project
focused on language semantics. The development toolchain pins LLVM 22.1.6. The
first executable slice used a clearly isolated C++ bootstrap backend; after
Phase 4, LLVM is the production backend and the C++ path remains only for
reproducible stage0 fallback builds. The frontend does not depend on either
backend.

## D002 - Inferred static typing

**Accepted.** Local inference preserves concise code while compile-time checking supports predictable performance and beginner-friendly diagnostics.

## D003 - Indentation-delimited blocks

**Accepted.** This favors visual clarity. The lexer, not the parser, converts indentation into explicit `Indent` and `Dedent` tokens.

## D004 - Reference counting

**Accepted.** Reference counting offers deterministic cleanup and a tractable first runtime. Cycles remain a documented limitation for version 1.

## D005 - Explicit failure and absence

**Accepted.** `Result[T, E]` and `Option[T]` replace exceptions and universal null. Their compiler implementation follows algebraic data types.

## D006 - Scalar control-flow slice

**Accepted.** Assignment is restricted to existing `var` bindings and must preserve their inferred type. Integer `for` ranges use `start..end` with an exclusive end; loop variables are immutable. `break` and `continue` are loop-only statements. `Float` is a distinct scalar type with no implicit conversion from `Int`, and `Char` is a single escaped byte literal in the stage0 bootstrap implementation.

## D007 - Resolved HIR and typed control-flow MIR

**Accepted.** The parsed AST remains a syntax-only representation. Resolution and
type checking produce HIR whose declarations have deterministic `SymbolId`
values and whose expressions carry checked types. HIR lowers into typed,
basic-block MIR with explicit locals, instructions, branches, loop edges, and
returns. All backends, including the temporary C++ bootstrap backend, consume
verified MIR. Integer range bounds are evaluated once from left to right before
iteration. See `COMPILER_ARCHITECTURE.md` for the representation invariants.

## D008 - Scalar LLVM ABI and production pipeline

**Accepted.** Verified scalar MIR lowers directly to LLVM 22 IR. `Int`,
`Float`, `Bool`, and `Char` map to `i64`, `double`, `i1`, and `i8`; the
pre-runtime `String` representation is a borrowed `ptr`; and `Unit` function
results use `void` with an internal placeholder where MIR requires a value.
Production builds use the baseline Windows `x86-64` target, LLVM's O2 module
pipeline, LLVM object/assembly emission, and the pinned Clang driver for native
linking. A C-compatible `main` wrapper calls Rocket's declaration-qualified
entry point. Temporary `printf` and `strcmp` calls support scalar printing and
string equality only until the Phase 5 runtime ABI replaces them. The C++ MIR
transpiler is retained exclusively as a build-time stage0 fallback when LLVM is
disabled.

## D009 - Runtime ABI v1, ARC, and retained slices

**Accepted.** Managed `String`, `Array[T]`, and `Slice[T]` values use opaque
pointers across the version-1 C ABI. Function parameters borrow managed values
at +0, function results return them at +1, and non-parameter MIR locals own one
reference. MIR contains explicit retain/release instructions so ownership is
verified independently of LLVM; replacement retains before releasing, and
function exits deterministically clean owning locals. ABI v1 reference counts
are non-atomic because Rocket 1.0 has no threading model. Cycles remain a
documented limitation.

Strings own valid UTF-8 bytes and carry an explicit byte length. Arrays own
contiguous storage and retain managed String elements. Slices are immutable,
exclusive-end views that retain and flatten onto a backing Array, preventing a
dangling view. Built-in collection element types are limited to the scalar
types and String until general generics. Index/slice bounds, signed Int
overflow, and Int division by zero terminate through deterministic runtime
diagnostics. The C++ stage0 fallback maps the same semantics to RAII containers
and checked helpers.

## D010 - Structural nominal types and ahead-of-MIR specialization

**Accepted.** Rocket types are structural compiler values containing a kind,
nominal declaration identity, and nested arguments. Structs and enums use
nominal equality; collections use structural element equality. Generic
functions infer type arguments from value operands and are monomorphized before
MIR, keeping MIR fully concrete and both backends free of generic semantics.

Structs and enums share an opaque ARC aggregate runtime representation with a
tag, typed slots, and a managed-field mask. `Option` and `Result` are built-in
generic enums, exhaustive `match` is their normal elimination form, and `?`
lowers to explicit MIR control flow rather than exceptions. Module imports form
a checked source graph with public visibility and deterministic qualified
symbols; draft 0.6 deliberately emits one artifact for the graph instead of
freezing a binary module format before the packaging milestone.

## D011 - Typed virtual standard modules

**Accepted.** The curated `std.*` namespace is compiler-provided in Phase 7.
HIR registers exact generic and concrete signatures and records a closed
intrinsic identity only for functions referenced by a program. MIR verifies
these calls, LLVM lowers them to `rocket_std_*` ABI v1 functions, and the
runtime returns ARC-managed `Option`/`Result` values for expected failure. This
keeps OS and parsing behavior outside the language core without depending on a
package system that does not yet exist.

JSON is a nominal built-in enum so Rocket code can inspect data with exhaustive
matching. Process execution accepts a separated program and argument array and
never invokes a shell. Randomness is deterministic and non-cryptographic. The
Stage 0 backend carries an isolated RAII implementation of the same surface and
is regression-tested against the production backend.

## D012 - Minimal manifest and ordinary-program tests

**Accepted.** A Rocket package uses one `rocket.toml`, `src/main.rocket`, and a
configurable test directory. Manifest paths must remain within the package root,
and that root is also the import root for entry and test compilation. Every test
file is an independent ordinary Rocket program whose integer exit status decides
pass/fail. This avoids introducing test-only syntax, reflection, or exceptions
before self-hosting while still supporting deterministic discovery and CI.

The canonical formatter is lexer-aligned, comment-preserving, and idempotent.
Diagnostics expose stable categorical `Rdddd` identities separately from human
messages. Editor integration consumes those contracts and is explicitly syntax
support rather than an unimplemented semantic language server.

## D013 - Textual LLVM IR for the self-hosted backend

**Accepted.** The Rocket-written compiler emits canonical, verified textual LLVM
IR and invokes the pinned Clang/LLD driver to create Windows x64 objects and
executables. It does not embed LLVM through C++ or expose a privileged
compiler-in-the-runtime operation. This keeps the compiler implementation in
Rocket, makes stage2/stage3 comparison direct, and preserves the C++ LLVM 22
backend permanently as reproducible Stage 0. Runtime ABI v1 remains the only
native interface generated programs depend on.

The compiler is allowed to use ordinary `std.string`, `std.collections`,
`std.file`, `std.path`, and `std.process` APIs. Byte traversal, persistent array
growth/composition, and process arguments are public, backend-parity-tested
library features rather than hidden bootstrap hooks.

## D014 - Rocket 1.0 freeze and self-contained Windows distribution

**Accepted.** The grammar, type system, ownership model, runtime ABI v1,
standard modules, diagnostic identities, manifest, formatter, and CLI described
by the versioned documentation are the Rocket 1.0 compatibility contract.
Compatible 1.x releases may add APIs and diagnostics but may not silently
change the behavior of valid 1.0 programs. An incompatible contract requires a
new major version and a recorded decision.

The production Windows x64 package contains the Rocket-written deterministic
stage3 compiler. It resolves its runtime and pinned native toolchain relative
to `process.executable_path()`, while the C++ compiler is retained as an
explicitly named stage0 artifact. The archive bundles static native link inputs
and is tested with developer-only environment variables removed and from a
working directory outside the package.

## D015 - Language maturity before application scope

**Accepted.** After Rocket 1.0, the primary objective is to develop Rocket into
a credible, well-respected general-purpose native language. Phases 11 through 20
cover practical mutable collections, scalable abstractions, C interoperability,
graphics validation, production libraries, dependency management, professional
tooling, concurrency, multiple platforms, and Rocket 2.0 hardening.

This objective does not require matching the total feature count, package count,
or decades of production history behind C++, Python, or Rust. Success requires a
coherent design, trustworthy compiler/runtime, excellent support for Rocket's
chosen domains, reproducible releases, and substantial independently maintained
software.

Raylib remains the first major graphics and FFI validation target, but Phase 14
uses a non-casino reference application. Casino implementation is no longer part
of the active language roadmap and may be planned separately only after Rocket
2.0 is accepted, unless the user explicitly records a later reprioritization.

## D016 - Copy-on-write collection value semantics

**Accepted.** Rocket 1.1 collections preserve value semantics through
copy-on-write rather than exposing shared mutable reference semantics or adding
a borrow checker. Mutation requires an explicit `var` binding. A uniquely owned
collection may update its storage in place; a collection shared by another
binding, Slice, or runtime owner is cloned before the mutation and the `var`
binding is rebound to the clone. Existing aliases and Slices remain stable
snapshots of the value they retained.

The first Phase 11 vertical slice is direct Array element assignment. Its index
and value are evaluated once from left to right, only `Array[T]` targets are
accepted, and the replacement must be exactly `T`. Runtime update functions
borrow the old Array and return an updated Array at +1. MIR stores that result in
an owning temporary before retaining it into the source binding and releasing
the prior value, so unique updates, shared clones, self-assignment, managed
elements, and bounds failure all follow the existing ARC contract. Slices remain
immutable; mutable slice views are not introduced.

Array capacity and growth use ordinary `std.collections` functions until the
general method model is introduced in Phase 12. `reserve` and `append` return an
updated Array and require an explicit source-level rebind for mutation. `pop`
returns `Option[std.collections.Pop[T]]`, carrying both the shortened Array and
the removed element without hidden out-parameters, exceptions, or premature
tuple syntax. Empty pop is `None`; negative reserve is a programmer-contract
runtime failure. Capacity is snapshot state, so the functional calls preserve
their input even when their result is ignored. Storage reuse requires a
compiler-proven direct consuming rebind.

`insert`, `remove`, and `clear` follow the same persistent rule. Insert accepts
indices from zero through the current length. Remove uses checked element bounds
and returns `std.collections.Removal[T]`, carrying the updated Array and removed
value. Clear preserves capacity in its returned empty snapshot.

Phase 11 Maps and Sets use deterministic insertion order as their public
iteration contract; internal lookup representation is never observable.
Eligible keys are `Int`, `Bool`, `Char`, and UTF-8 `String`. Float and aggregate
keys are excluded. Stable FNV-1a hashing is defined over canonical bytes, while
equality uses the existing scalar and length-aware String contracts. `Tuple2`
and `Tuple3` are ordinary immutable ARC aggregates rather than special ABI
values.

Phase 11 includes only collection algorithms with closed, explicit semantics:
search, equality filtering, scalar/String sorting, stable hash mapping, and
numeric sum folds. General higher-order map/filter/fold operations are assigned
to Phase 12 because adding them earlier would require an undocumented function-
pointer or callback ABI before first-class function and closure semantics exist.
Queue, Stack, and ByteBuffer remain transparent Array-backed library products so
their later methods are additive convenience.

## D017 - Static impl members and explicit receivers

**Accepted for the first Rocket 1.2 slice.** Structs and enums may have
same-module `impl` blocks. Instance methods are ordinary functions whose first
parameter is explicitly named `self` and exactly matches the impl owner;
associated functions omit `self`. Impl type parameters use `impl[T] Owner[T]`
so generic ownership and specialization remain explicit.

Member identity is the deterministic qualified name `Owner.member`. After the
receiver is typed, an instance dot call inserts it as argument zero and resolves
an ordinary direct function specialization. This deliberately adds no virtual
dispatch, inheritance, hidden mutation, method table, or runtime ABI surface.
Standard-library dot calls use the same rewrite onto existing intrinsics.
Individual methods control module visibility with `pub`; impl blocks are not
visibility-bearing declarations. Trait-driven selection is reserved for the
next Phase 12 slice and must extend, rather than silently change, this direct
inherent-member rule.

## D018 - Static traits, monomorphic closures, and persistent iterators

**Accepted for Rocket 1.2.** Traits are compile-time constraints rather than
runtime objects. Inherent members have priority and trait resolution must yield
one deterministic implementation. Lambdas lower to immutable ARC-managed
capture structs plus direct call functions, preserving the existing ABI and
ownership model. Their typed signatures inherit enclosing generic-function type
parameters and substitute them per monomorphic specialization. User iteration
is a persistent four-method protocol whose
advance operation returns a new cursor; loop lowering is explicit control-flow
MIR.

Associated constants are zero-argument direct functions, not global storage.
Rocket 1.2 keeps parameters positional, required, and fixed-arity, and caps user
monomorphization at 4,096 specializations per compilation. These choices avoid
hidden mutation, dynamic dispatch, order-dependent overload selection, and
unbounded accidental code growth.

## D019 - Narrow explicit C ABI and non-owning native values

**Accepted for Rocket 1.3.** Native calls require lexical `unsafe:` blocks and
use one frozen Windows x64 C ABI. The boundary admits fixed-width primitives,
non-dereferenceable `Pointer[T]`, opaque pointer handles, pointer-only native
structures, and synchronous non-storing callbacks to top-level noncapturing
Rocket functions. Managed Rocket values and by-value aggregates are excluded.

Native pointers and handles are never ARC values. Ownership, nullability,
release functions, error codes, and string byte lengths belong to the declared C
contract; ordinary Rocket modules should expose safe `Option`/`Result` wrappers
and keep their unsafe regions minimal. This design makes the unsafety visible
without pretending the compiler can verify foreign allocation or callback
lifetime behavior.

`rocket.toml` scopes linker inputs to `[native.windows-x64]`; library products
are selected by `[build].kind`. Binding generation supports a deliberately
closed C-header subset and emits public low-level declarations deterministically.
Rocket exports use unmangled C wrappers, while internal Rocket/runtime ABI v1 is
not exposed or changed. Broader calling conventions, direct strings, stored or
capturing callbacks, unions, variadics, and by-value native aggregates require a
future recorded ABI decision.

## D020 - Primitive raylib adapter and explicit resource tokens

**Accepted for Rocket 1.4.** raylib 6.0 is pinned as the first substantial FFI
validation target, but its by-value structures and C strings do not widen the
Rocket 1.3 ABI. A narrow adapter owns every raylib structure and represents
windows, frames, textures, fonts, audio devices, sounds, and temporary UTF-8
buffers with validated positive `Int` tokens. Safe Rocket code receives
`Result` values; stale, double-released, out-of-order, missing-asset, and
unavailable-device operations fail without foreign memory access.

Window resources must be released before the window, sounds before the audio
device, and frames exactly once before resource teardown. Strings are copied to
short-lived adapter buffers and callbacks are synchronous top-level functions
that are never stored. A deterministic test mode implements the same ABI and
state machine without platform devices. This preserves the general C ABI,
keeps all handwritten C++ application logic out of the reference program, and
defers automatic destructors or linear resource types to a future language
decision.

## D021 - Immutable byte buffers and explicit binary encodings

**Accepted for the Rocket 1.5 foundation.** Binary data uses the existing
`std.collections.ByteBuffer` product over `Array[Char]`; it does not add a
privileged mutable buffer, pointer type, or runtime object kind. Binary slicing,
reading, UTF-8 decoding, and unsigned integer encoding return `Result` for
data-dependent failures. Integer byte order and width are explicit in API names.

Binary file operations are synchronous and preserve bytes exactly. They return
ordinary `Result` values for host failures and document blocking behavior.
Stage0 and self-hosted compilers expose the same standard signatures and runtime
entry points. Stateful buffered streams may build on persistent cursor values in
a later slice without changing this representation or weakening error handling.

## D022 - Single-version resolution and content-addressed package sources

**Accepted for the Rocket 1.6 foundation.** Exact package versions use Semantic
Versioning 2.0.0, registry constraints select the highest stable match, and one
graph may contain only one version and checksum for a package name. Conflicts
are explicit diagnostics rather than resolver-order choices. Path dependencies
remain development inputs; Git dependencies require immutable revision pins.

`rocket.lock` is committed generated data with lexical ordering, exact graph
edges, sources, licenses, and SHA-256 checksums. The package-local cache is
content-addressed and reverified before use; symbolic links and implicit code
execution are rejected. Offline mode trusts neither filenames nor prior cache
state and succeeds only when every locked digest is present and valid. The first
implementation uses reviewed local/file registries in stage0 so network archive
and authentication policy cannot emerge accidentally. Production self-hosted
integration and authenticated transports are required follow-up milestones.

## D023 - Standalone, editor-neutral language-server process

**Accepted and completed for Rocket 1.7.** The semantic tooling entry
point is a standalone `rocket-lsp` executable using versioned Language Server
Protocol 3.17 JSON-RPC over standard streams. It reuses Rocket frontend
diagnostics and stable `Rdddd` codes without changing the compiler CLI or
runtime ABI.

Protocol 1.0 negotiates incremental synchronization and builds a bounded graph
from manifests, exact locks, compiler modules/HIR/types, standard modules, and
unsaved overlays. It provides semantic completion/imports, hover/signatures,
navigation/references/rename, semantic token deltas, stable code actions,
cancellation, stale-generation suppression, configuration, and source-free
latency telemetry. Independent clients use independent processes and requests
within one process retain arrival order. The server never executes builds or
package code as a side effect of editor analysis.

Native debugging deliberately uses the platform's CodeView/PDB contract plus a
versioned Rocket source-map sidecar instead of an editor-owned protocol.
Coverage, profiling, benchmarks, and CI output likewise use versioned JSON
schemas and are opt-in compiler commands. This keeps tooling editor neutral and
preserves the C++ stage0 as the audited host for features not yet expressed by
the self-hosted backend.

## D024 - Bounded stream tokens and explicit Unicode layers

**Accepted for Rocket 1.5.** Buffered file streams use opaque,
process-local integer tokens rather than exposing host pointers or adding a new
linear-resource kind. Tokens are kind-checked, become invalid on close, and
keep all host failures in `Result`. Buffer and per-read sizes are bounded, and
the API explicitly documents synchronous blocking behavior.

Rocket Strings remain valid UTF-8. Byte indexing stays available for binary
protocols, while scalar indexing is an explicit `Int`-based layer that excludes
surrogates. NFC/NFD normalization is opt-in and uses the Windows Unicode tables.
The first grapheme API implements practical boundaries for combining marks,
variation selectors, emoji modifiers, joiner sequences, and regional pairs;
locale-sensitive segmentation and a frozen Unicode-data version are not
claimed. Regular expressions operate on UTF-8 bytes and likewise require
explicit normalization when canonical equivalence matters.

## D025 - Platform-backed cryptography and offline certificate verification

**Accepted for Rocket 1.5.** Secure randomness, SHA-256, and HMAC-SHA-256 use
the reviewed Windows CNG providers loaded from the operating system. The legacy
seedable `std.random` module remains explicitly deterministic. Secure integer
sampling is unbiased and inclusive, and secret comparison never exits early on
the first unequal byte.

Authenticode verification uses WinVerifyTrust with UI disabled, whole-chain
revocation policy, and cache-only URL retrieval. This avoids hidden network
access and makes isolated tests reproducible, while documenting that revocation
freshness is limited by the host cache. Crypto input sizes are bounded and all
provider, path, and policy failures cross Rocket as `Result` values.

## D026 - Timeout-bounded socket tokens and system HTTPS policy

**Accepted for Rocket 1.5.** TCP connections and listeners use checked opaque
tokens shared by the native and stage0 standard libraries. DNS and all socket
failures are recoverable, byte counts are bounded, every blocking operation has
an explicit timeout, and close/cancel invalidates the token exactly once.

The HTTP client uses WinHTTP and never exposes an option to disable HTTPS
certificate verification. Its response type preserves status and binary body.
The first server layer deliberately implements one bounded HTTP/1.x request per
connection, rejects chunked request bodies, and emits `Connection: close`.
Asynchronous multiplexing remains Phase 18 work rather than an undocumented
threading promise in the Phase 15 API.

## D027 - Locale-neutral calendars and bounded operational helpers

**Accepted for Rocket 1.5.** UTC interchange uses one millisecond-precision ISO
shape independent of locale. Gregorian validation is explicit, while historical
local offsets and the configured time-zone identity come from Windows rules for
the requested instant.

Logging emits newline-safe UTC records and serializes same-process writers.
Command-line parsing distinguishes absent options from malformed missing values.
Configuration uses a deliberately bounded textual subset with duplicate-key
rejection rather than silently accepting an underspecified TOML dialect. These
helpers return `Option`/`Result` at data and host-failure boundaries and document
which operations can block.

## D028 - Explicit XPRESS streams and data-only ustar archives

**Accepted for Rocket 1.5.** Compression names its Windows XPRESS Huffman wire
format rather than presenting it as a generic codec. Input and expansion sizes
are bounded before allocation, and invalid streams are recoverable errors.

The first archive API treats archives as validated data rather than extracting
paths. Deterministic ustar output contains regular files only. Readers verify
checksums, type, count, total size, and safe relative names, rejecting traversal,
links, duplicates, and ambiguous platform paths. Applications explicitly choose
where any returned bytes are written.

## D029 - Parameterized system SQLite with bounded text results

**Accepted for Rocket 1.5.** Database handles wrap the Windows SQLite service in
full-mutex mode with an explicit busy timeout. The standard API prepares exactly
one statement per call and binds a caller-supplied parameter array, making the
safe path the default and avoiding an interpolation helper that would encourage
SQL injection.

Handles are invalid after close. Statement, parameter, column, row, and result
byte counts are bounded before values cross into Rocket. The initial foundation
returns text cells and documents the temporary empty-String representation of
SQL `NULL`; typed values, transactions, and migrations can be layered without
weakening the token or binding contracts.

## D030 - Ordinary Rocket testing facade and explicit XFAIL convention

**Accepted for Rocket 1.5.** Assertion composition lives in the bundled
`std.testing` Rocket module; compiler privilege is limited to secure temporary
creation/cleanup and coverage-counter storage. Cleanup accepts only exact roots
created by the same process, preventing a testing helper from becoming an
arbitrary recursive-delete API.

Test selection is a deterministic filename substring. The `.xfail.rocket`
suffix is the sole expected-failure marker: failure becomes `XFAIL`, unexpected
success becomes `XPASS`, and an empty filter selection is an error. Coverage
hooks emit versioned lexical JSON so later compiler instrumentation can reuse
the contract without changing the runtime ABI surface again.

## D031 - Locked package roots, signed registries, and inert dependencies

**Accepted for Rocket 1.6.** A dependency import is resolved only through the
committed lock graph into a rehashed content-addressed cache tree. Manifest
dependency keys are source import roots; package-local files cannot shadow them,
and a transitive import must be an edge declared by the importing locked
package. This makes normal compilation, not only `resolve`, enforce exact source
selection.

Registry trust uses an explicitly pinned ECDSA P-256 public-key fingerprint.
Signed canonical indexes cover immutable versions, namespace ownership and
transfer history, yanks, publisher provenance, and advisories. HTTPS retains
Windows certificate validation and a same-origin redirect bound. Archives are
bounded deterministic regular-file-only ustar data and reach the cache only by
a verified transactional rename. Remote Git is fetched by direct argument-vector
process creation with hooks, helpers, submodules, mutable refs, and unsafe
protocols disabled; the peeled commit must equal the requested object ID.

Authentication tokens are stdin-only and stored in Windows Credential Manager.
Publishing is preflighted, immutable, idempotent for identical bytes, and
namespace-authorized. Dependency source is inert: Rocket 1.6 has no build hooks,
and locked native inputs require an exact root allow-list before the existing
Phase 13 linker can see them. Documentation renders examples as text and audit
uses signed advisory and SPDX policy data without executing dependency code.
These choices keep package operations reproducible without widening runtime ABI
v1 or smuggling unrestricted execution into manifests.
