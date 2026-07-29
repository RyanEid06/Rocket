# Rocket 1.2 Release Contract

Rocket 1.2 is an additive release over Rocket 1.1. Rocket 1.0 and 1.1 source
keeps its existing meaning, runtime ABI v1 remains stable, and the C++ compiler
remains the reproducible stage0 bootstrap.

## Language surface

- Same-module impl blocks, explicit receivers, associated functions, and
  standard-library dot-call aliases.
- Static traits and `where` constraints with deterministic compile-time
  selection and no trait objects or virtual dispatch.
- Typed expression lambdas whose by-value captures are immutable ARC-managed
  closure aggregates; callbacks specialize through ordinary generics, including
  lambda signatures that reference enclosing generic-function type parameters
  and immediately invoked anonymous closures.
- Persistent user-defined iteration through `iterator`, `has_next`, `value`,
  and `advance` methods.
- Non-generic associated constants accessed as `Owner.NAME`.
- Positional, required, fixed-arity parameters and a hard limit of 4,096 user
  generic specializations per compilation.

## Compatibility and target

The supported release target is Windows x64 using the pinned MSVC, Ninja, LLVM,
Clang, and LLD toolchain. There is no dynamic function erasure, trait object,
inheritance, default/named/variadic argument syntax, or generic/trait associated
constant in 1.2. Managed closure captures follow the existing deterministic ARC
contract; reference cycles remain unsupported.

## Release gate

A 1.2 artifact is releasable only when Debug and Release tests pass with LLVM,
the LLVM-disabled stage0 matrices pass, stage0 and the Rocket-written compiler
run all Phase 12 fixtures identically, conformance and performance gates pass,
and `stage0 -> stage1 -> stage2 -> stage3` produces byte-identical stage2/stage3
LLVM IR. Generated, downloaded, bootstrap, package, and `.rocketc` artifacts
remain outside Git.
