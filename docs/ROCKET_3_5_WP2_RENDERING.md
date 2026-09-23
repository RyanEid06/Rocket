# Rocket 3.5 WP2: textures, canvas, and display

The supported rendering imports are `rocket.raylib.safe`, `rocket.graphics`,
`rocket.graphics.canvas`, and `rocket.graphics.input`. Game code does not need
the historical showcase wrapper or `rocket.raylib.native`.

## Texture ownership and drawing

Open a `safe.Window`, then call `safe.load_texture(window, path)` for each PNG,
JPEG, or other Raylib-supported image. The result is a `safe.Texture` handle.
`safe.texture_width` and `safe.texture_height` query its dimensions. Release
each texture with `safe.unload_texture` before closing the window. A second
unload, stale query, or draw with an unloaded texture fails deterministically.
The window refuses to close while textures or render targets remain live.

`safe.draw_texture` draws at native size. `safe.draw_texture_scaled` applies a
uniform positive scale. `safe.draw_texture_pro` selects an atlas rectangle and
maps it to a destination rectangle with a pivot, rotation in degrees, and tint.
A negative source width or height flips that atlas region. Invalid geometry and
out-of-bounds source regions return errors. `safe.draw_render_texture_pro`
uses the same geometry model for an off-screen scene.

Texture filters are typed `safe.TextureFilter` values: `point_filter`,
`bilinear_filter`, `trilinear_filter`, and the three `anisotropic_*x_filter`
choices. Query `safe.texture_filter_supported` and
`safe.texture_max_anisotropy` before selecting an anisotropic mode.
`safe.set_texture_filter` rejects an unsupported mode; it never silently
changes the requested mode. `safe.set_texture_filter_with_fallback` explicitly
tries a second mode and returns the one selected. `safe.get_texture_filter`
reports the current mode. A render target used for virtual-canvas output can
select point or bilinear scaling through `canvas.set_output_filter`; set this
outside an active frame.

## Window loop and virtual resolution

`safe.open_window_quality` accepts resizable, high-DPI, and MSAA choices.
Use `safe.window_should_close` in a normal loop, `safe.set_target_fps` for a
Raylib-owned frame-rate policy, and `safe.frame_time` and `safe.elapsed_time`
for timing. `safe.clear_background` clears the current frame or render target.
Window and framebuffer dimensions are distinct on a high-DPI display;
`safe.window_width`/`window_height`, `safe.framebuffer_width`/
`framebuffer_height`, and `safe.dpi_scale_x`/`dpi_scale_y` expose both.

For a fixed logical scene, create a render target at the chosen logical size,
draw into it with `canvas.begin_target`/`canvas.end_target`, then call
`canvas.present_current(window, frame, target, logical_size)`. It recalculates
letterboxing or pillarboxing from the current framebuffer each frame. Use
`canvas.pointer_position(window, logical_size)` for the matching pointer
conversion after resize or DPI changes; it returns `None` in the bars. The
lower-level `canvas.from_window`, `canvas.refresh`, and `canvas.present` remain
available when an application wants to retain and refresh the mapping itself.
`canvas.set_output_filter(window, target, false)` gives crisp nearest scaling;
`true` gives smooth bilinear scaling.

`safe.set_window_size`, `safe.set_window_fullscreen`, and
`safe.set_window_borderless` control the display. `safe.window_resized`,
`safe.window_fullscreen`, and `safe.window_borderless` query state. Save a
window screenshot with `safe.save_screenshot` or a deterministic logical
scene screenshot with `canvas.save_logical_screenshot`, after ending the frame.

## Image-backed example and verification

[`examples/rocket35_texture_table`](../examples/rocket35_texture_table/README.md)
loads checked-in felt, card-atlas, and chip-atlas PNGs from its package's
`assets/` directory. Run it with that directory as the working directory, or
copy the package's assets directory alongside the launched game. The sample
draws a casino scene entirely through the canonical modules and writes
`casino-table.png`. The WP2 native acceptance test runs it twice with real
Raylib rendering and checks that the screenshots are nontrivial 800×450 PNGs
with identical hashes. The deterministic package and adapter tests cover
filters, geometry, resize/DPI pointer mapping, resource cycles, and stale
handles.
