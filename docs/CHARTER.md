# Rocket Language Charter - Version 0.1

## Purpose

The language is for readable command-line tools, automation, and file-processing programs that start quickly and compile to standalone native executables.

## Design principles

1. Common code should be readable without type annotations everywhere.
2. Mistakes should be reported before execution with precise source locations.
3. Performance claims must be supported by reproducible comparisons.
4. The language should expose absence and recoverable failure through `Option` and `Result`, never universal null.
5. The implementation should remain small enough to understand and explain in a final-year project.

## Version 1 boundaries

Version 1 targets Windows x64 and excludes classes, concurrency, macros, a package manager, JIT compilation, and graphical IDE work. Reference counting will provide automatic heap management; reference cycles are initially a documented limitation.

The confirmed public names are **Rocket** for the language, `rocketc` for the compiler, and `.rocket` for source files.
