# Rocket Self-Hosting Contract - Phase 9

Phase 9 is complete only when the compiler implementation under `compiler/` is
Rocket source and the following chain succeeds on Windows x64:

```text
C++ stage0 -> Rocket stage1 -> Rocket stage2 -> Rocket stage3
```

Stage 0 is the permanently preserved C++ compiler. It compiles the Rocket
compiler sources into stage1. Stage1 and every later stage must lex, parse,
resolve, type-check, lower, and emit the next compiler without invoking stage0
or copying a previously built compiler. Each stage emits canonical textual LLVM
IR, then uses pinned Clang/LLD only as the object/link driver.

## Bootstrap implementation subset

The compiler sources use the frozen pre-1.0 Rocket syntax plus these specified
standard APIs:

- checked `string.byte_at` and `string.slice` for source traversal;
- mutable `string.Builder` construction for linear-time IR and formatter output;
- persistent generic `collections.concat` for compiler work lists;
- `process.arguments` for the native CLI;
- `file.create_directory` for contained package artifact directories;
- existing file, path, string, collection, environment, and direct-process APIs.

No compiler-only syntax, source preprocessor, C++ callback, or runtime compile
function is permitted. Stage0 and the production runtime implement the same
public calls, and both backends run the same bootstrap primitive fixture.

## Determinism and acceptance

- Stage2 and stage3 emit byte-identical canonical `.ll` for every compiler
  module after paths and stage labels are excluded from semantic output.
- Stage1, stage2, and stage3 report the same version and diagnostic codes.
- The self-hosted compiler passes compiler conformance fixtures, native runtime
  and standard-library integration fixtures, and all CLI/package workflows.
- `scripts/bootstrap.ps1` starts from a clean artifact directory, records hashes,
  and fails if any stage is substituted, stale, or non-deterministic.
- C++ stage0 remains independently buildable with LLVM disabled.

## Implemented Rocket compiler

`compiler/src/main.rocket` owns source loading, lexing, parsing, name and type
resolution, generic specialization, typed HIR, verified control-flow MIR,
explicit ARC ownership instructions, textual LLVM IR generation, diagnostics,
the canonical formatter, package manifests, and every public CLI workflow.
Standard-module calls lower through the same runtime ABI v1 used by stage0.

Run the complete proof from a pinned developer environment with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\bootstrap.ps1 -Configuration Release
```

The resulting `out/bootstrap/windows-release/SHA256SUMS.txt` records every
compiler executable and the equal stage2/stage3 IR hashes. The script also
runs successful and failing language fixtures plus package check, format, run,
and test workflows through the generated compiler.
