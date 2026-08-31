# Rocket Project Context and Chat Handoff

Read this file at the start of every new Rocket chat. Update it after completing a milestone or making a permanent design decision.

## Project identity

- **Language:** Rocket
- **Compiler:** `rocketc`
- **Source extension:** `.rocket`
- **Primary target:** Windows x64
- **Goal:** Maintain the completed, beginner-friendly, statically typed,
  LLVM-native Rocket 2.1 language across Windows x64, Linux x64, Linux ARM64,
  and macOS ARM64 without changing ABI v1.
- **Possible casino goal:** A separate local, single-player, play-money desktop
  application may be planned independently of Rocket language work.

## Locked decisions

- Bootstrap compiler: C++20; preserve it forever as `stage0`.
- Production backend: LLVM 22.1.6 ahead-of-time native compilation.
- Memory model: automatic reference counting; strong cycles must be prevented by
  using explicit `Weak[T]` back edges.
- Syntax: indentation blocks, explicit `let`/`var`, inferred local types, typed function boundaries.
- Error model: `Option[T]`, `Result[T, E]`, and `?`; never universal null or exceptions.
- Extensibility: official curated syntax sugar only; no arbitrary user macros in Rocket 1.0.
- Graphics: safe Rocket bindings over raylib.
- Casino scope: no real money, payments, accounts, or online multiplayer in version 1.
- Success means a coherent and trustworthy language used for substantial maintained
  software; matching the total feature count or decades-old ecosystem size of
  C++, Python, or Rust is not a completion requirement.

## Current implementation state

Rocket 2.0 security, performance, compatibility, and trust work is complete on
the Rocket 1.5 standard library, Rocket 1.6 package ecosystem, Rocket 1.7
professional tooling, and Rocket 1.8 ownership/concurrency release. Phase 19
completed on 2026-08-29 as the additive Rocket 2.1 portability release. The
production `rocketc` is written in Rocket,
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
- VS Code syntax/language/snippet/problem-matcher support; full Visual Studio
  Community 2026 GUI commands, Output, Error List, LSP, and native-debug
  integration; CMake fallback targets; and a checksummed relocatable Windows
  x64 developer-package workflow.
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
- Rocket 1.2 static impls and traits, deterministic generic constraints,
  monomorphized closure values, persistent user-defined iterators, associated
  constants, specialization limits, module visibility, and matching stage0 and
  self-hosted behavior.
- Rocket 1.3 explicit unsafe regions, narrow Windows x64 C imports and exports,
  primitive constants, non-owning pointers and opaque handles, pointer-only
  native layouts, synchronous non-storing callbacks, target-scoped native
  linker inputs, static/dynamic library products, deterministic C headers and
  Rocket bindings, safe-wrapper conventions, and matching stage0/self-hosted
  behavior.
- Rocket 1.4 pinned raylib 6.0 integration, generated primitive-only adapter
  bindings, safe window/drawing/input/texture/font/audio/callback wrappers,
  validated opaque resource tokens, deterministic cleanup, a reusable app
  scaffold and bundle workflow, and the non-casino Orbital Workshop reference
  application with matching stage0/self-hosted behavior.
- Rocket 1.5 production standard library with binary and buffered I/O, explicit
  endian codecs, Unicode layers, safe regular expressions, platform
  cryptography, DNS/TCP/HTTP(S), calendar/time-zone helpers, logging and CLI/
  configuration parsing, compression, safe data-only archives, parameterized
  SQLite, and an ordinary Rocket testing facade, with matching stage0 and
  self-hosted compiler signatures.
- Rocket 1.6 dependency ecosystem with Semantic Versioning, registry/path/
  revision-pinned Git manifest sources, deterministic lockfiles, signed
  registries, secure caching and transport, publishing, documentation,
  credential and namespace governance, advisory and license auditing, and
  matching stage0/self-hosted workflows.
- Rocket 1.7 language-server protocol 1.0 with compiler-backed incremental
  multi-package graphs, unsaved overlays, completion/imports, hover/signatures,
  definition/references/rename, semantic token deltas, code actions,
  configuration/cancellation/stale suppression, bounded source-free telemetry,
  a dependency-free VS Code client, and an independent non-VS-Code test client.
- Rocket 1.7 compiler-backed versioned documentation, deterministic native
  CodeView/PDB and source maps for optimized/unoptimized builds, coverage,
  profiling and benchmarking reports, machine compiler/test/build messages,
  and an evaluated incremental-AOT REPL prototype.
- Rocket 1.8 typed weak ownership, move-only unique buffers, structural
  `Send`/`Share`, checked atomic ARC promotion, dedicated thread handles, a
  bounded default executor, typed tasks and task groups, bounded FIFO channels,
  mutex guards, events, integer atomics, once cells, cooperative cancellation,
  monotonic timers/deadlines, `async fn`/`await`, and bounded-worker Windows
  asynchronous file, socket, and process APIs, with matching C++ stage0 and
  Rocket-written compiler behavior.
- Rocket 2.0 frozen Windows x64 contracts; bounded source, module, manifest, and
  package processing; deterministic frontend/package fuzzing; parser recovery
  and crash minimization; sanitizer presets; conservative content-addressed
  package artifact reuse; parallel package-build and large dependency-graph
  validation; 1.0-1.8 compatibility gates; signed, provenanced, checksummed,
  reproducible release tooling; security/governance policy; and complete book,
  migration, FFI, package-author, syntax, and release documentation.

Not implemented yet:

- Broader native calling conventions and the full raylib surface remain optional
  where `FFI_GUIDE.md` and the Phase 19 audit classify them as outside the
  portable C surface. The four Phase 19 targets are complete; independent
  external production use remains an ongoing maturity signal.

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
19. Completed: add target triples, Linux and macOS support, ARM64, supported
    cross-compilation paths, and multi-platform native validation without
    modifying the frozen Rocket 2.0 SDK.
20. Completed: freeze Rocket 2.0 with bounded-input hardening, fuzzing,
    sanitizer/minimization workflows, caching and scale validation,
    compatibility tests, signed reproducible release tooling, and complete
    learning/governance material. External use continues as post-release signal.

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

**Phase 12 - Rocket 1.2 scalable abstractions (completed)**

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
- Added statically selected traits, generic `where` constraints, deterministic
  ambiguity and completeness diagnostics, and direct trait-method lowering.
- Added typed expression lambdas. Captures lower to compiler-generated immutable
  ARC aggregates and generic callback calls specialize to direct closure calls.
- Added persistent user iterator integration using `iterator`, `has_next`,
  `value`, and `advance`, with explicit control-flow MIR and correct `continue`
  behavior.
- Added associated constants as zero-argument functions, kept parameters
  positional/required/fixed-arity, and capped user monomorphization at 4,096
  specializations per compilation.
- Mirrored parsing, module mapping, type resolution, and direct-call lowering in
  the Rocket-written compiler. Added stage0, production, self-hosted, generic,
  enum, standard-library, and multi-module fixtures.
- Verified pinned LLVM Debug and Release matrices (90/90 tests each) and
  LLVM-disabled stage0 Debug and Release matrices (61/61 tests each). The
  Rocket 1.2 conformance suite passes 48 cases and all performance gates pass.
- Verified deterministic `stage0 -> stage1 -> stage2 -> stage3` bootstrap with
  the Phase 12 fixtures. Stage2 and stage3 LLVM IR are byte-identical at SHA-256
  `924002739c44ff04a2e354455859eec0807af5a720293e2049037329180997fa`.
- Hardened generic closures so lambda parameter/result annotations inherit and
  concretely substitute enclosing generic-function type parameters in stage0
  and the self-hosted compiler, including managed captures and parity coverage.
- Closed the remaining closure-call parity gap: the self-hosted compiler now
  lowers immediately invoked anonymous lambdas through their concrete generated
  `.call` symbol and preserves enclosing substitutions through nested generated
  callables, with generic, managed-capture, and invalid-argument coverage.

**Phase 13 - Rocket 1.3 native interoperability and library production
(completed)**

- Added lexical `unsafe:` blocks and restricted all imported native calls to an
  explicit unsafe region without weakening ordinary type, control-flow, or ARC
  validation.
- Froze a narrow Windows x64 C ABI for primitive `extern fn` imports and
  unmangled `export fn` wrappers. Added compile-time `extern const`, non-owning
  `Pointer[T]`, pointer-valued `extern opaque`, pointer-only `extern struct`
  layouts, and exact top-level noncapturing `extern callback` values.
- Kept native pointers and handles outside Rocket ARC. Documented borrowed/owned
  handle conventions, explicit release calls, status translation, pointer-plus-
  length strings, and synchronous non-storing callback lifetimes.
- Added `[build]` executable/static/dynamic products and target-scoped
  `[native.windows-x64]` libraries, search paths, and header inputs. Static and
  dynamic Rocket libraries can omit `main` and emit deterministic C headers.
- Added deterministic `rocketc emit-header` and narrow-C-subset `rocketc bind`
  workflows. Generated binding declarations are public so handwritten modules
  can expose small safe wrappers.
- Implemented equivalent parsing, validation, MIR lowering, C ABI emission,
  native linking, library production, and generation behavior in the permanent
  C++ stage0 and Rocket-written compiler.
- Added bidirectional C/Rocket static and dynamic consumers, native handle,
  pointer-layout, callback, primitive/Bool, unsafe-failure, safe-wrapper,
  manifest, and deterministic stage0/self-host generation coverage.
- Verified pinned LLVM Debug and Release matrices (102/102 tests each) and
  LLVM-disabled stage0 Debug and Release matrices (66/66 tests each). The
  Rocket 1.3 conformance suite passes 57 cases and all six performance gates
  pass, including native package checking and static-library production.
- Verified deterministic `stage0 -> stage1 -> stage2 -> stage3` bootstrap with
  Phase 13 native package, library, header, and binding checks. Stage2 and
  stage3 LLVM IR are byte-identical at SHA-256
  `e30135a93fca7c049bf201d7cb6ca714c466dbe9fb6d2812e2038a8f32326560`.

**Phase 14 - Rocket 1.4 graphics, audio, and real application validation
(completed)**

- Pinned raylib 6.0 to tag commit
  `dbc56a87da87d973a9c5baa4e7438a9d20121d28` with an exact archive size and
  SHA-256 manifest. Downloaded source, generated bindings, native libraries,
  executables, build trees, and bundles remain ignored.
- Added a reviewed primitive-only C adapter over raylib and generated its
  low-level Rocket module through the Phase 13 `bind` pipeline. Native structs,
  pointers, and strings never cross the application boundary.
- Added safe Rocket window, frame, drawing, texture, font, input, timing, audio,
  sound, procedural tone, synchronous callback, buffer, and error APIs. Opaque
  positive tokens validate ownership, parent lifetimes, use-after-release,
  double release, cleanup order, and one-window/one-device constraints.
- Added the non-casino Orbital Workshop application with rendering, keyboard and
  mouse input, collections and state, file and texture asset loading,
  callback-driven animation, procedural audio with silent fallback, and
  deterministic cleanup. No C++ application logic was added.
- Added ABI, positive, negative, missing-asset, repeated startup/shutdown,
  callback, buffer, texture/font, input, audio stress, native-linking,
  binding-generation, scaffold, and reference-package tests across stage0 and
  the self-hosted compiler.
- Added Debug/Release validation and bundle scripts, a clean app scaffold,
  tutorial, architecture/ABI documentation, decision D020, Rocket 1.4 syntax
  and release references, third-party notices, and static Windows packaging.
- Verified pinned LLVM Debug and Release matrices (111/111 tests each) and
  LLVM-disabled stage0 Debug and Release matrices (71/71 tests each). The
  dedicated raylib validation passes 10/10 in both configurations.
- Verified 63 Rocket 1.4 conformance cases and all eight performance gates. A
  fresh scaffold built successfully and the Release showcase bundle contains
  only the executable, authored assets/docs, upstream license, and checksums.
- Verified deterministic `stage0 -> stage1 -> stage2 -> stage3` bootstrap with
  byte-identical stage2/stage3 LLVM IR and generated raylib bindings. The LLVM
  IR SHA-256 is
  `5e5a33f1a38ac2192ee71b972e79bfc67f1e5e85ba6e0fbe19ffe63fdfe7e407`.

**Phase 15 - Rocket 1.5 production standard library (completed)**

- Completed byte/binary support with big-endian codecs and checked buffered
  reader/writer tokens while preserving immutable `ByteBuffer` and runtime ABI
  v1.
- Added explicit Unicode scalar iteration, NFC/NFD normalization, practical
  grapheme APIs, and a bounded tagged Thompson-NFA regular-expression engine
  without recursive backtracking.
- Added Windows CNG secure randomness, SHA-256, HMAC-SHA-256, constant-time
  comparison, offline Authenticode validation, DNS/TCP, WinHTTP HTTP/HTTPS, and
  bounded HTTP/1.x server request/response foundations with explicit timeouts
  and checked cancellation/close behavior.
- Added locale-neutral UTC/calendar/time-zone helpers, injection-safe logging,
  deterministic CLI/config parsing, XPRESS Huffman compression, validated
  data-only deterministic ustar archives, and parameterized/bounded SQLite.
- Added the ordinary bundled `std.testing` Rocket facade over a narrow private
  host boundary, with assertions, fixture containment, unpredictable temporary
  roots, one-shot cleanup, test filtering, explicit `.xfail.rocket` handling,
  and deterministic coverage hooks. Build failures cannot be masked as XFAIL.
- Hardened regex long-miss behavior, TAR end-marker/prefix/checksum parsing,
  SQLite UTF-8/NUL handling, socket send deadlines, and the stage0 generated
  compiler's Windows stack reserve. Recorded decisions D024-D030 and the Rocket
  1.5 release/library/specification contracts.
- Verified pinned LLVM Debug and Release matrices (131/131 each), LLVM-disabled
  stage0 Debug and Release matrices (91/91 each), focused Phase 15 suites
  (18/18 per backend), 72 conformance cases, and all eight performance gates.
- Verified deterministic `stage0 -> stage1 -> stage2 -> stage3` bootstrap with
  every Phase 15 fixture checked by all stages and run by stage3. Stage2 and
  stage3 LLVM IR are byte-identical at SHA-256
  `7e0d139180b692ccaf265768154bd210a4c4da4b00b08db737e7f9aacce67418`.

**Phase 16 - Rocket 1.6 dependency management and package ecosystem - Completed**

- Completed Phase 16 on 2026-07-31 from cumulative handoff commit `9575d79`
  while preserving the existing Phase 17 foundation. The exact SemVer resolver,
  deterministic lockfile, SHA-256 content cache, locked/offline modes, tree, and
  audit foundation now drives ordinary compiler import lookup.
- Enforced the exact selected dependency graph in stage0 and the Rocket-written
  compiler. `check`, `build`, `run`, and `test` reject stale edges, undeclared
  cross-package imports, missing cache content, and poisoned sources; transitive
  imports work in executable and static-library consumers.
- Added signed ECDSA P-256 registry configuration, indexes, namespace ownership
  and transfer histories, yanks, revocations, and security advisories. Added
  immutable version publishing, reserved-name/case/typosquatting rules,
  transactional recovery, provenance reporting, affected-version evaluation,
  and stable compromised/yanked diagnostics.
- Added Windows Credential Manager login/logout with scoped and revocable tokens,
  deterministic source/document archives, package documentation generation and
  publication, and an authenticated bounded same-origin HTTPS service protocol.
  The signed file reference registry is the executable reference deployment.
- Added secure immutable Git acquisition using a direct argument vector, hardened
  Git configuration, exact object verification, and submodule/symlink/path
  rejection. Registry and Git cache installation use verified regular-file
  archives, size/count/path/case/device-name bounds, and transactional rename.
- Added SPDX expression and root license-policy enforcement, signed advisory
  auditing, exact native-dependency allowlisting, and unconditional dependency
  build-script denial. Resolve, audit, documentation, and publishing never run
  dependency code implicitly.
- Added `registry`, `login`, `logout`, `publish`, and `doc` CLI workflows. The
  Rocket-written production frontend consumes the exact graph itself and invokes
  the colocated C++ stage0 package-security host for the same credential,
  signature, HTTPS, Git, archive, and governance behavior.
- Added adversarial unit/end-to-end coverage for malicious archives, checksum and
  signature tampering, graph bypass, poisoned caches, namespace takeover and
  typosquatting, revoked credentials, yanks, compromised advisories, transaction
  interruption/recovery, native policy, and build-script denial. Independent
  utility, labels, rogue, and math packages are published and consumed
  transitively by real executable and library applications online and offline.
- Dependency verification passed with Git 2.47.1.windows.2, CMake
  3.31.6-msvc6, Ninja 1.13.1, MSVC 19.44.35228, LLVM 22.1.6, and raylib 6.0.
  LLVM Debug and Release passed 133/133 tests each; LLVM-disabled stage0 Debug
  and Release passed 93/93 each. All matrices include three Phase 16 workflows.
- Deterministic Release bootstrap passed. Stage2 and stage3 LLVM IR are identical
  at SHA-256
  `a019dd80b0975efad667588ecab0c886b314d6f77419d9a1342905af38c5202c`.
  The clean online and relocated locked-offline fixture executable is identical
  at SHA-256
  `5c84c5ab076b41df6e2c5aaac128680ece5d7a44db52dadb1203e83d0d0dc182`.
- Release conformance passed 78/78 cases; report SHA-256 is
  `cde895b31dfba59e71e5e64e4e7f943f013b7957bc36b74e0a5e641fda06860b`.
  All eight performance gates passed; report SHA-256 is
  `563fab122d28c632103dc8ce272ab986933f39bbcfd8b2b9269d976cc3856486`.
- Release packaging and sanitized relocation passed, including locked/offline
  Phase 16 use. The final ZIP is 251,357,847 bytes at SHA-256
  `53a57a13d62e38006946a11ab18b635ee532ec7da9c4d8058d024fc0f6d060dc`;
  its `SHA256SUMS.txt` is
  `176b8f1b9e10e1adea8783333f44ed4835898e5b198a437aaeba7088647e1414`.
- Deliberate Rocket 1.6 limits: no public hosted registry is claimed; the file
  registry is the executable reference and `PACKAGES.md` is the HTTPS contract.
  Credential storage is Windows-only. Dependency build scripts remain
  unsupported and native dependency inputs remain deny-by-default. Phase 17 was
  completed separately from those package-ecosystem guarantees, as recorded below.

**Phase 17 - Rocket 1.7 professional developer experience (completed)**

- Phase 17 started from reviewed Phase 16 completion tip
  `5ec449c0410f3989282f7603f1be6888b8400b82`, not `master` or either old
  foundation commit.
- `rocket-lsp` Protocol 1.0 provides bounded incremental UTF-16 synchronization,
  exact locked multi-package graphs, unsaved overlays, semantic completion and
  imports, hover/signature help, navigation, references/rename, semantic token
  deltas, idempotent actions, configuration, cancellation, stale suppression,
  and source-free telemetry. It reuses the compiler lexer/parser/HIR/type system.
- Documentation generation, CodeView/PDB plus `rocket-source-map-1` debugging,
  coverage/profile/benchmark reports, `rocket-message-1` JSON Lines, and the
  measured incremental-AOT REPL prototype are versioned and documented.
- Full pinned LLVM matrices passed: Debug 138/138 in 345.29 seconds and Release
  138/138 in 186.51 seconds. LLVM-disabled stage0 matrices passed Debug 95/95 in
  473.31 seconds and Release 95/95 in 457.47 seconds.
- Deterministic bootstrap passed in Debug and Release. Both stage2/stage3 LLVM IR
  files are identical at SHA-256
  `bc4be23c7e06024ba9337bcfcdd1ffca470735dd8f542c09b0ea9720812a82ed`.
  Debug and Release `SHA256SUMS.txt` hashes are respectively
  `56a786c2a62210c484441ec3d7da56674536e8507577ad20454d22efdf5aab54`
  and `4f4bd6499e8de4cad81030e289f785ba3de47087a077c09a4b9ea3d47684ac11`.
- Conformance passed 78/78 in both configurations; report SHA-256 values are
  `1aaa4e133740fcf0ae473d259fdd7d188f97534a18e22dc0a84c28ac914fc88a`
  (Debug) and
  `bd224f96f50f2f8cda38337c4d585cd61869a83460b0452492bc90c0149867d0`
  (Release).
- All 8/8 performance gates passed. Debug measured 0.019/0.738/152.756/155.655/
  0.740/0.430/0.173/0.870 seconds and Release measured
  0.023/0.229/80.917/87.119/0.123/0.460/0.173/0.800 seconds for hello check/build,
  compiler HIR/MIR, native check/build, and raylib check/build. Report hashes are
  `edec538eb57d302df012239f3aaad37d2d6b1d0b2bb9d3a4b240df172c9e3e30`
  and `869fe034a33478a748b14a6f0d40aaca4818fb3d05a1c50c7f2441ec8a9777ab`.
- The independent Node.js LSP client passed an actual framed session over a
  four-file locked dependency graph (226 bytes), an unsaved overlay, and semantic
  dependency completion in 7 ms. Its report SHA-256 is
  `c2fb07c5709c59e1bb87b0d9a4473a0e678fb6ffaeb3afdde16564967d2d22fd`.
- Debug and optimized binaries both passed PDB procedure/local/line-record and
  source-map validation, including the `int_overflow.rocket` panic location.
  The validation report SHA-256 is
  `e17922223817ce53a19c7a3f4a3bc2578090d21907df08588100916624b7b0ca`.
- Coverage, profiling, three-iteration benchmarking, and strict compiler
  build/test JSON Lines validation passed. The report hashes are
  `6b613f7d67ac96acc8dbdfbd5f8f8b02281099f608d4ba5f947cb342a67a7654`,
  `47254ba7d5b860c0d06ebcc1f66076d28f6ae6328548b3905cc0eb34e765884b`,
  and `7ec23a1de87d0b2b9a4db47eb082385b91121dbeefc654fd2b76f045500e4e0d`.
- Release packaging and sanitized relocation passed with the compiler, LSP,
  stage0, tools, debug artifacts, coverage workflow, locked/offline package use,
  and editor client. The final ZIP is 251,735,184 bytes at SHA-256
  `c6dd4de275e45849f43b9305c27676baf69ce7f1e2faee9a4d51086e796c4121`;
  its `SHA256SUMS.txt` is
  `33ed9ec16f8c352f6429d69bc6656778d1d13cf299ab527041994f58f3f1ec27`.
- Deliberate Rocket 1.7 limits: native debugging is Windows x64 CodeView/PDB;
  coverage reports executed points rather than a zero-hit percentage model;
  profiling supplies deterministic entry counters rather than a sampling
  profiler; cancellation prevents queued/stale publication but does not preempt
  an already-running synchronous frontend pass; Rocket has no conditional
  inactive-source syntax; and the AOT REPL replays accumulated expressions, so
  it does not promise durable mutable state, declaration redefinition, resource
  lifetime preservation, or JIT latency.

**Documentation and frozen-contract reconciliation**

- Reconciled the README, charter, project summary, language specification,
  diagnostic catalog, tooling guide, self-hosting contract, standard-library
  wording, and completed syntax-dictionary headings with Rocket 1.5 as the
  then-current completed release and Rocket 1.6/1.7 as incomplete foundations.
- Marked the Rocket 1.0 release contract and syntax dictionary as intentional
  historical compatibility references without changing their frozen behavior.
- Restored stage0/self-hosted parity for `rocketc new`: both implementations now
  scaffold independent user packages at the frozen `0.1.0` starting version,
  with a focused CTest regression.
- Verified the pinned LLVM Debug and Release matrices at 132/132 tests each.
  The deterministic Release bootstrap passes with byte-identical stage2/stage3
  LLVM IR at SHA-256
  `d02a1473d0fc67d0af8e3678c7aada751d76c0052121166080dcbc43e97548e8`.

**Phase 16/17 stabilization and branch reconciliation**

- Reconciled the completed Phase 16/17 lineage with the later `master`
  documentation/scaffold commit. Preserved the frozen `rocketc new` package
  version of `0.1.0` and its stage0/self-hosted parity regression while keeping
  the completed Rocket 1.6 and 1.7 contracts and handoff state authoritative.
- Fixed an intermittent Windows package-cache commit failure caused by virus
  scanners or other readers briefly opening a newly populated transaction
  directory without delete sharing. Cache commits now retry only bounded
  transient access/sharing/lock violations for up to two seconds, retain atomic
  same-volume rename semantics, and accept a concurrently installed destination
  only after verifying its exact source checksum.
- Added a deterministic Windows regression that holds the transaction directory
  open without delete sharing, releases it after 200 ms, and proves the bounded
  retry commits the verified cache tree successfully.
- GitHub review found no open Phase 16/17 bug reports, review comments, or review
  threads. The obsolete Phase 17 foundation pull request and earlier milestone
  branches are superseded by the cumulative stabilized history.
- Full pinned LLVM matrices passed 139/139 tests in Debug and Release. Full
  LLVM-disabled stage0 matrices passed 96/96 tests in Debug and Release. These
  include the three Phase 16 package workflows, the Phase 17 tooling suites, the
  independent editor-neutral LSP client, and the self-hosted package-scaffold
  parity regression.

**Phase 18 - Rocket 1.8 robust ownership, concurrency, and asynchronous I/O
(completed 2026-08-02)**

- Specified the complete safe surface in `CONCURRENCY.md`, `SPEC.md`,
  `STDLIB.md`, decision D032, the R4101-R4106 diagnostic catalog, the 1.8 syntax
  dictionary, release contract, migration guide, and a runnable ownership/
  buffer/task example. Existing Rocket 1.0-1.7 source and synchronous APIs are
  unchanged.
- Added identity-bearing `Weak[T]` handles with atomic upgrade/expiration and
  an explicit weak-back-edge cycle strategy. Added move-only
  `UniqueBuffer[T]` thaw/get/set/append/slice/freeze operations while preserving
  immutable `Array`/`Slice` observable value semantics.
- Added structural `Send`/`Share` checking, scoped and move-only escape rules,
  graph promotion from cheap thread-confined ARC to atomic shared ARC, and
  stable R4101-R4106 diagnostics in both compilers. Native pointers/opaque
  handles, unique buffers, guards, and task groups cannot cross concurrency
  boundaries in safe code.
- Added a bounded default executor (1-64 workers, queue limit 65,536), affine
  typed task handles with one consuming join/await, borrowing status/cancel,
  cooperative task cancellation, same-executor helping for
  nested waits, deterministic drain/join shutdown, dedicated thread handles,
  finite task groups, FIFO bounded channels with backpressure, resource-limited
  unbounded channels, close/drain semantics, mutex guards, events,
  sequentially consistent `AtomicInt`, and
  seeded or empty exactly-once cells. Scalar and managed payloads are preserved
  through mutex, once, channel, task-group, and thread boundaries. Dropped
  groups cancel and join every child; dropped
  undetached thread handles join.
- Added contextual `async fn` and prefix `await` through lexer/parser/AST,
  typed HIR, explicit async-call/await MIR, verifier rules, C++ LLVM lowering,
  the C++ fallback emitter, and the Rocket-written LLVM emitter. Both LLVM
  compilers capture arguments in managed aggregate contexts and call the same
  bounded runtime task-spawn entry through generated thunks; async functions
  return `Result[T, String]` and require `T: Send`.
- Added cooperative cancellation tokens/children/current-task observation,
  monotonic deadlines and waitable timers, overlapped-event async file
  read/write, Winsock-readiness socket connect/accept/send/receive, and direct
  Windows child-process execution with cancellation/deadline termination. All
  work is dispatched through the bounded executor; no operation creates an
  unbounded thread per request.
- Runtime stress covers 10,000 weak self-cycle allocations, a weak-broken
  multi-object cycle, concurrent weak upgrade/destruction, high-contention
  retain/release with exactly-once destruction, task cancellation before/during/
  after completion, nested awaits, group ordering/drop cleanup, thread lifetime,
  finite contended mutex waits, 8x10,000 atomic increments, concurrent once
  reads and exactly-one initialization, event wakeup/cancellation, scalar and
  managed channels, FIFO/backpressure/close/disconnect/resource-limit races,
  512-task pool contention, 1,000 real executor startup/drain/shutdown cycles,
  primitive groups, nested `?` cleanup, and detached-thread ownership. Parser,
  HIR, MIR-verifier, C++ emitter, LLVM IR, positive native,
  negative diagnostic, stage0, and self-hosted fixture coverage is included.
- Dependency verification passed with Git 2.47.1, CMake 3.31.6-msvc6, Ninja
  1.13.1, Clang/LLVM 22.1.6, MSVC 19.44.35228 x64, and raylib 6.0.
- Final-source dependency verification passed with Git 2.54.0.windows.1,
  CMake 4.3.2, Ninja 1.13.1, Clang/LLVM 22.1.6, MSVC 19.51.36252 x64, and
  raylib 6.0. The verifier now rejects any native tool that exits nonzero.
- Final-source pinned LLVM Debug and Release passed 210/210 tests in 481.59 and
  133.59 seconds of CTest time (528.3 and 176.0 seconds end-to-end).
  LLVM-disabled stage0 Debug and Release passed 165/165 in 392.80 and 349.03
  seconds of CTest time (410.2 and 387.8 seconds end-to-end).
- The focused Phase 18 Release selection passed 78/78 twice in 24.35 and 24.20
  seconds (26.2 and 26.0 seconds end-to-end). It contains 77 `phase18`-labelled
  tests plus the self-host compiler fixture dependency; every wait-oriented
  native test retains a finite CTest timeout.
- The strengthened deterministic Release bootstrap passed in 489.4 seconds.
  Stage0, stage1, stage2, and stage3 agreed on all 14 positive and 15 negative
  Phase 18 fixtures. Stage2 and stage3 canonical LLVM IR are byte-identical at
  SHA-256 `d2bb814269f3c05fd823b21e6ab0e6b908fcca99f37d28d5fd1130a50d01ba23`.
  In that standalone bootstrap, stage1/stage2/stage3 executable hashes were
  respectively
  `070d2dfba2c27e01f90675b5ae998e51466ea9147e4b64ad0c17d28afeb0f18a`,
  `4c8e09a965eafd347f4be4f4656fd958acb5d5b9febad5c5d24bd2ae1ed9ad2b`,
  and `013f20d9dd6c3612dc1d3eceb79cd91bfc918ccdad35f61fda5b7e187439a2bc`;
  and its bootstrap `SHA256SUMS.txt` was
  `c1a42db7de91fb2812e0ae8fd5a9b60e17ddc912035de2e00c5fddfa1d2b72f8`.
- Release conformance passed 90/90 in 29.3 seconds; report SHA-256 is
  `0e98a67c23332df7311059828172288f1f8bb61af0c426dcaa9d69da30b04d8c`.
- All 11 Release performance gates passed in 98.4 seconds end-to-end:
  hello check/build 0.009/0.181 seconds; compiler HIR/MIR 43.937/50.155;
  native check/build 0.133/0.351; raylib check/build 0.082/0.534; and Phase 18
  concurrency check, async build, and task-group build 0.020/0.186/0.172.
  Report SHA-256 is
  `22007e4ca181623b360c1b9f2d90a5e9a9b648933d2880b74d15827dce610780`.
- Final self-contained packaging and sanitized relocation passed in 496.4
  seconds. The relocated
  compiler discovered only bundled tools, compiled and ran Rocket 1.0/1.1 and
  Phase 18 programs, exercised debug/coverage and locked-offline Phase 16 use,
  and produced the ownership/concurrency output `41`, `3`, `42`. The final
  `rocket-1.8.0-windows-x64.zip` is 268,864,815 bytes at SHA-256
  `0311bb7f305d6e2d3c23eb18ba34040cb9267261fd5a52fe31e9dba3c3cb4576`.
  Its 913-entry `SHA256SUMS.txt` is
  `79ca17067d58585421d4867b1f2af01d65783895bad1eb55375a0bf9c71423d6`;
  the package-run `BOOTSTRAP_SHA256SUMS.txt` is
  `f089af554078ebd0b558773036701b92626628fefdf76a2d9cd9f849b5792d93`.
- Validation found no compiler, runtime, ownership, concurrency, cancellation,
  lifetime, or asynchronous-I/O defect. It found and fixed three release-gate
  defects: native tool exit statuses were ignored, bootstrap lacked explicit
  Phase 18 stage parity, and sanitized relocation lacked Phase 18 execution.
- Deliberate Rocket 1.8 limits: Windows is the only async backend; file transfer
  is overlapped but socket/process coordination does not use a general IOCP
  dispatcher; process tasks inherit standard streams and return only an exit
  code; only one bounded default pool is public; task groups accept a finite
  existing task array rather than dynamic spawn; and strong cycles still leak
  unless a programmer makes at least one back edge weak. These are explicit
  contracts, not hidden claims of process capture, user-created pools, dynamic
  groups, tracing collection, or multi-platform event support.

**Phase 20 - Rocket 2.0 security, performance, compatibility, and trust
(completed 2026-08-02; Phase 19 completed 2026-08-29)**

- Recorded owner decision D033: the Rocket 2.0 Windows x64 release freezes its
  runtime ABI v1 and cumulative Rocket 1.0-1.8 surface; C++ stage0 remains
  permanent. Rocket 2.1 later completed the portable target set.
- Added deterministic `R1003` limits for 4 MiB sources/overlays, 4,096-module,
  64 MiB, and 64-level import graphs, 1 MiB manifests, 64 KiB manifest lines, 4,096 entries,
  1,024 dependencies, and bounded source discovery in both compilers.
- Added deterministic lexer/parser/MIR/manifest generation and malformed-input
  hardening. It found a real parser recovery hang on a stray top-level dedent;
  recovery now consumes indentation tokens and has a focused regression.
  Added MSVC AddressSanitizer configuration for the LLVM-disabled stage0,
  frontend, and runtime (the pinned prebuilt LLVM libraries remain in the
  normal matrices), resource-exhaustion tests, and a deterministic line-based
  compiler-reproducer minimizer.
- Added conservative `rocket-build-cache-1` package artifact reuse in stage0
  and the Rocket-written compiler. The key covers exact package bytes,
  compiler/runtime hashes, target/options/product, dependency identities, and
  native inputs. Tests prove initial miss, unchanged hit, source invalidation,
  rebuilt hit, and cached execution. Independent package processes are also
  validated concurrently.
- Added a compatibility matrix over representative 1.0-1.8 source/package/tool
  contracts and 2.0 versioning. Added a generated 16-package dependency chain,
  repeated cached execution, two parallel package builds, four raylib headless
  tests, and the ownership/concurrency reference application.
- Added release channels, clean-tree official-release policy, Authenticode
  binary signing, detached CMS checksum signing, `RELEASE-PROVENANCE.json`,
  complete checksum verification, fixed-timestamp deterministic ZIP creation,
  double-build archive comparison, and sanitized relocation verification.
  Local development artifacts remain explicitly unsigned/non-official.
- Published `SECURITY.md`, `CONTRIBUTING.md`, the Rocket book, Rocket 2.0 syntax
  freeze/release/migration contracts, FFI guide, package-author guide, and
  updated specifications, tooling, package, self-hosting, diagnostics, charter,
  roadmap, README, and decision journal.
- Final LLVM Debug/Release matrices passed 215/215 in 482.97/152.99 seconds of
  CTest time. LLVM-disabled stage0 Debug/Release passed 169/169 in
  446.79/397.05 seconds. MSVC AddressSanitizer passed 16/16 and optimized
  Phase 20 passed 17/17, each using 2,000 deterministic frontend and 256
  manifest cases.
- Final-source deterministic Release bootstrap passed stage0-stage3 parity;
  stage2/stage3 IR is byte-identical at SHA-256
  `a538c9fbe6762072e2d6fb131ce827e997346db7c8e4b48488877e366d0b2b84`.
  Release conformance passed 90/90 and all 11 performance budgets passed.
- Final 64-level-bound local unsigned/non-official packaging passed the
  215-test rebuild, bootstrap, checksum verification, sanitized relocation,
  and double-archive reproduction in 608.4 seconds. The external
  268,906,612-byte `rocket-2.0.0-windows-x64.zip` SHA-256 is
  `6896b8f6a883dc5bfa17c4f92b76fb272a40dad18b42e2db0403c92c70751220`;
  its 927-entry checksum-file SHA-256 is
  `97c16627e460fec855a1546422dc8286ca26ae50c3b0ad916c16e0de70d4e3f2`.
- Final validation evidence, report hashes, and deliberate limitations are
  mirrored in `PHASE_20_AUDIT.md`. No external
  production user or official certificate signature is claimed without
  external evidence; adoption remains post-release maintenance input.
- Post-freeze Visual Studio Community 2026 integration now upgrades the existing
  `Rocket.Language.VisualStudio` identity to a real version-2 VSPackage/MEF
  VSIX. Context-sensitive GUI Build/Run/Test/Stop/Debug commands discover the
  nearest package or a standalone source, run hidden redirected process trees,
  write to a dedicated Rocket Output pane, and translate `rocket-message-1`
  diagnostics into navigable Error List tasks. Tools > Options contains
  portable compiler/LSP/environment/argument settings.
- The VSIX connects `.rocket` content to the existing `rocket-lsp` process and
  launches the native Visual Studio debugger only after validating the Rocket
  executable, PDB, and source map. It preserves the frozen Rocket 2.0 compiler,
  runtime, stage0, LSP, CodeView, and sidecar contracts; Phase 19 later
  completed and no casino work is included in this historical integration.
- Focused extension tests cover package/standalone discovery, JSON diagnostics
  and summaries, source maps, Windows argument parsing, and hidden redirected
  processes. VSIX inspection preserves identity/version/assets and rejects
  embedded checkout paths. The installed Community 2026 instance contains
  version 2.0.3 with both the generated VSPackage and LSP MEF assets. Existing
  CMake targets and Visual Studio launch scripts remain fallback automation.
- Post-integration Debug and Release matrices passed 215/215 in 465.59 and
  139.49 seconds of CTest time. The final focused VSIX suite passed 26/26. The
  final package rebuild SHA-256 is
  `599caa26c31ef4621a801e474d8aaade5f9b9f700a132a0385c1252f82410153`;
  its VSPackage DLL SHA-256 is
  `ebf45bb18bf138253bdc87060c1ba3aca7ca9a4a44d50b77d33e09143f7b513b`.
  The GUI-accepted installation was built from the same 2.0.3 source state;
  its installed DLL SHA-256 is
  `4741a6a64db8d0d5050f0cc090e7a89724b21bad65ad89e00d432466f12ebc2c`.
  Visual Studio ActivityLog recorded a clean begin/end load for
  `RocketPackage` with no Rocket-specific warning or error. The recursive demo
  backend built, printed `55` plus the preserved local demo edit, passed 1/1
  tests, and produced an unoptimized PDB containing `fibonacci`, line records,
  and locals.
- Fixed GUI environment validation passing the active `.rocket` file as a
  process working directory. Validation now resolves a repository root or real
  directory before launching `rocketc --version` and `rocket-lsp --version`;
  focused tests cover both active-file and active-directory inputs, and the
  corrected installed command returns normally against the real demo.
- The final installed VSPackage enumerated all seven Rocket commands in a real
  Visual Studio 18 automation host. Build, Run, and Test each entered and left
  the extension's active-command state against `examples/visualstudio_demo`;
  the expected failing immutable-assignment build also completed after its
  structured diagnostic path ran. A normal VSIX-loaded `.rocket` session
  started `rocket-lsp.exe` as a child process, and the server exited with its
  Visual Studio host. Headless hosts now avoid activating Output/Error List UI
  while normal interactive windows retain automatic pane/task display.
- Owner-assisted visible GUI acceptance completed in Visual Studio Community
  2026 on 2026-08-08 against `examples/visualstudio_demo`. **Extensions >
  Rocket** exposed all seven commands. Environment validation found
  `rocketc 2.0.0` and `rocket-lsp 1.0.0`; GUI Build succeeded, GUI Run printed
  `Rocket pilot`, `3`, `42`, `84`, recursive Fibonacci `55`, and the preserved
  local `YoWassupCookie` edit in the Rocket pane; GUI Test passed 1/1.
- A temporary invalid edit produced live squiggles and structured `R2001`
  Error List rows with `main.rocket` line numbers. Double-clicking an entry
  selected line 29, and removing the temporary lines returned the document and
  Error List to clean state. LSP initialization was visible, and hover displayed
  `fn fibonacci(value: Int) -> Int`.
- Native GUI debugging initially exposed a real defect: Visual Studio's console
  launch flags opened an external Windows Terminal. VSIX 2.0.2 replaced that
  path with a hidden suspended process, redirected streams, and native attach;
  2.0.3 also recognizes and automatically continues only the attach-generated
  `ntdll` break. Final one-click Debug opened no external terminal and stopped
  directly at `main.rocket` line 13. Locals displayed `value = 10`, Call Stack
  displayed `fibonacci` and `main`, F11 entered a second recursive `fibonacci`
  frame at line 10, and **Stop Rocket** returned Visual Studio to design mode
  with `main.exe` gone. Rocket source breakpoints are never auto-continued.
- Deliberate VSIX boundary: terminal-free Run/Debug does not offer interactive
  console stdin. Run has no console, and the hidden Debug process reads from
  `NUL`; stdout/stderr, program arguments, and application-level file or GUI
  input remain supported.

**Rocket 3 WP11 - default arguments**

- Added ordinary function and method defaults to the permanent C++20 stage0
  and Rocket-written compiler. Required parameters precede defaulted parameters;
  defaults type-check in declaration context, may bind legal earlier parameters,
  specialize with generics, and cross module boundaries through interface
  metadata. Explicit positional/named arguments override defaults.
- Calls retain receiver/callee and written-argument left-to-right evaluation,
  then evaluate omitted defaults in parameter order before positional MIR
  normalization. Runtime ABI v1 and backend ABI remain unchanged. Lambda,
  callback, trait-declaration, enum-payload, extern, and struct-field defaults
  remain documented and parser-tested exclusions.
- Formatter, LSP signature help, package documentation/search metadata, language
  specifications, stable diagnostics, and source/API-versioning guidance now
  agree with the compiler behavior. The native HIR regression now locates
  `main` by symbol identity; 20 consecutive runs completed without the prior
  `rocketc_hir_tests.exe` memory-read application fault.
- The final focused selection passed 5/5 and stage0/self-host predecessor parity
  passed 8/8. Fresh Debug and Release suites passed 226/226 each. The coherent
  LLVM-disabled MSVC Release selection passed 18/18. Deterministic Windows x64
  Release bootstrap passed six self-tests, two HIR/MIR compiler checks, and the
  WP10/WP11 stage3 matrices; stage2/stage3 IR matched at SHA-256
  `4aa87fe969ff42d8806c938a24106d2a14bad91a76f23cbda063ae27ed8eb210`.
- Generated WP11 state remained under `out/rocket3-provisional/wp11`, occupied
  5.692 GiB, and did not cross the 4 GiB per-process or 20 GiB operation guards.

**Rocket 3 WP11A - complete named-callable parity**

- Removed WP10's remaining named-callable exclusions in the permanent C++20
  stage0 and Rocket-written compiler. Closure values, immediately invoked
  lambdas, every registered standard intrinsic, and the `print` built-in now
  accept positional, all-named, reordered, and positional-then-named calls with
  stable unknown, duplicate, missing, conflict, and type diagnostics.
- Added fully labeled enum payload declarations such as
  `Value(amount: Int, label: String)`. Labeled constructors accept every named
  call form and carry public labels through cross-module metadata; legacy
  anonymous payloads remain valid and positional-only, while mixed labeled and
  anonymous entries are rejected. Enum-payload defaults and the other WP11
  default exclusions remain intentionally unsupported.
- HIR records compiler-owned intrinsic/built-in names, local closure parameter
  names, public enum labels, positional ABI order, and written evaluation order.
  MIR evaluates the callee/receiver and written operands left-to-right before
  normalization. Runtime ABI v1 and backend ABI remain unchanged. Formatter,
  LSP signature help, documentation/search metadata, VS Code, and Visual Studio
  expose the same contracts.
- The final focused WP10/WP11/WP11A stage0/self-host selection passed 12/12.
  Fresh full Debug and Release suites passed 228/228 each. The coherent
  LLVM-disabled MSVC Release stage0/predecessor selection passed 19/19.
  Deterministic Windows x64 Release bootstrap passed six self-tests, two
  HIR/MIR compiler checks, and the stage3 WP10/WP11/WP11A matrices; stage2 and
  stage3 IR matched at SHA-256
  `d6a8e980c386837045a0e84ad997ac3024149663e697ed76593d16be968c632f`.
- Generated WP11A state remained under `out/rocket3-provisional/wp11a`, occupied
  5.112 GiB, and stayed below the 4 GiB per-process and 20 GiB operation guards.

**Rocket 3 WP12 - complete standard math module**

- Added the final compiler-owned `std.math` standard module in both C++20 stage0
  and the Rocket-written compiler. Its Float and explicit Int API covers the
  full constants, scalar, rounding, exponential/logarithmic, trigonometric,
  conversion, interpolation, smoothing, and bounded-motion inventory; named
  parameter metadata is shared with LSP signature help and documentation.
- Added deterministic runtime implementations and LLVM plus LLVM-disabled
  stage0 lowering without changing runtime ABI v1. The requirements document
  freezes the final names and the NaN/infinity, invalid-range, rounding,
  saturation, signed-zero, endpoint, and no-overshoot contracts.
- The required RED fixture failed before implementation with unknown
  `std.math` call diagnostics. The repaired stage0/self-host matrix now passes
  76/76 numeric/domain vectors in each compiler, with named-surface, package
  documentation, formatter, language-server, and four-target lowering gates.
  Fresh Debug and Release CTest suites passed 231/231 each; the LLVM-disabled
  WP10/WP11/WP11A/WP12 stage0/predecessor selection passed 4/4, while
  LLVM/self-host native execution remains intentionally outside that fallback
  configuration.
- Deterministic Windows x64 Release bootstrap passed six self-tests, two
  HIR/MIR checks, and stage3 WP10/WP11/WP11A/WP12 matrices. Stage2/stage3 IR
  matched at SHA-256
  `bac28a1ae6bb945ae92686e0d6aea9441bc86f58e8e06c5b316f187cbd669ef7`.

## Current next task

**Phase 19 remains the accepted Rocket 2.1 portability baseline, and Rocket 3
WP12 is complete and LOCAL-GREEN; native non-Windows numeric confirmation for
R3-F04-008 remains a WP34/F29 target-laboratory acceptance item. The
lowest-numbered next eligible Rocket 3 packet
is WP13, easing and complete motion; WP14 remains blocked until WP13 completes.
The exact one-packet scope and success-only handoff are in
`ROCKET_3_0_GRAPHICS_UI_IMPLEMENTATION_PLAN.md`. Visual Studio extension 2.0.3,
its reproducible CMake/script fallbacks, and the preserved owner demo edit
remain baseline state.**

## New-chat prompt

Use this at the beginning of a new Rocket chat:

```text
Read AGENTS.md and docs/PROJECT_CONTEXT.md first. We are working on Rocket.
Inspect the repository before editing, preserve existing user changes, implement the
requested work, run relevant validation, update PROJECT_CONTEXT.md when needed, and
commit completed work.
```
