# Rocket 3.5 WP3 casino effects scene

This Scroll2Roll-style scene loads checked-in felt, card, and chip PNGs through
`rocket.raylib.safe`. It draws the table into an 800×450 logical render
texture, uses additive blending for chip highlights, and presents that texture
through a GLSL fragment shader when `shader_supported` reports a capable
backend. It then resizes the window and recomposes the same logical canvas.
The fallback presents the texture directly when shaders are unsupported.

Run `rocketc run .` here with the Rocket native SDK installed. The scene saves
`casino-effects-before.png` and `casino-effects-resized.png` in the working
directory. CTest's `rocket35_render_effects_scene_native` builds it as a
normal package, runs it with repository-owned assets, and validates both
captures. The application imports only documented standard-library modules;
it contains no showcase wrapper or private adapter calls.

See [the WP3 rendering guide](../../docs/ROCKET_3_5_RENDER_EFFECTS.md) for the
scope, shader, uniform, post-processing, and fallback APIs.
