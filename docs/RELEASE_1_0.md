# Rocket 1.0 Release Contract

> Historical compatibility baseline: this document intentionally describes the
> frozen Rocket 1.0 release. The latest completed release is Rocket 1.8; see
> [RELEASE_1_5.md](RELEASE_1_5.md) and the [roadmap](ROADMAP.md) for current
> status. Later additions do not rewrite the 1.0 contract.

Rocket 1.0 is the first frozen, self-hosted release of the language. The
production compiler is written in Rocket; the permanently preserved C++20
compiler is the reproducible stage0 bootstrap implementation.

## Frozen public surface

The following documents together define the 1.0 compatibility contract:

- `SPEC.md`: grammar, types, control flow, modules, matching, propagation, and
  ownership semantics.
- `STDLIB.md`: built-in module names, signatures, results, and contract-failure
  behavior.
- `DIAGNOSTICS.md`: stable `Rdddd` diagnostic categories and output shape.
- `TOOLING.md`: manifest, package layout, formatter, test runner, and CLI.
- `COMPILER_ARCHITECTURE.md`: typed HIR/MIR invariants and runtime ABI v1.

Rocket 1.x may add source-compatible functions, diagnostics, and tooling.
Existing valid 1.0 source must retain its meaning, and runtime ABI v1 remains
link-compatible throughout 1.x. Incompatible changes require Rocket 2.0.

## Distribution

`scripts/package-compiler.ps1 -Configuration Release` creates
`out/package/rocket-1.0.0-windows-x64.zip` with this layout:

```text
rocket-1.0.0-windows-x64/
  bin/rocketc.exe             # Rocket-written stage3 compiler
  bin/clang.exe
  bin/llvm-lib.exe
  bin/lld-link.exe
  lib/rocket_runtime.lib
  lib/clang/                  # compiler-rt resources
  lib/msvc/                   # static C/C++ link inputs
  lib/ucrt/
  lib/um/                     # Windows SDK import libraries
  stage0/rocketc-stage0.exe   # preserved C++ bootstrap compiler
  docs/
  editors/
  BOOTSTRAP_SHA256SUMS.txt
  SHA256SUMS.txt
```

The production compiler discovers every toolchain input relative to its own
executable. The packaging gate removes `ROCKET_CLANG`, `ROCKET_RUNTIME`, `LIB`,
`LIBPATH`, and `INCLUDE`, uses an isolated working directory, and proves native
check/build/run/direct-execution without an activated developer shell.

## Validation gates

The release is accepted only when all of these commands pass:

```powershell
.\scripts\build.ps1 -Configuration Debug
.\scripts\build.ps1 -Configuration Release
.\scripts\build-stage0.ps1 -Configuration Debug
.\scripts\build-stage0.ps1 -Configuration Release
.\scripts\bootstrap.ps1 -Configuration Release
.\scripts\conformance.ps1 -Configuration Release
.\scripts\performance.ps1 -Configuration Release
.\scripts\package-compiler.ps1 -Configuration Release
```

The bootstrap report must mark `deterministic true` and record identical
stage2/stage3 LLVM IR hashes. Conformance covers successful language, module,
runtime, formatter, package, and CLI workflows plus stable visibility, cycle,
and checked-overflow failures. Performance gates use generous regression
ceilings, not marketing benchmarks: hello check <= 5 seconds, hello native
build <= 15 seconds, compiler HIR self-check <= 120 seconds, and compiler MIR
self-check <= 180 seconds on the release machine.

## Known 1.0 limitations

- Windows x64 is the only supported target.
- ARC is single-threaded and reference cycles are not collected.
- There is no concurrency, macro system, package registry/manager, JIT, debugger,
  semantic language server, or graphical standard library.
- Strings expose UTF-8 byte traversal; general Unicode scalar/grapheme APIs are
  outside 1.0.
- Tests are ordinary native programs rather than a reflection-based test API.

Graphics and casino work are not part of this release. The next direction will
be chosen explicitly after the Rocket 1.0 handoff.
