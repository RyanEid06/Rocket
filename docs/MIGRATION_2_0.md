# Migrating to Rocket 2.0

Rocket 2.0 is source-compatible with valid Rocket 1.0-1.8 programs. It adds no
new grammar, ownership rule, standard-library signature, manifest field, lock
format, runtime ABI entry, or FFI type. Rebuild applications and dependencies
with `rocketc 2.0.0`; no source rewrite is expected.

Package `build` and `run` now use the conservative `rocket-build-cache-1` marker
under `.rocketc`. Do not commit it. Any compiler/runtime/source/options/native
input change invalidates the entry. Remove `.rocketc` when an external native
library changes in place without a manifest/path change.

Inputs above the documented 2.0 resource limits now fail deterministically
instead of consuming unbounded memory or time. Split a source file, package, or
dependency graph when it exceeds those limits. Automation may match `R1003` but
should not depend on diagnostic wording.

Release automation should consume `RELEASE-PROVENANCE.json` and
`SHA256SUMS.txt`, verify the detached signature for trusted releases, and treat
only stable, clean, signed `official` artifacts as official Rocket releases.
Unsigned local/nightly/preview packages are development artifacts.

Rocket 2.0 itself remains Windows x64-only. The separate additive Rocket 2.1
portability work is described by `MIGRATION_2_1.md`; do not infer non-Windows
support from this historical 2.0 contract.
