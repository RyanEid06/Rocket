# Rocket Package and Registry Contract 2.0

Rocket 1.6 makes third-party source selection reproducible and treats registry,
archive, credential, advisory, and native-input handling as security boundaries.
This contract is additive to the Rocket 1.0 package layout and does not change
runtime ABI v1. Rocket 2.0 freezes this package/registry contract and adds only
bounded input processing and the local artifact cache described below.

`rocket.toml` is limited to 1 MiB, 64 KiB per line, 4,096 entries, and 1,024
dependencies. Rocket source discovery is limited to 4,096 files and 64 MiB.
Package `build`/`run` may reuse `rocket-build-cache-1` only after hashing the
compiler, runtime, complete non-generated package tree, target/options, output,
dependency identities, and native inputs. This acceleration is local and never
weakens lock, registry, archive, or cache-integrity verification.

## Package identities and manifests

Every published identity is `(namespace, name, version)`. `namespace` and
`name` use ASCII letters, digits, `_`, and `-`, begin with a letter or `_`, and
are compared case-insensitively for registry uniqueness. Source-level imports
continue to use a dependency's manifest key, so a package can remain
beginner-friendly while ownership is explicit:

```toml
[package]
namespace = "example"
name = "application"
version = "1.6.0"
license = "MIT"
entry = "src/main.rocket"
registry = "https://packages.example.test"
registry-key = "sha256:0123456789abcdef..."

[dependencies]
math = "^1.2.0"
local_text = "path:../local_text"
reviewed_git = "git:https://example.test/reviewed.git#0123456789abcdef0123456789abcdef01234567"

[package-policy]
allowed-licenses = "Apache-2.0;MIT"
allow-native = "example/reviewed_native@1.0.0"
build-scripts = "deny"
```

Versions are exact Semantic Versioning 2.0.0 values. Registry requirements are
exact versions or one of `=`, `>`, `>=`, `<`, `<=`, `^`, and `~` followed by a
complete version. Resolution chooses the highest matching non-yanked stable
version. A pre-release participates only when the requirement names one.

Path dependencies must be relative to the declaring manifest. They are
development inputs and cannot be published. Git dependencies require an
absolute `https://` URL and a 40- or 64-digit immutable commit identifier.
Legacy reviewed local Git exports remain accepted only through a `path:`
dependency; `.rocket-revision` files are not a substitute for remote object
verification.

SPDX license expressions use known SPDX identifiers joined by uppercase `AND`,
`OR`, and `WITH`. Invalid identifiers or expressions are manifest errors.
Registry packages require a license. A root package may restrict the accepted
licenses with `allowed-licenses`; the policy is applied transitively.

## Locked compilation and imports

`rocket.lock` lock format 2 records the root identity, every exact namespace,
name, version, source identity, source SHA-256, registry signing-key
fingerprint, publisher identity, license, and sorted dependency edge. The file
is generated in lexical identity order and contains no token, password, private
key, machine path, timestamp, or mutable registry URL query.

When a package has dependencies, `check`, `build`, `run`, `test`, `emit-ir`,
`emit-asm`, `emit-header`, and `doc` require a current lockfile. Before parsing
source, the compiler verifies every selected cache tree against its lock
checksum. An import whose first component is a dependency key always resolves
inside that dependency's exact cached root; a same-named local module cannot
shadow or bypass the selected graph. Transitive imports are checked against the
declaring package's locked dependency edges. Imports outside the selected graph
produce `R3005`.

The cache is `<root>/.rocketc/cache/sha256/<digest>`. Hashing covers sorted
relative paths and exact bytes while excluding generated/build/VCS directories.
Links, reparse points, unsafe names, duplicate or case-colliding paths, and files
that change during hashing are rejected. Downloads and extraction use bounded
same-volume temporary transactions. A verified rename is the only cache commit;
stale interrupted transactions are removed safely on retry. Existing cache
content is always rehashed and suspicious content is never repaired implicitly.

`--offline` forbids every network and source lookup. Locked online commands may
refill a missing cache entry only from the exact locked source and must reproduce
its checksum. Clean online and relocated locked-offline builds therefore select
identical source bytes. The pinned compiler/linker must additionally produce
byte-identical artifacts for the Phase 16 acceptance fixtures.

## Signed registry protocol

The repository contains a file-backed reference registry used by conformance
tests and supports the same protocol over authenticated HTTPS. A registry has a
stable ID and an ECDSA P-256 signing key. Clients pin the SHA-256 fingerprint of
the exported public key in `registry-key`; HTTPS alone never substitutes for
metadata authentication.

Canonical version indexes, namespace ownership records, transfer histories,
yank state, publisher provenance, and advisory snapshots are UTF-8 text signed
over their exact bytes. Signatures use SHA-256 plus ECDSA P-256 and are verified
before any archive is trusted. Package archives are deterministic ustar files
containing regular files only. Limits are 64 MiB downloaded/archive content,
1,024 entries, 100 UTF-8 bytes per path, and 30 seconds per connect/send/receive
operation. Extraction rejects absolute/traversing/backslash/drive/device paths,
links, non-regular entries, duplicate names, case collisions, bad checksums,
trailing data, and expansion beyond the documented limit.

HTTPS requests use the Windows certificate policy with no certificate-disable
option. At most three redirects are followed; every redirect must remain HTTPS,
must preserve the pinned registry origin, and never forwards credentials to a
different authority. Responses are bounded before allocation. Temporary
download names and errors never contain bearer tokens.

The reference paths are versioned:

```text
GET  /v1/registry.toml
GET  /v1/index/<name>.toml
GET  /v1/index/<name>.sig
GET  /v1/packages/<namespace>/<name>/<version>.tar
GET  /v1/advisories.toml
GET  /v1/advisories.sig
POST /v1/auth/verify
POST /v1/packages
```

The publish request body is a strict
`application/vnd.rocket.publish.v1+tar` envelope containing `package.tar`,
`docs/index.html`, and `docs/search.json`. Identity, SPDX license, source and
archive SHA-256 values are repeated in bounded `X-Rocket-*` headers. The
`Idempotency-Key` is `sha256:<archive checksum>`. Publish never follows a
redirect, so its bearer credential cannot leave the pinned origin.

Public reads may omit authentication. Private reads and every write use
`Authorization: Bearer`; non-HTTPS network registries are rejected.

## Immutable Git acquisition

Remote Git acquisition never constructs a shell command. Rocket starts the
reviewed `git.exe` directly with a separated argument vector, disabled hooks,
disabled credential helpers, disabled `ext` and `file` protocols, no tags, and
an isolated temporary repository. It fetches the requested object, peels it to
a commit, and requires the resulting object ID to equal the manifest pin.
Submodules and symbolic links are rejected. Rocket exports the verified tree to
a bounded archive, validates it with the same package-archive rules, hashes the
resulting source tree, and commits only that tree to the content-addressed
cache. Locked offline use never depends on a Git checkout or installed Git.

## Authentication and publishing

`rocketc login <registry> --token-stdin` reads one token without echoing it,
verifies its registry ID and status, and stores it as a generic secret in
Windows Credential Manager. `logout` deletes it. Credentials are never accepted
in a manifest, URL, lockfile, ordinary command argument, diagnostic, report, or
log. Credentials are scoped (`publish`, `yank`, `owner`, or `security`),
revocable, and bound to an auditable owner identity.

`rocketc publish <package>` performs all checks before network or registry
mutation: package/version/SPDX validation, a clean source inventory, no path or
Git dependencies, current dependency lock, permitted native policy,
deterministic archive and documentation generation, safe archive re-read,
checksum, ownership, namespace, credential scope, yank/advisory, and immutable
version checks. Publishing the same checksum is an idempotent retry; publishing
different bytes for an existing identity is refused. File-registry commits use
transactional renames, and HTTPS retries use an idempotency key derived from the
archive checksum.

Namespaces have verified owners and append-only transfer history. Registry
administration enforces reserved names and rejects case variants or edit-
distance-one names owned by another namespace unless a recorded review allows
them. Ownership transfer requires the current owner's `owner` scope and names
the accepting owner. Published archives and version records are immutable.
Yanking only changes signed discovery metadata; it never deletes source.
Emergency compromise records are signed advisories and may yank affected
versions without altering their archived evidence.

The file-backed reference registry exposes deterministic administration for
tests and private deployments:

```powershell
rocketc registry init <directory> --id <registry-id> --owner <identity> --token-stdin
rocketc registry transfer <directory> <namespace> <new-owner>
rocketc registry yank <directory> <namespace>/<name>@<version> --reason <text>
rocketc registry advisory <directory> <advisory-file>
rocketc registry revoke <directory> <credential-id>
```

These commands apply the same credentials, signatures, immutability, audit log,
and namespace rules as HTTPS service implementations.

## Documentation and auditing

`rocketc doc <package> [--output <directory>]` emits deterministic UTF-8 HTML
and JSON search metadata for public functions, types, traits, constants, and
modules. Rocket 1.7 builds this index through the real lexer/parser instead of
regular-expression declaration guessing. Output includes package
namespace/name/version, stable per-kind symbol anchors, source links and lines,
cross-links for related types, searchable declarations/documentation, and
fenced examples as text. Malformed or incomplete source fails with the original
stable Rocket diagnostic code and location. Documentation never compiles or
executes an example. `publish` includes the same deterministic bytes under the
immutable version's documentation record.

`rocketc tree` shows exact identities, sources, and provenance. `rocketc audit`
verifies the lock graph, cache trees, signatures, SPDX expressions, root license
policy, namespace ownership, publisher provenance, yank state, and signed
advisories. Advisory ranges use the same deterministic SemVer evaluator as the
resolver. A compromised match is an error; a yank is an error when
`deny-yanked = "true"` and otherwise a warning. CI receives nonzero status for
integrity, policy, yank-policy, or advisory failures.

## Native dependencies and build scripts

Dependency source is inert during resolve, audit, documentation, and publish.
Rocket 1.6 introduces no dependency build-script execution. The only accepted
`build-scripts` policy is `deny`; any dependency declaring a build hook is an
error. Dependency `[native.windows-x64]` inputs are also inert and rejected
unless the root's exact `allow-native` list names the locked
`namespace/name@version`. Even when allowed, only declared library/header files
inside the verified source tree are made available to the ordinary Phase 13
link pipeline; no command, environment mutation, network access, or generated
output is implied.

A future build capability requires a new decision defining reviewed inputs,
outputs, target, environment, network access, cache key, sandbox, resource
limits, and explicit user approval. It cannot be introduced by manifest data
alone.

## Commands

```powershell
rocketc resolve <package> [--locked|--offline]
rocketc tree <package>
rocketc audit <package>
rocketc doc <package> [--output <directory>]
rocketc login <registry> --token-stdin
rocketc logout <registry>
rocketc publish <package>
```

No public Rocket registry is claimed by the repository. The file-backed
reference registry and HTTPS protocol are the executable governance and
security contract that a public service must satisfy.
