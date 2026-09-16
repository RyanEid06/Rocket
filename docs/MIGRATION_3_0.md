# Migrating from Rocket 2.1 to Rocket 3.0

Rocket 3.0 is additive. Valid Rocket 2.1 source remains valid, package and
target identities are unchanged, and runtime ABI v1 is preserved. Existing
projects do not need a mandatory source rewrite.

## Adopt language ergonomics incrementally

Positional calls remain supported. Named arguments may be introduced where
they improve clarity, and declaration defaults may remove repeated trailing
arguments. Public parameter names are source-facing API once callers use them,
so rename them deliberately. Labeled enum payload construction is likewise
optional; existing positional construction remains valid.

## Use the bundled math, motion, graphics, and UI modules

New projects can import `std.math`, `rocket.motion`, `rocket.graphics`,
`rocket.graphics.shapes`, `rocket.graphics.input`, `rocket.graphics.canvas`,
`rocket.raylib.safe`, `rocket.ui`, and the `rocket.ui.*` modules documented in
[STDLIB.md](STDLIB.md) and
[ROCKET_3_0_SYNTAX_DICTIONARY.md](ROCKET_3_0_SYNTAX_DICTIONARY.md).

Keep raw native handles inside small reviewed `unsafe:` boundaries. Prefer the
checked `Result`-returning wrappers and value-owned tokens. UI frames,
render-target/scissor/blend scopes, and asset references enforce explicit
lifetime rules; stale, duplicate, wrong-context, and out-of-order use is a
programmer contract failure rather than undefined behavior.

## Respect bounded state

The default UI identity capacity is 2,048 entries with eight unseen frames of
retention. Text measurement and typed asset stores default to 256 entries.
Applications that need larger explicit bounds should document and test the
chosen capacity instead of adding unbounded caches.

## Asset-store location

The accepted typed asset store is the reference package module
`examples/raylib_showcase/src/rocket_assets.rocket`, imported as
`src.rocket_assets`. There is no `rocket.assets` standard-library module in
Rocket 3.0.0.

## Targets and packages

The production target rows remain `windows-x64`, `linux-x64`, `linux-arm64`,
and `macos-arm64`. Existing `[target.<alias>]`, `[native.<alias>]`, lockfile,
registry, and safe-path rules are unchanged. Use `rocketc target --verbose` and
an explicit `--target-sdk` for supported cross builds; `run` and native test
execution still reject a non-native target.

## Compatibility checklist

1. Keep the existing `rocket.toml` and `rocket.lock`.
2. Run `rocketc check`, `rocketc fmt --check`, and the package's focused tests.
3. Replace raw raylib calls only when adopting the new safe graphics/UI layer.
4. Confirm any public parameter-name changes before adopting named calls.
5. Keep resource cleanup explicit and do not retain frame-scoped tokens.
6. Consult [RELEASE_3_0.md](RELEASE_3_0.md) for accepted targets and known
   limitations.
