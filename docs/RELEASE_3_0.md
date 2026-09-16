# Rocket 3.0.0 release

Rocket 3.0.0 is the additive graphics, UI, motion, language-ergonomics, and
quality release built on the accepted Rocket 2.1 portability contract. It
preserves valid predecessor source, the permanent C++20 stage0, package and
target identities, and runtime ABI v1.

The source release is identified by tag `v3.0.0`. The accepted implementation
tree is `a9e7ea261f25c4438eb3594312a79e88da99eda0`; WP35 adds only release
identity, documentation, traceability, and packaging metadata on top of that
accepted tree.

## Highlights

- Named arguments, declaration defaults, and labeled enum payloads with
  stage0/self-host, formatter, language-server, and diagnostic parity.
- Complete `std.math` and `rocket.motion` easing, tween, timeline, and reduced-
  motion APIs.
- Safe value-owned graphics, shape, input, typography, render-target, shader,
  display-quality, and virtual-canvas APIs over the reviewed raylib boundary.
- Bounded immediate-mode UI contexts, layouts, themes, styles, controls,
  containers, dialogs, overlays, tooltips, and toasts.
- A bounded typed asset-store reference implementation in the showcase package.
- Unified public error/lifetime behavior, calibrated state bounds and
  performance budgets, visual regression, and focused public examples.

## Acceptance evidence

GitHub Actions native run
[`35079104907`](https://github.com/RyanEid06/Rocket/actions/runs/35079104907)
completed in 1 h 45 min 31 s. Its four native jobs and eight cross-build/native-
execution jobs all passed:

| Native host | Job duration | LLVM Debug | LLVM Release | Stage0 Debug | Stage0 Release | Bootstrap |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Windows x64 | 1 h 5 min 20 s | 288/288 | 288/288 | 209/209 | 209/209 | 184 cases |
| Linux x64 | 1 h 22 min 50 s | 288/288 | 288/288 | 209/209 | 209/209 | 184 cases |
| Linux ARM64 | 1 h 24 min 40 s | 288/288 | 288/288 | 209/209 | 209/209 | 184 cases |
| macOS ARM64 | 1 h 35 min 59 s | 288/288 | 288/288 | 209/209 | 209/209 | 185 cases |

Every host produced matching stage2/stage3 canonical IR SHA-256
`ef3f6bae64cf43965693cfd43ddf9c28b8e22eaed28f878669990329bafe42af`.
The supported cross paths—Windows x64 to Linux x64/Linux ARM64 and Linux x64
to Windows x64/Linux ARM64—built successfully and each artifact then executed
on its native destination runner.

GitHub Actions visual run
[`35079104860`](https://github.com/RyanEid06/Rocket/actions/runs/35079104860)
passed Windows x64, Linux x64, Linux ARM64, and macOS ARM64. The macOS native
matrix includes the hosted software-OpenGL fallback rather than skipping GPU
coverage.

The accepted WP34 relocation candidates contained 982 Windows files, 540 Linux
x64 files, 524 Linux ARM64 files, and 483 macOS ARM64 files. Their archive
SHA-256 values were, respectively:

- `8a0f6d29e3aa76e32b7b47a9bf8bb207f888bb922d0963446633635994706287`
- `4182b53ce91eb7a337ddf2062033ff3577a68724ce46f6669db3cf8f0ccaead1`
- `9fb2ca05e9e23c7a0d3ddc4cc19d2f94d9b3d399bf1cdbcf036e3914dee45a26`
- `08e7ad7467bd560f0540e42c736eded5700471b43981fd5d71260a3ad0ea4c25`

These are acceptance-candidate hashes, not permanent download URLs. The
repository publishes the annotated source tag as the durable release identity;
unsigned CI SDK artifacts remain short-lived verification evidence. Official
signed binary publication is intentionally absent because no release signing
certificate is configured.

## Release commands

The accepted platform evidence used the commands encoded in
`.github/workflows/phase19-native.yml` and `.github/workflows/rocket3-visual.yml`.
WP35 itself uses only focused release checks:

```powershell
cmake --preset phase19-windows-x64-release
cmake --build --preset phase19-windows-x64-release
ctest --test-dir out\phase19\build\phase19-windows-x64-release -R "^(phase19_release_tooling|cli_version)$" --output-on-failure
git diff --check
git status --short --branch
git ls-remote origin refs/heads/master refs/tags/v3.0.0
```

The focused Windows Release build completed successfully, the two selected
tests passed `2/2`, and the built compiler reported `rocketc 3.0.0`. The
release-tooling test also verified that all 167 atomic requirement IDs are
present in the traceability matrix and that current release documents contain
no pre-release WP35 status markers.

No full build/test matrix is repeated in WP35; WP34 is the immutable acceptance
gate for the implementation being released.

## Intentional limitations

- Runtime ABI remains v1; Rocket 3.0 does not introduce a new ABI generation.
- `rocket.assets` is not a bundled module. The accepted typed asset store is
  `src.rocket_assets` inside `examples/raylib_showcase`.
- Windows ARM64 remains recognized but is not a production-supported target.
- Cross execution is never implicit; emitted artifacts run only on a matching
  native destination.
- No official binary signing key or certificate is configured, so WP35 does
  not fabricate signed artifacts or provenance claims.
- Scroll2Roll remains on the preserved Rocket 2.0 Windows SDK unless a separate
  migration is explicitly authorized.

See [MIGRATION_3_0.md](MIGRATION_3_0.md) for source guidance and
[ROCKET_3_0_TRACEABILITY.md](ROCKET_3_0_TRACEABILITY.md) for the complete
requirements closure.
