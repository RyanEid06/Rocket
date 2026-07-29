# Rocket Project Context and Chat Handoff

Read this file at the start of every new Rocket chat. Update it after completing a milestone or making a permanent design decision.

## Project identity

- **Language:** Rocket
- **Compiler:** `rocketc`
- **Source extension:** `.rocket`
- **Primary target:** Windows x64
- **Goal:** Grow the beginner-friendly, statically typed, LLVM-native Rocket 1.0 foundation into a credible, well-respected general-purpose native language with practical collections, scalable abstractions, native interoperability, production libraries, packages, professional tooling, safe concurrency, and multiple platforms.
- **Possible casino goal after Rocket 2.0:** A separate local, single-player, play-money desktop application may be planned only after the language-maturity roadmap is accepted, unless the user explicitly reprioritizes it.

## Locked decisions

- Bootstrap compiler: C++20; preserve it forever as `stage0`.
- Production backend: LLVM 22.1.6 ahead-of-time native compilation.
- Memory model: automatic reference counting; cycles are a documented version-1 limitation.
- Syntax: indentation blocks, explicit `let`/`var`, inferred local types, typed function boundaries.
- Error model: `Option[T]`, `Result[T, E]`, and `?`; never universal null or exceptions.
- Extensibility: official curated syntax sugar only; no arbitrary user macros in Rocket 1.0.
- The complete Rocket 2.0 language-maturity roadmap comes before casino work.
- Graphics after self-hosting: safe Rocket bindings over raylib.
- Casino scope: no real money, payments, accounts, or online multiplayer in version 1.
- Success means a coherent and trustworthy language used for substantial maintained
  software; matching the total feature count or decades-old ecosystem size of
  C++, Python, or Rust is not a completion requirement.

## Current implementation state

Rocket 1.1 is self-hosted. The production `rocketc` is written in Rocket,
bootstraps deterministically through stage3, emits canonical LLVM IR, and links
against the statically linked runtime ABI v1. The C++20 compiler remains the
reproducible `stage0` implementation.

Implemented:

- Indentation-aware lexer with line/column locations.
- Parser and AST for functions, bindings, assignment, control flow, structs,
  enums, generics, exhaustive matches, imports, aggregate expressions, and `?`.
- Resolved HIR with structural nested types, deterministic declaration symbols,
  lexical name resolution, concrete generic specializations, resolved aggregate
  fields/variants, and checked expression types.
- Typed, basic-block MIR with explicit locals, aggregate construction/field/tag
  operations, propagation branches, short-circuit branches, loop edges, and
  terminators, plus structural/type verification.
- Scalar MIR-to-LLVM lowering with a verified Windows x64 ABI, O2 optimization,
  object and assembly emission, native linking, and working `emit-ir`, `build`,
  `run`, and `emit-asm` commands.
- Versioned runtime ABI v1 with explicit MIR ARC, borrowed managed parameters,
  owned managed returns, deterministic local cleanup, and runtime-backed scalar
  printing.
- Owned immutable UTF-8 String values with explicit byte lengths, allocation,
  content equality, and deterministic destruction.
- Typed `Array[T]` literals and retained `Slice[T]` views for nested scalar and
  managed elements, with checked indexing/slicing, copy-on-write element
  assignment, explicit capacity, persistent growth/removal, and deterministic
  managed-element destruction.
- Generic tuples, insertion-ordered Map/Set products, stable hashing, searching,
  sorting, equality filtering, hash mapping, numeric folds, and transparent
  Queue/Stack/ByteBuffer products.
- Opaque ARC structs and enums with scalar/managed fields, generic constructors,
  payload variants, exhaustive pattern matching, and deterministic destruction.
- Built-in generic `Option[T]` and `Result[T, E]` enums plus exception-free `?`
  propagation lowered to explicit MIR control flow.
- Package-relative source modules with `import`, `pub` visibility, qualified
  deterministic symbols, alias diagnostics, and import-cycle detection.
- Typed virtual standard modules for strings, generic collections, UTF-8 paths
  and files, JSON, CSV, deterministic randomness, direct processes, environment,
  and wall/monotonic time, with ordinary `Option`/`Result` failures.
- Validated `rocket.toml` packages, package-root imports, ignored package
  artifacts, deterministic source/test discovery, ordinary-program native tests,
  an idempotent comment-preserving formatter, and stable `Rdddd` diagnostics.
- VS Code syntax/language/snippet/problem-matcher support, Visual Studio 2026
  `.rocket` recognition and CMake check/run/test targets, and a checksummed
  relocatable Windows x64 developer-package workflow.
- Checked signed Int literals and arithmetic, including overflow and
  division-by-zero diagnostics.
- Isolated MIR-to-C++ stage0 backend that remains buildable when LLVM is disabled.
- Hello World, recursive Fibonacci, lexer/parser/sema/HIR/MIR/bootstrap-codegen/
  LLVM-codegen/runtime suites, a golden diagnostic fixture, native lifetime and
  bounds regressions, and allocation stress coverage.
- Pinned LLVM 22.1.6, Ninja 1.13.1, and MSVC Windows x64 development setup.
- A self-contained, checksummed Windows x64 distribution with executable-relative
  toolchain discovery, an isolated relocation test, and a separately named
  stage0 compiler.
- Frozen Rocket 1.0 language, standard-library, diagnostic, manifest, formatter,
  CLI, and runtime ABI contracts with conformance and performance gates.
- Additive Rocket 1.1 collection, mutation, syntax, standard-library, release,
  conformance, performance, and packaging contracts.
- Rocket 1.2 development impl blocks with statically resolved struct/enum
  methods, associated functions, generic specialization, module visibility, and
  standard-library dot-call aliases in both stage0 and the self-hosted compiler.

Not implemented yet:

- Traits, generic constraints, function values, closures, user-defined
  iterators, a stable C FFI, graphics bindings, a production-scale standard
  library, third-party dependency management and registry, semantic language-
  server/debugger features, robust concurrency/async support, and non-Windows
  targets. No casino implementation has begun.

## Canonical build commands

Use the pinned toolchain path:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\dependencies\verify.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Debug
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Release
```

Build output is in `out/build/windows-debug` and `out/build/windows-release`. The ignored legacy `build/` directory is not part of the supported workflow.

## Ordered roadmap

1. Stabilize the repository, documentation, tests, and initial Git baseline.
2. Complete scalar language syntax and diagnostics.
3. Introduce resolved HIR and typed MIR.
4. Implement the scalar LLVM backend and retire production reliance on C++ transpilation.
5. Add runtime ABI, ARC, strings, arrays, slices, and lifetime testing.
6. Add structs, enums, generics, pattern matching, `Option`, `Result`, and modules.
7. Build modules, files, collections, JSON, CSV, randomness, processes, time, and other practical standard-library APIs.
8. Build the formatter, test runner, diagnostic catalog, VS Code support, package layout, and documentation.
9. Rewrite the compiler in Rocket and prove `stage0 -> stage1 -> stage2 -> stage3` self-hosting.
10. Freeze Rocket 1.0, run conformance and performance tests, and publish the self-contained compiler.
11. Add practical collections and controlled mutation: mutable arrays, maps,
    sets, tuples, iteration, hashing, sorting, and managed-lifetime safety.
12. Add scalable abstractions: methods, traits, generic constraints, function
    values, closures, and user-defined iterators.
13. Add explicit unsafe boundaries, stable C FFI, library production, callbacks,
    native package inputs, and deterministic binding generation.
14. Validate native interoperability with safe raylib graphics/audio APIs and a
    substantial non-casino reference application.
15. Expand the standard library for binary I/O, Unicode, regex, secure
    randomness, crypto integrations, networking/HTTP, time zones, logging,
    archives, databases, and application testing.
16. Add reproducible dependency resolution, lockfiles, secure caching, auditing,
    publishing, documentation generation, and a governed package registry.
17. Add professional tooling: a semantic language server, incremental analysis,
    documentation tools, native debugging, profiling, benchmarking, and coverage.
18. Add robust ownership and concurrency: weak references, cycle handling,
    thread-sharing rules, tasks, channels, structured concurrency, and async I/O.
19. Add target triples, Linux and macOS support, ARM64, supported cross-
    compilation paths, and multi-platform release validation.
20. Freeze Rocket 2.0 after security hardening, fuzzing, performance work,
    compatibility validation, signed reproducible releases, complete learning
    material, and sustained external application use.

## Definition of the completed self-hosting gate

Rocket 1.0 satisfied this gate when:

- Rocket source management, lexer, parser, module loader, type checker, MIR, CLI, and LLVM-C backend are written in Rocket.
- C++ `stage0` builds Rocket `stage1`; stage1 builds stage2; stage2 builds stage3.
- Stage2 and stage3 produce identical canonical LLVM IR or normalized binaries.
- The self-hosted compiler passes the full compiler, runtime, standard-library, and CLI suites.
- A clean Windows machine can reproduce the bootstrap from documentation.

## Phase handoff checklist

At the end of each phase:

1. Run Debug and Release builds and all relevant tests.
2. Update the language specification and decision journal.
3. Record completed work, known limitations, and the exact next task below.
4. Commit the finished milestone with a clear message.
5. Start a new chat only after the handoff section is current.

## Completed work

**Phase 0 - baseline stabilization**

- Reviewed the intentionally uncommitted repository before editing.
- Replaced stale MinGW build instructions with the pinned MSVC/Ninja/LLVM workflow.
- Corrected documentation that implied LLVM was unavailable; LLVM is pinned and discoverable, while IR lowering remains future work.
- Normalized the project documentation to ASCII Markdown to avoid encoding ambiguity.
- Split the monolithic frontend test into independent lexer, parser, semantic-analysis, and bootstrap-codegen CTest suites.
- Verified `dependencies/verify.ps1`: LLVM 22.1.6, Ninja 1.13.1, MSVC, CMake, and Git are available.
- Verified `scripts/build.ps1 -Configuration Debug` and `Release`; each configuration passed all 4 CTest suites.
- Smoke-tested the Debug CLI with `check examples/hello.rocket` and `run examples/fibonacci.rocket` (output: `55`).

Known limitations remain those in the implementation-state list above; no language behavior changed during Phase 0.

**Phase 2 - core syntax**

- Added assignment with inferred-type preservation and `var` mutability enforcement.
- Added exclusive integer ranges, `for`, `break`, and `continue` with loop-context checking.
- Added `Float` and byte-sized `Char` literals, types, diagnostics, and bootstrap lowering.
- Added short-circuiting `and`/`or` plus unary `not` with explicit precedence and `Bool` enforcement.
- Strengthened statement-level parser recovery and added a golden diagnostic fixture and CLI CTest coverage.
- Fixed toolchain verification so native tools that report version banners on stderr, including `cl.exe`, are handled correctly.
- Verified the pinned LLVM 22.1.6, Ninja 1.13.1, and MSVC 19.51.36252 toolchain.
- Verified Debug and Release builds; each configuration passed all 6 CTest tests.
- Smoke-tested the MSVC-built Debug CLI across all Phase 2 features (output: `29`, `1.5`, `R`).

**Phase 3 - compiler architecture**

- Specified the resolved HIR and typed MIR invariants in `COMPILER_ARCHITECTURE.md` and recorded decision D007.
- Added deterministic `SymbolId` allocation, lexical resolution, declaration-bound calls, and checked types on every HIR expression.
- Added typed MIR locals, instructions, basic blocks, explicit branches, loop edges, returns, and a verifier for symbol, local, type, call, and control-flow invariants.
- Lowered `and` and `or` into explicit short-circuit control flow and fixed range-bound evaluation to once, left-to-right, before iteration.
- Moved the temporary C++ bootstrap backend from raw AST consumption to verified MIR consumption.
- Added focused HIR and MIR suites and updated bootstrap-codegen regression coverage; Debug and Release each pass all 8 CTest tests.
- Verified the pinned LLVM 22.1.6, Ninja 1.13.1, MSVC 19.51.36252, CMake 4.3.2, and Git 2.54.0 toolchain.
- Smoke-tested Hello World, recursive Fibonacci (output: `55`), and MIR-lowered scalar control flow (output: `8`, `2`).

**Phase 4 - scalar LLVM backend**

- Recorded the scalar LLVM ABI in `COMPILER_ARCHITECTURE.md` and decision D008:
  `Int=i64`, `Float=double`, `Bool=i1`, `Char=i8`, borrowed pre-runtime
  `String=ptr`, and `Unit=void` for function results.
- Lowered verified MIR functions, locals, constants, unary/binary operations,
  declaration-bound calls, branches, and returns to LLVM 22 IR.
- Added a C-compatible native entry point, baseline `x86-64` target machine,
  LLVM verification, the O2 module pipeline, object emission, and assembly
  emission.
- Implemented `emit-ir` and moved LLVM-enabled `build`, `run`, and `emit-asm`
  to the production LLVM path with the pinned Clang driver performing native
  linking. Windows child processes use direct Unicode process creation so paths
  and forwarded arguments are not interpreted by a shell.
- Added temporary C ABI lowering for scalar `print` and content-based string
  equality. Borrowed string constants are explicitly limited to the pre-runtime
  phase.
- Preserved and independently validated the C++ MIR transpiler with
  `ROCKETC_ENABLE_LLVM=OFF`; all 8 applicable stage0 fallback tests pass.
- Added LLVM IR-shape, O2 promotion, object-emission, assembly, full scalar
  operator, native execution, and CLI regressions. Debug and Release each pass
  all 13 CTest tests.
- Hardened Visual Studio environment activation, pinned MSVC compiler/linker/
  librarian selection, and repaired the official LLVM archive's stale DIA SDK
  import path at configure time without modifying downloaded dependencies.
- Verified LLVM 22.1.6, Clang 22.1.6, Ninja 1.13.1, MSVC 19.51.36252, CMake
  4.3.2, and Git 2.54.0. Smoke-tested LLVM-native Hello World and recursive
  Fibonacci (output: `55`).

**Phase 5 - runtime, ownership, and collections**

- Recorded runtime ABI v1 and decision D009. Managed parameters borrow at +0,
  managed results return at +1, non-parameter managed MIR locals own one
  reference, and copies/replacements/function exits contain explicit verified
  retain/release instructions.
- Added the statically linked Rocket runtime with opaque ARC allocations,
  deterministic type destructors, UTF-8 validation, length-aware owned String
  values, runtime-backed printing, and content equality. Removed generated-code
  dependence on temporary `printf` and `strcmp` calls.
- Added non-empty typed `Array[T]` literals and exclusive `Slice[T]` views for
  Int, Float, Bool, Char, and String. Slices retain and flatten onto their
  backing Array; indexing and slicing perform signed bounds checks; Arrays
  retain and release managed String elements.
- Added compile-time signed Int literal range checks and runtime checks for
  signed addition, subtraction, multiplication, negation, division overflow,
  and division by zero in both LLVM and stage0 backends.
- Added lexer/parser/sema/HIR/MIR/backend coverage, native aliasing and managed
  function-boundary regressions, expected bounds/arithmetic failures, and a
  10,000-iteration String/Array/Slice leak stress test.
- Added `scripts/build-stage0.ps1`. The LLVM-disabled Debug stage0 build passes
  all 9 applicable tests and successfully compiles/runs the collection fixture
  through generated C++, including the expected checked-overflow exit.
- Verified the pinned LLVM 22.1.6, Ninja 1.13.1, MSVC 19.51.36252, CMake 4.3.2,
  and Git 2.54.0 toolchain. Debug and Release each pass all 19 tests.

**Phase 6 - structural types, algebraic data types, and modules**

- Replaced the closed built-in type enumeration with structural values carrying
  nominal declaration identities and recursively nested type arguments while
  preserving the Phase 5 collection ABI.
- Added immutable generic structs, positional construction, field access,
  payload enums, exhaustive `match`/`case`, immutable payload bindings, and
  complete match-based return analysis.
- Added inferred generic-function specialization before MIR with deterministic
  canonical specialization symbols and concrete backend signatures.
- Implemented built-in `Option[T]` and `Result[T, E]` enums plus postfix `?` as
  explicit MIR success/failure branches and owned early returns.
- Added opaque ARC aggregate runtime storage, managed-field masks, managed
  aggregate collection elements, nested aggregate destruction, and a
  10,000-iteration aggregate lifetime stress extension.
- Added recursive package-relative module loading, `pub` visibility, module
  aliases, fully qualified deterministic declarations, missing/private access
  diagnostics, and import-cycle detection. Draft 0.6 compiles a checked source
  graph into one artifact rather than freezing a binary module format.
- Extended both the LLVM production backend and C++ stage0 fallback. The
  LLVM-disabled Debug compiler passes all 13 applicable tests and compiles/runs
  the full Phase 6 fixture through generated C++.
- Added focused parser, semantic, HIR, MIR, runtime, module, LLVM-native, and
  negative diagnostic coverage. Debug and Release each pass all 25 tests; the
  native type-system fixture prints `10`, `20`, `generic`, `42`, `10`,
  `matched`, and the module fixture prints `6`, `4`.
- Hardened MSVC discovery to include a temporarily incomplete Visual Studio
  installer instance when its C++ workload remains present.

**Phase 7 - practical standard library**

- Reserved `std.*` as a virtual module namespace and added statically checked
  HIR intrinsic signatures for strings, generic collections, files, paths,
  JSON, CSV, deterministic randomness, processes, environment, and time.
- Added the `rocket_std_*` ABI implementation over managed runtime values.
  Expected I/O, conversion, parsing, and process failures return ordinary
  `Option`/`Result` aggregates and obey the existing MIR ARC contract.
- Added the nominal `std.json.Json` and `JsonField` types, recursive RFC-style
  parsing/stringification, CSV quoting and CRLF encoding, UTF-8 Windows paths,
  direct no-shell process spawning, sorted directory listing, and unbiased
  half-open random integer ranges.
- Extended LLVM intrinsic lowering and the permanent C++ Stage 0 backend. The
  no-LLVM backend includes an isolated RAII library implementation and compiles
  and runs the same full Phase 7 integration fixture as the production backend.
- Added direct runtime/lifetime tests, compiler checks, LLVM IR coverage, native
  all-module execution, the Draft 0.7 specification, decision D011, and the
  standard-library reference. Debug and Release pass all 29 tests; the
  LLVM-disabled Debug build passes all 16 applicable tests, including the full
  native parity fixture.

**Phase 8 - developer tooling and packages**

- Added `rocketc new` and a containment-checked `rocket.toml` layout with a
  shared import root for entries and tests. Existing standalone-file commands
  remain supported; package artifacts live under ignored `.rocketc` directories.
- Added `rocketc test` with recursive sorted discovery, independent native
  programs, exit-status pass/fail behavior, per-test results, and a summary.
- Added an LF/four-space/canonical-spacing formatter with comment and literal
  preservation, idempotence coverage, directory discovery, and CI-safe
  `rocketc fmt --check` behavior.
- Added stable categorical `Rdddd` diagnostic identities, expanded golden/code
  tests, the diagnostic catalog, and a VS Code problem matcher.
- Added VS Code syntax highlighting, indentation/brackets, snippets, repository
  tasks, a relocatable runtime/toolchain lookup, and checksummed compiler-package
  script using the pinned Clang/LLD pair with explicit pre-1.0 MSVC/SDK requirements.
- Added Draft 0.8 package/tooling semantics, decision D012, focused unit tests,
  and a package fixture exercising check, format, root imports, and two native
  tests through both backends. Debug and Release pass all 37 tests; the
  LLVM-disabled Debug build passes all 24 applicable tests. The default archive
  was generated and its relocated compiler executed a package with the bundled
  Clang/LLD pair and sibling runtime.

**Phase 9 - self-hosted compiler**

- Added the Rocket compiler package under `compiler/`. Its Rocket source owns
  source traversal, the indentation lexer, parser, module graph, visibility,
  type checking and generic specialization, typed HIR, verified control-flow
  MIR, explicit ARC operations, stable diagnostics, and canonical textual LLVM
  IR generation.
- Added real generic structs and functions, user and built-in enum constructors,
  exhaustive matching, `Option`/`Result` propagation, all stable standard-module
  signatures, checked Int arithmetic, managed aggregates/collections, and the
  runtime ABI v1 to the self-hosted pipeline.
- Ported manifest loading, package-root module resolution, cycle and private
  visibility diagnostics, canonical token-based formatting, and the complete
  `check`, `build`, `run`, `test`, `fmt`, `new`, `emit-ir`, and `emit-asm` CLI.
- Added the public compiler-building APIs `string.byte_at`,
  `string.byte_value_at`, `string.slice`, `string.Builder`,
  `collections.concat`, `file.create_directory`, and `process.arguments` to
  both stage0 and the production runtime without adding privileged compile
  hooks.
- Added `scripts/bootstrap.ps1`. It creates genuine stage1, stage2, and stage3
  compilers, requires byte-identical stage2/stage3 IR, records SHA-256 hashes,
  checks version parity, and runs compiler, native runtime, standard-library,
  checked-failure, formatter, and package workflows through the generated
  compiler. The LLVM-disabled stage0 remains independently buildable.

**Phase 10 - Rocket 1.0 freeze and distribution**

- Froze the versioned grammar, type system, standard modules, stable diagnostic
  identities, runtime ABI v1, package manifest, formatter, and CLI contracts.
- Added `process.executable_path()` and executable-relative self-hosted toolchain
  discovery so the production compiler is independent of its working directory.
- Added named 1.0 conformance and performance gates plus a self-contained
  Windows x64 package containing stage3, the runtime, pinned Clang/LLD,
  compiler-rt, and static native link inputs. The distribution is validated with
  developer environment variables removed and the C++ compiler is preserved as
  a separate stage0 artifact.

**Phase 11 - Rocket 1.1 practical collections and controlled mutation (completed)**

- Completed copy-on-write Array element assignment plus capacity, `reserve`,
  `append`, `pop`, `insert`, `remove`, and `clear` with alias/Slice snapshots,
  checked failures, geometric growth, and managed-element ARC correctness.
- Added `Tuple2`, `Tuple3`, generic insertion-ordered `Map`/`Set`, eligible-key
  diagnostics, standard FNV-1a hashing, deterministic iteration snapshots,
  searching, sorting, equality filtering, hash mapping, numeric folds, and
  Array-backed Queue/Stack/ByteBuffer products.
- Implemented identical standard signatures, intrinsic identities, nominal
  types, lowering, runtime declarations, and native calls in C++ stage0 and the
  Rocket-written compiler. The production compiler's work-list helper now uses
  public `collections.append` during parsing/lowering and every bootstrap stage.
- Added positive native fixtures; invalid key, negative reserve, insert/remove
  bounds fixtures; fixed hash and duplicate-order oracles; and managed mutation,
  Map/Set, aliasing, and 10,000-iteration lifetime stress coverage.
- Promoted the compiler and package workflow to 1.1.0, finalized the syntax and
  standard-library references, added the additive 1.1 release contract, and
  updated conformance, performance, bootstrap, package, and relocation tooling.
- Verified LLVM-disabled MSVC stage0 Debug and Release matrices (43/43 tests),
  then pinned LLVM 22.1.6 Debug and Release matrices (41 build steps and 65/65
  tests per configuration). Rocket 1.1 conformance passed 31 cases and all four
  performance gates passed.
- Verified deterministic `stage0 -> stage1 -> stage2 -> stage3` bootstrap with
  Phase 11 conformance programs. Stage2 and stage3 LLVM IR are byte-identical at
  SHA-256 `9b5c70cc2458eda810df52496697c006654f61eef7534b8557ba8132e2593bd0`.

**Phase 12 - Rocket 1.2 scalable abstractions (started: methods slice)**

- Added `impl` blocks for same-module structs and enums. Generic impl parameters
  use `impl[T] Owner[T]`; instance methods declare an explicit first `self`
  parameter and associated functions omit it.
- Resolved methods to deterministic `Owner.member` symbols and lowered dot calls
  to ordinary direct calls with the receiver inserted exactly once. No method
  table, virtual dispatch, inheritance, implicit mutation, or runtime ABI change
  was introduced.
- Added generic method specialization, per-method `pub` visibility, public
  cross-module calls, and String/Array/Slice/Map/Set aliases over existing
  standard intrinsics.
- Mirrored parsing, module mapping, type resolution, and direct-call lowering in
  the Rocket-written compiler. Added stage0, production, self-hosted, generic,
  enum, standard-library, and multi-module fixtures.
- Verified pinned LLVM Debug and Release matrices (71/71 tests each) and
  LLVM-disabled stage0 Debug and Release matrices (47/47 tests each). The
  Rocket 1.2 development conformance suite passes 36 cases.
- Verified deterministic `stage0 -> stage1 -> stage2 -> stage3` bootstrap with
  the Phase 12 fixtures. Stage2 and stage3 LLVM IR are byte-identical at SHA-256
  `f63bf709007604a2138a4d2ff3b16024f01e02ad7d0a10696c7d4850ec8ad70e`.

## Current next task

**Phase 12 - Rocket 1.2 scalable abstractions and functional values.**

The methods/associated-functions slice is implemented. Next, specify and
implement traits and deterministic generic constraints, then first-class
functions/closures and user-defined iterators in roadmap order. Preserve the
complete Rocket 1.1 compatibility surface and deterministic bootstrap proof. Do
not begin Phase 13, graphics, or casino implementation first.

## New-chat prompt

Use this at the beginning of a new Rocket chat:

```text
Read AGENTS.md and docs/PROJECT_CONTEXT.md first. We are working on Rocket.
Continue from the "Current next task" section, inspect the repository before editing,
preserve existing user changes, implement only the stated phase, run its validation,
update PROJECT_CONTEXT.md, and commit the completed milestone.
```
