# Rocket Phase 20 Audit

Phase 20 is complete for Rocket 2.0 on its supported Windows x64 target. Phase
19 portability was explicitly deferred by owner direction; this audit does not
claim Linux, macOS, ARM64, cross-compilation, official certificate signing, or
independent production adoption.

Post-audit note (2026-08-08): the compatible Visual Studio Community 2026 VSIX
integration was completed after the Phase 20 language freeze. It consumes the
frozen compiler, LSP, diagnostics, CodeView, and source-map contracts and does
not alter the Phase 20 results below. Its evidence is recorded in
`PROJECT_CONTEXT.md` and `editors/visualstudio/README.md`.

## Contract and implementation mapping

| Requirement | Implementation and evidence |
| --- | --- |
| Frozen 2.0 surface | `SPEC.md`, `STDLIB.md`, `PACKAGES.md`, `TOOLING.md`, `CONCURRENCY.md`, `FFI_GUIDE.md`, decision D033, and the 2.0 syntax/release/migration documents freeze the cumulative 1.0-1.8 grammar, type/ownership rules, runtime ABI v1, package/tool protocols, and Windows x64 C FFI. No grammar was added. |
| Malformed/resource-bounded inputs | Stage0 and the Rocket-written loader bound files/overlays to 4 MiB and graphs to 4,096 files/64 MiB/64 nested imports. Manifests and discovery have explicit byte/count limits. `R1003`, deterministic fuzzing, exhaustion, and parser-recovery tests cover refusal. |
| Fuzzing, sanitizers, minimization | `hardening_tests.cpp` uses fixed-seed lexer/parser/MIR and manifest generation with repeatability checks. `windows-asan` instruments the LLVM-disabled stage0/frontend/runtime because the pinned prebuilt LLVM static libraries are not ASan-compatible. `minimize-crash.ps1` deterministically reduces line-based reproducers while retaining exit/code matching. |
| Stable caching and parallel work | Both compilers implement conservative `rocket-build-cache-1` whole-package keys. Tests prove miss/hit/invalidation/reuse/cached execution. The generated application gate builds two independent packages concurrently; one compiler invocation remains deterministic and single-process. |
| Scale and applications | The gate generates a 16-package dependency chain plus application, resolves and builds it, repeats cached runs, runs four raylib headless tests, and executes the ownership/concurrency reference application. |
| Compatibility | Eleven cases cover the 2.0 version plus representative Rocket 1.0-1.8 source, packages, native interop, machine protocol, and ownership/concurrency contracts. All passed in Debug, Release, stage0, and sanitizer-appropriate matrices. |
| Release trust | Packaging records `rocket-release-provenance-1`, complete SHA-256 sums, release channels, clean/dirty state, and fixed-timestamp deterministic ZIP reproduction. Official stable mode requires a clean tree and supplied signing certificate, signs three binaries and checksums, and verifies trust. The observed local artifact is deliberately unsigned and non-official. |
| Learning and governance | `SECURITY.md`, `CONTRIBUTING.md`, `BOOK.md`, `FFI_GUIDE.md`, `PACKAGE_AUTHOR_GUIDE.md`, `MIGRATION_2_0.md`, `RELEASE_2_0.md`, and the existing full references are published and cross-linked. |

The hardening suite discovered one genuine compiler defect: top-level parser
recovery could stop forever on a stray `Dedent`. Module-scope recovery now
consumes stray indentation tokens, and a regression parses an indented
top-level `return` to a finite syntax error. It also found and fixed one test
isolation defect: sanitizer-built native libraries now stay in the sanitizer
build tree instead of contaminating normal `.rocketc/native` outputs. Cache
parity also made the test fixture remove copied ignored `.rocketc` artifacts
before asserting a first-build miss.

## Observed validation

All commands ran from the repository root on 2026-08-02 with Git
2.54.0.windows.1, CMake 4.3.2, Ninja 1.13.1, Clang/LLVM 22.1.6, MSVC
19.51.36252 x64, and raylib 6.0.

| Command | Observed result |
| --- | --- |
| `scripts/build.ps1 -Configuration Debug` | Passed 215/215; final clean CTest rerun took 482.97 seconds. |
| `scripts/build.ps1 -Configuration Release` | Passed 215/215 in 152.99 seconds CTest time (194.7 seconds end-to-end including compilation). |
| `scripts/build-stage0.ps1 -Configuration Debug` | LLVM-disabled stage0 passed 169/169 in 446.79 seconds CTest time (462.0 seconds end-to-end). |
| `scripts/build-stage0.ps1 -Configuration Release` | LLVM-disabled stage0 passed 169/169 in 397.05 seconds CTest time (434.6 seconds end-to-end). |
| `scripts/hardening.ps1 -Sanitizers` | Final MSVC AddressSanitizer stage0/frontend/runtime passed 16/16 with 2,000 frontend and 256 manifest cases in 33.37 seconds CTest time. Report SHA-256: `b773063e83dfb26b0b8f166109f6ec2305f28b1de82fceef9b3c533895646f88`. |
| `scripts/hardening.ps1 -Configuration Release` | Optimized Phase 20 selection passed 17/17 in 14.57 seconds with the same deterministic case counts. Report SHA-256: `ca1e883a3385808c8b3e16dc88d32aa945334bdefbbb933023dec74485475a15`. |
| `scripts/bootstrap.ps1 -Configuration Release -SkipStage0Build` | Final package-run proof passed stage0-stage3 parity. Stage2/stage3 IR is byte-identical at `a538c9fbe6762072e2d6fb131ce827e997346db7c8e4b48488877e366d0b2b84`; bootstrap proof SHA-256 is `3662d6b2ac7117fca70909aa4914837b52483a735c3bd219ae052e54403f0a12`. |
| `scripts/conformance.ps1 -Configuration Release` | Passed 90/90. Report SHA-256: `f4ae1b0ac44e907872678fbb3e5fd2d1ffc1ba39de3022f9bff1c38d2d2952f5`. |
| `scripts/performance.ps1 -Configuration Release` | Passed all 11 budgets: 0.010, 0.160, 48.709, 54.395, 0.058, 0.148, 0.079, 0.190, 0.018, 0.445, and 0.170 seconds. Report SHA-256: `e70f458116c91fb0c1439d4aadb109032573b80619a2cb0b1665dfa1019a5136`. |
| Release compatibility report | Passed 11/11 representative 1.0-2.0 cases. SHA-256: `b05b3f466e52c181f5abee09f47d9a826e93f59f309413993f5ab72a178cf851`. |
| Release application report | Passed 17-package graph, three repeated runs, two concurrent builds, four raylib tests, and ownership/concurrency execution. SHA-256: `8bd50bb047c2ad818b4bf047efe31cd1fed4421ba904b1568bea3083391503c9`. |
| `scripts/package-compiler.ps1 -Configuration Release -ReleaseChannel local` | Final 64-level-bound run passed in 608.4 seconds: 215-test rebuild, deterministic bootstrap, checksum verification, sanitized relocation, and double archive reproduction. The external 268,906,612-byte archive SHA-256 is `6896b8f6a883dc5bfa17c4f92b76fb272a40dad18b42e2db0403c92c70751220`; its 927-entry checksum-file SHA-256 is `97c16627e460fec855a1546422dc8286ca26ae50c3b0ad916c16e0de70d4e3f2`. |

The final package-run executables are stage1
`0bbf4a718be4c2dda9ecd667a227b69074d6d9d415e38df2423468e6ef379c81`,
stage2 `f69cf78728f0083785a2d0385a1e4d354f4fa20f33f1ca81909b89f4413ccf9a`,
and stage3 `d2009a5dd0e6745fff75c902c53323dc72a4fb9ff3c8bea83360fdc675bf618e`.
Executable equality is not required; canonical stage2/stage3 IR equality is the
bootstrap authority.

## Deliberate limits and honest claims

- Windows x64 is the only supported target; Phase 19 remains deferred.
- The compiler cache is conservative whole-package reuse, not per-module code
  generation. The LSP retains its independent incremental analysis graph.
- One compiler process is single-process; parallel validation uses independent
  package compiler processes.
- Windows MSVC ASan covers stage0/frontend/runtime. The pinned non-instrumented
  LLVM distribution is covered by both full LLVM matrices, not linked into the
  sanitizer binary.
- The measured artifact is local, dirty-tree, unsigned, and non-official by
  policy. The official stable signing path is implemented and rejects missing
  certificates/dirty trees, but no private certificate was available or used.
- No independent production user is claimed. External adoption and feedback
  remain ongoing post-release maturity evidence rather than a fabricated gate.
