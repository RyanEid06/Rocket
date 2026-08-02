# Rocket Self-Hosting Contract

Phase 9 established the permanent self-hosting gate: the compiler implementation
under `compiler/` is Rocket source and the following chain succeeds on Windows
x64. Rocket 2.0 continues to pass the same gate:

```text
C++ stage0 -> Rocket stage1 -> Rocket stage2 -> Rocket stage3
```

Stage 0 is the permanently preserved C++ compiler. It compiles the Rocket
compiler sources into stage1. Stage1 and every later stage must lex, parse,
resolve, type-check, lower, and emit the next compiler without invoking stage0
or copying a previously built compiler. Each stage emits canonical textual LLVM
IR, then uses pinned Clang/LLD only as the object/link driver.

## Bootstrap implementation subset

The compiler sources use the frozen Rocket 1.0 syntax plus additive Rocket 1.1
collection APIs, Rocket 1.2 abstraction features, and the Rocket 1.3 native
interoperability surface, with these specified standard APIs:

- checked `string.byte_at` and `string.slice` for source traversal;
- mutable `string.Builder` construction for linear-time IR and formatter output;
- persistent generic `collections.append` for compiler work-list growth and
  `collections.concat` for batch composition;
- `process.arguments` for the native CLI;
- `file.create_directory` for contained package artifact directories;
- existing file, path, string, collection, environment, and direct-process APIs.

The Rocket 1.3 compiler parses impls, traits and constraints, lambdas,
associated constants, user-defined iteration, unsafe blocks, native declarations,
and native exports. Stage0 and the Rocket compiler independently validate and
lower the same narrow C ABI, build native library products, consume target-aware
linker configuration, and generate byte-identical C headers and Rocket bindings.
These features participate in the normal stage2/stage3 determinism proof.

Rocket 1.4 adds no compiler-only graphics syntax. Stage0 and the self-hosted
compiler both resolve generated public native constants across modules,
preserve native manifest inputs when compiling package tests, normalize Windows
system-library inputs for Clang/LLD, and generate byte-identical raylib adapter
bindings. Bootstrap checks and builds the reference application and runs its
headless native suite without launching the interactive window.

No compiler-only syntax, source preprocessor, C++ callback, or runtime compile
function is permitted. Stage0 and the production runtime implement the same
public calls, and both backends run the same bootstrap primitive fixture.

Rocket 1.5 preserves that rule for the expanded production standard library.
The self-hosted compiler owns matching intrinsic identities, nominal HTTP
types, declarations, and runtime mappings for every host-backed API. It also
loads the public `std.testing` facade from bundled Rocket source using the same
installed-versus-repository search policy as stage0. Bootstrap conformance
checks all Phase 15 integration fixtures so no API exists only in the C++
compiler or only in the LLVM runtime.

Rocket 1.6 adds exact locked dependency imports to both loaders. The
Rocket-written loader reads `rocket.lock`, maps each import to the selected
SHA-256 cache root, and rejects edges not declared for the current owning
package with `R3005`. Its package commands expose byte-for-byte Stage 0 output
by invoking the fixed, distributed `rocketc-stage0.exe` package security host
through `process.run` with a separated argument array. This delegation is
limited to resolver/registry/credential/documentation operations; lexing,
parsing, semantic analysis, MIR, LLVM generation, and stage-to-stage compiler
construction remain Rocket-owned and do not invoke Stage 0.

## Determinism and acceptance

- Stage2 and stage3 emit byte-identical canonical `.ll` for every compiler
  module after paths and stage labels are excluded from semantic output.
- Stage1, stage2, and stage3 report the same version and diagnostic codes.
- The self-hosted compiler passes compiler conformance fixtures, native runtime
  and standard-library integration fixtures, and all CLI/package workflows.
- Stage0 and the self-hosted CLI produce identical `resolve`, `tree`, `audit`,
  locked, and offline output; the self-hosted compiler checks transitive imports
  after the original registry and path sources are removed.
- Stage0 and self-hosted `emit-header`/`bind` output is byte-identical, and both
  build and run the same native package and library fixtures.
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
compiler executable and the equal stage2/stage3 IR hashes. The script also runs
successful and failing language fixtures, native package and library builds,
deterministic header/binding generation, and package check, format, run, and
test workflows through the generated compiler.

Rocket 1.7 does not move or replace this bootstrap boundary. The standalone
LSP reuses C++ stage0 frontend libraries as a developer tool and never
participates in stage1/stage2/stage3 reproduction. Native CodeView generation,
coverage/profile hooks, benchmark orchestration, `--debug`, and JSON host output
remain audited stage0 services; the Rocket-written CLI forwards only those
explicit commands to its colocated `stage0/rocketc-stage0.exe`. Ordinary
self-hosted check/build/run, exact package graph loading, and deterministic
bootstrap stay Rocket-written. The full bootstrap gate must still prove
byte-identical stage2/stage3 IR before Phase 17 can ship.

Rocket 1.8 also preserves the boundary. Both compilers parse and type-check
`async fn`/`await`, derive the same `Send`/`Share` results, enforce move and
scoped-lifetime diagnostics, and lower async calls to the bounded runtime
executor through captured aggregate contexts and compiler-generated entry
thunks. Bootstrap conformance includes ownership, buffer, task, group, thread,
cancellation, file, socket, and process fixtures; the stage2/stage3 IR equality
check remains the release authority.

Rocket 2.0 adds no compiler-only language feature. Stage0 and the Rocket-written
compiler enforce the same source/manifest resource bounds and conservative
package artifact-cache contract. The cache is outside the bootstrap authority:
clean stage directories and byte-identical stage2/stage3 IR remain mandatory.
