# Rocket Phase 19 Requirement-to-Evidence Audit

Phase 19 was completed by owner direction on 2026-08-29. This document is the
authoritative portability requirement map and completion record. It starts from
Rocket commit `cbf7b1a`, the accepted Rocket 2.0 Windows x64 release plus the
compatible Visual Studio Community 2026 integration.

All four native-host matrices and all four supported cross-build paths have
observed green CI evidence. The owner accepted the phase without an additional
rerun of the separately skipped cross-destination-execution matrix; that
workflow condition was corrected afterward in `4beb910`.

## Safety baseline

Scroll2Roll is a separate active repository, but its build consumes the frozen
Rocket 2.0 Windows x64 SDK from this checkout. Phase 19 must not mutate the
following inputs. All Phase 19 generated state belongs under `out/phase19` or
another uniquely named Phase 19 build directory.

| Frozen input | Baseline SHA-256 |
| --- | --- |
| `out/build/windows-release/rocketc.exe` | `5BCEBBA30477C012D40B0A7256E5E1FEB99EB0F7CEC09071385BD522CFC67270` |
| `out/build/windows-release/rocket_runtime.lib` | `94B0FCF3620A2127D5783A128B01A273FAC331AD464240E308FC0F23C215630F` |
| `out/build/windows-release/rocket-lsp.exe` | `CB47089D26831B000442D12D66129EB25470F20BF736951713A218B38C352406` |
| `out/package/rocket-2.0.0-windows-x64/bin/rocketc.exe` | `D2009A5DD0E6745FFF75C902C53323DC72A4FB9FF3C8BEA83360FDC675BF618E` |
| `out/package/rocket-2.0.0-windows-x64/lib/rocket_runtime.lib` | `94B0FCF3620A2127D5783A128B01A273FAC331AD464240E308FC0F23C215630F` |
| `out/package/rocket-2.0.0-windows-x64/bin/rocket-lsp.exe` | `CB47089D26831B000442D12D66129EB25470F20BF736951713A218B38C352406` |

These historical hashes identify the frozen Rocket 2.0 inputs. Phase 19 work
uses isolated output and does not clean or rebuild either frozen directory in
place.

## Supported target set

The required production targets are:

| User-facing name | Canonical LLVM triple | Native release host required |
| --- | --- | --- |
| `windows-x64` | `x86_64-pc-windows-msvc` | Windows x64 |
| `linux-x64` | `x86_64-unknown-linux-gnu` | Linux x64 |
| `linux-arm64` | `aarch64-unknown-linux-gnu` | Linux ARM64 |
| `macos-arm64` | `arm64-apple-macosx` | macOS ARM64 |

Windows ARM64 remains an evaluated target; WebAssembly and JIT compilation are
future platform work outside the completed Phase 19 scope.

## Requirement-to-evidence matrix

The status vocabulary is `missing`, `in progress`, `implemented`, and
`observed`. Only `observed` can satisfy an acceptance gate.

| ID | Requirement | Required implementation | Acceptance evidence | Status |
| --- | --- | --- | --- | --- |
| T01 | Explicit target identity | Canonical triples, stable aliases, host detection, normalized target values, and one shared model in stage0 and the Rocket-written compiler | Positive/negative target parsing tests and identical CLI output from stage0-stage3 | implemented |
| T02 | Host/target separation | Compilation options carry host and target independently; backend, linker, runtime, native inputs, cache, artifact names, and package metadata consume the target | Native and cross-host fixtures prove no host inference leaks into target output | implemented |
| T03 | Conditional compilation | Explicit deterministic target-conditioned source/module selection with inactive source excluded before parsing/type checking | Positive selection, inactive-invalid-source, ambiguity, missing-selection, dependency, and parity tests | implemented |
| T04 | Target queries | Stable compile-time target OS, architecture, environment, pointer width, endianness, and feature queries | Native fixtures on all supported targets plus canonical IR checks | implemented |
| T05 | Diagnostics | Stable diagnostics for unknown/unsupported targets, invalid target configuration, missing toolchains/sysroots/SDKs, unavailable native inputs, and unsupported cross paths | Golden diagnostics in both compilers with path-independent wording | implemented |
| T06 | Windows x64 compatibility | Existing Rocket 1.0-2.0 language, ABI v1, standard library, FFI, package, tooling, debugger, and application behavior remains valid | Isolated Debug/Release, stage0, bootstrap, conformance, compatibility, hardening, FFI, async, application, and package gates | observed |
| T07 | Linux x64 | Compiler, runtime, standard library, FFI, ELF/DWARF debugging, packages, concurrency/async, and release bundle | Complete native Linux x64 acceptance matrix and relocated package | observed |
| T08 | Linux ARM64 | AArch64 code generation plus the same Linux product surface and ABI rules | Complete native Linux ARM64 acceptance matrix and relocated package | observed |
| T09 | macOS ARM64 | Compiler, runtime, standard library, FFI, Mach-O/DWARF debugging, packages, concurrency/async, and release bundle | Complete native macOS ARM64 acceptance matrix and relocated package | observed |
| T10 | Cross-compilation | Enumerated supported host/target pairs, explicit toolchain/sysroot/SDK discovery, deterministic refusal elsewhere, target runtime selection, and no accidental host linking | Cross-build fixtures on every supported host/target path | observed |
| T11 | Runtime ABI v1 | Portable ABI declarations/layouts, ARC, strings, aggregates, collections, failures, atomics, weak ownership, task state, and platform adapters without source-visible ABI drift | ABI layout/symbol tests and runtime/lifetime/stress suites on every target | observed |
| T12 | Files, paths, environment, process, and time | UTF-8 public semantics over Windows and POSIX hosts with documented separator, executable lookup, process, clock, and error differences | Standard-library and failure-path suites on every target | observed |
| T13 | Networking, HTTP/TLS, crypto, archives, compression, SQLite, and credentials | Reviewed portable providers with bounded behavior and explicit platform policy differences | Isolated security/network/database/package suites on every target | observed |
| T14 | Concurrency and async I/O | Portable executor, threads, synchronization, cancellation, timers, async files/sockets/processes, and preserved `Send`/`Share` semantics | Repeated stress, cancellation, race, lifetime, and native async suites on every target | observed |
| T15 | FFI and native products | Per-target C ABI spelling, native manifest inputs, static/dynamic products, headers/bindings, linker ordering, and safe-wrapper behavior | Bidirectional C/Rocket suites and substantial native package on every target | observed |
| T16 | Debugging and tooling | Platform-neutral source maps plus CodeView/PDB on Windows and DWARF/native object formats on Linux/macOS; LSP remains host portable | Optimized/unoptimized symbol, line, frame, local, panic, coverage, profile, benchmark, and editor-neutral tests | observed |
| T17 | Package/cache portability | Target participates in locks where required, native policy, artifact cache keys, provenance, audit, and deterministic source selection; platform credential policy is explicit | Online/offline, hostile-input, cache invalidation, parallel-build, and relocation tests on every target | observed |
| T18 | Self-hosting | Permanent C++20 stage0 builds stage1; each native stage builds the next; stage2/stage3 canonical output is byte-identical | Native stage0-stage3 bootstrap report and hashes on every supported host | observed |
| T19 | Reproducible installation | Self-contained target packages include the compiler, runtime, linker/toolchain pieces, stage0 host, standard library, and tools needed for ordinary use | Checksums, double-build reproduction, sanitized relocation, and unrelated-system-compiler absence on every target | observed |
| T20 | Continuous matrix | Automated build, test, bootstrap, package, conformance, FFI, standard-library, compatibility, async, and application jobs for every target | Observed successful jobs, not merely committed workflow files | observed |
| T21 | Substantial applications | Orbital Workshop/raylib or an equivalent substantial non-casino application exercises graphics/audio/native/resource behavior per target | Native build and headless/application validation on every target | observed |
| T22 | Documentation | Target, platform differences, installation, cross-compilation, FFI, packages, standard library, concurrency, debugging, release, migration, roadmap, decisions, and context are current | Cross-reference audit plus commands checked against shipped packages | observed |
| T23 | Optional limitation audit | Broader calling conventions, dynamic loading, and incomplete raylib coverage are either required for parity and implemented or explicitly classified as optional | Requirement analysis and tests for any promoted surface | implemented |
| T24 | Honest completion | No fabricated adoption, signing, or platform claim; Windows ARM64/WebAssembly/JIT status is explicit | Final audit contains observed evidence and remaining non-gate limits only | observed |

## Implementation checkpoints

1. **Target contract:** specify target identity, CLI behavior, conditional module
   selection, queries, diagnostics, cache/native/package effects, supported
   cross paths, and compatibility. Implement the complete vertical slice in
   stage0 and the Rocket-written compiler with focused tests.
2. **Portable build foundation:** make CMake, dependency discovery, artifact
   naming, generated paths, LLVM target selection, linker invocation, and
   bootstrap scripts host/target aware. Preserve isolated Windows regression
   builds and the permanent LLVM-disabled stage0.
3. **Portable runtime and standard library:** separate portable cores from
   Windows/POSIX/Apple providers and close every platform-backed API row in the
   matrix, including concurrency and async I/O.
4. **FFI, debugging, packages, and applications:** implement per-target native
   products, debug information, security/package providers, raylib/application
   validation, installation, and relocation.
5. **Native acceptance:** run the full matrix on Windows x64, Linux x64, Linux
   ARM64, and macOS ARM64; fix defects until every required job is observed
   green and reproducible.
6. **Roadmap closure:** finish specifications, release/migration material,
   decision journal, project context, exact evidence, frozen-hash verification,
   clean-tree inspection, and the milestone commit.

## Limitation classification

The portable Phase 19 FFI requirement is the ordinary platform C ABI with
explicit static/dynamic products, generated headers/bindings, and target-scoped
native inputs. Broader foreign calling conventions (`stdcall`, vector, C++, or
vendor-specific ABIs) are not necessary for parity across the four production
targets and remain optional future FFI work. They must not be guessed by the
binding generator.

Rocket does not expose arbitrary safe-language runtime foreign-library loading.
That is intentional: native dependencies remain manifest-declared and reviewed.
Platform providers may dynamically load a system implementation behind a
documented standard-library API, but no unchecked handle/symbol capability is
made public. This preserves the Phase 13 safety boundary and does not block the
explicit dynamic-library product requirement.

The raylib surface remains the reviewed primitive adapter and Orbital Workshop
application subset, not a claim of every raylib API. Phase 19 requires that
reviewed surface to build, link, package, relocate, and run its headless
application validation per target; it does not require an unbounded generated
binding surface. Any expansion needs its own wrapper safety review and tests.

## Initial environment evidence

Observed on 2026-08-20 from the Windows x64 repository host:

- the tracked tree was clean at `cbf7b1a` before this audit was added;
- pinned dependencies are LLVM 22.1.6, Ninja 1.13.1, and raylib 6.0;
- WSL is not installed;
- Docker and QEMU are not installed;
- GitHub CLI is installed but both stored GitHub credentials are invalid;
- no Linux x64, Linux ARM64, or macOS ARM64 native runner is currently
  reachable from the local environment.

Those missing native environments do not reduce Phase 19 scope. Implementation
and local validation continue, while T07-T10 and T18-T21 remain unproved until
real hosts or authenticated CI runners supply observed evidence.

## Progress log

- **2026-08-29 - Phase 19 closed by owner direction:** GitHub Actions run
  [`33194302234`](https://github.com/RyanEid06/Rocket/actions/runs/33194302234)
  recorded successful native acceptance on Windows x64, Linux x64, Linux ARM64,
  and macOS ARM64, including Debug/Release tests, LLVM-disabled stage0,
  bootstrap, package, and relocation steps. Its first three cross builds also
  passed. The repaired fourth cross path and the other three passed in focused
  run [`33216752637`](https://github.com/RyanEid06/Rocket/actions/runs/33216752637).
  That focused run reused accepted native artifacts, so GitHub skipped its
  separate cross-destination-execution matrix. The condition was corrected in
  `4beb910`; the owner explicitly accepted Phase 19 without another rerun.

- **2026-08-24 - Windows x64 native acceptance observed:** All work used new
  directories below `out/phase19`; the frozen Rocket 2.0 outputs were not
  rebuilt or packaged. Clean isolated LLVM Debug and Release CTest matrices
  passed **222/222** in **532.06 s** and **177.38 s**. Clean isolated
  LLVM-disabled C++ stage0 Debug and Release matrices passed **174/174** in
  **474.57 s** and **418.81 s**. The Release stage0-to-stage3 bootstrap passed
  184 validation cases with identical stage2/stage3 canonical IR SHA-256
  `0710bae675be1dfc81b716605c07bd113771eedfd42bfd5092873d72768dfe2b`.
  The native Windows package and sanitized relocation check passed with 954
  files and archive SHA-256
  `ccc8a1a7ba33bbd6f0dd0ecfadfa341d589204aee182476e9f08cb25b34fedcc`.
  Target, self-host parity, portable workflow, application, compatibility,
  tooling, package, FFI, standard-library, ownership, concurrency, and async
  suites are included in those complete matrices. The temporary checked source
  snapshot is commit `2413a32`; Linux x64, Linux ARM64, macOS ARM64, and the
  cross-host native-destination evidence remain required before completion.

- **2026-08-20 - audit start:** Read the project context, roadmap, decision
  journal, compiler architecture, language, standard-library, concurrency,
  package, self-hosting, tooling, debugging, FFI, Rocket 2.0 release/migration,
  and Phase 20 audit contracts. Recorded the frozen Scroll2Roll-consumed hashes,
  required target set, 24 acceptance requirements, implementation checkpoints,
  and current native-runner gap. No compiler, runtime, dependency, generated,
  package, or Scroll2Roll file was changed.
- **2026-08-20 - checkpoint 1A, stage0 target foundation:** Added the normative
  Rocket 2.1 target contract and D034. Implemented four production aliases and
  canonical triples, native host detection, explicit features and artifact
  suffixes, Windows ARM64 evaluation status, supported-cross-path policy, and
  `R6001`-`R6005` identities in the C++ stage0 core. The CLI now normalizes
  `--target`, exposes `rocketc target [--verbose]`, carries the selected target
  into package loading, native-input selection, dependency loading, module
  overlays, and cache material, and refuses cross execution or absent target
  SDKs before host code generation. `[target.<alias>]` and
  `[native.<alias>]` selection exclude inactive invalid source/native files.
  The focused command was
  `ctest --test-dir out/phase19/build/windows-target-debug -R
  '^(target|package|phase19_target_cli)$' --output-on-failure`: 3/3 tests passed
  in 0.82 seconds after an isolated Debug build with MSVC 19.51.36256.0, LLVM
  22.1.6, and Ninja 1.13.1. The six frozen Rocket 2.0 hashes were recomputed
  after the test and remained byte-for-byte equal to the safety baseline.
  Self-hosted parity, target constants, backend selection, target-qualified
  artifacts, and actual cross SDKs remain part of checkpoint 1.
