# Rocket 2.0 Package Author Guide

Create a package with `rocketc new`, keep public source under `src/`, tests under
`tests/`, and declare an exact semantic version and license in `rocket.toml`.
Run `fmt --check`, `test`, `doc`, `resolve --locked`, and `audit` before
publishing. Commit `rocket.lock`; exclude `.rocketc` and credentials.

Prefer the smallest dependency graph. Registry dependencies use signed metadata
and semantic constraints, Git dependencies require an immutable commit, and
path dependencies are local development inputs that cannot be published.
Packages have no executable build scripts. Native inputs require an explicit
allow-list and Windows x64 manifest section.

Public packages should document exported modules, failure values, ownership,
thread-safety, resource bounds, unsafe/native requirements, and supported
Rocket versions. Treat a yanked or advisory-affected dependency as an upgrade
task, not something to bypass with a disabled check.

Rocket 2.0 package builds use a conservative content cache in `.rocketc`.
Package source, manifest, compiler/runtime identity, target/options, dependency
identity, and native configuration participate in the key. The cache is a
local acceleration and never replaces the lockfile or registry checksum.

The normative manifest, lock, registry, cache, publishing, namespace,
credential, advisory, and archive rules are in `PACKAGES.md`. Rocket 2.1
packages may use an explicit `[target.<alias>]` source overlay and
`[native.<alias>]` inputs for the four production target names in `TARGETS.md`.
Do not claim a package is portable merely because it parses: test it on every
target it declares, and document any target-specific native capability.
