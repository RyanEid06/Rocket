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
- Memory model: automatic reference counting; strong cycles must be prevented by
  using explicit `Weak[T]` back edges.
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

Rocket 1.5 standard-library, Rocket 1.6 package ecosystem, Rocket 1.7
professional developer experience, and Rocket 1.8 ownership/concurrency release
are complete on the self-hosted Rocket 1.4 foundation. The production `rocketc` is written in Rocket,
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

Not implemented yet:

- Broader native calling conventions, dynamic native loading, the full raylib
  surface, non-Windows targets, and the
  Rocket 2.0 security/performance/compatibility release gates. No casino
  implementation has begun.

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

**Phase 18 - Rocket 1.8 robust ownership, concurrency, and asynchronous I/O (completed)**

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
- Added a bounded default executor (1-64 workers, queue limit 65,536), repeatable
  typed task results, cooperative task cancellation, same-executor helping for
  nested waits, deterministic drain/join shutdown, dedicated thread handles,
  finite task groups, FIFO bounded channels with backpressure and close/drain
  semantics, mutex guards, events, sequentially consistent `AtomicInt`, and
  seeded once cells. Dropped groups cancel and join every child; dropped
  undetached thread handles join.
- Added contextual `async fn` and prefix `await` through lexer/parser/AST,
  typed HIR, explicit async-call/await MIR, verifier rules, C++ LLVM lowering,
  the C++ fallback emitter, and the Rocket-written LLVM emitter. Both LLVM
  compilers capture arguments in managed aggregate contexts and call the same
  bounded runtime task-spawn entry through generated thunks; async functions
  return `Result[T, String]` and require `T: Send`.
- Added cooperative cancellation tokens/children/current-task observation,
  monotonic deadlines and timers, and bounded-worker async file read/write,
  loopback socket connect/accept/send/receive, and direct Windows child-process
  execution with cancellation/deadline termination. No operation creates an
  unbounded thread per request.
- Runtime stress covers 10,000 weak self-cycle allocations, a weak-broken
  multi-object cycle, concurrent weak upgrade/destruction, high-contention
  retain/release with exactly-once destruction, task cancellation before/during/
  after completion, nested awaits, group ordering/drop cleanup, thread lifetime,
  finite contended mutex waits, 8x10,000 atomic increments, concurrent once
  reads, event wakeup/cancellation, and channel FIFO/backpressure/close/leak
  behavior. Parser, HIR, MIR-verifier, C++ emitter, LLVM IR, positive native,
  negative diagnostic, stage0, and self-hosted fixture coverage is included.
- Dependency verification passed with Git 2.47.1, CMake 3.31.6-msvc6, Ninja
  1.13.1, Clang/LLVM 22.1.6, MSVC 19.44.35228 x64, and raylib 6.0.
- The exact required build matrices passed: pinned LLVM Debug 179/179 in
  947.39 seconds and Release 179/179 in 329.28 seconds; LLVM-disabled stage0
  Debug 136/136 in 611.77 seconds and Release 136/136 in 598.08 seconds. The
  focused Phase 18 suite passed 41/41 twice in 40.23 and 40.36 seconds.
- Release bootstrap passed in 1168.2 seconds. The final documentation-refresh
  bootstrap checksum file records stage1
  `5fd03434a03b3081016fced801d73a54043a15131a9013548ad179ff3e5ae612`,
  stage2 `1f89a2b3f00fe7e71335352ec478d06c10f090472407a81d446a9e6adfdb2caf`,
  and stage3 `cc28755d5b89a8fc5b97e4d9b8416a1fd0c184580f19c3a2ae4124f07de3ca40`.
  Stage2/stage3 LLVM IR is byte-identical at
  `413766d67b0d0496bc163a63a6e329f75247e5b96cefa637c65f3f780d3e9b6e`;
  `SHA256SUMS.txt` is
  `6d52d6ba9dc18bab39808dda6f995f940dd03941973e12d3a9c7339eae3c6a43`.
- Release conformance passed 90/90 in 25.8 seconds; its report SHA-256 is
  `6bb2f64fdcf19eb6e3ed4fd500a8970c1adcdc4faa3a29a8dcbfa4893c6b23cd`.
  All 11 performance budgets passed: 0.016/0.350/118.034/126.288/0.091/0.341/
  0.100/0.831/0.039/0.317/0.336 seconds for hello check/build, compiler HIR/
  MIR, native check/build, raylib check/build, and Phase 18 concurrency check/
  async build/group build. The report SHA-256 is
  `e36a2e65196e3727441c0275b8abde6b8434e05e2721e8c40f97e1a869102ec9`.
- Final Release packaging and sanitized relocation passed in 1278.5 seconds,
  including a fresh 179/179 suite in 351.05 seconds, bootstrap, native build/
  run, coverage, collection execution, and locked/offline package audit. The
  final ZIP is 252,073,532 bytes at SHA-256
  `055e2f9ed645ce1f7e17dfdb7db8fbffa11f0e5f3c2f7166f2660345eb120963`;
  its `SHA256SUMS.txt` is
  `c5430dc40bc7ed9358c5e124dcffc6e54f5980ed09d28eddd3c106914df60870`.
- Deliberate Rocket 1.8 limits: Windows is the only async backend; file/socket
  operations are bounded-worker blocking rather than IOCP-overlapped; process
  tasks inherit standard streams and return only an exit code; only one bounded
  default pool is public; task groups accept a finite existing task array rather
  than dynamic spawn; channels are bounded only; once cells are seeded at
  construction; and strong cycles still leak unless a programmer makes at least
  one back edge weak. These are explicit contracts, not hidden claims of IOCP,
  output capture, user-created pools, dynamic groups, unbounded channels, lazy
  once initialization, or tracing collection.

## Current next task

**Review the completed Rocket 1.8 milestone. Phase 19 is the next roadmap item,
but do not begin multi-platform work or casino implementation without an
explicit owner instruction. Preserve the C++ stage0 and every Rocket 1.0-1.8
compatibility, bootstrap, package, tooling, ownership, and concurrency gate.**

## New-chat prompt

Use this at the beginning of a new Rocket chat:

```text
Read AGENTS.md and docs/PROJECT_CONTEXT.md first. We are working on Rocket.
Continue from the "Current next task" section, inspect the repository before editing,
preserve existing user changes, implement only the stated phase, run its validation,
update PROJECT_CONTEXT.md, and commit the completed milestone.
```
