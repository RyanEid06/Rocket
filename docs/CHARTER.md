# Rocket Language Charter

## Purpose

Rocket is a readable, statically typed language for command-line tools,
automation, native libraries, desktop applications, games, and services that
start quickly and compile ahead of time to standalone native programs. The
language should remain approachable for a beginner without preventing
substantial maintained software.

## Design principles

1. Common code should be readable without type annotations everywhere.
2. Mistakes should be reported before execution with precise source locations.
3. Performance claims must be supported by reproducible comparisons.
4. The language should expose absence and recoverable failure through `Option` and `Result`, never universal null.
5. The compiler, runtime, and bootstrap chain should remain reproducible,
   reviewable, and understandable rather than hiding core behavior behind
   opaque generated systems.
6. Security, compatibility, and performance claims must have specifications and
   repeatable acceptance tests.
7. New features should be complete vertical slices across the specification,
   stage0, self-hosted compiler, runtime or tooling, tests, and documentation.

## Compatibility and roadmap boundaries

Rocket 1.0 intentionally targeted Windows x64 and excluded classes,
concurrency, macros, a package manager, JIT compilation, and semantic IDE
features. That surface remains the compatibility baseline, not a claim that
later compatible releases cannot add libraries or tooling. Rocket 2.0 freezes
the cumulative Rocket 1.0-1.8 language, ownership, package, tooling, runtime ABI
v1, and Windows x64 FFI contracts. Phase 19 is the additive Rocket 2.1
portability effort. It may add the explicit target and platform contracts in
`TARGETS.md`, but it cannot weaken the frozen Windows compatibility boundary or
call a platform supported before native-host acceptance evidence exists.

The C++20 compiler remains the permanent reproducible `stage0`, runtime ABI v1
remains the frozen Rocket 2.x ABI, and incompatible language changes require a
new major-version decision and migration plan. The owner accepted Phase 20's
technical Windows x64 release without claiming unobserved external adoption.
Compatible editor and tooling integrations may consume the frozen CLI, LSP,
diagnostic, CodeView, and source-map contracts without reopening the language
roadmap; the completed Visual Studio Community 2026 VSIX follows this rule.

The confirmed public names are **Rocket** for the language, `rocketc` for the compiler, and `.rocket` for source files.
