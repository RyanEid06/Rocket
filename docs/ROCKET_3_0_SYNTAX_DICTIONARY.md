# Rocket 3.0 Syntax and API Dictionary

> **Current development reference:** This dictionary describes the accepted Rocket
> 3 Wave B surface on the completed Rocket 2.1 baseline. Wave C is ready but
> Rocket 3.0 is not a final release contract. Current repository status is in
> DOCUMENTATION_STATUS.md, and packet scope is in the Rocket 3 requirements and
> implementation plan.

The normative grammar and semantics remain in docs/SPEC.md, and stable library
signatures remain in docs/STDLIB.md. The older 1.0-1.8 and 2.0 dictionaries are
historical compatibility references; they are not the current Rocket 3 surface.

## 1. Callable syntax

Rocket 3 Wave B keeps positional calls and adds named operands, declaration
defaults, labeled enum payloads, and parity across the supported callable kinds.

### Named arguments

A named operand uses the declared parameter name followed by a colon:

~~~rocket
fn add(left: Int, right: Int) -> Int:
    return left + right

let answer = add(right: 22, left: 20)
~~~

Positional operands may precede named operands, but no positional operand may
follow the first named operand. Named operands may be reordered. Unknown,
duplicate, missing, conflicting, or wrong-typed operands are compile errors
using the stable R4002/R4005 categories.

The supported named-call targets are ordinary functions and methods, extern
functions and struct constructors, closure values and immediately invoked
lambdas, compiler built-ins and registered standard-library intrinsics, and enum
variants whose payload entries are all explicitly labeled.

Public function parameter names, compiler-owned callable names, and public enum
payload labels are source-compatibility commitments. Closure parameter names
are local source contracts and are not exported as runtime ABI symbols.

### Default parameters

Ordinary functions and methods may default a trailing run of parameters:

~~~rocket
fn greet(name: String, punctuation: String = "!") -> String:
    return name + punctuation

let short = greet("Ada")
let explicit = greet(name: "Ada", punctuation: ".")
~~~

Every required parameter precedes every defaulted parameter. Defaults are
type-checked in declaration context, may reference earlier parameters and
declaration-scope names, and participate in generic specialization. An explicit
positional or named operand overrides its default.

The callee or receiver is evaluated first, written operands are evaluated
left-to-right, omitted defaults are evaluated in declaration order, and the
normalized declaration-order operands are then lowered to the unchanged runtime
and native ABI v1.

Defaults remain unsupported for lambda and callback parameters, trait method
declarations, enum payloads, extern functions, and struct fields. Variadic
arguments are not part of Wave B.

### Labeled enum payloads

A variant labels every payload entry or none:

~~~rocket
enum Message:
    Value(amount: Int, label: String)
    Empty

let message = Message.Value(label: "score", amount: 42)
~~~

Labeled variants accept positional, all-named, or positional-then-named
construction. Legacy anonymous payload declarations remain positional-only.
Labeled and anonymous payload entries may not be mixed in one variant.

### Compatibility and diagnostics

HIR records source evaluation order and normalizes operands before MIR and
backend lowering. Formatter, LSP signature help, generated documentation,
stage0, and the self-hosted compiler use the same names and defaults. The
runtime ABI v1 and backend calling convention do not change. R4002 reports
unknown names with deterministic typo suggestions; R4005 reports arity,
missing/duplicate/conflicting operands, unsupported defaults, and invalid
call forms.

## 2. Compiler-owned scalar additions

std.math is a compiler-owned module with no graphics policy. Wave B exposes:

- pi(), tau(), and e();
- Float abs, min, max, clamp, and sign;
- Int abs_int, min_int, max_int, clamp_int, and sign_int;
- floor, ceil, round, trunc, fract, sqrt, pow, exp, log, log10, sin, cos,
  tan, asin, acos, atan, and atan2;
- radians, degrees, lerp, inverse_lerp, remap, smoothstep, and smootherstep;
  and
- approach and move_towards.

Float results follow the documented IEEE-754 behavior, including NaN,
infinity, signed zero, exact interpolation endpoints, and non-clamped
intermediate progress. Int absolute-value and range behavior is explicit in
the Rocket 3 requirements.

## 3. Bundled Rocket source modules

These modules are ordinary Rocket source under stdlib/rocket, not privileged
syntax or an alternate runtime.

### rocket.motion

Public value types are Vec2, Color, Easing, MotionPolicy, FloatTween, Vec2Tween,
ColorTween, FloatSample, Vec2Sample, ColorSample, Timeline, and TimelineSample.

Constructors and policies include vec2, color, policy, linear_easing, the
in_/out_/in_out_ Quad, Cubic, Quart, Sine, Back, Bounce, and Elastic easing
constructors, float_tween, vec2_tween, color_tween, delay, sequence, parallel,
repeat, yoyo, and cancel. Use apply_easing, sample_float, sample_vec2,
sample_color, and sample_timeline to obtain value-owned samples. Convenience
constructors are fade, move, slide, scale, rotate, pulse, and color_transition.
The pure easing evaluators are linear, in_quad/out_quad/in_out_quad,
in_cubic/out_cubic/in_out_cubic, in_quart/out_quart/in_out_quart,
in_sine/out_sine/in_out_sine, in_back/out_back/in_out_back,
in_bounce/out_bounce/in_out_bounce, and in_elastic/out_elastic/in_out_elastic.
The named constructor forms are linear_easing, in_quad_easing,
out_quad_easing, in_out_quad_easing, in_cubic_easing, out_cubic_easing,
in_out_cubic_easing, in_quart_easing, out_quart_easing, in_out_quart_easing,
in_sine_easing, out_sine_easing, in_out_sine_easing, in_back_easing,
out_back_easing, in_out_back_easing, in_bounce_easing, out_bounce_easing,
in_out_bounce_easing, in_elastic_easing, out_elastic_easing, and
in_out_elastic_easing.
Endpoints are exact, intermediate progress is not implicitly clamped, and
reduced-motion policy snaps nonessential work to its final state.

### rocket.graphics

Public value types are Vec2, Size, Rect, Transform2D, Color, TextStyle,
TextMetrics, TextLayout, WindowConfig, and VirtualCanvas.

Core constructors are vec2, size, rect, transform2d, text_style, text_metrics,
text_layout, window_config, and virtual_canvas. text_style defaults
letter_spacing to 0.0 and line_height to 1.0; window_config defaults high_dpi,
msaa4x, and resizable to true.
Text alignment helpers are text_align_left, text_align_center,
text_align_right, text_align_top, text_align_middle, text_align_baseline, and
text_align_bottom; overflow helpers are text_overflow_clip and
text_overflow_ellipsis. `text_style_is_valid`, `text_layout_is_valid`, and
`virtual_canvas_is_valid` are pure validity predicates.

Geometry/color helpers include vector arithmetic, finite/validity predicates,
rectangle normalization, translation, scaling, containment/intersection,
transform_point, RGB/RGBA/HSV/hex conversion, alpha changes, mixing,
interpolation, lightening, darkening, saturation changes, and pure
point_in_rect/point_in_circle hit testing.
The corresponding helpers are vec2_add, vec2_subtract, vec2_scale,
vec2_is_finite, size_is_valid, rect_is_finite, rect_has_nonnegative_size,
rect_normalized, rect_translate, rect_scale, rect_contains, rect_intersects,
rect_intersection, transform_point, color_from_rgb, color_from_rgba,
color_with_alpha, color_from_hex, color_from_hsv, color_mix, color_lerp,
color_lighten, color_darken, color_saturate, color_desaturate, point_in_rect,
and point_in_circle.

fit_virtual_canvas computes an aspect-preserving fitted viewport and
virtual_canvas_logical_to_physical, virtual_canvas_physical_to_logical, and
virtual_canvas_rect_to_physical share one scale/viewport rule. Physical points
outside the viewport and the exclusive right/bottom edge return `None()`.

### rocket.graphics.shapes

Drawing functions accept typed rocket.graphics values and return
Result[Bool, String]:

- draw_rect, draw_rect_outline, draw_rounded_rect, and
  draw_rounded_rect_outline;
- draw_circle, draw_circle_outline, draw_ellipse, draw_ring, draw_ring_sector,
  and draw_sector;
- draw_line, draw_thick_line, draw_triangle, draw_triangle_outline,
  draw_polygon, draw_polygon_outline, and draw_bezier; and
- draw_gradient_rect and draw_gradient_circle.

Thickness defaults to 1.0, roundness to 0.25, polygon rotation to 0.0, and
rectangle gradients to vertical. Invalid or non-finite geometry returns Err
before the native boundary.

### rocket.graphics.input

pointer_position(window) reports framebuffer coordinates. pointer_down,
pointer_pressed, and pointer_released default button to 0.
pointer_position_in_canvas converts a physical pointer through a VirtualCanvas
and returns Option.None for invalid mappings or letterbox/pillarbox space.

### rocket.graphics.canvas

framebuffer_size, from_window, refresh, dpi_scale, and display_revision connect
a VirtualCanvas to live display state. create_target, begin_target, end_target,
and unload_target manage logical-size render targets. present composites a
target into the fitted viewport; begin_clip/end_clip map logical clipping to
checked scissor scopes; save_logical_screenshot exports logical content.
resize, set_fullscreen, and set_borderless request a checked transition and
return a recomputed mapping.

### rocket.ui

WidgetId, Context, UiFrame, Response, and Interaction are public value types.
new_context(namespace, capacity = 4096) creates a bounded namespaced store.
begin_frame snapshots pointer and keyboard input once; end_frame commits the
frame and evicts unseen IDs.

widget_id and child_id use deterministic length-prefixed identity composition.
register_widget rejects duplicate IDs and capacity exhaustion. interact handles
half-open hit testing, pointer press/hold/release, keyboard activation,
disabled widgets, and modal capture. request_focus, activate_modal,
clear_modal, enter_modal, and exit_modal make ownership explicit.
response_is_current and response_is_current_context reject stale responses.
The activation_pressed, cancel_pressed, focus_next_pressed, and
navigation_left_pressed/navigation_right_pressed/navigation_up_pressed/
navigation_down_pressed helpers expose the frame's keyboard snapshot; active_is,
focused_is, and modal_is query committed context state. Concrete controls remain
later Wave C work.

### rocket.ui.layout

Sizing, Insets, HorizontalAlignment, VerticalAlignment, Anchor, SafeArea, and
LayoutItem are public values. Sizing constructors are fixed, fill, content,
and percent; alignment constructors cover start, center, and end; anchors
cover all nine standard positions.

insets, insets_all, no_safe_area, safe_area, and layout_item build explicit
layout policies. resolve_size, inset_rect, content_rect, row, column, grid,
stack, and anchor_rect return checked rectangles or Result errors. Percentages
are in [0, 1]; fill distribution charges gaps, margins, padding, safe areas,
and fixed/content/percentage sizes deterministically.
The alignment constructors are horizontal_start, horizontal_center,
horizontal_end, vertical_start, vertical_center, and vertical_end. The anchor
constructors are top_left, top_center, top_right, center_left, center,
center_right, bottom_left, bottom_center, and bottom_right.

### rocket.ui.theme

ColorTokens, SpacingTokens, RadiusTokens, TypographyTokens, MotionTokens, and
Theme are public value types. color_tokens builds an explicit semantic palette;
spacing_tokens, radius_tokens, typography_tokens, and motion_tokens provide
named/default token scales; theme groups them; dark_theme provides the bundled
dark/table palette; and theme_is_valid checks public values constructed directly.

Semantic colors cover background, surface, raised surface, table treatment,
action/hover/press, primary/muted text, success/warning/error, border, and focus.
Numeric validation rejects non-finite or negative values, typography sizes must
be positive, and color channels remain in [0, 1].

### rocket.ui.styles

ControlState, TextStyle, BorderStyle, ShadowStyle, ImageStyle, PanelStyle,
ButtonStyle, StyleSet, and StyleStates are public value types. Constructor
functions expose source-stable names and defaults; default_styles maps a Theme to
all five control states. The validation predicates cover colors, dimensions,
text sizes, offsets, and opacity.

resolve(states, state) uses the fixed priority disabled, pressed, hovered,
focused, then normal. These modules contain no controls, drawing, native
resources, inheritance, containers, or retained caches.

### rocket.raylib.safe

This is the narrow reviewed native boundary over the raylib adapter. It owns
Window, Frame, RenderTexture, RenderTargetScope, and ScissorScope tokens,
exposes checked lifecycle, geometry, input, render-target, scissor, DPI,
resize/fullscreen/borderless, and logical screenshot calls, and translates
native status values to Result. It exposes no raylib pointers or structures to
safe Rocket callers. Its compatibility entry points include open_window,
close_window, begin_frame/end_frame/abort_frame, the legacy color-based
draw_* primitives, pointer/key queries, framebuffer/DPI queries,
set_window_size/set_window_fullscreen/set_window_borderless,
create_render_texture/render_texture_width/render_texture_height,
draw_render_texture/draw_render_texture_framebuffer,
save_render_texture_png, and begin/end scissor and render-target scopes.
The exact query names are pointer_pressed, pointer_down, pointer_released,
key_pressed, key_down, pointer_framebuffer_x, pointer_framebuffer_y,
framebuffer_width, framebuffer_height, dpi_scale_x, dpi_scale_y, and
display_revision. Resource cleanup and scope names include unload_render_texture,
begin_render_target, end_render_target, begin_scissor,
begin_framebuffer_scissor, and end_scissor.
Normal application code should prefer the typed graphics modules above.

## 4. Typed asset-store reference package

The accepted Wave B typed asset store is currently package-local, not a
standard-library import:

~~~text
examples/raylib_showcase/src/rocket_assets.rocket
import src.rocket_assets
~~~

The public value types are AssetStore, TextureRef, FontRef, SoundRef, MusicRef,
and ShaderRef. The public operations are:

- open(window, audio, package_root = ".") and cleanup(store);
- load_texture, load_font, load_sound, load_music, and load_shader;
- typed texture, font, sound, music, and shader lookups; and
- borrow_texture, borrow_font, borrow_sound, borrow_music, and borrow_shader.

Paths remain inside the canonical package root; absolute paths, traversal,
symlink escapes, duplicate logical names, wrong-type lookup, missing assets,
stale references, and unsafe cleanup order produce recoverable errors. Cleanup
is explicit and idempotent, and invalidates references after success.

The Rocket 3 design's rocket.assets / stdlib/rocket/assets names are an
intended future namespace, not an import that exists in this checkout. Use the
reference package path above until a later packet promotes a standard module.

## 5. Wave C boundary

Wave C starts from the published master status commit and continues Eddy's
WP25 -> WP26 -> WP27 -> WP30 queue:

- WP25: public themes and styles;
- WP26: controls;
- WP27: containers and transient UI; and
- WP30: bounded state and cache integration.

WP29 unified error/lifetime hardening, WP31 performance budgets, WP32 visual
regression, and WP34/F29 final cross-target acceptance remain later gates. This
dictionary therefore documents the accepted Wave B surface, not a claim that
Rocket 3.0 is complete.
