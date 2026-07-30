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
features. That surface remains the frozen compatibility baseline, not a claim
that later compatible 1.x releases cannot add libraries or tooling. Rocket 1.5
is the latest completed milestone; package-management and professional-tooling
foundations exist for 1.6 and 1.7, while concurrency and additional platforms
remain roadmap work.

The C++20 compiler remains the permanent reproducible `stage0`, runtime ABI v1
remains compatible throughout Rocket 1.x, and incompatible language changes
require the Rocket 2.0 process. The full language-maturity roadmap through 2.0
comes before work on the separate casino application.

The confirmed public names are **Rocket** for the language, `rocketc` for the compiler, and `.rocket` for source files.
