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
3. Poll input, begin one frame, draw, and end that exact frame.
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
