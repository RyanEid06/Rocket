# Rocket

Rocket is a beginner-friendly, statically typed language for native command-line programs and applications. Local types are inferred, blocks use indentation, and the self-hosted compiler produces native code through LLVM.

Rocket 2.0 security, performance, compatibility, and trust work is complete on
the cumulative Rocket 1.0-1.8 language. Phase 19, the additive Rocket 2.1
portability release, is complete: it preserves valid Rocket 2.0 source and
runtime ABI v1 while adding explicit target selection, native Linux and macOS
hosts, and documented cross-compilation paths. The Windows x64, Linux x64,
Linux ARM64, and macOS ARM64 native matrices and the supported cross-build
paths have passed. Exact evidence and the remaining optional limitations are
recorded in [`PHASE_19_AUDIT.md`](docs/PHASE_19_AUDIT.md),
[`TARGETS.md`](docs/TARGETS.md), and [`PROJECT_CONTEXT.md`](docs/PROJECT_CONTEXT.md).

This page is the current public summary. The complete documentation inventory
and historical/current boundary are in
[`DOCUMENTATION_STATUS.md`](docs/DOCUMENTATION_STATUS.md).

## Rocket 3.0 WP34 status

Wave D is accepted on implementation tree
`4c06d544e457541b09acb92aeb37565f065c15f9`. WP29 `d115832`, WP31
`2f8a73e`, WP32 `29e843f`, and WP33 `f1363ba` remain visible behind non-squash
integration merges `0cf9910` and `4c06d54`. The integrated Windows Debug and
Release suites each pass `288/288`; the LLVM-disabled Release predecessor
matrix passes `209/209`; deterministic stage0 -> stage3 bootstrap passes with
matching IR SHA-256
`ef3f6bae64cf43965693cfd43ddf9c28b8e22eaed28f878669990329bafe42af`.
Rocket 2.1 conformance (`90` cases), compatibility (`11` cases), application
validation (`32` packages and `11` raylib checks), hardening (`17/17`), and the
976-file Windows SDK package/relocation gate also pass.

WP34/F29 is accepted on implementation tree
`a9e7ea261f25c4438eb3594312a79e88da99eda0`. Native acceptance run
[`35079104907`](https://github.com/RyanEid06/Rocket/actions/runs/35079104907)
passed all four native hosts, all four supported cross builds, and all four
native destination-execution jobs. Every native host passed Debug and Release
`288/288` plus LLVM-disabled stage0 Debug and Release `209/209`; bootstrap
passed 184 cases on Windows and Linux and 185 on macOS with matching
stage2/stage3 IR SHA-256
`ef3f6bae64cf43965693cfd43ddf9c28b8e22eaed28f878669990329bafe42af`.
Visual run
[`35079104860`](https://github.com/RyanEid06/Rocket/actions/runs/35079104860)
passed on Windows x64, Linux x64, Linux ARM64, and macOS ARM64. WP35 release
documentation and traceability closure has not started.

The accepted asset-store implementation is the reference-package module
`examples/raylib_showcase/src/rocket_assets.rocket`, imported as
`src.rocket_assets`. The planned `rocket.assets` namespace is not a module in
this checkout yet.

WP34 full compatibility/platform acceptance is complete. WP35 is the next
planned Rocket 3 packet and requires separate authorization.

This repository contains an indentation-aware frontend, resolved and typed HIR,
verified control-flow MIR, diagnostics, and a genuine LLVM 22 backend for
optimized native executables. Its linked ABI-v1 runtime provides ARC,
owned UTF-8 strings, checked integer arithmetic, nested collections, structs,
enums, and safe bounds failures. The production compiler is written in Rocket
and deterministically bootstraps through stage3. The C++ MIR transpiler remains
available only as the reproducible stage0 fallback when LLVM is explicitly disabled.

## Current language subset

- Functions with typed parameters and return values
- Inferred immutable `let` and mutable `var` bindings
- `Int`, `Float`, `Bool`, `Char`, `String`, and `Unit`
- Owned `Array[T]` literals, retained `Slice[T]` views, and checked indexing
- Generic structs and functions, payload enums, exhaustive `match`, and field access
- Static methods, traits, generic constraints, associated constants, and deterministic specialization
- Typed lambdas with ARC-managed captures and user-defined persistent iterators
- Explicit `unsafe:` native boundaries, C imports/exports, pointers, opaque handles,
  pointer-only native layouts, and synchronous callbacks
- `Option[T]`, `Result[T, E]`, and exception-free postfix `?` propagation
- typed `Weak[T]`, move-only `UniqueBuffer[T]`, structural `Send`/`Share`,
  atomic ARC publication, and stable concurrency diagnostics
- `async fn`, prefix `await`, bounded tasks, dedicated thread handles,
  structured task groups, cancellation, monotonic timers, channels, mutexes,
  events, atomics, seeded/exactly-once publication, and bounded Windows-event
  asynchronous I/O work
- Package-relative `import` modules with explicit `pub` visibility
- Named and default arguments for the accepted Rocket 3 callable surface, plus
  labeled enum payload construction
- Bundled `std.math`, `rocket.motion`, `rocket.graphics`, `rocket.ui`, and
  `rocket.raylib.safe` modules, with typed shapes, input, canvas, layout, and
  typography helpers
- Editor-neutral `rocket-lsp` Protocol 1.0 semantic tooling with incremental
  multi-package analysis, unsaved overlays, navigation, refactoring, semantic
  tokens, a full Visual Studio Community 2026 client, and dependency-free VS
  Code plus independent Node.js clients
- Typed `std` modules for strings, collections, binary buffers and buffered I/O,
  Unicode, safe regular expressions, cryptography, networking and HTTP,
  calendars, logging, CLI/config parsing, compression, safe archives, SQLite,
  testing, files, paths, JSON, CSV, randomness, processes, and time
- Arithmetic, comparisons, `and`/`or`/`not`, and function calls
- Assignment, `if`/`else`, `while`, integer `for` ranges, `break`, `continue`, and `return`
- Built-in `print`
- `rocket.toml` executable/static/dynamic products, target-aware native inputs,
  deterministic C headers/bindings, formatting, native test discovery, and coded diagnostics

## Build on Windows

First install the pinned development dependencies once, then verify and build with the supplied scripts:

```powershell
.\dependencies\bootstrap.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\dependencies\verify.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Debug
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Release
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\bootstrap.ps1 -Configuration Release
```

The scripts activate the Microsoft x64 build environment and use the pinned Ninja and LLVM installation. Build output is written to `out/build/windows-debug` and `out/build/windows-release`.

For the purple Visual Studio Community 2026, build and install the
repository-owned `Rocket.Language.VisualStudio` 2.0.3 extension. It provides
normal GUI Build, Run, Test, Stop, and Debug commands; nearest-package and
standalone-file discovery; hidden redirected execution; a Rocket Output pane;
navigable Error List diagnostics; the existing LSP feature set; and native
CodeView/PDB debugging without an external terminal. Setup, validation, and
honest limitations are documented in the
[Visual Studio guide](editors/visualstudio/README.md) and
[tooling reference](docs/TOOLING.md#visual-studio-2026).

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\test-visualstudio-extension.ps1
```

Then try:

```powershell
.\out\build\windows-debug\rocketc.exe check .\examples\hello.rocket
.\out\build\windows-debug\rocketc.exe run .\examples\hello.rocket
.\out\build\windows-debug\rocketc.exe emit-ir .\examples\hello.rocket
.\out\build\windows-debug\rocketc.exe emit-asm .\examples\hello.rocket
.\out\build\windows-debug\rocketc.exe bind .\native\vendor.h --output .\native_bindings.rocket
.\out\build\windows-debug\rocketc.exe emit-header .\my-library --output .\my-library\.rocketc\my_library.h
.\out\build\windows-debug\rocketc.exe new .\out\hello-package
.\out\build\windows-debug\rocketc.exe test .\out\hello-package
.\out\build\windows-debug\rocketc.exe fmt .\out\hello-package --check
.\out\build\windows-debug\rocketc.exe build .\examples\hello.rocket --debug
.\out\build\windows-debug\rocketc.exe coverage .\examples\hello.rocket
.\out\build\windows-debug\rocketc.exe profile .\examples\hello.rocket
.\out\build\windows-debug\rocketc.exe benchmark .\examples\hello.rocket --iterations 10
.\out\build\windows-debug\rocketc.exe check .\examples\hello.rocket --message-format=json
```

`emit-ir` prints verified, unoptimized LLVM IR. `build`, `run`, and `emit-asm`
use LLVM's O2 pipeline; build artifacts are kept in a source-adjacent ignored
`.rocketc` directory. To validate the preserved stage0 fallback, configure a
separate build with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-stage0.ps1 -Configuration Debug
```

See [the current Rocket 3.0 syntax dictionary](docs/ROCKET_3_0_SYNTAX_DICTIONARY.md),
[the documentation status ledger](docs/DOCUMENTATION_STATUS.md),
[the Rocket 1.0 syntax dictionary](docs/ROCKET_1_0_SYNTAX_DICTIONARY.md),
[language specification](docs/SPEC.md), [tooling/package guide](docs/TOOLING.md),
[package and registry contract](docs/PACKAGES.md),
[language-server protocol](docs/LANGUAGE_SERVER.md),
[native debugging guide](docs/DEBUGGING.md),
[incremental-AOT REPL evaluation](docs/REPL.md),
[diagnostic catalog](docs/DIAGNOSTICS.md), [standard-library reference](docs/STDLIB.md),
[Rocket 1.1 syntax additions](docs/ROCKET_1_1_SYNTAX_DICTIONARY.md),
[Rocket 1.2 syntax](docs/ROCKET_1_2_SYNTAX_DICTIONARY.md),
[Rocket 1.3 native syntax](docs/ROCKET_1_3_SYNTAX_DICTIONARY.md),
[Rocket 1.4 syntax](docs/ROCKET_1_4_SYNTAX_DICTIONARY.md),
[Rocket 1.5 library dictionary](docs/ROCKET_1_5_SYNTAX_DICTIONARY.md),
[Rocket 1.8 syntax](docs/ROCKET_1_8_SYNTAX_DICTIONARY.md),
[Rocket 2.0 syntax freeze](docs/ROCKET_2_0_SYNTAX_DICTIONARY.md),
[ownership and concurrency contract](docs/CONCURRENCY.md),
[the Rocket book](docs/BOOK.md), [FFI guide](docs/FFI_GUIDE.md),
[package author guide](docs/PACKAGE_AUTHOR_GUIDE.md),
[Rocket 2.0 migration guide](docs/MIGRATION_2_0.md),
[Rocket 2.1 migration guide](docs/MIGRATION_2_1.md),
[Rocket 2.0 release contract](docs/RELEASE_2_0.md),
[Rocket 2.1 release contract](docs/RELEASE_2_1.md),
[project charter](docs/CHARTER.md), and [roadmap](docs/ROADMAP.md).

Rocket 2.0 freezes runtime ABI v1. Thread-confined values retain cheap
plain ARC; checked publication promotes a complete managed graph to atomic ARC.
The compiler rejects unsafe transfers, move reuse, scoped-handle escape, and
invalid suspension with stable `R4101`-`R4106` diagnostics.

## Packaging and release status

The frozen Rocket 2.0 Windows x64 archive remains the stable SDK consumed by
Scroll2Roll. Rocket 2.1 packages are generated under `out/phase19` and do not
overwrite that frozen SDK. Use the target-aware package workflow documented in
[the Rocket 2.1 release contract](docs/RELEASE_2_1.md); it must not overwrite
the frozen SDK.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package-compiler.ps1 -Configuration Release
```

The package uses the Rocket-written stage3 compiler and includes the runtime,
pinned Clang/LLD, compiler-rt resources, and native static link libraries. See
[the Rocket 2.0 release contract](docs/RELEASE_2_0.md),
[the tooling guide](docs/TOOLING.md), [language-server contract](docs/LANGUAGE_SERVER.md),
and [project context](docs/PROJECT_CONTEXT.md) for compatibility, limitations,
and the complete validation matrix.
