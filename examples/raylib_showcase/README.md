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
  texture, render texture, and shader must be unloaded before the window is
  closed.
- `begin_frame` returns a single-use `Frame` token. Drawing requires that exact
  token; `end_frame` consumes it after every nested render/scissor/blend scope
  has closed. `abort_frame` deterministically unwinds all live scopes before
  consuming the frame, providing the cleanup path for early returns and `Result`
  propagation. Frames are synchronous and never stored.
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

## Safe geometry surface

The reviewed adapter now covers filled, outlined, rounded, and gradient
rectangles; circles, ellipses, rings, sectors, and circle gradients; lines,
triangles, regular polygons, and outlines; and Bezier lines plus quadratic and
cubic curves. Outline calls accept an explicit nonnegative thickness where the
raylib primitive supports it. Curves take `Array[Point]`; the safe wrapper copies
those values into a short-lived integer token and destroys it after the
synchronous draw, so neither a native pointer nor a raylib `Vector2` crosses the
Rocket boundary.

Float coordinates must be finite; the adapter also rejects finite values that
cannot be represented by raylib's float geometry.
Widths, heights, radii, and thicknesses are nonnegative; an outer ring radius
must be at least its inner radius; rounded-rectangle roundness is in `[0, 1]`;
regular polygons have at least three sides; quadratic Bezier point counts are
`3 + 2n`, and cubic counts are `4 + 3n`. The Rocket wrapper rejects the public
contract errors before the foreign call. The native adapter repeats those
checks, enforces the narrower float range for unsafe callers, and validates the
active frame and point-buffer tokens before drawing.

## Advanced textures and filtering

`draw_texture_pro(frame, texture, source, dest, origin, rotation, tint)` takes
value-based `Rect` and `Point` arguments. Source coordinates are nonnegative;
nonzero signed source dimensions support flipping and must stay within the
texture. Coordinates, sizes, pivot, rotation, and tint are validated before
drawing; stale frame/texture tokens and wrong-window use return errors.

Choose point, bilinear, trilinear, or anisotropic 4x/8x/16x using the
`texture_filter_*` functions. `texture_filter_supported` and
`texture_max_anisotropy` query the current backend; native anisotropy is zero
without an active window or a supported extension. Trilinear selection generates
mipmaps when needed and returns an unavailable error if they cannot be created.
Anisotropic modes use a bilinear base; switching away clears anisotropy.
`set_texture_filter_with_fallback` takes an explicit fallback and returns the
mode actually selected. `get_texture_filter` reports the last successful mode.
The deterministic backend's `set_test_anisotropy` controls capability tests.

For a real GPU regression, run `rocket_phase15_raylib_adapter_tests` with the
absolute path to `assets/orbit.ppm`. It opens a hidden window and checks that
trilinear selection does not silently degrade to bilinear. Without the argument,
the executable runs only deterministic tests suitable for headless CI.

## Render textures and scoped render state

`create_render_texture(window, width, height)` creates a checked GPU target at
an explicit virtual resolution. A `RenderTexture` belongs to the creating
window, cannot be unloaded during a frame or while targeted, and becomes a
stale token after one successful unload. `draw_render_texture` composites it
with source/destination rectangles, pivot, rotation, and tint; a negative source
height performs the raylib render-texture vertical correction. The source must
remain inside the target and a target cannot sample itself while it is active.
`save_render_texture_png` performs a vertically corrected readback after the
frame and reports path, image, and export failures through `Result`.

`begin_render_target`, `begin_scissor`, and `begin_blend` return distinct
single-use scope tokens. All three participate in one global LIFO stack, so an
out-of-order end is an explicit lifecycle error. Nested targets restore the
parent framebuffer; nested scissors use the deterministic intersection of all
active scissor rectangles and restore the parent rectangle; nested blend modes
restore the parent mode. Reviewed blend choices are alpha, additive,
multiplied, add-colors, subtract-colors, and premultiplied alpha. Custom raylib
blend equations are intentionally not exposed. These value-only scopes support
virtual canvases, UI layers, transitions, and future shader passes without
letting native structures, pointers, or backend handles cross the Rocket ABI.

## Safe shaders and effects

Call `shader_supported(window)` before selecting a shader path. Load reviewed
vertex/fragment files with `load_shader`, or sources with
`load_shader_from_memory`; at least one stage must be present. Both return a
checked `Shader` token and report unsupported backends, missing files, and
invalid programs as distinct `Result` errors. Each supplied file stage is read
successfully before compilation, so raylib cannot silently substitute a default
stage. `unload_shader` invalidates the shader and all uniform tokens created
from it, rejects active shaders, and is required before closing the window.

`shader_uniform` creates a shader-bound token with an explicit Float, Int,
Vec2 (`Point`), or Color type. The matching `set_shader_*` function validates
the requested type against the linked program's active-uniform metadata and
then validates the token and value before calling raylib. Arrays, samplers, and
other unreviewed uniform types return an explicit type error. Raw locations,
pointers, and backend handles never cross the Rocket API. `begin_shader` returns a
single-use `ShaderScope`. Shader, render-target, scissor, and blend scopes share
the global LIFO stack; nested shader scopes restore their parent, and
`abort_frame` unwinds them deterministically. This lets an effect render inside
a `RenderTexture` pass while preserving the safe ownership boundary.

## Window and display quality

`open_window_quality` configures resizable, high-DPI, and MSAA4x flags before
raylib creates the window. The flags are requests: use the logical-size,
framebuffer-size, and DPI-scale queries for the live values. Monitor selection
and information, exclusive fullscreen, borderless fullscreen, and checked PNG
screenshots are available through value-only functions. Each optional feature
has an explicit capability query and returns an unavailable error if the target
cannot perform it.
Because raylib 6.0 adds process-lifetime pre-window flags, native reopen may keep
the same quality flags or add more; attempting to remove an already-used flag
returns an explicit unavailable error.

Display transitions increment `window_display_revision`; viewport and
virtual-canvas users should recompute cached mapping when the revision changes.
Window metrics remain cached for the duration of a frame, including nested
render-target scopes, so a temporary target cannot masquerade as the window
framebuffer.
Mouse input remains in logical coordinates, while `mouse_framebuffer_x` and
`mouse_framebuffer_y` convert with the current physical-to-logical ratio.
Exclusive and borderless fullscreen never remain enabled together. Screenshots
must be requested outside an active frame and capture the physical framebuffer.
