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

Rocket is an early C++ `stage0` prototype, not yet an LLVM compiler.

Implemented:

- Indentation-aware lexer with line/column locations.
- Parser and AST for functions, bindings, `if`/`else`, `while`, `return`, calls, arithmetic, comparisons, strings, integers, and booleans.
- Semantic checks for `Int`, `Bool`, `String`, `Unit`, scopes, function calls, local inference, and basic return analysis.
- Temporary C++ transpiler for `rocketc check`, `build`, `run`, and `emit-asm`.
- Hello World, recursive Fibonacci, and frontend unit tests split into lexer, parser, semantic-analysis, and bootstrap-codegen suites.
- Pinned LLVM 22.1.6, Ninja 1.13.1, and MSVC Windows x64 development setup.

Not implemented yet:

- Assignment, `for`, ranges, `break`, `continue`, floats, chars, logical operators, and strong diagnostics.
- Typed HIR/MIR.
- LLVM IR lowering; `emit-ir` is currently a stub.
- Runtime ABI, ARC, arrays, structs, enums, generics, modules, `Option`, `Result`, standard library, formatter, test runner, package layout, editor support, and self-hosting.

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
7. Build the practical standard library and release-quality tooling.
8. Rewrite the compiler in Rocket and prove `stage0 -> stage1 -> stage2 -> stage3` self-hosting.
9. Freeze and publish Rocket 1.0.
10. Add raylib bindings, then console game engines, then the graphical casino.

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

## Current next task

**Phase 1: complete scalar language syntax and diagnostics.**

- Specify and implement assignment with mutability enforcement.
- Add `for` loops, ranges, `break`, and `continue`.
- Add `Char` and `Float` lexical, semantic, bootstrap-backend, and test support.
- Strengthen parser recovery and add golden diagnostic and CLI tests.
- Update the specification, decision journal, and this handoff after the milestone.

## New-chat prompt

Use this at the beginning of a new Rocket chat:

```text
Read AGENTS.md and docs/PROJECT_CONTEXT.md first. We are working on Rocket.
Continue from the "Current next task" section, inspect the repository before editing,
preserve existing user changes, implement only the stated phase, run its validation,
update PROJECT_CONTEXT.md, and commit the completed milestone.
```
