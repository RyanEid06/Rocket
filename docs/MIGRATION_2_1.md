# Migrating to Rocket 2.1 portability

Rocket 2.1 is additive to the frozen Rocket 2.0 language and runtime ABI v1.
Valid Rocket 1.0-2.0 source continues to compile without a source rewrite.
The new portability surface is explicit target selection, not a new language
preprocessor or host-dependent behavior.

## Select a target deliberately

Use `rocketc target --verbose` to inspect the native host and selected target.
Use `--target <alias-or-triple>` for a build intended for a different supported
target. The production aliases are `windows-x64`, `linux-x64`, `linux-arm64`,
and `macos-arm64`; their canonical triples and diagnostics are normative in
`TARGETS.md`.

For a cross build, supply a documented target SDK/sysroot with `--target-sdk`.
Rocket never silently borrows host headers or libraries, and `run`/the native
part of `test` refuse cross execution rather than using an undeclared emulator.

## Package changes

Portable package source remains under the ordinary source root. Use an optional
`[target.<alias>]` section only for a target-specific module overlay, and use
`[native.<alias>]` only for explicit target-native inputs. Inactive target
roots and native sections are not parsed, linked, or hashed. Target-qualified
artifacts live below `.rocketc/targets/<alias>/`.

## Compatibility and availability

No Rocket 2.0 source, ABI-v1, package lock, or public standard-library contract
is removed. Platform differences are documented `Result` behavior rather than
implicit host fallbacks. Consult `PHASE_19_AUDIT.md` before distributing a
target package: the production label is conditional on recorded native-host
acceptance, not merely on a successful cross build.
