# Building a 2D Game with Rocket 3.5

The supported game path is the bundled `rocket.*` modules. Start with the
[`scroll2roll_vertical_slice`](../examples/scroll2roll_vertical_slice/) package;
its source is a runnable example of the complete stack. The historical
`src.rocket_raylib` and `src.rocket_assets` modules remain compatibility code
for the old showcase and are not part of this path.

## Start a package

Create a `rocket.toml` with `[package]`, an entry such as
`src/main.rocket`, and the ordinary target native-library entries shown in
the vertical slice. A released Rocket SDK supplies the production adapter and
pinned Raylib libraries. From the package root, use `rocketc check .`,
`rocketc build .`, and run the resulting target executable. The package root
is also the explicit root passed to `rocket.assets`; do not depend on a
process working directory chosen by a launcher.

Typical imports are:

```rocket
import rocket.graphics
import rocket.graphics.canvas
import rocket.graphics.input
import rocket.graphics.shapes
import rocket.motion
import rocket.raylib.safe
import rocket.assets
import rocket.ui
import rocket.ui.controls
import rocket.ui.containers
import rocket.ui.render
import rocket.ui.styles
import rocket.ui.theme
```

## Window, logical canvas, and input

Open a resizable, high-DPI window with `safe.open_window_quality`; it returns
`Result[safe.Window, String]`. Use `safe.begin_frame` and `safe.end_frame` once
per visible frame. Check the result of each operation and use
`safe.abort_frame` to unwind a failed frame. `safe.window_should_close`
controls the loop. Set a display policy with `safe.set_target_fps`, or measure
your own frame loop with `safe.elapsed_time`.

Choose a fixed logical size, such as `graphics.size(960.0, 540.0)` (the same
16:9 layout as 1920×1080). Allocate its target with
`canvas.create_target(window, 960, 540)`. Each frame:

1. `canvas.begin_target(frame, target)` and draw the logical scene.
2. `canvas.end_target(scope)` before presenting.
3. `canvas.present_current(window, frame, target, logical_size)` to preserve
   aspect ratio and letterbox as needed.
4. `safe.end_frame(frame)`.

Call `canvas.from_window(window, logical_size)` for a fresh mapping before
UI hit testing. It reads the current framebuffer, so resize and DPI changes
are reflected without cached coordinate guesses. `canvas.pointer_position`
and `rocket.graphics.input.pointer_position_in_canvas` provide logical
pointer coordinates. `canvas.dpi_scale` and `safe.framebuffer_width` /
`safe.framebuffer_height` expose live output metrics. Use
`canvas.set_output_filter(window, target, true)` for smooth scaling or `false`
for nearest-neighbor pixel art. For screenshots, call
`canvas.save_logical_screenshot` or `safe.save_screenshot` outside an active
frame. The latter captures the presented physical framebuffer.

## Draw the scene

Use `rocket.graphics.shapes` for rounded panels, borders, circles, lines,
and gradients. `safe.draw_texture_pro` maps a source rectangle to a
destination rectangle with a pivot, rotation, and tint. This draws whole
textures, card-atlas regions, and rotated cards through one geometry model.
`safe.point_filter`, `safe.bilinear_filter`, and the other typed filter values
work with `safe.set_texture_filter`; query support or use
`safe.set_texture_filter_with_fallback` when requesting anisotropy.

`safe.begin_blend(frame, safe.additive_blend())` returns a scope; end it with
`safe.end_blend`. Shaders use the same checked nesting model. Query
`safe.shader_supported` before loading one. Locate named uniforms with
`safe.shader_uniform`, set their typed values, then bracket the presentation
pass with `safe.begin_shader` / `safe.end_shader`. If shaders are unavailable,
present the target without a shader. Scissor, shader, blend, and target scopes
must end in reverse order before the frame ends.

Text uses a loaded `safe.Font` and `graphics.text_style` with
`safe.draw_text_layout`; this supports measured alignment, wrapping, and
clipping. Load a real licensed font as a package asset for final art rather
than depending on a host-installed font.

## Own assets and audio

Open one `assets.AssetStore` with an explicit package root and a live window
and audio device. `assets.open_graphics` is available if the game has no audio.
Use logical names with `assets.load_texture`, `load_font`, `load_sound`,
`load_music`, and `load_shader`. Each returns a typed borrowed reference.
The store owns the native resource. `assets.borrow_*` gives a checked
`safe.*` handle while the store remains live; never unload those handles
independently. Duplicate logical names, wrong-type lookups, missing paths,
and references borrowed after cleanup return errors. Paths are relative to
the store's package root and cannot escape it.

Open audio with `safe.open_audio` before opening an audio-capable store.
`safe.play_sound` is appropriate for a short click. Stream music with
`safe.play_music`, call `safe.update_music` on **every loop iteration while
playing**, and use `safe.pause_music`, `safe.resume_music`, `safe.stop_music`,
and `safe.set_music_volume` as needed. Audio failure should be handled as a
`Result` error. The vertical slice demonstrates a looping stream alongside
sound effects.

## UI and motion

Create a `ui.Context` once. For every frame, call
`ui.begin_frame(context, window, mapping)`, then
`controls.button(ui_frame, widget_id, label, bounds, style_states)`.
`ButtonResult.response` contains hover, pressed, and click state;
`ButtonResult.control` carries the selected visual style. End the UI frame
with `ui.end_frame(result.frame)` and use its returned context on the next
frame. Draw the control with `render.draw_button` inside the logical target.
Use `containers.panel` with `render.draw_panel` for common panels. Themes and
style sets are data, so a game can supply its own palette without rebuilding
the interaction state machine.

Use `motion.pulse`, `motion.rotate`, and `motion.sample_float` with elapsed
time for card and chip animation. A `motion.policy` can respect reduced-motion
preferences. Motion affects the game's coordinates; it does not own native
resources.

## Shutdown order

End all frame and nested scopes. Unload the logical render target and close
any separately owned font. Stop playback if needed, then call
`assets.cleanup(store)` **before** closing the audio device and window.
`cleanup` invalidates every store reference. Finally call
`safe.close_audio` and `safe.close_window`, checking each result. Repeated
startup and shutdown should follow the same order.

The [API inventory](ROCKET_3_5_API_INVENTORY.md) maps every public symbol in
the historical showcase wrapper to its supported replacement or explains
why it is outside this path.
