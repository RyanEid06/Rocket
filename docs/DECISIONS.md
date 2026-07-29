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
ownership model. User iteration is a persistent four-method protocol whose
advance operation returns a new cursor; loop lowering is explicit control-flow
MIR.

Associated constants are zero-argument direct functions, not global storage.
Rocket 1.2 keeps parameters positional, required, and fixed-arity, and caps user
monomorphization at 4,096 specializations per compilation. These choices avoid
hidden mutation, dynamic dispatch, order-dependent overload selection, and
unbounded accidental code growth.
