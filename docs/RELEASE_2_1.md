# Rocket 2.1 portability release contract

Rocket 2.1 is the additive Phase 19 portability release. It preserves valid
Rocket 2.0 source, the permanent C++20 `stage0`, runtime ABI v1, and the frozen
Windows x64 SDK used by Scroll2Roll. Phase 19 outputs must be isolated under
`out/phase19`; they may never rebuild, replace, or package over the frozen 2.0
SDK directories.

## Targets and packages

The only prospective production targets are `windows-x64`, `linux-x64`,
`linux-arm64`, and `macos-arm64`, with the exact triples and cross-compilation
policy in `TARGETS.md`. Windows ARM64 is an evaluated non-production target.
Each native package bundles its matching compiler, stage0, runtime, standard
library, linker/toolchain components, target metadata, licenses, and a checksum
manifest so an ordinary native compile does not depend on an unrelated system
compiler after installation.

Packages are checksummed and tested after relocation to a sanitized path. Apple
packages are linked and packaged on a macOS host because the Apple SDK and
linker are host-bound. Official signing material and independent external
adoption are not repository-implementable acceptance requirements and must not
be fabricated.

## Release gate

For every target, the release gate requires clean Debug and Release builds; the
complete compiler, conformance, standard-library, package, FFI, compatibility,
concurrency/async, debugger, and substantial-application suites; stage0 through
stage3 bootstrap determinism; package checksums; and sanitized relocation.
Actual results, versions, hashes, commands, timings, and limitations belong in
`PHASE_19_AUDIT.md`. A CI definition, emulation, or cross-build is not native
acceptance evidence.
