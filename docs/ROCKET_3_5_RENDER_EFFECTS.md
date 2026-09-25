# Rocket 3.5 rendering effects (WP3)

Import `rocket.raylib.safe`, `rocket.graphics`, and `rocket.graphics.canvas` for
the supported rendering path. The working, image-backed scene is
[`examples/rocket35_render_effects`](../examples/rocket35_render_effects/README.md).
It uses only bundled public modules.

## Blend scopes

`safe.begin_blend(frame, mode)` returns `Result[BlendScope, String]`. Modes are
typed values returned by `alpha_blend()`, `additive_blend()`,
`multiplied_blend()`, `premultiplied_alpha_blend()`, `add_colors_blend()`, and
`subtract_colors_blend()`. End each successful scope with `safe.end_blend(scope)`
in reverse nesting order. The adapter shares one stack with render-target,
scissor, and shader scopes. Ending a parent scope while a child is active
returns `raylib: invalid lifecycle state`; using an ended or aborted scope
returns `raylib: stale or already released handle`. A frame cannot end until
all its scopes end. An aborted frame invalidates its scopes.

```rocket
match safe.begin_blend(frame, safe.additive_blend()):
    case Err(error):
        print(error)
    case Ok(scope):
        let drawn = safe.draw_texture(frame, chip, 100, 100, safe.color(255, 255, 255, 180))
        let ended = safe.end_blend(scope)
```

## Shader loading and uniforms

Call `safe.shader_supported(window)` before loading. It returns a `Result` so
stale windows remain distinguishable from valid windows without shader support.
On an unsupported backend, draw the render texture without a shader.

`safe.load_shader(window, vertex_path, fragment_path)` loads files;
`safe.load_shader_from_source(window, vertex_source, fragment_source)` compiles
strings. An empty vertex or fragment argument selects Raylib's default stage;
both cannot be empty. Use `safe.unload_shader(shader)` after all shader scopes
have ended and before closing the owning window. File paths follow the
application's normal asset path policy; pass a package-root-resolved path when
the working directory may differ.

Lookup is by name and expected type:

```rocket
match safe.shader_uniform(shader, "warmth", safe.float_uniform()):
    case Err(error):
        print(error)
    case Ok(uniform):
        let configured = safe.set_shader_float(shader, uniform, 0.35)
```

The other type constructors are `int_uniform()`, `vec2_uniform()`, and
`color_uniform()`. Setters are `set_shader_int`, `set_shader_vec2` (takes
`graphics.Vec2`), and `set_shader_color` (takes `safe.Color`). Wrong types,
missing names, stale handles, nonfinite floats, and invalid color channels
return explicit errors. A uniform token belongs to its shader and is invalid
after that shader unloads. Shader compilation errors include the backend's
diagnostic text when available, including compiler messages from a real OpenGL
driver. The adapter's `rlv_shader_diagnostic_*` symbols are internal and are
not application APIs.

Raylib 6.0 has a setter for its process-global trace callback but no getter.
The adapter installs its diagnostic callback only around the synchronous
`LoadShaderFromMemory` call and then clears it. An application that installs
its own Raylib callback outside Rocket cannot have that callback restored by
Rocket shader loading; concurrent external callback installation is unsupported.

## Post-processing a logical canvas

Keep the render texture at the chosen logical resolution. Recompute canvas
placement for each presentation, especially after a window resize or DPI move:

```rocket
match canvas.begin_target(frame, target):
    case Err(error):
        print(error)
    case Ok(target_scope):
        let drawn = safe.draw_texture(frame, artwork, 0, 0, safe.color(255, 255, 255))
        let ended = canvas.end_target(target_scope)

# Query once at setup, load the shader only when supported, and carry the
# resulting Option[safe.Shader] into the frame loop.
match shader:
    case None:
        let shown = canvas.present_current(window, frame, target, graphics.size(800.0, 450.0))
    case Some(effect):
        match safe.begin_shader(frame, effect):
            case Err(error):
                print(error)
            case Ok(scope):
                let shown = canvas.present_current(window, frame, target, graphics.size(800.0, 450.0))
                let ended = safe.end_shader(scope)
```

`canvas.present_current` reads the current framebuffer size and computes fresh
letterboxing, so a fixed logical target composes correctly at a new window
size. Apply `canvas.set_output_filter(window, target, true)` for smooth scaling,
or `false` for point sampling. The WP3 scene compares effect-off and effect-on
captures of the same scene against the tint formula, then saves a capture after
resize. The fixture also verifies this sequence with a deterministic backend
and checks the no-shader fallback.
On a headless display that does not change its physical framebuffer after a
resize request, the native scene reports that limitation and still verifies
the shader composition path; the deterministic fixture tests resized
framebuffer mapping on every platform.
