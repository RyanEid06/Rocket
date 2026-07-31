# Rocket 1.6 Release Contract

Rocket 1.6 completes the dependency-management and package-ecosystem phase for
Windows x64. Reusable Rocket packages can be resolved, imported, audited,
documented, published through the signed file reference registry, or exchanged
with a service implementing the versioned authenticated HTTPS protocol.

The reproducibility unit is the committed lockfile plus SHA-256 source cache.
Normal compiler commands reject stale, missing, poisoned, or graph-bypassing
dependencies. The pinned LLVM/LLD path uses deterministic PE linking, and the
Phase 16 executable fixture proves the clean online and relocated locked-offline
artifacts are byte-identical.

Security properties include ECDSA P-256 signed registry configuration,
indexes, ownership histories, yanks, and advisories; pinned signing-key
fingerprints; bounded same-origin HTTPS; immutable direct-argument Git
acquisition; strict regular-file ustar archives; transactional recovery;
Windows Credential Manager tokens; immutable versions; scoped/revocable
credentials; namespace, reserved-name, case, and typosquatting policy; SPDX and
advisory audit failures; and an explicit deny-by-default native/build-script
capability model. Resolve, audit, documentation, and publish never execute
dependency code.

The production Rocket compiler exposes the same package commands and consumes
the same exact graph. Registry, credential, signing, HTTPS, Git, and archive
operations use the preserved, colocated C++ Stage 0 package security host. The
frontend, import enforcement, semantic pipeline, MIR, and code generation
remain Rocket-written, and bootstrap still proves equal stage2/stage3 LLVM IR.

Rocket 1.6 does not claim a public hosted registry, non-Windows credential
backend, dependency build scripts, implicit native-code approval, source-level
package discovery, or Phase 17 language-server completion. The file registry
is the executable reference deployment, and `docs/PACKAGES.md` is the protocol
contract for HTTPS services.

Release acceptance requires dependency verification, Debug and Release builds,
LLVM and LLVM-disabled Stage 0 matrices, deterministic bootstrap, conformance,
performance gates, distribution relocation/package checks, the full CTest
suite, and the adversarial Phase 16 package tests.
