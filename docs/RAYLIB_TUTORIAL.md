# Build a Rocket raylib application

This tutorial uses the complete Phase 14 scaffold. Start from a pinned Rocket
developer checkout rather than downloading raylib manually:

```powershell
.\dependencies\bootstrap.ps1
.\scripts\build.ps1 -Configuration Release
.\scripts\new-raylib-app.ps1 -Destination .\examples\my_graphics_app -Name my_graphics_app
cmake -S .\examples\my_graphics_app -B .\out\my-graphics-app `
  -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DROCKET_ROOT="$PWD" `
  -DROCKETC="$PWD\out\build\windows-release\rocketc.exe"
cmake --build .\out\my-graphics-app
```

The scaffold keeps the generated module under `generated/` and native objects
under `.rocketc/`; both are ignored. Edit `src/showcase.rocket` for application
behavior. Keep `native/rocket_raylib_adapter.*` policy-only and do not add C++
application state or rendering decisions there.

The basic lifecycle is:

1. Open a window and handle `Err`.
2. Load textures/fonts and optionally open audio/create sounds.
3. Poll input, begin one frame, draw, close nested render scopes in reverse
   order, and end that exact frame. Use `abort_frame` to unwind scopes on an
   early-return cleanup path.
4. Unload sounds and close audio.
5. Unload fonts/textures and close the window.

Always release a temporary result before returning from an error branch. A
window refuses to close while textures or fonts remain live; an audio device
refuses to close while sounds remain live. These failures make cleanup bugs
visible but do not replace the application's responsibility to clean up.

Run `rocketc test` through the scaffold build or use
`scripts/run-raylib-validation.ps1`. Tests enable the deterministic backend
before creating resources, script input, stress audio and repeated startup, and
assert that every registry returns to zero. Do not enable the testing module in
production code.

## Drawing geometry safely

`src.rocket_raylib` provides the reviewed geometry boundary. It includes
rectangles (filled, outlined, rounded, and vertical/horizontal/four-corner
gradients), circles and ellipses, rings and sectors, lines, triangles, regular
polygons, and quadratic/cubic Bezier curves. All calls require the current
single-use `Frame` and return `Result[Bool, String]`.

```rocket
let outline = rocket_raylib.draw_rectangle_outline(
    frame, 32.0, 40.0, 240.0, 96.0, 2.0, rocket_raylib.white())
let curve = rocket_raylib.draw_bezier_quadratic(
    frame,
    [rocket_raylib.Point(20.0, 180.0),
     rocket_raylib.Point(160.0, 40.0),
     rocket_raylib.Point(300.0, 180.0)],
    3.0,
    rocket_raylib.accent())
```

Rocket rejects non-finite coordinates, negative dimensions/radii/thickness,
invalid ring ordering, polygon side counts below three, invalid roundness, and
malformed Bezier point counts before the native boundary. The adapter repeats
those checks and additionally rejects finite values outside raylib's float
range. All failures return `Err` without drawing. Curve points are copied
through a short-lived adapter token; raw raylib structures and pointers are
never exposed.

## Advanced textures

Use `draw_texture_pro(frame, texture, source, dest, origin, rotation, tint)`
with `rect(x, y, width, height)` and `point(x, y)` values for sprite regions,
scaling, pivot rotation in degrees, and tinting. Source dimensions may be
negative for flipping, but their absolute extents must remain inside the
texture. The adapter rejects non-finite and float-overflowing coordinates.

`set_texture_filter_with_fallback(window, texture, preferred_mode, fallback_mode)`
returns the mode actually applied. For example, request
`texture_filter_anisotropic_16x()` with `texture_filter_bilinear()` as the
explicit fallback. Native support is queried from the active graphics context;
test mode can simulate lower limits. Trilinear mode creates mipmaps on demand
and fails explicitly if mipmaps are unavailable. `get_texture_filter(texture)`
reports the last successful selection. These APIs preserve checked resource
tokens and expose no raw graphics handles.

## Virtual targets, clipping, and blending

Create a virtual-resolution layer with
`create_render_texture(window, width, height)`. Drawing into it is an explicit
scope, and compositing it back to the window uses the same checked rectangle,
pivot, rotation, and tint values as advanced textures:

```rocket
match rocket_raylib.begin_render_target(frame, layer):
    case Err(error):
        let cleaned = rocket_raylib.abort_frame(frame)
        return 1
    case Ok(target_scope):
        match rocket_raylib.begin_scissor(
            frame, rocket_raylib.rect(16.0, 16.0, 288.0, 148.0)):
            case Err(error):
                let cleaned = rocket_raylib.abort_frame(frame)
                return 1
            case Ok(clip_scope):
                let drawn = rocket_raylib.draw_circle(
                    frame, 160, 90, 24.0, rocket_raylib.accent())
                let clip_ended = rocket_raylib.end_scissor(clip_scope)
        let target_ended = rocket_raylib.end_render_target(target_scope)

let composed = rocket_raylib.draw_render_texture(
    frame, layer,
    rocket_raylib.rect(0.0, 0.0, 320.0, -180.0),
    rocket_raylib.rect(0.0, 0.0, 1280.0, 720.0),
    rocket_raylib.point(0.0, 0.0), 0.0, rocket_raylib.white())
```

The negative source height corrects raylib render-texture orientation. Target,
scissor, and blend scopes share one LIFO stack: close the most recently opened
scope first. Nested targets restore the parent framebuffer; nested scissor
rectangles intersect and then restore the parent; nested blend scopes restore
the prior reviewed mode. Available modes are alpha, additive, multiplied,
add-colors, subtract-colors, and premultiplied alpha. Custom native blend
equations are not exposed.

After ending or aborting the frame, `save_render_texture_png` can persist a
vertically corrected target image. Unload each render texture before closing
its window. Stale tokens, wrong-window use, invalid dimensions/source regions,
out-of-order scope ends, active-resource unloads, and failed image exports all
return explicit errors.

## Shaders and post-processing effects

Check `shader_supported(window)` and keep a non-shader fallback for unavailable
graphics backends. Load a vertex/fragment pair from files with `load_shader`,
or pass source strings to `load_shader_from_memory`; either stage may be empty,
but not both. Create each uniform token with its reviewed type and use only the
matching setter:

```rocket
match rocket_raylib.load_shader_from_memory(
        window, "", fragment_source):
    case Err(error):
        return 1
    case Ok(shader):
        match rocket_raylib.shader_uniform(
                shader, "tint", rocket_raylib.shader_uniform_color()):
            case Err(error):
                let released = rocket_raylib.unload_shader(shader)
                return 1
            case Ok(tint):
                let configured = rocket_raylib.set_shader_color(
                    shader, tint, rocket_raylib.accent())
        match rocket_raylib.begin_shader(frame, shader):
            case Err(error):
                let cleaned = rocket_raylib.abort_frame(frame)
                let released = rocket_raylib.unload_shader(shader)
                return 1
            case Ok(shader_scope):
                let drawn = rocket_raylib.draw_render_texture(
                    frame, layer,
                    rocket_raylib.rect(0.0, 0.0, 320.0, -180.0),
                    rocket_raylib.rect(0.0, 0.0, 1280.0, 720.0),
                    rocket_raylib.point(0.0, 0.0), 0.0,
                    rocket_raylib.white())
                let shader_ended = rocket_raylib.end_shader(shader_scope)
```

Float, Int, Vec2 (`Point`), and Color uniforms are supported. Uniform tokens
belong to one shader and expose no native location. Lookup checks the linked
program's active-uniform metadata; a mismatched requested type, sampler, array,
or other unreviewed GLSL type returns a distinct type error. Missing/unreadable
files, invalid programs, and unavailable shader backends are also distinct
errors. Shader scopes participate in the same LIFO ordering as render-target,
scissor, and blend scopes; nested
shaders restore the parent. End or abort the frame before unloading the shader,
and unload every shader before closing its window.
