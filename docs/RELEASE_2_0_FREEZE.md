# Rocket 2.0 Windows x64 Hardening and Freeze Contract

## Status and authority

This is the authoritative Phase 20 contract for Rocket 2.0. It defines the
release boundary, the supported platform, and the evidence required before a
Rocket 2.0 release can be declared. It is a planning and hardening contract,
not a release announcement: no Phase 20 acceptance gate has run or passed by
virtue of this document.

Rocket 2.0 supports **Windows x64 only**. The supported environment is the
repository-pinned MSVC, Windows SDK, Ninja, LLVM/Clang/LLD toolchain and the
self-contained Windows x64 compiler package produced from it. Linux, macOS,
ARM64, target triples, cross-compilation, and portability abstractions are
explicitly outside Rocket 2.0. Phase 19, which owns that work, is deferred
until after Rocket 2.0; it is not completed, cancelled, or silently folded into
this phase.

The C++20 compiler remains the permanent reproducible `stage0`. The
Rocket-written compiler remains the production compiler. Both are required to
implement the frozen behavior and the `stage0 -> stage1 -> stage2 -> stage3`
bootstrap remains a release gate.

This document is the Phase 20 release boundary. The documents named below are
the normative definitions of each frozen surface. A conflict is resolved by the
more specific named normative document; any intentional Rocket 2.0 change
requires a recorded decision, specification update, migration guidance, and a
newly approved release-candidate validation plan before implementation.

## Frozen public contracts

Rocket 2.0 freezes the complete accumulated Rocket 1.0-1.8 surface on Windows
x64. "Freeze" means that valid source retains its accepted/rejected status and
observable specified behavior, public signatures retain their types and
failure behavior, stable protocol/schema versions retain their wire shape, and
ABI v1 consumers remain link-compatible. Additions, removals, reinterpretations,
or platform-dependent alternatives are not permitted during hardening unless
they meet the exception process above.

| Surface | Frozen contract and normative source |
| --- | --- |
| Grammar and lexical behavior | `SPEC.md` and the Rocket 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, and 1.8 syntax dictionaries freeze indentation, comments, tokens, literals, declarations, expressions, control flow, imports, patterns, generics, `impl`/traits, lambdas, `unsafe`, and `async`/`await`. No grammar is added for Phase 20. |
| Type system and semantics | `SPEC.md` freezes primitive widths and checked arithmetic; nominal/generic typing and deterministic specialization; modules/visibility; `Option`, `Result`, and `?`; collections and copy-on-write values; trait selection; native-type exclusions; and the type rules for weak, move-only, `Send`, `Share`, `Task`, and scoped values. |
| Ownership and concurrency | `CONCURRENCY.md`, `SPEC.md`, and `STDLIB.md` freeze ARC +0/+1 ownership, deterministic cleanup, weak upgrade/expiration, move and scope checks, atomic publication and documented memory ordering, bounded executor/task/channel/group behavior, cancellation/deadlines, and Windows async file/socket/process semantics and limits. |
| Standard library | `STDLIB.md` freezes every public `std.*` module name, type, signature, deterministic ordering rule, documented bound, blocking/cancellation behavior, and `Option`/`Result` failure boundary through Rocket 1.8. Windows-backed behavior remains Windows-backed; it does not imply a portable API promise. |
| Runtime ABI | `COMPILER_ARCHITECTURE.md` freezes runtime ABI v1, including `rocket_rt_abi_version() == 1`, opaque managed representations, fixed-width scalar mappings, managed parameter/result ownership, ARC/weak rules, runtime failure behavior, and the additive ABI-v1 entry-point discipline. Runtime internals remain private, but shipped ABI-v1 libraries must remain link-compatible. |
| C FFI and library products | `SPEC.md`, `COMPILER_ARCHITECTURE.md`, `RELEASE_1_3.md`, and `TOOLING.md` freeze the narrow Windows x64 C ABI, its fixed primitive mappings, unsafe-only imports, pointer-only native aggregates, synchronous non-storing callbacks, unmangled exports, manifest native inputs, static/DLL product behavior, and deterministic `bind`/`emit-header` output. Unsupported calling conventions, variadics, direct managed values, by-value aggregates, dynamic loading, and stored callbacks remain unsupported. |
| Packages and artifacts | `PACKAGES.md`, `TOOLING.md`, and `RELEASE_1_6.md` freeze `rocket.toml`, `rocket.lock`, source identities, SemVer resolution, lock-graph import enforcement, signed registry/HTTPS protocol, immutable Git acquisition, archive bounds, credential behavior, publishing, audit, and deny-by-default dependency code/native-input policy. |
| Diagnostics | `DIAGNOSTICS.md` freezes stable `Rdddd` identities, source-span behavior, text/JSON diagnostic shape, and `rocket-message-1` machine messages. Existing codes must not be repurposed; message wording may improve only without changing code, severity, required fields, or machine interpretation. |
| Formatter and CLI | `TOOLING.md` freezes manifest/package discovery, ignored `.rocketc` artifacts, sorted test discovery and XFAIL behavior, formatter spelling/comment rules, idempotence, `rocketc` commands/options/exit behavior, and package/library workflows. |
| Tooling protocols and observability | `LANGUAGE_SERVER.md`, `DEBUGGING.md`, `TOOLING.md`, and `REPL.md` freeze LSP Protocol 1.0, `rocket-source-map-1`, CodeView/PDB workflow, coverage/profile/benchmark schemas, compiler JSON Lines messages, and the stated non-guarantee for the incremental-AOT REPL prototype. |
| Compatibility promises | The historical `RELEASE_1_*.md` contracts, `MIGRATION_1_8.md`, and this document freeze source compatibility for valid Rocket 1.0-1.8 Windows x64 programs, package consumers, generated C interfaces, diagnostic/protocol consumers, and runtime ABI v1 clients. The historical release documents remain compatibility baselines; this contract does not rewrite their evidence or limitations. |

The distribution contract is a self-contained Windows x64 archive containing
the stage3 compiler, the separate stage0 compiler, runtime ABI v1 library,
pinned native toolchain inputs, documentation, editor tooling, and checksums.
It must discover bundled tools relative to itself and must not require an
activated developer shell or an unrelated system compiler.

## Phase 20 acceptance gates

Every gate below is required for each release candidate unless marked as a
one-time baseline gate. Results must name the commit, pinned dependency
versions, Windows version/architecture, command line, seed or fault schedule,
elapsed time, artifact hashes, and pass/fail outcome. A failure, timeout,
sanitizer finding, crash, unreproduced discrepancy, or untriaged security issue
blocks promotion. Passing one gate does not waive another.

### Fuzzing and malformed input

- Maintain deterministic, versioned fuzz targets for lexer/layout, parser/AST,
  HIR/type checking, MIR verification, LLVM/optimizer input, runtime managed
  values, package manifests/locks, registry indexes/signatures/archives, C
  header binding input, formatter input, LSP/JSON-RPC, and compiler JSON Lines.
- Before the first RC, seed each target with every positive/negative regression
  fixture in its domain and retain every minimized failure as a deterministic
  regression. Each target must execute at least 24 CPU-hours on Windows x64 per
  RC, with a minimum of 10 million generated inputs or the target's documented
  saturation threshold, whichever is greater.
- Run a seven-consecutive-day continuous fuzz campaign against the candidate
  source before final promotion. There must be no compiler crash, sanitizer
  failure, hang beyond the target timeout, nondeterministic result, or accepted
  malformed input that violates its contract. Every discovery must have a
  minimized reproducer, owner, disposition, and regression before promotion.

### Sanitizers, runtime checking, and fault injection

- Run the complete LLVM-enabled Debug/Release and LLVM-disabled stage0
  Debug/Release matrices under the strongest supported Windows x64 memory and
  undefined-behavior instrumentation for the compiler, runtime, package host,
  and raylib adapter. A clean run has zero sanitizer reports and zero suppressed
  findings; any justified exclusion must be documented and approved before RC
  testing begins.
- Exercise a deterministic fault-injection matrix covering allocation,
  filesystem, process creation, registry/HTTPS/Git acquisition, archive/cache
  transaction, crypto/signature, socket, timer, async-I/O, executor queue, and
  native-link failure boundaries. Every declared injection point must be hit at
  least once for both stage0 and stage3, must produce the specified cleanup and
  diagnostic/result behavior, and must not leak, corrupt the cache, or hang.
- Repeat lifecycle and contention stress with leak/destructor accounting. The
  expected count is exactly zero outstanding managed allocations and exactly
  the specified destruction count after every successful and injected-failure
  scenario.

### Compatibility and contract conformance

- Maintain a versioned compatibility corpus containing all Rocket 1.0-1.8
  positive programs, negative diagnostics, formatting fixtures, package/lock
  fixtures, FFI/header consumers, LSP transcripts, source-map/debug fixtures,
  and machine-message fixtures. Stage0 and stage3 must agree on every expected
  acceptance result, output, stable diagnostic code, and protocol/schema field.
- The complete corpus must pass on every RC in Debug and Release. Formatter
  output must be byte-identical after a second run; generated headers, bindings,
  lockfiles, documentation output, and schema fixtures must be byte-deterministic.
- Compile and run representative released package applications in clean online,
  locked, and relocated locked-offline modes. Rebuild ABI-v1 native consumers
  against the candidate runtime and prove they link and run without source
  changes. No compatibility exception is implied by an undocumented behavior.

### Performance and scalability

- Establish the one-time Windows x64 reference-machine baseline from five
  isolated Release runs of the existing check/build, compiler HIR/MIR, native,
  raylib, concurrency, async, package-resolution, formatter, and LSP workloads.
  Record the median, slowest run, input revision, machine inventory, power plan,
  toolchain, and measurement script in the repository before RC performance
  comparison begins.
- For each RC, repeat each workload five times on that reference machine. Its
  median may not regress by more than 10 percent and its slowest run may not
  regress by more than 20 percent from the recorded baseline; the pre-existing
  release-budget ceilings also remain mandatory. Any variance exclusion requires
  a documented, reproducible environmental cause and owner approval.
- Demonstrate a clean build, incremental rebuild, parallel build, and locked
  multi-package build using the same versioned workloads. Cache keys and outputs
  must be deterministic and stale/mismatched cache entries must be rejected.

### Reproducibility, packaging, and signing

- On two independently provisioned clean Windows x64 environments, build the
  candidate from the same commit twice in separate paths. Dependency
  verification, stage0-through-stage3 bootstrap, canonical stage2/stage3 LLVM
  IR, generated interfaces, lock-derived source selection, and the unsigned
  distribution payload/checksum manifest must be byte-identical. Any artifact
  that necessarily embeds a signing timestamp must be separated from the
  reproducible payload and identified in the provenance record.
- Perform clean-machine and sanitized relocation tests with developer-shell and
  ambient toolchain environment variables removed. The package must compile,
  run, test, format, use locked-offline dependencies, exercise FFI and the
  shipped Windows async surface, and use only bundled tools.
- Produce SHA-256 checksums for every package entry and a provenance statement
  binding source commit, toolchain digests, build commands, test reports, and
  unsigned payload hash. Authenticode-sign Windows executables and libraries
  with the approved release identity, sign the checksum/provenance manifest,
  verify both signatures and certificate policy in a clean environment, and
  retain verification output with the RC evidence.

### Documentation, security process, and release candidates

- Every frozen public item must have one normative reference, an accurate
  compatibility statement, and a runnable or automatically checked example
  where code is shown. Link checking, terminology/status checking, syntax-dictionary
  consistency, command validation, and documentation build checks must pass
  with no stale claim that Phase 19 is complete, multi-platform support exists,
  or Phase 20 has passed before evidence does so.
- Publish and rehearse a vulnerability intake, severity/triage, embargo,
  advisory, supported-version, patch, and disclosure process. Run one tabletop
  incident and one signed advisory rehearsal before RC1; record only the
  process evidence, never fabricated vulnerability claims.
- Produce at least two release candidates from distinct commits. Each must pass
  all gates above; RC2 must incorporate and revalidate all RC1 findings. Keep
  RC1 available for a minimum of 14 calendar days before RC2 promotion, with no
  unresolved release-blocking issue. Final 2.0 promotion additionally requires
  the independently maintained application evidence required by `ROADMAP.md`.

## Evidence and release disposition

Phase 20 is complete only when a final release report maps every gate to
immutable evidence and states the exact package version, hashes, toolchain,
commands, counts, timings, accepted compatibility corpus revision, signed
artifacts, remaining deliberate limitations, and external-application evidence.
Until then, Rocket 1.8 is the latest completed release and this document is a
contract for future validation, not proof that any validation has passed.
