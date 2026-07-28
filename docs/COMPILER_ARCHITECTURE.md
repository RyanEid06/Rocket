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
| struct / enum | opaque `ptr` | `ptr` (+1 owned) |
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
or another managed type retain every stored element and release them in deterministic order at
destruction. `Slice[T]` stores an owning reference to the backing Array plus an
offset and length; slicing a Slice flattens offsets while retaining the same
owner. Index and slice functions validate signed bounds before accessing data.

The runtime reports bounds failures, invalid UTF-8, allocation failures,
reference-count corruption, integer overflow, and integer division by zero to
standard error and exits with status 101. Reference-count cycles remain a
documented Rocket 1.0 limitation.

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
