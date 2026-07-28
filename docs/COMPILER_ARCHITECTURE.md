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

These representations intentionally cover the scalar language subset. Aggregate
layout, ownership operations, and ARC instructions will extend MIR during the
runtime and type-system phases without weakening these invariants.

## Scalar LLVM backend

The production backend consumes only verified MIR and maps scalar values to the
following LLVM types on the Windows x64 target:

| Rocket type | LLVM value type | Function result ABI |
| --- | --- | --- |
| `Int` | `i64` | `i64` |
| `Float` | `double` | `double` |
| `Bool` | `i1` | `i1` |
| `Char` | `i8` | `i8` |
| `String` | `ptr` | `ptr` |
| `Unit` | internal `i8` placeholder | `void` |

- MIR locals begin as entry-block stack slots. The LLVM O2 pipeline promotes
  eligible slots and performs the production optimization pass sequence.
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
- Until Phase 5 introduces the runtime ABI, string constants are immutable,
  borrowed, null-terminated byte sequences. `print` and string equality use
  temporary C ABI calls to `printf` and `strcmp`. These calls are backend
  implementation details, not the permanent Rocket runtime ABI.

The C++ MIR transpiler remains buildable when `ROCKETC_ENABLE_LLVM=OFF` as the
reproducible stage0 fallback. LLVM-enabled builds never route normal production
commands through that transpiler.
