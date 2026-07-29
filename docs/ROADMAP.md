# Rocket Master Roadmap

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
   interoperability and graphics validation project in Phase 14. A casino may be
   planned separately only after Rocket 2.0 is accepted, unless the user
   explicitly changes this decision.

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

### Phase 12 - Rocket 1.2: scalable abstractions and functional values

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

### Phase 13 - Rocket 1.3: native interoperability and library production

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

### Phase 14 - Rocket 1.4: graphics, audio, and real application validation

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

### Phase 15 - Rocket 1.5: production standard library

**Purpose:** Cover the common facilities required by tools, games, services, and
desktop applications without forcing every project to reinvent foundations.

**Library work:**

- Add byte buffers, binary I/O, buffered streams, and encoding primitives.
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

### Phase 16 - Rocket 1.6: dependency management and package ecosystem

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

**Acceptance gate:**

- Clean and offline machines reproduce locked builds byte-for-byte where target
  toolchains permit.
- Resolver conflict, malicious archive, checksum, namespace, and compromised-
  dependency scenarios have explicit tests and diagnostics.
- Several independent packages are published and consumed by real applications.

### Phase 17 - Rocket 1.7: professional developer experience

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

**Acceptance gate:**

- Editor features are tested against multi-package projects and incomplete code.
- Debug information accurately maps optimized and unoptimized executables back to
  Rocket source.
- Tooling protocols remain versioned, documented, and usable outside VS Code.

### Phase 18 - Rocket 1.8: robust ownership, concurrency, and asynchronous I/O

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

### Phase 19 - Rocket 1.9: multi-platform compiler and runtime

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

### Phase 20 - Rocket 2.0: security, performance, compatibility, and trust

**Purpose:** Turn the accumulated language platform into a stable major release
that external developers can reasonably adopt and maintain.

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
- Recruit external users, resolve adoption feedback, and require multiple
  independently maintained production projects before declaring 2.0 mature.

**Acceptance gate:**

- All supported targets pass clean-machine bootstrap, conformance, performance,
  fuzzing, package, FFI, debugger, and application release gates.
- Compatibility and security policies are operational, not aspirational.
- Rocket 2.0 is used successfully by developers other than its original authors
  for substantial maintained software.

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
