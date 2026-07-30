# Rocket Package Resolution 1.6 Foundation

This document specifies the first Phase 16 package-management milestone. It is
additive to the Rocket 1.0 package layout and does not change source-language
semantics or runtime ABI v1.

## Manifest contract

Every package version is an exact Semantic Versioning 2.0.0 version. Leading
`v`, missing components, and numeric leading zeroes are rejected. A package may
declare SPDX-style license text and its default registry:

```toml
[package]
name = "application"
version = "1.6.0"
license = "MIT"
entry = "src/main.rocket"
registry = "../registry"

[dependencies]
math = "^1.2.0"
local_text = "path:../local_text"
reviewed_git = "git:../reviewed_git#0123456789abcdef0123456789abcdef01234567"
```

Registry requirements are exact versions or one of `=`, `>`, `>=`, `<`, `<=`,
`^`, and `~` followed by a complete semantic version. Resolution chooses the
highest matching stable version. Pre-release versions participate only when the
requirement itself names a pre-release.

Path locations must be relative and are resolved from the manifest that declares
them. Git
dependencies must name a 40- or 64-hex-digit immutable revision. The foundation
accepts a reviewed local Git export containing a `.rocket-revision` file equal
to that pin. Remote Git and HTTPS registry transport will be added only with the
authenticated download and archive-validation layer; they are never silently
delegated to a shell.

The current registry transport is a local directory or `file://` location with
this immutable layout:

```text
registry/
  package_name/
    1.2.0/
      rocket.toml
      src/
```

The version directory and manifest version must agree. A single dependency
graph cannot contain two versions or two different checksums for one package
name; that situation is a deterministic conflict rather than order-dependent
selection.

## Locking and caching

`rocketc resolve` writes `rocket.lock`. The lockfile is generated in lexical
package order, records every exact version, source identity, license, transitive
edge, and `sha256:` source checksum, and must be committed. Repeating resolution
without an input change produces identical bytes.

Sources are copied into `.rocketc/cache/sha256/<digest>`. The digest covers each
non-generated package file in lexical relative-path order and its exact bytes.
Symbolic links are rejected, generated/build/VCS directories are excluded, a
temporary cache transaction is verified before rename, and an existing cache
entry is rehashed before use. Cache corruption is an error; Rocket never repairs
or replaces suspicious content implicitly.

Commands are:

```powershell
rocketc resolve .
rocketc resolve . --locked
rocketc resolve . --offline
rocketc tree .
rocketc audit .
```

`--locked` resolves current manifests and fails if the committed lock differs.
`--offline` performs no source selection and requires every locked checksum to
exist and verify in the local cache. `tree` renders locked direct and transitive
edges. `audit` verifies lock structure, unique versions, dependency edges,
SHA-256 cache contents, and license metadata for registry packages.

The resolver commands are implemented first in the preserved C++ stage0 tool.
Wiring cached packages into production imports, matching Rocket-written CLI
commands, authenticated network transport, signed registry metadata, advisory
feeds, publishing, and documentation generation remain subsequent Phase 16
milestones.

## Registry governance contract

The future public registry must enforce these rules before it is accepted:

- published `(namespace, name, version)` records and source archives are
  immutable; yanking only changes discovery metadata;
- namespaces have explicit verified owners, transfer history, reserved-name
  policy, and anti-typosquatting review;
- publishing uses scoped, revocable authentication and records an auditable
  owner identity without putting credentials in manifests or lockfiles;
- checksums and signed index snapshots are verified before extraction, and
  archive paths, links, duplicate names, case collisions, and size limits are
  validated before cache mutation;
- security reports have a private intake, response deadlines, advisory IDs,
  affected semantic-version ranges, and a documented emergency-yank process;
- build scripts and native dependencies never receive unrestricted implicit code
  execution. Any future build capability must declare reviewed inputs, outputs,
  target, environment, and sandbox policy.

No public Rocket registry is claimed by this foundation milestone.
