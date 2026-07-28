# Rocket

Rocket is an experimental, beginner-friendly, statically typed language for native command-line programs and applications. Local types are inferred, blocks use indentation, and the compiler is designed to produce native code through LLVM.

This repository currently contains the first vertical slice: an indentation-aware frontend, basic semantic analysis, diagnostics, and a temporary C++ bootstrap backend. LLVM 22.1.6 is pinned and verified for development, but LLVM IR lowering has not yet been implemented; the bootstrap backend is not the intended final architecture.

## Current language subset

- Functions with typed parameters and return values
- Inferred immutable `let` and mutable `var` bindings
- `Int`, `Bool`, `String`, and `Unit`
- Arithmetic, comparisons, function calls, `if`/`else`, `while`, and `return`
- Built-in `print`

## Build on Windows

First install the pinned development dependencies once, then verify and build with the supplied scripts:

```powershell
.\dependencies\bootstrap.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\dependencies\verify.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Debug
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Release
```

The scripts activate the Microsoft x64 build environment and use the pinned Ninja and LLVM installation. Build output is written to `out/build/windows-debug` and `out/build/windows-release`.

Then try:

```powershell
.\out\build\windows-debug\rocketc.exe check .\examples\hello.rocket
.\out\build\windows-debug\rocketc.exe run .\examples\hello.rocket
.\out\build\windows-debug\rocketc.exe emit-asm .\examples\hello.rocket
```

See [the language specification](docs/SPEC.md), [project charter](docs/CHARTER.md), and [roadmap](docs/ROADMAP.md).
