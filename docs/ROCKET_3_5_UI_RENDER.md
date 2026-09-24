# Rocket 3.5 rendered game UI (WP6)

Import `rocket.ui.render` with the existing UI modules. The renderer draws in
logical pixels into a Raylib frame or logical render target. For a resizable or
high-DPI game, create a target at the chosen logical resolution, draw the UI
while that target is active, then present it with `rocket.graphics.canvas`.
The canvas handles letterboxing, framebuffer scaling, and pointer mapping.
Refreshing the canvas after a resize keeps those mappings current. Style sizes,
padding, borders, and positions remain logical pixels; do not multiply them by
the device scale.

`rocket.ui.render` supplies these functions:

| Function | Input and behavior |
| --- | --- |
| `draw_panel(frame, panel)` | Draws a `containers.Panel` fill, optional offset shadow, and border using its `PanelStyle`. Radius is a logical pixel radius. |
| `begin_panel_content_clip(frame, panel)` / `end_panel_content_clip(scope)` | Clips child drawing to `panel.content_bounds` on the active logical target when `clip_children` is true. Scopes must be ended in order. |
| `draw_button(frame, font, button)` | Draws a `controls.Button` with its already selected style and centered label. |
| `draw_label(frame, font, text, horizontal_align, vertical_align, wrap, clip, overflow)` | Uses the existing font layout backend and `graphics.text_align_*` / `graphics.text_overflow_*` constants. The optional arguments default to left/top, no wrap, clipping, and clip overflow. |
| `draw_image(frame, image, texture_ref, fit, clip)` | Borrows a WP5 `assets.TextureRef`, applies tint and opacity, and draws with contain, cover, or stretch policy. Clipping defaults to true. |
| `draw_image_from_store(frame, image, store, fit, clip)` | Finds the texture by `image.asset_name` in a live WP5 asset store, then draws it. |
| `draw_progress_bar(frame, bounds, fraction, panel_style, fill)` | Draws a value in the inclusive 0–1 range with a styled track and fill. |

The image fit constructors are `image_contain()` (show all artwork with empty
space as needed), `image_cover()` (center crop to fill), and `image_stretch()`
(fill by changing aspect ratio). The texture remains owned by `AssetStore`;
clean up the store after the frame and before closing the window.

The button renderer accepts `controls.Button` directly. `controls.button`
already performs hit testing, disabled and focus handling, and resolves the
`styles.StyleStates` entry from its `ui.Response`. Draw its `ButtonResult.control`:

```rocket
match controls.button(ui_frame, ui.widget_id("continue"), "Continue", bounds, styles.default_styles(palette)):
    case Err(message):
        return Err(message)
    case Ok(result):
        let next_ui_frame = result.frame
        let clicked = result.response.clicked
        return render.draw_button(frame, font, result.control)
```

Use `styles.default_styles(theme.dark_theme())` for a ready-made theme, or
construct/replace `PanelStyle`, `ButtonStyle`, `TextStyle`, and `ImageStyle`
values. The stdlib renderer contains no game-specific palette. `ShadowStyle`
renders an offset silhouette at the specified color and alpha; the current
backend has no blurred shadow primitive, so `blur` does not add a Gaussian
blur. Games may still draw custom controls with `rocket.graphics.shapes` and
`rocket.raylib.safe`.

The current `rocket.ui.containers` and input model supports clipping but has
no scroll position, wheel event, or scroll interaction state. WP6 therefore
does not add a scrollable container. Scrollable lists are unnecessary for the
Scroll2Roll game HUD/menu contract and would require a separate interaction
model.

The [WP6 fixture](../tests/fixtures/rocket35_ui_render_package/) exercises
stage0, self-hosted, target-surface, native adapter, and visual output. Five
reviewed images under `tests/visual/goldens/wp6/` cover button states,
panel/card styling, label layout, image fit, and composition. The scenes draw
at 640×360 logical resolution and present into a 960×540 window through the
current framebuffer mapping, exercising the canvas scaling path. The
[`premium_card_table.rocket`](../examples/rocket3_graphics_ui/examples/premium_card_table.rocket)
example uses the canonical renderer for its reusable table panel and button,
while its playing cards, chips, and texture accent remain custom art.
