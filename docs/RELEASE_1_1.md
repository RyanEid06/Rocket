# Rocket 1.1 Release Contract

Rocket 1.1 is an additive release over the frozen Rocket 1.0 contract. Every
valid Rocket 1.0 program remains valid with the same observable behavior. The
complete base contract remains in [RELEASE_1_0.md](RELEASE_1_0.md); this document
defines only the 1.1 additions and their release gates.

## Additive language and library surface

- Direct element assignment is available through a mutable `Array[T]` binding.
  Arrays retain copy-on-write value semantics, so existing aliases and Slices
  keep their pre-mutation snapshots.
- Arrays expose explicit capacity and persistent `reserve`, `append`, `pop`,
  `insert`, `remove`, and `clear` operations with checked bounds and growth.
- `Tuple2`, `Tuple3`, insertion-ordered `Map`, and insertion-ordered `Set` are
  built-in generic collection products. Map keys and Set elements are limited
  to `Int`, `Bool`, `Char`, and `String`.
- Stable FNV-1a hashing, searching, equality filtering, scalar/String sorting,
  hash mapping, and numeric sum folds are available in `std.collections`.
- `Queue`, `Stack`, and `ByteBuffer` provide transparent Array-backed product
  types. Phase 12 methods may add convenience without changing their 1.1 data
  representation.

The normative syntax and semantic details are in
[ROCKET_1_1_SYNTAX_DICTIONARY.md](ROCKET_1_1_SYNTAX_DICTIONARY.md),
[SPEC.md](SPEC.md), and [STDLIB.md](STDLIB.md).

## Compatibility and ABI

Rocket 1.1 does not remove or reinterpret Rocket 1.0 syntax, diagnostics,
package manifests, formatter behavior, CLI commands, or standard-library APIs.
Runtime ABI v1 remains the native boundary. New runtime entry points are
additive and use the existing ARC ownership convention.

## Supported target and package

The supported target remains Windows x64 using pinned LLVM 22.1.6, Ninja
1.13.1, MSVC, and the Windows SDK. `scripts/package-compiler.ps1` creates
`out/package/rocket-1.1.0-windows-x64.zip`, containing the Rocket-written stage3
compiler, runtime, pinned Clang/LLD resources, native static link inputs, and the
separate C++ stage0 compiler.

## Required validation

A Rocket 1.1 milestone is release-ready only after all of these pass:

1. Stage0 Debug and Release build/test matrices.
2. LLVM-enabled Debug and Release build/test matrices, including native Phase
   11 execution by stage0 and the self-hosted compiler.
3. Managed-value mutation, reallocation, Map/Set lifetime, alias/Slice snapshot,
   deterministic insertion-order, fixed-hash, bounds, and invalid-key tests.
4. Deterministic `stage0 -> stage1 -> stage2 -> stage3` bootstrap, with
   byte-identical stage2/stage3 LLVM IR and Phase 11 fixtures in the bootstrap
   conformance set.
5. The Rocket 1.1 conformance suite, performance gates, checksummed package, and
   isolated relocation test when producing the distributable archive.

Generated build, bootstrap, report, package, downloaded dependency, and cache
artifacts remain outside Git.
