# Rocket Project Context and Chat Handoff

Read this file at the start of every new Rocket chat. Update it after completing a milestone or making a permanent design decision.

## Project identity

- **Language:** Rocket
- **Compiler:** `rocketc`
- **Source extension:** `.rocket`
- **Primary target:** Windows x64
- **Goal:** A beginner-friendly, statically typed, LLVM-native language that becomes self-hosted before any serious casino development.
- **Casino goal after Rocket 1.0:** Local, single-player, play-money desktop casino with Blackjack, five-reel Slots, and Jacks-or-Better Video Poker.

## Locked decisions

- Bootstrap compiler: C++20; preserve it forever as `stage0`.
- Production backend: LLVM 22.1.6 ahead-of-time native compilation.
- Memory model: automatic reference counting; cycles are a documented version-1 limitation.
- Syntax: indentation blocks, explicit `let`/`var`, inferred local types, typed function boundaries.
- Error model: `Option[T]`, `Result[T, E]`, and `?`; never universal null or exceptions.
- Extensibility: official curated syntax sugar only; no arbitrary user macros in Rocket 1.0.
- Self-hosting comes before casino work.
- Graphics after self-hosting: safe Rocket bindings over raylib.
- Casino scope: no real money, payments, accounts, or online multiplayer in version 1.

## Current implementation state

Rocket is a C++ `stage0` compiler with a production LLVM backend. It is not yet
self-hosted.

Implemented:

- Indentation-aware lexer with line/column locations.
- Parser and AST for functions, bindings, assignment, `if`/`else`, `while`, integer `for` ranges, loop control, returns, calls, arithmetic, comparisons, logical operators, and scalar literals.
- Resolved HIR with deterministic declaration symbols, lexical name resolution, resolved calls, and checked expression types.
- Typed, basic-block MIR with explicit locals, operations, short-circuit branches, loop edges, and terminators, plus structural/type verification.
- Scalar MIR-to-LLVM lowering with a verified Windows x64 ABI, O2 optimization,
  object and assembly emission, native linking, and working `emit-ir`, `build`,
  `run`, and `emit-asm` commands.
- Temporary C ABI shims for scalar `print` and borrowed string-literal equality;
  the owned string/runtime ABI is the next milestone.
- Isolated MIR-to-C++ stage0 backend that remains buildable when LLVM is disabled.
- Hello World, recursive Fibonacci, lexer/parser/sema/HIR/MIR/bootstrap-codegen/
  LLVM-codegen suites, a golden diagnostic fixture, and native CLI regressions.
- Pinned LLVM 22.1.6, Ninja 1.13.1, and MSVC Windows x64 development setup.

Not implemented yet:

- Complete control-flow return analysis and a broader golden diagnostic catalog.
- Runtime ABI, ARC, owned UTF-8 strings, arrays, slices, structs, enums,
  generics, modules, `Option`, `Result`, standard library, formatter, test
  runner, package layout, editor support, and self-hosting.

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
11. Bind raylib and add safe Rocket APIs for windows, drawing, input, audio, and assets.
12. Build and test Blackjack, Slots, and Video Poker engines entirely in Rocket.
13. Create Casino v1: graphical lobby, shared wallet, profiles, saves, animations, audio, and a distributable application.

## Definition of the self-hosting gate

Casino development does not begin until:

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

## Current next task

**Phase 5: implement the runtime ABI, ARC, strings, arrays, and slices.**

- Define a versioned Rocket runtime ABI and ownership conventions at function,
  local, aggregate, and FFI boundaries.
- Add explicit retain/release behavior to MIR and LLVM lowering while keeping
  the C++ stage0 bootstrap reproducible.
- Implement owned UTF-8 `String` values and replace the temporary `printf`/
  `strcmp` lowering with Rocket runtime calls.
- Add `Array[T]` and `Slice[T]` representation, indexing, bounds checks, and
  deterministic destruction.
- Add focused allocation, aliasing, lifetime, bounds-failure, native execution,
  and leak/stress tests; document cycles as a version-1 limitation.
- Update the decision journal, specification, architecture, and this handoff
  after the milestone.

## New-chat prompt

Use this at the beginning of a new Rocket chat:

```text
Read AGENTS.md and docs/PROJECT_CONTEXT.md first. We are working on Rocket.
Continue from the "Current next task" section, inspect the repository before editing,
preserve existing user changes, implement only the stated phase, run its validation,
update PROJECT_CONTEXT.md, and commit the completed milestone.
```
