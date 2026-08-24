# Rocket 2.0 Release Contract

Rocket 2.0 is the trust and stability release for the completed Windows x64
language. It adds no source-language feature and did not perform the Phase 19
platform expansion. Valid Rocket 1.0-1.8 source remains
compatible, the C++20 compiler remains permanent stage0, and runtime ABI v1 is
the frozen 2.0 ABI.

## Frozen surface

The cumulative contracts in `SPEC.md`, `STDLIB.md`, `PACKAGES.md`,
`TOOLING.md`, `LANGUAGE_SERVER.md`, `CONCURRENCY.md`, and `FFI_GUIDE.md` define
the Rocket 2.0 grammar, type and ownership systems, standard library, manifest
and lock formats, machine protocols, runtime ABI v1, and Windows x64 C FFI.
Compatible 2.x patches may clarify diagnostics, repair defects, improve
performance, and add libraries or tooling without invalidating that surface.
Incompatible changes require a new major-version decision and migration plan.

## Compatible post-release tooling

`Rocket.Language.VisualStudio` 2.0.3 is a compatible post-freeze tooling patch.
It connects Visual Studio Community 2026 to the already frozen compiler CLI,
`rocket-message-1`, `rocket-lsp` Protocol 1.0, CodeView/PDB, and
`rocket-source-map-1` contracts. It adds GUI Build/Run/Test/Stop/Debug,
hidden redirected execution, Rocket Output, navigable Error List diagnostics,
semantic editor features, and native debugging without changing Rocket source,
runtime ABI v1, package formats, stage0, or the production compiler.

The extension remains Windows x64-specific, matching the supported Rocket 2.0
target. Its no-terminal Run/Debug workflow does not provide interactive console
stdin; application arguments, captured stdout/stderr, files, GUI input, and
networking remain supported. Exact GUI and package evidence is recorded in
`PROJECT_CONTEXT.md`.

Phase 19 later resumed as the additive Rocket 2.1 portability release. Its
targets, packaging rules, and acceptance state are defined separately in
`TARGETS.md`, `RELEASE_2_1.md`, and `PHASE_19_AUDIT.md`; that work does not
alter this historical Rocket 2.0 release contract.

## Hardening and scale contract

- A source file or editor overlay is at most 4 MiB; one module graph is at most
  4,096 files, 64 MiB of Rocket source, and 64 nested import levels.
- `rocket.toml` is at most 1 MiB, one line is at most 64 KiB, one manifest has
  at most 4,096 entries and 1,024 dependencies, and package source discovery is
  bounded to 4,096 files and 64 MiB.
- Resource refusal is deterministic `R1003` or the existing package/tooling
  diagnostic category. Malformed input must terminate without a crash or hang.
- `rocket-build-cache-1` reuses package artifacts only when the compiler,
  runtime, target, optimization mode, output contract, dependency identities,
  native inputs, and sorted package bytes agree. Standalone-file commands do
  not use this cache. Independent package builds may run concurrently; one
  compiler invocation remains deterministic and single-process.
- The hardening gate includes deterministic lexer/parser/MIR and manifest
  generation, malformed and exhaustion cases, parser recovery regressions,
  crash minimization, AddressSanitizer over the LLVM-disabled stage0/frontend/
  runtime (the pinned prebuilt LLVM libraries are validated separately),
  compatibility, cache invalidation, and
  multi-package application tests.

## Release channels and artifacts

`local`, `nightly`, and `preview` artifacts may be unsigned and must still carry
provenance and checksums. An `-Official` release must use the `stable` channel,
a clean Git tree, and `-SigningCertificateThumbprint`. The package workflow
signs the compiler, language server, and stage0 host, emits signed checksums and
provenance, creates the archive twice with fixed entry timestamps, rejects
unequal hashes, and performs sanitized relocation.

```powershell
# Local validation artifact
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package-compiler.ps1 `
  -Configuration Release -ReleaseChannel local

# Official stable artifact
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package-compiler.ps1 `
  -Configuration Release -ReleaseChannel stable -Official `
  -SigningCertificateThumbprint <thumbprint>
```

Private signing keys and certificates never enter the repository or package.

## Acceptance

The Windows x64 technical release gate requires dependency verification; LLVM
and LLVM-disabled stage0 Debug/Release matrices; sanitizer and Phase 20
hardening; deterministic bootstrap; conformance; performance; compatibility;
large/parallel application validation; and a reproducible relocated package.
Observed evidence belongs in `PHASE_20_AUDIT.md` and `PROJECT_CONTEXT.md`.

External adoption cannot be manufactured by a repository test. Owner-directed
Phase 20 completion accepts the technical 2.0 release while keeping independent
production usage and feedback as an ongoing maturity signal and maintenance
input, not as fabricated validation evidence.
