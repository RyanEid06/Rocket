# Rocket 3.5 WP7 acceptance evidence

This document records the Windows reference run and the exact release checks
required for the Scroll2Roll readiness decision. WP7 is accepted only after the
native and cross-platform workflow artifacts for the pushed commit are green.
The branch is `rocket35/eddie`; this work does not change `master` or start the
casino product.

## Reference configuration

- Host: Windows x64, build 10.0.26200.9457, AMD Radeon(TM) Graphics OpenGL
  renderer. A 1280x720 high-DPI window produced a 1600x900 physical screenshot.
- Toolchain: MSVC 19.44.35207, pinned LLVM 22.1.6, Ninja 1.13.1,
  CMake 3.31.6-msvc6, Python 3.12.14, pinned Raylib 6.0.
- The package uses the normal `rocketc check` and `rocketc build` flow. The
  acceptance test launches from outside the package with an explicit absolute
  asset root. Stage0 and self-hosted compilers both check all four target
  surfaces for the vertical slice and canonical showcase: 16 checks total.

## Acceptance scene and measurement contract

The 960x540 logical scene uses a textured table, front/back card art, chip
atlas, licensed font, rendered panel and button, UI hover/pressed styles,
`rocket.motion`, one 960x540 render target, additive chip glow, an optional
shader presentation pass, a sound effect on click, and updated streamed music.
The store owns four textures, one font, one sound, one music stream, and an
optional shader: seven or eight entries. The render target is separately
owned. There are 17 top-level drawing submissions per frame, including two
clears and presentation; UI and text expand into further native draws. Every
frame opens and closes one target scope, one blend scope, and, when supported,
one shader scope.

`ROCKET_SLICE_BENCHMARK=1` runs 440 frames. Frames 40 and 80 resize the window;
frames 120 and 160 enter and leave fullscreen. The first 200 frames warm up and
exercise display transitions. The final 240 frames are timed with
`safe.elapsed_time` around music update, UI, drawing, and presentation. The
1000 FPS target reduces pacer influence; normal play uses 60. Each test cycles
startup and shutdown three times and checks both PNG captures plus the reviewed
logical golden. The golden checksum is recorded in
`tests/visual/goldens/wp7/manifest.json`.

The reference-machine performance budget is comparative: remeasure in the
same mode on the same host and backend, and investigate a sustained average
over twice the recorded Release baseline or a worst frame over three times its
baseline. This is a regression trigger, not a universal FPS promise.

## Local results

Windows x64 results were collected on 2026-09-25. The full CTest runs used the
repository's serial release mode. Each includes the compiler, package,
graphics/UI, WP1-WP6, adapter, visual, showcase, premium example, and WP7
registrations. The standalone benchmark reran the stage0 WP7 test with no
other CTest workload. Every cycle compared the 960x540 logical image with the
reviewed golden at mean error 0.000 and changed-pixel ratio 0.0000; the
physical capture was 1600x900 on this host.

| Gate | Passed | Failed | Evidence |
| --- | ---: | ---: | --- |
| Windows x64 Release full CTest | 307 | 0 | `out/rocket35/wp7/Release/final-serial-ctest.log` |
| Windows x64 Debug full CTest | 307 | 0 | `out/rocket35/wp7/Debug/final-serial-ctest.log` |
| Release standalone WP7 stage0 | 1 | 0 | `out/rocket35/wp7/Release/reference-benchmark.log` |
| Debug standalone WP7 stage0 | 1 | 0 | `out/rocket35/wp7/Debug/reference-benchmark.log` |
| Migrated showcase, 120-frame native run from outside package | 1 | 0 | `out/rocket35/wp7/golden-candidate/showcase-run.log` |

| Serial benchmark | Cycle 1 average / worst | Cycle 2 average / worst | Cycle 3 average / worst |
| --- | --- | --- | --- |
| Release | 0.663157 / 1.8445 ms | 0.660962 / 3.5224 ms | 0.663450 / 2.0363 ms |
| Debug | 1.106310 / 3.3447 ms | 1.105390 / 3.3173 ms | 1.079790 / 2.3483 ms |

Across the three 240-frame measurement windows, Release averaged 0.662523 ms
and had a 3.5224 ms worst frame; Debug averaged 1.097163 ms and had a
3.3447 ms worst frame. These values describe the reference Windows host only.
The two full suites each ran stage0 and self-hosted WP7 scenes for three fresh
process lifetimes. Together they covered 12 startup/shutdown cycles, 24 window
resizes, 24 fullscreen transitions, and 12 logical/physical screenshot pairs.

The package tests cover compiler and package behavior, existing Rocket 3
graphics, UI, assets, motion, WP1-WP6, native adapters, the canonical showcase,
premium table, visual goldens, stage0/self-hosted parity, and the new WP7
acceptance. Native adapter fixtures separately exercise stale handles, scope
nesting, repeated resource ownership, and teardown.

## Cross-platform release gates

The pushed branch triggers `.github/workflows/rocket3-visual.yml` and
`.github/workflows/phase19-native.yml`. The latter records per-run host and
tool versions plus CTest JUnit counts and validates native Debug/Release and
stage0 builds on Windows x64, Linux x64, Linux arm64, and macOS arm64. It then
packages and relocates each SDK, performs the supported four cross-build pairs,
and runs destination artifacts on their native hosts. The visual workflow
captures WP7 logical and physical screenshots on the same four platforms.
The workflow artifacts, tied to the pushed commit, are the authoritative
evidence for these external gates.

Headless Windows and Linux CI runners set `ROCKET_AUDIO_NULL_BACKEND=1` to use
Raylib's miniaudio null sink while still exercising sound and streamed-music
loading, updates, ownership, and shutdown. This is an opt-in native backend
selection in Rocket's pinned Raylib build; regular runs use the operating
system audio device. The local fresh-build probes confirmed both the null sink
and the normal WASAPI path with successful cleanup. Windows self-hosted visual
acceptance copies the same pinned Mesa OpenGL files used by stage0.

The historical `src.rocket_raylib` and `src.rocket_assets` modules remain
compatibility code because the showcase's 11 tests and existing tooling still
consume them. The application itself and the WP7 slice use the canonical
modules. The full public-symbol disposition and removal gate are in
`docs/ROCKET_3_5_API_INVENTORY.md`.
