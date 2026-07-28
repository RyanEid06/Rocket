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
