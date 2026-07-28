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

Future aggregate and generic types must extend these invariants without
weakening them.

## Scalar LLVM backend

The production backend consumes only verified MIR and maps scalar values to the
following LLVM types on the Windows x64 target:

| Rocket type | LLVM value type | Function result ABI |
| --- | --- | --- |
| `Int` | `i64` | `i64` |
| `Float` | `double` | `double` |
| `Bool` | `i1` | `i1` |
| `Char` | `i8` | `i8` |
| `String` | opaque `ptr` | `ptr` (+1 owned) |
| `Array[T]` | opaque `ptr` | `ptr` (+1 owned) |
| `Slice[T]` | opaque `ptr` | `ptr` (+1 owned) |
| `Unit` | internal `i8` placeholder | `void` |

- MIR locals begin as entry-block stack slots initialized to the scalar zero or
  managed null value. The LLVM O2 pipeline promotes eligible slots and performs
  the production optimization pass sequence.
- MIR basic blocks and terminators lower directly to LLVM blocks, branches, and
  returns. Short-circuit behavior therefore remains encoded by MIR control flow.
- Rocket functions retain declaration-ID-qualified native names. A C-compatible
  `i32 @main()` wrapper invokes Rocket's required `fn main() -> Int` entry point.
- The target triple and data layout come from the pinned LLVM Windows toolchain;
  code generation uses the baseline `x86-64` CPU rather than host-specific CPU
  features so build outputs do not depend on the developer machine's processor.
- `emit-ir` prints verified unoptimized IR for inspection. `build`, `run`, and
  `emit-asm` run LLVM's O2 module pipeline; object and assembly emission use the
  LLVM target machine, and the pinned Clang driver performs native linking.
- Checked `Int` operations use LLVM signed-overflow intrinsics and explicit
  runtime-failure edges. Collection and String operations lower only through
  the Rocket runtime ABI; generated modules do not call `printf` or `strcmp`.

The C++ MIR transpiler remains buildable when `ROCKETC_ENABLE_LLVM=OFF` as the
reproducible stage0 fallback. LLVM-enabled builds never route normal production
commands through that transpiler.

## Runtime ABI v1 and ownership

The production linker combines each generated object with the statically built
Rocket runtime. `rocket_rt_abi_version()` reports version 1. ABI declarations
use `extern "C"`, fixed-width scalar arguments, and opaque pointers; C++ layout
is not exposed to generated code.

All managed runtime allocations begin with an internal strong-reference header
and a type-specific destructor. ABI v1 is single-threaded, so reference counts
are non-atomic. Null is accepted only by internal retain/release operations to
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
retain every stored element and release them in deterministic order at
destruction. `Slice[T]` stores an owning reference to the backing Array plus an
offset and length; slicing a Slice flattens offsets while retaining the same
owner. Index and slice functions validate signed bounds before accessing data.

The runtime reports bounds failures, invalid UTF-8, allocation failures,
reference-count corruption, integer overflow, and integer division by zero to
standard error and exits with status 101. Reference-count cycles remain a
documented Rocket 1.0 limitation.
