# Rocket raylib showcase

This non-casino reference application validates Rocket 1.4 against the pinned
raylib 6.0 native library. All event loop, state, collections, input handling,
rendering decisions, asset loading, callback use, audio policy, and cleanup are
Rocket source. The C++ adapter contains no application behavior; it only maps a
primitive C ABI onto raylib and validates resource tokens.

## Build and run

From the repository root in PowerShell:

```powershell
.\dependencies\bootstrap.ps1
. .\dependencies\activate.ps1
cmake --preset windows-release
cmake --build --preset windows-release --target rocket_raylib_build
Set-Location .\examples\raylib_showcase
.\.rocketc\rocket-raylib-showcase.exe
```

The current directory must contain `assets/`. Use the arrow keys to steer the
orbiter, click to relocate it, press Space to play the procedural tone, and
press Escape to exit. If audio initialization fails, the application prints a
message and continues silently. Failure to create the graphics window is a hard
startup error.

Run the headless native validation suite with:

```powershell
.\scripts\run-raylib-validation.ps1 -Configuration Debug
.\scripts\run-raylib-validation.ps1 -Configuration Release
```

Create a distributable static Windows bundle under `out/package` with:

```powershell
.\scripts\package-raylib-showcase.ps1 -Configuration Release
```

Create an editable application scaffold with the same safe boundary and tests:

```powershell
.\scripts\new-raylib-app.ps1 -Destination .\examples\my_raylib_app -Name my_raylib_app
cmake -S .\examples\my_raylib_app -B .\out\my-raylib-app `
  -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DROCKET_ROOT="$PWD" `
  -DROCKETC="$PWD\out\build\windows-release\rocketc.exe"
cmake --build .\out\my-raylib-app
```

The generated low-level binding and native build outputs remain ignored.

## Ownership and lifetime contract

- A `Window` owns one raylib window/context. Only one may be live. Every loaded
  texture must be unloaded before the window is closed.
- `begin_frame` returns a single-use `Frame` token. Drawing requires that exact
  token; `end_frame` consumes it. Frames are synchronous and never stored.
- A `Texture` owns one GPU texture and is released exactly once with
  `unload_texture`. Copied or forged Rocket values are harmless: stale tokens
  produce `Err` and never dereference foreign memory.
- An `AudioDevice` owns the process audio device. All `Sound` values created or
  loaded through it must be unloaded before `close_audio`.
- `Sound` owns one raylib sound buffer. Playback borrows it synchronously;
  `unload_sound` releases it exactly once.
- Rocket strings are copied byte-by-byte into short-lived native UTF-8 buffers.
  The adapter borrows each buffer only for the duration of one call. Embedded
  NUL is rejected because the adapter's C-string calls cannot preserve it.
- Colors, coordinates, time, keys, and mouse values cross as frozen Phase 13
  primitives. raylib structures never cross the ABI.
- The adapter invokes callbacks synchronously and never stores them. The safe
  wrapper exposes the reviewed `apply_pulse` operation rather than a storable or
  capturing callback value.
- `src/rocket_raylib_testing.rocket` enables a deterministic no-window/no-audio
  backend for automated tests only. Production application code does not import
  it.

The API is memory-safe at the Rocket boundary, but it is deliberately explicit
about logical lifetime errors because Rocket 1.4 does not add linear types or
destructors.
