# Rocket 1.0

Rocket is a beginner-friendly, statically typed language for native command-line programs and applications. Local types are inferred, blocks use indentation, and the self-hosted compiler produces native code through LLVM.

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
- `Option[T]`, `Result[T, E]`, and exception-free postfix `?` propagation
- Package-relative `import` modules with explicit `pub` visibility
- Typed `std` modules for strings, collections, files, paths, JSON, CSV, randomness, processes, and time
- Arithmetic, comparisons, `and`/`or`/`not`, and function calls
- Assignment, `if`/`else`, `while`, integer `for` ranges, `break`, `continue`, and `return`
- Built-in `print`
- `rocket.toml` packages, deterministic formatting, native test discovery, and coded diagnostics

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

Then try:

```powershell
.\out\build\windows-debug\rocketc.exe check .\examples\hello.rocket
.\out\build\windows-debug\rocketc.exe run .\examples\hello.rocket
.\out\build\windows-debug\rocketc.exe emit-ir .\examples\hello.rocket
.\out\build\windows-debug\rocketc.exe emit-asm .\examples\hello.rocket
.\out\build\windows-debug\rocketc.exe new .\out\hello-package
.\out\build\windows-debug\rocketc.exe test .\out\hello-package
.\out\build\windows-debug\rocketc.exe fmt .\out\hello-package --check
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
[diagnostic catalog](docs/DIAGNOSTICS.md), [standard-library reference](docs/STDLIB.md),
[project charter](docs/CHARTER.md), and [roadmap](docs/ROADMAP.md).

## Rocket 1.0 release

Build the checksummed, relocation-tested, self-contained Windows x64 archive:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package-compiler.ps1 -Configuration Release
```

The package uses the Rocket-written stage3 compiler and includes the runtime,
pinned Clang/LLD, compiler-rt resources, and native static link libraries. See
[the Rocket 1.0 release contract](docs/RELEASE_1_0.md) for compatibility,
limitations, and the complete validation matrix.
