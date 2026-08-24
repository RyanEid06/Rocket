# Rocket Master Roadmap

**Current status (updated 2026-08-24):** Rocket 2.0 remains frozen on Windows
x64, including the compatible Visual Studio Community 2026 integration. By
owner direction, Phase 19 is active as the additive Rocket 2.1 portability
release. Its roadmap closes only after Windows x64, Linux x64, Linux ARM64,
and macOS ARM64 have direct native-host acceptance evidence. The casino remains
a separate future project and is not part of Rocket Phase 19.

## Long-term objective

Rocket will grow from its completed, self-hosted 1.0 foundation into a credible,
well-respected general-purpose native language. The target is not to copy every
feature or match the decades-old package ecosystems of C++, Python, Rust, Java,
or C#. The target is a coherent language that developers can trust for command-
line tools, automation, desktop applications, games, services, libraries, and
Rocket's own development tools without routinely falling back to another
language.

A respected Rocket release must be:

- correct, memory-safe by default, deterministic, and explicit about unsafe code;
- pleasant for both small programs and large multi-package projects;
- interoperable with existing native libraries;
- supported by practical libraries, packages, documentation, and editor tools;
- reproducible on every supported platform; and
- proven by substantial applications and sustained real-world use.

The C++20 compiler remains the permanent reproducible `stage0`. Every language
feature must also be implemented in the Rocket-written production compiler and
must preserve deterministic `stage0 -> stage1 -> stage2 -> stage3` bootstrap
validation.

## Release principles

1. Complete one phase at a time. Do not begin a later phase while an earlier
   phase has unfinished correctness, specification, bootstrap, or documentation
   work.
2. Prefer a small coherent feature set over compatibility-driven feature
   accumulation. Rocket does not need every construct found in another language.
3. Keep source compatibility within a stable release line. Incompatible syntax,
   semantics, or ABI changes are reserved for a major release.
4. Every feature requires specification, stage0 and self-hosted compiler work,
   HIR/MIR/backend/runtime work as applicable, positive and negative tests,
   documentation, and deterministic bootstrap validation.
5. Treat libraries, packages, editor support, diagnostics, debugging, security,
   and release engineering as part of the language product rather than optional
   extras.
6. Do not begin the casino product during this roadmap. Raylib is used as a real
   interoperability and graphics validation project in Phase 14. Rocket 2.0 is
   now accepted and frozen; a casino remains a separate project that begins
   only after the owner's final post-Rocket step and explicit direction.

## Completed foundation

### Phase 1 - Repository and build foundation - Completed

- Stabilized the repository and Windows x64 build.
- Pinned MSVC, Ninja, CMake, and LLVM dependencies.
- Established specifications, tests, documentation, and reproducible commands.

### Phase 2 - Core scalar language - Completed

- Added bindings, assignment, functions, control flow, loops, ranges, primitive
  values, operators, type checking, and stable diagnostics.

### Phase 3 - Compiler architecture - Completed

- Introduced resolved HIR, typed MIR, deterministic symbols, verified control
  flow, and a backend-independent compiler pipeline.

### Phase 4 - Native LLVM backend - Completed

- Added verified LLVM IR, optimization, object/assembly generation, native
  linking, and the permanent LLVM-disabled C++ stage0 fallback.

### Phase 5 - Runtime and foundational collections - Completed

- Added runtime ABI v1, ARC ownership, UTF-8 strings, arrays, slices, checked
  indexing, checked arithmetic, and lifetime tests.

### Phase 6 - Structural types and modules - Completed

- Added structs, enums, generics, exhaustive pattern matching, `Option`,
  `Result`, `?`, imports, packages, and public visibility.

### Phase 7 - Practical standard library - Completed

- Added string and collection helpers, files, paths, JSON, CSV, deterministic
  randomness, processes, environment access, and time APIs.

### Phase 8 - Developer tooling and packages - Completed

- Added package manifests, formatting, native tests, coded diagnostics, VS Code
  syntax/task support, and relocatable compiler packaging.

### Phase 9 - Self-hosted compiler - Completed

- Rewrote the production compiler in Rocket and proved deterministic stage1,
  stage2, and stage3 bootstrap behavior while preserving C++ stage0.

### Phase 10 - Rocket 1.0 - Completed

- Froze the Rocket 1.0 language, runtime ABI, CLI, formatter, diagnostics, and
  package contracts.
- Passed Debug, Release, stage0, bootstrap, conformance, performance, packaging,
  checksum, and relocation gates.

## Post-1.0 language roadmap

### Phase 11 - Rocket 1.1: practical collections and controlled mutation - Completed

**Status:** Completed. Stage0, the self-hosted compiler, MIR, both backends, and
runtime ABI v1 implement the complete additive collection surface. Debug,
Release, deterministic bootstrap, conformance, and performance gates pass.

**Purpose:** Remove the largest day-to-day limitation in Rocket 1.0 and make
stateful application code natural without abandoning memory safety.

**Language and runtime work:**

- Define explicit mutability and aliasing rules for aggregate fields and
  collection elements.
- Add mutable array operations: element assignment, `append`, `pop`, `insert`,
  `remove`, `clear`, reserve/capacity, and safe growth.
- Add generic `Map[K, V]` and `Set[T]` with deterministic iteration behavior.
- Define equality and hashing contracts for all eligible key types.
- Add tuples for lightweight heterogeneous values and multiple-result patterns.
- Add deterministic collection iteration plus searching, sorting, equality
  filtering, stable-hash mapping, and numeric folding. General callback-based
  map/filter/fold operations follow Phase 12 function values and closures.
- Add queues, stacks, and dynamically growing byte buffers as library types.
- Preserve ARC correctness during mutation, reallocation, aliasing, and nested
  managed-value destruction.

**Acceptance gate:**

- Stage0 and the self-hosted compiler implement identical semantics.
- Mutation aliasing, bounds, hashing, iteration order, and managed lifetime tests
  pass under stress.
- The Rocket compiler adopts the new collection APIs in at least one production
  subsystem without bootstrap nondeterminism.
- Rocket 1.0 source remains compatible and the syntax dictionary is updated.

### Phase 12 - Rocket 1.2: scalable abstractions and functional values - Completed

**Delivered:** Static impls and traits, deterministic generic constraints,
monomorphized closure values, persistent user-defined iterators, associated
constants, positional fixed-arity parameter policy, a 4,096-specialization
limit, cross-module visibility, and matching stage0/self-hosted behavior.

**Purpose:** Make large Rocket programs modular and reusable without introducing
class inheritance or hidden control flow.

**Language and runtime work:**

- Add methods and associated functions for structs, enums, and library types.
- Design traits/interfaces based on explicit composition rather than inheritance.
- Add generic constraints and deterministic trait implementation selection.
- Add first-class function values, lambdas, capturing closures, and clear
  closure-lifetime rules.
- Add iterators and `for` integration for user-defined iterable types.
- Add associated constants and improved module-level public API organization.
- Decide and document a coherent policy for default, named, and variadic
  parameters; add only the forms that remain unambiguous.
- Improve generic diagnostics and prevent accidental code-size explosions from
  monomorphization.

**Acceptance gate:**

- Trait selection and generic specialization are deterministic and ambiguity is
  diagnosed at compile time.
- Closure capture, escape, ARC cleanup, and iterator invalidation tests pass.
- A multi-module application demonstrates public traits, generic constraints,
  callbacks, and iterators without privileged compiler behavior.

### Phase 13 - Rocket 1.3: native interoperability and library production - Completed

**Delivered:** Explicit unsafe blocks; primitive C imports/exports; pointers,
opaque handles, layout declarations, and synchronous callbacks; target-aware
Windows x64 native inputs; static and dynamic library products; importable,
deterministic Rocket bindings and C headers; and matching stage0/self-hosted
behavior with native consumers and bootstrap coverage.

**Purpose:** Let Rocket use the existing native software ecosystem and let other
languages consume Rocket libraries.

**Language and tooling work:**

- Specify an explicit `unsafe` boundary and a stable C-compatible FFI surface.
- Import C functions, constants, primitive layouts, pointers, opaque handles,
  callbacks, and supported structures.
- Export Rocket functions and build static and dynamic libraries.
- Define ownership, lifetime, string, error, and callback rules across the ABI.
- Add target-aware native linker inputs and library search configuration to
  `rocket.toml`.
- Build a deterministic C-header binding generator for the supported ABI subset.
- Add safe-wrapper conventions so ordinary application code does not manipulate
  raw pointers or unchecked foreign state.

**Acceptance gate:**

- Bidirectional C/Rocket calls pass ABI, callback, failure, and lifetime tests.
- A generated binding package and a handwritten safe wrapper produce identical
  behavior.
- FFI misuse is confined to explicit unsafe code and cannot silently violate
  managed Rocket invariants.

### Phase 14 - Rocket 1.4: graphics, audio, and real application validation - Completed

**Purpose:** Prove the FFI, runtime, package layout, and developer experience in
a substantial event-driven native application.

**Platform work:**

- Generate low-level raylib bindings through the Phase 13 FFI pipeline.
- Build safe Rocket APIs for windows, drawing, textures, fonts, input, audio,
  timing, assets, and resource cleanup.
- Define deterministic resource ownership and shutdown behavior.
- Add application templates, asset packaging, Debug/Release workflows, and
  distributable Windows bundles.
- Build a non-casino reference application large enough to exercise state,
  collections, callbacks, file loading, graphics, audio, and tests.

**Acceptance gate:**

- The reference application contains no handwritten C++ application logic.
- Resource leak, repeated startup/shutdown, missing-asset, and input/audio stress
  tests pass.
- A clean Windows machine can build and run the packaged application from the
  published instructions.

### Phase 15 - Rocket 1.5: production standard library - Completed

**Delivered:** Buffered binary streams and endian codecs; explicit Unicode
scalar/normalization/grapheme layers; a bounded Thompson-NFA regex engine;
Windows-backed cryptography and certificate validation; DNS, TCP, HTTP/HTTPS
client and bounded server foundations; calendars/time zones; logging, CLI and
configuration helpers; XPRESS compression and safe deterministic ustar data;
parameterized SQLite; and an ordinary Rocket testing facade with fixtures,
XFAIL/filtering, secure temporary roots, and coverage hooks. All public resource
and failure boundaries use bounded `Option`/`Result` contracts with matching
stage0 and self-hosted behavior.

**Purpose:** Cover the common facilities required by tools, games, services, and
desktop applications without forcing every project to reinvent foundations.

**Library work:**

- Immutable byte buffers, exact binary file I/O,
  bounds-checked slicing, UTF-8 conversion, and explicit little-endian unsigned
  integer codecs with stage0/self-hosted parity.

- Continue the byte/binary foundation with buffered streams and
  additional encoding primitives.
- Add complete Unicode scalar iteration, normalization boundaries, and practical
  grapheme-aware APIs.
- Add regular expressions through a safe, documented implementation or binding.
- Add secure randomness, hashing, cryptographic primitives, and certificate
  handling through reviewed platform/library integrations.
- Add sockets, DNS, HTTP client/server foundations, HTTPS, timeouts, and
  cancellation-aware I/O.
- Add dates, calendars, time zones, logging, command-line parsing, configuration,
  compression/archive support, and database foundations such as SQLite.
- Add a proper testing library with assertions, fixtures, expected failures,
  temporary resources, filtering, and code-coverage hooks.

**Acceptance gate:**

- APIs use `Option`/`Result` consistently and document security and blocking
  behavior.
- Network, Unicode, crypto, archive, database, and failure-path integration tests
  run in isolated reproducible environments.
- Standard modules are implemented as ordinary Rocket packages wherever compiler
  privilege is unnecessary.

### Phase 16 - Rocket 1.6: dependency management and package ecosystem - Completed

**Status:** Completed on 2026-07-31. Stage0 and the Rocket-written compiler
consume the same exact locked import graph and expose matching package-security
workflows. The signed file reference registry is the executable deployment;
authenticated HTTPS services implement the versioned protocol in `PACKAGES.md`.

**Purpose:** Make reusable third-party Rocket libraries discoverable,
reproducible, secure, and easy to publish.

**Package work:**

- Extend `rocket.toml` with semantic versions, registry, Git, and local-path
  dependencies.
- Add deterministic version resolution and a committed lockfile.
- Add checksummed downloads, local caches, offline builds, dependency trees,
  duplicate-version diagnostics, and reproducible source selection.
- Design a package registry with immutable published versions, ownership,
  yanking, security reporting, and namespace policies.
- Add package publishing, authentication, documentation generation, licensing
  metadata, and dependency auditing.
- Define build-script and native-dependency policy without granting unrestricted
  implicit code execution.

**Completed handoff checklist:**

- [x] Integrate resolved packages into normal compiler module lookup so `check`,
  `build`, `run`, and `test` consume the exact dependency roots and checksums
  recorded in `rocket.lock`; imports must never bypass the selected graph.
- [x] Bring the Rocket-written production compiler CLI to feature parity with the
  C++ stage0 `resolve`, `tree`, `audit`, locked, and offline workflows, with the
  same deterministic output and stable diagnostics.
- [x] Implement authenticated HTTPS registry transport with bounded downloads,
  redirect and timeout policy, TLS validation, checksum and signed-index
  verification, safe archive extraction, transactional cache installation, and
  no credential leakage in manifests, lockfiles, diagnostics, or logs.
- [x] Implement Git dependency acquisition without shell-command construction:
  require an immutable commit, verify the fetched object matches the requested
  revision, reject moving refs/submodules unless explicitly reviewed, and make
  locked offline reuse independent of a working Git checkout.
- [x] Turn the registry governance design into tested behavior: immutable
  `(namespace, name, version)` records, verified namespace ownership and
  transfer history, reserved-name and anti-typosquatting rules, yanking without
  deletion, auditable scoped/revocable credentials, and emergency security
  actions.
- [x] Add `login`/credential management and `publish` commands with preflight
  validation, deterministic source archives, duplicate/case-collision/link/path
  traversal and decompression-limit checks, ownership enforcement, immutable
  version refusal, and safe retry behavior.
- [x] Add package documentation generation and publication with API cross-links,
  examples, package/version identity, deterministic output, and no execution of
  untrusted examples during publishing.
- [x] Expand licensing and dependency auditing with SPDX validation, license-policy
  configuration, signed advisory data, affected-version evaluation, yanked and
  compromised-package reporting, provenance display, and useful nonzero exit
  statuses for CI.
- [x] Finalize an explicit native-dependency/build-script capability policy. If a
  build hook is introduced, declare and validate its inputs, outputs, target,
  environment, network access, cache key, sandbox, and user approval; never run
  dependency code implicitly during resolve, audit, or documentation.
- [x] Prove clean and offline locked builds produce byte-identical source selection
  and build artifacts where the pinned toolchain permits, including relocation
  to a machine with an empty package cache.
- [x] Add end-to-end tests for conflicting constraints, hostile archives, checksum
  and signature failures, namespace takeover/typosquatting, revoked credentials,
  yanks, compromised advisories, poisoned caches, interrupted downloads, and
  deterministic recovery.
- [x] Publish several independent test packages through the real workflow and
  consume them transitively from real executable and library applications in
  both online and locked-offline modes.

**Acceptance gate:**

- [x] Clean and offline machines reproduce locked builds byte-for-byte where target
  toolchains permit.
- [x] Resolver conflict, malicious archive, checksum, namespace, and compromised-
  dependency scenarios have explicit tests and diagnostics.
- [x] Several independent packages are published and consumed by real applications.

**Completion evidence:**

- Pinned dependency verification passed with Git 2.47.1.windows.2, CMake
  3.31.6-msvc6, Ninja 1.13.1, MSVC 19.44.35228, and LLVM 22.1.6.
- LLVM Debug and Release matrices passed 133/133 tests each; LLVM-disabled
  stage0 Debug and Release matrices passed 93/93 tests each. Each matrix
  includes three Phase 16 package, registry, and self-hosted workflows.
- Release bootstrap passed with byte-identical stage2/stage3 LLVM IR at SHA-256
  `a019dd80b0975efad667588ecab0c886b314d6f77419d9a1342905af38c5202c`.
- Release conformance passed 78/78 cases; all eight performance gates passed.
- The clean online and relocated locked-offline executable is byte-identical at
  SHA-256 `5c84c5ab076b41df6e2c5aaac128680ece5d7a44db52dadb1203e83d0d0dc182`.
- Distribution relocation/package verification passed. The final Windows x64
  ZIP is 251,357,847 bytes at SHA-256
  `53a57a13d62e38006946a11ab18b635ee532ec7da9c4d8058d024fc0f6d060dc`.

**Deliberate limits:** Rocket 1.6 ships a signed file reference registry and an
HTTPS service contract, not a public hosted registry. Credential storage is
Windows-only. Dependency build scripts remain unsupported and dependency native
inputs remain deny-by-default unless the root manifest approves an exact locked
identity. Phase 17 editor and language-server work was completed separately below.

### Phase 17 - Rocket 1.7: professional developer experience - Completed

**Completed from Phase 16 tip `5ec449c`.** Protocol 1.0 is a standalone,
editor-neutral LSP 3.17 process backed by the compiler's bounded multi-package
semantic graph and unsaved overlays. Rocket 1.7 also ships versioned docs,
native debugging, coverage/profile/benchmark/CI schemas, and an evaluated AOT
REPL prototype. Exact final validation evidence is recorded below and in
`PROJECT_CONTEXT.md`.

**Purpose:** Make Rocket efficient to learn, navigate, debug, maintain, and
operate in substantial codebases.

**Tooling work:**

- Build a semantic language server with completion, hover types/documentation,
  go-to-definition, find references, rename, signature help, semantic tokens,
  code actions, and automatic imports.
- Add incremental project analysis and bounded editor latency on large packages.
- Add a documentation generator with cross-links, examples, search metadata,
  and package-version awareness.
- Integrate native debugging with breakpoints, source mapping, stack traces,
  variable inspection, and panic/runtime-failure locations.
- Add code coverage, benchmarking, profiling hooks, and machine-readable compiler
  and test output.
- Evaluate a REPL based on incremental AOT compilation; a JIT remains optional.

**Completed handoff checklist:**

- [x] Replace protocol 0.1's self-contained-document analysis with an incremental,
  dependency-aware project graph spanning package manifests, locked
  dependencies, standard modules, open unsaved overlays, and multi-file imports.
  Support bounded invalidation, cancellation, stale-result suppression, and
  measured latency/memory limits on large workspaces.
- [x] Add semantic completion and automatic imports with deterministic ranking,
  visibility checks, incomplete-code recovery, dependency awareness, and edits
  that preserve valid Rocket source.
- [x] Add hover types and documentation plus signature help, including generics,
  traits, overload/specialization information, parameter position, and links to
  versioned generated package documentation.
- [x] Add go-to-definition, find references, and workspace rename across modules and
  packages. Distinguish definitions from textual matches and reject unsafe or
  conflicting renames before producing edits.
- [x] Add versioned semantic-token support for declarations, references, types,
  fields, parameters, traits, native symbols, and inactive/error regions, with
  deterministic full and incremental responses.
- [x] Add code actions for stable compiler diagnostics and safe source
  transformations, including missing imports where unambiguous; actions must be
  tested for idempotence and must not silently change program meaning.
- [x] Support negotiated incremental document synchronization in addition to the
  bounded full-sync baseline, robust workspace/configuration changes, request
  cancellation, concurrent clients' request ordering, and protocol-level
  performance telemetry that does not leak source text.
- [x] Complete the language-level documentation generator with symbol cross-links,
  examples, search metadata, source locations, package/version awareness,
  deterministic output, and malformed/incomplete-source diagnostics.
- [x] Emit native debug information and integrate a documented editor-neutral debug
  adapter or equivalent workflow for breakpoints, source mapping, stack traces,
  variable inspection, and precise panic/runtime-failure locations in both
  Debug and optimized builds.
- [x] Finish code-coverage collection and reports, benchmark harnesses, profiler
  hooks/symbolization, and stable machine-readable compiler/test/build output
  suitable for CI and editor integrations.
- [x] Evaluate and document an incremental-AOT REPL prototype, including state
  lifetime, redefinition, diagnostics, dependency loading, startup latency, and
  platform limitations; keep JIT support optional unless the evidence justifies
  it.
- [x] Add editor-neutral conformance tests for every request and capability,
  multi-package and incomplete-code fixtures, UTF-16/ranged-edit edge cases,
  cancellation/staleness races, malformed and oversized messages, deterministic
  output, and bounded latency regression gates.
- [x] Validate debugging against optimized and unoptimized executables and prove the
  versioned protocol and tooling can be used by at least one client other than
  the existing dependency-free VS Code extension.

**Acceptance gate:**

- Editor features are tested against multi-package projects and incomplete code.
- Debug information accurately maps optimized and unoptimized executables back to
  Rocket source.
- Tooling protocols remain versioned, documented, and usable outside VS Code.

**Acceptance evidence:**

- Pinned LLVM Debug and Release passed 138/138 tests; LLVM-disabled stage0 Debug
  and Release passed 95/95. Conformance passed 78/78 in each configuration and
  all eight Debug and Release latency gates passed.
- Debug and Release stage2/stage3 LLVM IR are identical at SHA-256
  `bc4be23c7e06024ba9337bcfcdd1ffca470735dd8f542c09b0ea9720812a82ed`.
- The editor-neutral Node.js client analyzed a locked four-file project plus an
  unsaved overlay and returned dependency completion in 7 ms. Debug and optimized
  PDB/source-map validation, panic locations, compiler/build/test JSON Lines,
  coverage, profiling, benchmarking, packaging, and sanitized relocation passed.
- Exact report and distribution hashes, timings, and deliberate limitations are
  recorded in `PROJECT_CONTEXT.md`.

### Phase 18 - Rocket 1.8: robust ownership, concurrency, and asynchronous I/O - Completed

**Status:** Completed on 2026-08-02. The final-source release matrix,
deterministic bootstrap, conformance, performance, self-contained package, and
sanitized relocation all passed.

**Purpose:** Support responsive applications and parallel services while keeping
Rocket's default programming model understandable and safe.

**Language and runtime work:**

- Add weak references and an explicit strategy for detecting or preventing ARC
  reference cycles.
- Add unique mutable buffers and copy-on-write optimization where semantics stay
  predictable.
- Define `Send`/`Share`-style constraints or an equally explicit rule for values
  crossing thread boundaries.
- Add threads, synchronization primitives, thread pools, channels, and tasks.
- Add structured concurrency, cancellation, timers, and `async`/`await`.
- Make shared managed ownership atomic only where required and retain a low-cost
  single-threaded path.
- Integrate asynchronous files, sockets, processes, and timers with platform
  event systems.

**Acceptance gate:**

- Data races are prevented by the safe language surface or confined to explicit
  unsafe code.
- Leak, cycle, deadlock, cancellation, task-lifetime, and high-contention stress
  suites pass repeatedly.
- Compiler, runtime, and async scheduling behavior are specified rather than
  depending on undocumented implementation accidents.

**Acceptance evidence:**

- Final-source LLVM Debug/Release passed 210/210; LLVM-disabled stage0
  Debug/Release passed 165/165; and the focused Phase 18 selection passed 78/78
  twice, including compiler, runtime stress, native, and self-hosted checks.
- The strengthened bootstrap proved all 14 positive and 15 negative Phase 18
  fixtures agree across stage0-stage3 and produced byte-identical stage2/stage3
  LLVM IR at SHA-256
  `d2bb814269f3c05fd823b21e6ab0e6b908fcca99f37d28d5fd1130a50d01ba23`.
- Release conformance passed 90/90 and all 11 performance budgets passed.
  Self-contained packaging and sanitized relocation passed, including a
  relocated Phase 18 ownership/concurrency program. Exact timings, report
  hashes, package metadata, and deliberate limits are recorded in
  `PROJECT_CONTEXT.md` and `PHASE_18_AUDIT.md`.

### Phase 19 - Rocket 2.1: multi-platform compiler and runtime - Active

**Status:** Resumed by owner direction on 2026-08-20. The frozen Rocket 2.0
Windows x64 SDK remains an immutable compatibility boundary. Phase 19 is an
additive 2.1 release and cannot close on implementation or CI configuration
alone: every production target needs direct observed native-host Debug/Release,
stage0-stage3, package, relocation, and complete-suite evidence in
`PHASE_19_AUDIT.md`.

**Purpose:** Make Rocket a portable language rather than a Windows-only tool.

**Platform work:**

- Add explicit target triples, conditional compilation, target feature queries,
  and platform-specific package sections.
- Support Linux x64 and ARM64, then macOS ARM64, with Windows ARM64 evaluated
  after the runtime and toolchain are stable.
- Port filesystem, process, time, networking, async, debugger, packaging, and
  standard-library behavior with documented platform differences.
- Add host/target separation and supported cross-compilation paths.
- Establish continuous build, test, bootstrap, package, and application matrices
  on every supported target.
- Evaluate WebAssembly only after native targets are production-quality; JIT
  compilation remains an optional separate project.

**Acceptance gate:**

- Self-hosting, conformance, FFI, package, standard-library, and substantial
  application suites pass on every supported platform.
- Platform-specific behavior is represented in specifications and APIs rather
  than hidden behind inconsistent implementations.
- Published compiler packages install and run without requiring an unrelated
  system compiler toolchain.
- Windows ARM64 is an evaluated non-production target until its own native
  acceptance matrix exists; WebAssembly and JIT are separate future work.

### Phase 20 - Rocket 2.0: security, performance, compatibility, and trust - Completed

**Completed 2026-08-02.** Owner direction accepted the technical Windows x64
release while Phase 19 remains deferred. Rocket 2.0 freezes the cumulative
Rocket 1.0-1.8 source, ownership, standard-library, package, tooling, runtime
ABI v1, and FFI contracts without adding grammar.

**Hardening and release work:**

- Freeze the Rocket 2.0 grammar, type system, ownership/concurrency model,
  standard library, package format, tool protocols, runtime ABI, and FFI.
- Run continuous lexer, parser, type-system, IR, optimizer-boundary, runtime,
  package, and protocol fuzzing.
- Add sanitizer, fault-injection, malformed-input, resource-exhaustion, and
  compiler-crash minimization workflows.
- Establish incremental and parallel compilation, stable caching, performance
  budgets, representative benchmarks, and regression tracking.
- Validate large multi-package builds and long-running real applications.
- Add signed releases, provenance, checksums, reproducible build evidence,
  vulnerability handling, release channels, and compatibility testing against
  supported older source and packages.
- Publish a language book, standard-library documentation, FFI guide, package
  author guide, tooling guide, migration guide, and governance/contribution
  process.
- Continue recruiting external users and resolving adoption feedback as ongoing
  release maintenance. Independent production usage remains a maturity signal;
  it is not represented as observed evidence by this repository-only phase.

**Acceptance gate:**

- The supported Windows x64 target passes bootstrap, conformance, performance,
  hardening, package, FFI, debugger, compatibility, and application gates.
- Bounded inputs, deterministic fuzzing, parser recovery, crash minimization,
  sanitizer presets, a conservative content cache, parallel package-build
  validation, and 16-package application validation are executable tests.
- Official stable packaging requires a clean tree and supplied signing
  certificate; emits provenance, checksums and a detached signature; proves
  deterministic archive reproduction; and verifies sanitized relocation.
- `SECURITY.md`, `CONTRIBUTING.md`, the Rocket book, 2.0 release/migration/syntax
  documents, FFI guide, package-author guide, and existing full reference guides
  make security and compatibility policies operational.
- Exact observed commands, counts, limitations, and hashes are recorded in
  `PROJECT_CONTEXT.md` and `PHASE_20_AUDIT.md`; no external adoption is claimed.

## Post-roadmap tooling maintenance

### Visual Studio Community 2026 integration - Completed

**Completed 2026-08-08.** `Rocket.Language.VisualStudio` 2.0.3 upgrades the
existing extension identity without changing Rocket 2.0 source, compiler,
runtime ABI, stage0, LSP, CodeView, or source-map contracts.

- Context-sensitive GUI Build, Run, Test, Stop, and Debug commands discover the
  nearest `rocket.toml` or treat an active `.rocket` file as standalone.
- Compiler, environment, application, and debuggee processes use hidden-window
  execution and redirected streams. Build and program output remains inside
  Visual Studio, and native Debug opens no external terminal.
- `rocket-message-1` diagnostics populate navigable Error List entries, and the
  existing `rocket-lsp` server supplies completion, hover, navigation,
  references, rename, symbols, semantic tokens, formatting, and live
  diagnostics.
- The native debugger consumes Rocket's existing executable, CodeView PDB, and
  `rocket-source-map-1` artifacts for breakpoints, stepping, call stacks, and
  represented locals. The final GUI pass stopped directly in recursive
  `fibonacci`, displayed `value = 10`, stepped into a second recursive frame,
  and stopped the hidden process normally.
- Focused extension tests passed 26/26; portable VSIX inspection rejects
  checkout-specific paths. Existing scripts and CMake targets remain fallback
  automation.

This is a compatible tooling patch, not Phase 19 or a new language phase. The
terminal-free workflow is non-interactive for console stdin; arguments,
stdout/stderr, files, GUI input, and networking remain available.

## Required implementation path for every new feature

Every feature in Phases 11 through 20 follows this order:

1. Write the user-facing problem, examples, syntax, semantics, failure behavior,
   ownership implications, and compatibility decision.
2. Update the language, standard-library, tooling, runtime, or ABI specification
   before treating the design as accepted.
3. Implement lexer/parser, name resolution, type checking, HIR, MIR, backend,
   runtime, package, and tooling changes as applicable.
4. Implement equivalent behavior in C++ stage0 and the Rocket-written compiler.
5. Add focused positive, negative, diagnostic, native-execution, lifetime,
   security, and performance tests proportional to the feature's risk.
6. Run Debug and Release builds plus the stage0 and deterministic stage1-stage3
   bootstrap gates.
7. Update the syntax dictionary, examples, reference documentation, decision
   journal, project context, and migration notes.
8. Commit only after the entire vertical feature slice is reproducible.

## Planning reality

The technical roadmap is realistic, but it is a multi-year language and
ecosystem program rather than a short sequence of coding tasks. A small focused
team can build a respected language if it limits scope, completes vertical
slices, protects compatibility, publishes reliable releases, and attracts real
users. Matching the number of packages, platforms, contributors, and production
years behind C++, Python, or Rust is not a realistic completion criterion.

Rocket succeeds when its design is coherent, its compiler and runtime are
trustworthy, its supported domains work exceptionally well, and external
developers voluntarily build and maintain substantial software with it.
