# Rocket 1.8

Rocket is a beginner-friendly, statically typed language for native command-line programs and applications. Local types are inferred, blocks use indentation, and the self-hosted compiler produces native code through LLVM.

Rocket's 1.8 ownership, concurrency, and asynchronous-I/O implementation is
complete on top of the Rocket 1.6 package ecosystem and 1.7 tooling. The exact
final-source release-validation reruns still outstanding are recorded in
[`PROJECT_CONTEXT.md`](docs/PROJECT_CONTEXT.md) and
[`PHASE_18_AUDIT.md`](docs/PHASE_18_AUDIT.md); no final artifact is claimed yet.
Phases 19 and 20 must pass the [master roadmap](docs/ROADMAP.md) before the
separate casino application may begin.

This repository contains an indentation-aware frontend, resolved and typed HIR,
verified control-flow MIR, diagnostics, and a genuine LLVM 22 backend for
optimized Windows x64 executables. Its linked ABI-v1 runtime provides ARC,
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
- Editor-neutral `rocket-lsp` Protocol 1.0 semantic tooling with incremental
  multi-package analysis, unsaved overlays, navigation, refactoring, semantic
  tokens, and dependency-free VS Code plus independent Node.js clients
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

For the purple Visual Studio Community 2026, install the repository-owned Rocket
extension and launch the project with the pinned toolchain environment as
described in the [tooling guide](docs/TOOLING.md#visual-studio-2026).

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

See [the Rocket 1.0 syntax dictionary](docs/ROCKET_1_0_SYNTAX_DICTIONARY.md),
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
[ownership and concurrency contract](docs/CONCURRENCY.md),
[Rocket 1.8 migration guide](docs/MIGRATION_1_8.md),
[Rocket 1.8 release contract](docs/RELEASE_1_8.md),
[project charter](docs/CHARTER.md), and [roadmap](docs/ROADMAP.md).

Rocket 1.8 extends runtime ABI v1 additively. Thread-confined values retain cheap
plain ARC; checked publication promotes a complete managed graph to atomic ARC.
The compiler rejects unsafe transfers, move reuse, scoped-handle escape, and
invalid suspension with stable `R4101`-`R4106` diagnostics.

## Rocket 1.8 release

Build the checksummed, relocation-tested, self-contained Windows x64 archive:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package-compiler.ps1 -Configuration Release
```

The package uses the Rocket-written stage3 compiler and includes the runtime,
pinned Clang/LLD, compiler-rt resources, and native static link libraries. See
[the Rocket 1.8 release contract](docs/RELEASE_1_8.md),
[the tooling guide](docs/TOOLING.md), [language-server contract](docs/LANGUAGE_SERVER.md),
and [project context](docs/PROJECT_CONTEXT.md) for compatibility, limitations,
and the complete validation matrix.
