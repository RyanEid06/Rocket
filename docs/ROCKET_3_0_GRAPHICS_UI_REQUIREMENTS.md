# Rocket 3.0 Graphics, UI, and Language-Ergonomics Requirements

**Status:** Approved requirements baseline for integrated Rocket 3.0 development
on the accepted Rocket 2.1 portability baseline.

**Release identity:** The work defined here targets Rocket 3.0.0. Its predecessor
is the accepted Rocket 2.1 portability baseline at
`19596db860d4105d2226c98be2693edc5632aaf0`.

**Companion plan:** `docs/ROCKET_3_0_GRAPHICS_UI_IMPLEMENTATION_PLAN.md`

## 1. Purpose

Rocket 3.0 makes polished, responsive, visually faithful 2D game interfaces a
first-class Rocket capability. It combines additive language ergonomics,
complete math and motion foundations, a safe production raylib surface, typed
graphics and game-UI libraries, asset ownership, and deterministic visual
testing.

The target is not merely to expose additional raylib symbols. The target is a
coherent stack:

```text
Rocket application
        |
        v
    rocket.ui
        |
        v
 rocket.graphics
        |
        v
rocket.raylib.safe
        |
        v
   raylib 6.0
```

Supporting modules are `std.math`, `rocket.motion`, `rocket.assets`, and the
visual-regression toolchain.

## 2. Normative language and interpretation

The words **must**, **must not**, **should**, and **may** are normative. In this
document, **retain** means retain a requirement from the approved provisional
design; it never means that the capability is assumed to exist in the current
repository.

Every requirement is classified during implementation as one of:

- `EXISTING`: current implementation and evidence already satisfy it.
- `PARTIAL`: useful implementation exists but one or more required behaviors,
  platforms, tests, or documents are absent.
- `MISSING`: no qualifying implementation exists.
- `OBSOLETE`: the repository has a better accepted mechanism and the reason for
  replacement is recorded.

No broad statement such as "graphics implemented" satisfies an atomic
requirement. Every atomic ID must map to an implementation location, tests,
documentation, supported targets, and acceptance evidence.

## 3. Governance, compatibility, and isolation

### Release and language governance

- `R3-GOV-001`: Rocket 3.0 is authorized to add backward-compatible
  named-argument and default-argument grammar.
- `R3-GOV-002`: Every valid program accepted by the Rocket 2.1 baseline must
  remain valid with the same observable behavior unless a separately approved
  Rocket 3.0 decision identifies an unavoidable incompatibility.
- `R3-GOV-003`: Runtime ABI v1 remains the default hard boundary. Any proposed
  ABI change is blocked until the owner approves a separate decision describing
  migration, compatibility, packaging, and bootstrap consequences.
- `R3-GOV-004`: The permanent C++20 compiler remains reproducible `stage0`; the
  Rocket-written compiler must preserve behavior and deterministic bootstrap.
- `R3-GOV-005`: raylib 6.0 remains the primary renderer. Dear ImGui, raygui, and
  NanoVG are not the production game-UI stack. raygui may be used only for
  optional developer tooling after the Rocket-native UI is accepted.
- `R3-GOV-006`: Required dependencies must remain local, free, offline-capable,
  and free of API keys, subscriptions, cloud execution, or per-use charges.
- `R3-GOV-007`: Scroll2Roll is not migrated in Rocket 3.0. It may be inspected
  read-only as design evidence; its migration is a separate project after the
  Rocket 3.0 release. Only that later migration may measure the practical target
  of approximately 95 percent or greater fidelity against approved Scroll2Roll
  references; Rocket's internal showcase is not evidence of that result.

### Integrated development and delivery discipline

- `R3-ISO-001`: Rocket 3.0 work occurs on `master` in the main Rocket checkout,
  which contains the accepted Rocket 2.1 baseline. Each successful packet
  commits and pushes its code, evidence, and rotated handoff prompt.
- `R3-ISO-002`: Phase 19 is complete and no longer restricts Rocket 3.0 file
  eligibility. Compiler, runtime, standard-library, native adapter, SDK, build,
  package, and release files may be changed when they are within the named
  packet's scope and receive the required validation.
- `R3-ISO-003`: Generated packet output belongs below
  `out/rocket3-provisional` inside the main Rocket checkout and is never tracked.
- `R3-ISO-004`: `GREEN`, `YELLOW`, and `RED` describe engineering risk rather
  than Phase 19 ownership: `GREEN` is a self-contained change, `YELLOW` crosses
  an existing subsystem and requires focused regression coverage, and `RED`
  changes ABI, compiler/bootstrap, supported-target, package, or release
  behavior and requires its packet's full acceptance evidence.
- `R3-ISO-005`: Internal foundation code begins `PROVISIONAL`.
  `INTEGRATION-READY` means audited against the accepted Rocket 2.1 baseline;
  `PUBLIC` means its Rocket 3.0 API gate is accepted; `ACCEPTED` means all
  release gates pass.
- `R3-ISO-006`: WP09 audits the rebased foundation commits against current
  architecture before public promotion. Code is retained only when its tests,
  interfaces, and ownership model remain valid.
- `R3-ISO-007`: Confirm no Rocket build/test process from this task remains,
  run commands sequentially, do not automatically retry timeouts, and stop and
  report a task process that exceeds 4 GiB or continues growing rapidly.

## 4. Ownership and layer boundaries

- `std.math` owns general scalar mathematics and contains no graphics policy.
- `rocket.motion` owns easing, typed tweens, timelines, and reduced-motion
  behavior; it does not mutate arbitrary application objects through reflection.
- `rocket.raylib.safe` is the only production layer that crosses the reviewed
  native raylib adapter. It owns resource tokens, scope validation, and native
  error translation.
- `rocket.graphics` owns Rocket-facing geometry, color, drawing, typography,
  input mapping, and virtual-canvas abstractions. Normal application code does
  not call raw generated bindings.
- `rocket.ui` owns immediate-mode frame lifecycle, stable IDs, interaction,
  layout, themes, style resolution, controls, containers, and bounded retained
  state.
- `rocket.assets` owns cached typed resource loading, lookup, borrowing, and
  deterministic cleanup.
- Visual-comparison tooling owns golden-image comparison and failure artifacts;
  it is not linked into production applications.

## 5. Feature catalog

The catalog contains 30 independently traceable feature groups. Public names
shown here describe the required developer-facing concepts. Exact signatures
are frozen only at the designated public API gate.

### F01 - Release governance and compatibility

- `R3-F01-001`: Record a Rocket 3.0 decision authorizing additive grammar while
  preserving valid predecessor source and runtime ABI v1 by default.
- `R3-F01-002`: Record exact predecessor tag, commit, target matrix, compiler
  versions, raylib version, artifact hashes, and worktree state at integration.
- `R3-F01-003`: Preserve every existing hardening, cache, relocation,
  provenance, checksum, signing-where-present, and reproducibility gate.
- `R3-F01-004`: Every milestone ends with focused evidence and a checkpoint
  commit; historical results are never reported as current evidence.

### F02 - Named arguments

- `R3-F02-001`: Positional-only calls remain valid.
- `R3-F02-002`: All-named calls and positional arguments followed by named
  arguments are valid; positional arguments after the first named argument are
  compile errors.
- `R3-F02-003`: Named arguments may be reordered and bind to declared parameter
  names.
- `R3-F02-004`: Unknown, duplicate, missing, positional/named-conflicting, and
  wrong-typed arguments produce stable compile-time diagnostics; typo
  suggestions use existing diagnostic support.
- `R3-F02-005`: Initial supported callables are direct functions, methods,
  extern functions, and struct constructors. Every excluded callable category
  is documented and tested.
- `R3-F02-006`: Public parameter names are source-compatibility commitments and
  are represented in cross-module/interface metadata used by calls.
- `R3-F02-007`: Stage0, the self-hosted compiler, formatter, LSP, documentation
  generator, and every currently supported editor integration agree on syntax,
  semantics, formatting, and diagnostics.
- `R3-F02-008`: After the default-argument foundation is complete,
  standard-library intrinsics and compiler built-ins accept named arguments
  through explicit, stable compiler-owned parameter names; their names are
  surfaced consistently in signature help and generated documentation.
- `R3-F02-009`: Closure values and immediately invoked lambdas accept named
  arguments using their declared parameter names. Closure parameter names are
  local source contracts and are not promoted to exported runtime ABI symbols.
- `R3-F02-010`: Enum variants may declare labeled payloads such as
  `Value(amount: Int, label: String)`. Labeled variants accept positional,
  named, and positional-then-named construction; legacy anonymous payloads
  remain valid and positional-only, and labeled and anonymous payload entries
  may not be mixed within one variant.
- `R3-F02-011`: Public enum-payload labels are source-compatibility
  commitments represented in cross-module metadata. Unknown, duplicate,
  missing, conflicting, and wrong-typed enum-constructor arguments use the
  same stable diagnostics and typo suggestions as other named calls.
- `R3-F02-012`: Completing callable parity preserves receiver/callee and
  written-argument evaluation order, normalizes before MIR/backend calling
  conventions, changes neither runtime ABI v1 nor backend ABI, and is verified
  in stage0, self-host, formatter, LSP, documentation, editor, compatibility,
  and deterministic-bootstrap evidence.

### F03 - Default arguments

- `R3-F03-001`: Ordinary functions and methods may declare defaults; required
  parameters precede defaulted parameters.
- `R3-F03-002`: Defaults type-check in declaration context, may reference legal
  earlier parameters and declaration-scope functions/constants, and behave
  predictably under generic specialization.
- `R3-F03-003`: Evaluation order is receiver/callee, written arguments
  left-to-right, omitted defaults in parameter order, then normalized call.
- `R3-F03-004`: Explicit arguments override defaults and omitted required
  arguments remain compile errors.
- `R3-F03-005`: Initial exclusions are lambda defaults, callback defaults,
  trait-declaration defaults, enum-payload defaults, extern defaults, and
  struct-field defaults.
- `R3-F03-006`: Named/default calls normalize before lower-level calling
  conventions so the runtime ABI and backend ABI remain unchanged.
- `R3-F03-007`: Side effects, source lookup, diagnostic locations, cross-module
  defaults, and API-versioning consequences are specified and tested in both
  compilers and deterministic bootstrap.

**WP11 acceptance (2026-08-30):** `R3-F03-001` through `R3-F03-007` are GREEN
for the scoped ordinary-function and method surface. Stage0 and self-host carry
declaration defaults through HIR/interface metadata and cross-module calls,
type-check in declaration context with earlier-parameter and generic behavior,
evaluate written arguments before omitted defaults, and normalize before the
unchanged runtime/backend ABI. Stable negative coverage retains every
`R3-F03-005` exclusion. Formatter, LSP, package documentation, and language
specifications agree on the syntax and source/API-versioning contract. Fresh
Debug and Release suites passed `226/226`, the LLVM-disabled predecessor gate
passed `18/18`, and deterministic stage2/stage3 IR matched at SHA-256
`4aa87fe969ff42d8806c938a24106d2a14bad91a76f23cbda063ae27ed8eb210`.

### F04 - `std.math`

- `R3-F04-001`: Provide `pi`, `tau`, and `e` constants or zero-argument
  functions following final Rocket standard-module conventions.
- `R3-F04-002`: Provide scalar `abs`, `min`, `max`, `clamp`, and `sign`, with
  explicit Int variants where Rocket cannot overload safely.
- `R3-F04-003`: Provide `floor`, `ceil`, `round`, `trunc`, and `fract`.
- `R3-F04-004`: Provide `sqrt`, `pow`, `exp`, `log`, and `log10`.
- `R3-F04-005`: Provide `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, and
  `atan2`.
- `R3-F04-006`: Provide radians/degrees conversion, `lerp`, `inverse_lerp`,
  `remap`, `smoothstep`, `smootherstep`, `approach`, and `move_towards`.
- `R3-F04-007`: Specify Float domain results, NaN/infinity behavior supported
  by Rocket, invalid and zero-width ranges, Int overflow behavior, and exact
  endpoint expectations.
- `R3-F04-008`: The implementations behave consistently on every supported
  target within documented floating-point tolerances.

### F05 - Easing

- `R3-F05-001`: Provide the conceptual family `Linear`, `InQuad`, `OutQuad`,
  `InOutQuad`, `InCubic`, `OutCubic`, `InOutCubic`, `InQuart`, `OutQuart`,
  `InOutQuart`, `InSine`, `OutSine`, `InOutSine`, `InBack`, `OutBack`,
  `InOutBack`, `InBounce`, `OutBounce`, `InOutBounce`, `InElastic`,
  `OutElastic`, and `InOutElastic`. Final spelling follows the Rocket 3.0
  naming gate without reducing the family.
- `R3-F05-002`: Easing functions accept `Float` progress, preserve exact
  endpoints at 0 and 1, and do not implicitly clamp intermediate values.
- `R3-F05-003`: Back and Elastic retain intentional overshoot; Bounce has
  deterministic piecewise behavior.
- `R3-F05-004`: Tests cover endpoints, representative interior values,
  symmetry where applicable, overshoot, and target consistency.

### F06 - Tweens, timelines, convenience motion, and reduced motion

- `R3-F06-001`: Typed tween foundations support `Float`, `Vec2`, and `Color`.
- `R3-F06-002`: Timelines support delay, sequence, parallel, repeat, and yoyo
  with deterministic completion and cancellation behavior.
- `R3-F06-003`: Convenience constructors cover fade, move, slide, scale,
  rotate, pulse, and color transition and compile to ordinary tween/timeline
  primitives.
- `R3-F06-004`: Application state remains explicitly owned; no reflection-based
  arbitrary object mutation is introduced.
- `R3-F06-005`: Reduced-motion mode snaps nonessential motion to the final state
  and permits only specifically documented essential reduced transitions.
- `R3-F06-006`: Zero/negative duration, repeat counts, large time deltas, yoyo
  boundaries, and completed/cancelled timelines have defined behavior.

### F07 - Safe raylib geometry backend

- `R3-F07-001`: Safely expose filled/outlined rectangles, rounded rectangles,
  configurable outline thickness, and rectangle gradients.
- `R3-F07-002`: Safely expose circles, outlined circles, ellipses, rings, ring
  sectors, circle sectors, and circle gradients.
- `R3-F07-003`: Safely expose lines, thick lines, triangles,
  `triangle_outline`, polygons, `polygon_outline`, and Bezier lines/curves.
- `R3-F07-004`: Geometry validation rejects non-finite values and defined
  negative dimensions before crossing the native boundary.
- `R3-F07-005`: The backend is a reviewed portable adapter surface, not raw
  structure or pointer exposure.

### F08 - Advanced textures and filtering

- `R3-F08-001`: Advanced texture drawing supports source rectangle,
  destination rectangle, pivot/origin, rotation, and tint.
- `R3-F08-002`: Filtering supports point, bilinear, trilinear, and raylib/
  platform-supported anisotropic modes.
- `R3-F08-003`: Unsupported filter modes have explicit capability/fallback
  behavior and never silently select a materially different mode.
- `R3-F08-004`: Invalid or stale texture tokens, invalid source regions, and
  wrong-window use fail through the defined error contract.

### F09 - Render targets, clipping, and blending

- `R3-F09-001`: Checked render textures support virtual resolution,
  compositing, screenshots, UI layering, transitions, and shader passes.
- `R3-F09-002`: Resource ownership, cleanup, stale-token detection,
  wrong-window use, and valid nesting are enforced.
- `R3-F09-003`: Scoped scissoring clips child drawing and detects invalid scope
  order or nesting.
- `R3-F09-004`: Scoped blend modes provide reviewed modes used by overlays,
  highlights, glow, transitions, particles, and UI effects.
- `R3-F09-005`: Scope cleanup remains deterministic on ordinary early-return
  and `Result` propagation paths supported by Rocket.

### F10 - Safe shaders

- `R3-F10-001`: Load, validate, report failure, and unload shaders through
  checked resource tokens.
- `R3-F10-002`: Uniform lookup and setting support only reviewed primitive and
  graphics value types with explicit type errors.
- `R3-F10-003`: Scoped shader rendering integrates safely with render textures
  and scope nesting.
- `R3-F10-004`: The subset supports glow, vignette, highlights, color grading,
  atmosphere, transitions, and post-processing without application-level
  unsafe pointers.

### F11 - Window, display, and rendering quality

- `R3-F11-001`: Support high DPI, MSAA4x configuration, resizing, fullscreen,
  borderless fullscreen, monitor information, framebuffer information, and
  screenshots.
- `R3-F11-002`: Logical window size, physical framebuffer size, DPI scale, and
  monitor selection are distinct documented values.
- `R3-F11-003`: Every display transition preserves or recomputes
  `VirtualCanvas` mapping and pointer conversion correctly.
- `R3-F11-004`: Unsupported platform/window capabilities return defined
  results rather than divergent silent behavior.

### F12 - Rocket graphics core types

- `R3-F12-001`: Provide typed concepts equivalent to `Vec2`, `Size`, `Rect`,
  `Transform2D`, `Color`, `TextStyle`, `TextMetrics`, `WindowConfig`, and
  `VirtualCanvas`.
- `R3-F12-002`: Geometry defines containment, intersection, normalization,
  translation, scaling, and finite-value validation with deterministic edge
  inclusion.
- `R3-F12-003`: `Transform2D` defines translation, rotation, scale, pivot, and
  composition order.
- `R3-F12-004`: Foundation implementations are internal kernels; final
  constructors, module names, and named/default parameters are frozen only at
  the public API gate.

### F13 - Color

- `R3-F13-001`: Support concepts equivalent to `Color.from_rgb`,
  `Color.from_rgba`, `Color.from_hex`, and `Color.from_hsv`.
- `R3-F13-002`: Support concepts equivalent to `with_alpha`, `mix`, `lerp`,
  `lighten`, `darken`, `saturate`, and `desaturate`.
- `R3-F13-003`: Malformed hex returns a defined `Result`; accepted hex formats
  and alpha defaults are explicit.
- `R3-F13-004`: Hue wraps deterministically; saturation, value, alpha, and
  channels clamp deterministically.
- `R3-F13-005`: Conversion, interpolation, clamping, and round-trip tests use
  stable numeric vectors.

### F14 - Rocket-facing shapes

- `R3-F14-001`: Provide concepts equivalent to `rect`, `rect_outline`,
  `rounded_rect`, `rounded_rect_outline`, `circle`, `circle_outline`, `ellipse`,
  `ring`, `ring_sector`, `sector`, `line`, `thick_line`, `triangle`,
  `triangle_outline`, `polygon`, `polygon_outline`, and `bezier`.
- `R3-F14-002`: Provide concepts equivalent to `gradient_rect` and
  `gradient_circle`.
- `R3-F14-003`: Outline APIs expose useful thickness configuration.
- `R3-F14-004`: The layer converts typed Rocket values to safe-backend calls;
  it is not a set of aliases for raw generated bindings.

### F15 - Input mapping and public hit testing

- `R3-F15-001`: Provide rectangle and circle point-containment helpers with
  documented boundary inclusion.
- `R3-F15-002`: Provide pointer position, down, pressed, and released states.
- `R3-F15-003`: With a `VirtualCanvas`, pointer positions are converted from
  physical framebuffer coordinates to logical coordinates.
- `R3-F15-004`: Pointer behavior outside the logical viewport is explicit and
  cannot accidentally activate UI controls.

### F16 - Typography

- `R3-F16-001`: Measure text size, bounds, and baseline metrics using the actual
  selected font, size, spacing, and line-height configuration.
- `R3-F16-002`: Horizontal alignment supports left, center, and right; vertical
  alignment supports top, middle, baseline, and bottom.
- `R3-F16-003`: Text styling supports font selection/weight strategy, size,
  letter spacing, and line spacing/height.
- `R3-F16-004`: Text containers support maximum width, wrapping, multiline
  layout, clipping, and ellipsis.
- `R3-F16-005`: Applications never estimate centering from guessed glyph width.
- `R3-F16-006`: Measurement caches are bounded and invalidated by relevant
  font/resource/style changes.

### F17 - `VirtualCanvas`

- `R3-F17-001`: Map a logical design size such as 1920x1080 to arbitrary
  framebuffer sizes while preserving aspect ratio.
- `R3-F17-002`: Compute deterministic viewport, scale, letterbox/pillarbox, and
  logical-to-physical and physical-to-logical transforms.
- `R3-F17-003`: Input outside the logical viewport is represented explicitly as
  outside rather than clamped onto an active control.
- `R3-F17-004`: Logical screenshots, clipping, resize, fullscreen, and DPI
  changes use the same mapping contract.
- `R3-F17-005`: Zero or negative logical/physical dimensions produce the
  defined error and never divide by zero.

### F18 - Immediate-mode UI context and interaction

- `R3-F18-001`: Provide concepts equivalent to `Context`, `UiFrame`, and
  `Response` with one clear begin/end frame lifecycle.
- `R3-F18-002`: Stable widget IDs support deterministic composition and detect
  duplicates within a frame.
- `R3-F18-003`: Pointer, keyboard, focus, disabled state, and modal capture are
  centralized rather than reimplemented by applications.
- `R3-F18-004`: Invalid context/frame nesting, use outside a frame, and stale
  responses are detectable contract violations.
- `R3-F18-005`: Foundation ID/state kernels do not freeze final public UI
  constructors or module layout.

### F19 - Layout

- `R3-F19-001`: Support Row, Column, Grid, Stack, and Anchor layouts.
- `R3-F19-002`: Sizing supports fixed, fill, content, and percentage policies.
- `R3-F19-003`: Spacing supports padding, margin, gap, Insets, and SafeArea
  where platform information exists.
- `R3-F19-004`: Horizontal and vertical alignment are explicit.
- `R3-F19-005`: Anchors include concepts equivalent to `top_left`,
  `top_center`, `top_right`, `center_left`, `center`, `center_right`,
  `bottom_left`, `bottom_center`, and `bottom_right`.
- `R3-F19-006`: Invalid, negative, overflowing, underspecified, and overfull
  layouts have deterministic behavior and diagnostics.

### F20 - Themes, tokens, and styles

- `R3-F20-001`: Themes centralize semantic colors for backgrounds, surfaces,
  felt/table treatment, actions, text, status, and borders.
- `R3-F20-002`: Themes centralize spacing, radius, typography-size, and motion
  duration tokens.
- `R3-F20-003`: Provide style data equivalent to `TextStyle`, `PanelStyle`,
  `ButtonStyle`, `BorderStyle`, `ShadowStyle`, and `ImageStyle`.
- `R3-F20-004`: Style resolution supports normal, hovered, pressed, disabled,
  and focused control states.
- `R3-F20-005`: Foundation work may stabilize data and merge/resolution rules
  but not final constructors, hierarchy, module placement, or names.

### F21 - Controls

- `R3-F21-001`: Provide text, image, separator, badge, pill, button, and icon
  button controls.
- `R3-F21-002`: Buttons centralize hit testing, state transitions, focus,
  disabled behavior, response reporting, and style selection.
- `R3-F21-003`: Control behavior is deterministic across pointer and keyboard
  activation and respects modal capture and virtual-canvas bounds.
- `R3-F21-004`: Rocket UI remains a focused game-UI system rather than a large
  desktop widget framework.

### F22 - Containers, dialogs, and transient UI

- `R3-F22-001`: Panels support background, border, border thickness, corner
  radius, padding, child clipping, and shadow.
- `R3-F22-002`: Provide dialog, overlay, tooltip, and toast primitives.
- `R3-F22-003`: Modal overlays capture input and focus according to explicit
  stacking rules.
- `R3-F22-004`: Child clipping composes with nested layout and safe scissor
  scopes without leaking native scope state.

### F23 - Typed asset store

- `R3-F23-001`: Support textures, fonts, sounds, music, and shaders under unique
  names/IDs.
- `R3-F23-002`: Cache physical assets, provide typed lookup and borrowing, and
  prevent duplicate loads of an already cached physical resource.
- `R3-F23-003`: Detect duplicate names, wrong-type lookup, missing/invalid
  resources, stale references, and unsafe unload ordering.
- `R3-F23-004`: Centralized cleanup is explicit, dependency-safe, idempotent,
  and compatible with Rocket ownership.
- `R3-F23-005`: Asset paths preserve package-root security, relocation, and
  offline behavior established by the accepted Rocket 2.1 baseline.

### F24 - Graphics/UI error and lifetime contracts

- `R3-F24-001`: Classify failures as compile-time errors, recoverable `Result`
  failures, or programmer contract violations.
- `R3-F24-002`: Define errors for missing/invalid texture, font, audio, shader,
  external asset, render target, and wrong-window resource use.
- `R3-F24-003`: Define stale reference, duplicate asset, wrong-type lookup,
  invalid geometry, invalid canvas, invalid scope nesting, duplicate widget ID,
  and invalid UI lifecycle failures.
- `R3-F24-004`: Intentional value semantics such as documented color clamping
  are not errors.
- `R3-F24-005`: No invalid operation is silently ignored and no resource token
  is replaced with a fake handle in tests or production.

### F25 - Bounded UI and cache state

- `R3-F25-001`: Widget-retained state and measurement/resource caches never
  grow without an explicit bound.
- `R3-F25-002`: Capacity, unseen-retention window, deterministic eviction,
  active/focus cleanup, and exhaustion behavior are measured before freezing
  values. The provisional 4,096-ID and 120-frame values are calibration inputs,
  not accepted constants.
- `R3-F25-003`: A stress test cycles at least 100,000 dynamic widget IDs and
  proves the retained set remains bounded.
- `R3-F25-004`: Font/resource changes invalidate affected cached measurements
  without flushing unrelated entries.

### F26 - Performance

- `R3-F26-001`: Measure per-frame/native allocations, temporary strings, layout
  allocation/recomputation, repeated text measurement, asset lookup, FFI calls,
  render-target switches, texture uploads, and state/cache growth.
- `R3-F26-002`: A warmed steady-state scene creates no unexpected native
  resources, performs no repeated texture uploads, and exhibits no unbounded
  state/cache growth.
- `R3-F26-003`: Budgets are calibrated from final integrated infrastructure,
  recorded with method and environment, and never weakened solely to make a
  regression pass.
- `R3-F26-004`: Foundation performance tooling uses synthetic deterministic
  kernels and is not registered with the repository-wide performance/release
  matrix until WP31 defines final budgets and evidence.

### F27 - Visual regression and comparator

- `R3-F27-001`: Canonical scenes cover geometry, rounded geometry, outlines,
  rings/sectors, gradients, typography and alignments, transformed textures,
  filtering where testable, clipping, blending, shaders, VirtualCanvas,
  layouts, anchors, control states, and a premium combined showcase.
- `R3-F27-002`: Comparator outputs generated image, difference image, heatmap,
  per-channel/aggregate metrics, changed-pixel ratio, and changed-region bounds.
- `R3-F27-003`: Golden updates are explicit and reviewable; tests never
  overwrite approved references automatically.
- `R3-F27-004`: Windows x64 runs the complete strict canonical suite. Every
  other supported target runs a selected portability subset plus structural
  and numerical checks.
- `R3-F27-005`: Tolerances are calibrated using known-good final-platform
  scenes. The earlier values 8, MAE 1.5, and 0.5 percent changed pixels are
  starting observations only.
- `R3-F27-006`: Platform-specific references/tolerances require evidence for
  font, GPU/driver, DPI, or shader differences and may not hide regressions.
- `R3-F27-007`: Foundation comparator work starts with deterministic raw RGBA
  kernels and synthetic fixtures; WP32 adds PNG, screenshot, CMake/CTest, and CI
  integration with the final visual-regression acceptance surface.

### F28 - Examples and capability showcase

- `R3-F28-001`: Focused examples cover named/default calls, math, easing/motion,
  shapes, colors, textures/transforms, typography, VirtualCanvas, layout,
  anchors, controls, themes, assets, and reduced motion.
- `R3-F28-002`: A neutral premium card-table-style showcase exercises dark
  surfaces, rounded panels, borders, typography, gradients, neutral card-like
  artwork, buttons, chip-like circles, hover, motion, and virtual resolution.
- `R3-F28-003`: The showcase contains no Scroll2Roll implementation or game
  rules and is never claimed as evidence of Scroll2Roll visual fidelity.
- `R3-F28-004`: Examples use public Rocket 3.0 APIs and are included in package,
  relocation, visual, and documentation validation.

### F29 - Cross-platform, compiler, and compatibility acceptance

- `R3-F29-001`: Every new language feature covers specification, lexer if
  needed, parser, AST, HIR/binding/type checking, MIR, backend, stage0,
  self-hosted compiler, metadata, formatter, LSP/tooling, diagnostics, positive
  and negative tests, and bootstrap determinism.
- `R3-F29-002`: Every public graphics/native capability is either supported on
  every final production target or has a stable documented capability failure;
  no target silently exposes a partial incompatible API.
- `R3-F29-003`: Preserve existing examples, packages, raylib scaffold,
  predecessor programs, runtime ABI v1, stage0 behavior, self-host parity,
  bootstrap determinism, package security, and release hardening.
- `R3-F29-004`: Target acceptance uses direct native-host evidence required by
  the accepted Rocket 2.1 portability contract; workflow configuration or
  cross-compilation alone is not native evidence.
- `R3-F29-005`: No no-op, placeholder, hard-coded test path, fake handle,
  bypassed validation, skipped cleanup, weakened test, deleted failing test, or
  historical-only result counts as completion.

### F30 - Documentation, traceability, and release

- `R3-F30-001`: Keep current equivalents of SPEC, STDLIB, compiler
  architecture, FFI, graphics/raylib, UI, package/SDK, project context, roadmap,
  changelog/release, migration, syntax dictionary, README, book/tutorial,
  decisions, examples, and compatibility audit synchronized.
- `R3-F30-002`: The traceability matrix maps every atomic ID to audit state,
  owning packet, implementation, tests, docs, target coverage, maturity state,
  and evidence.
- `R3-F30-003`: Release acceptance records exact current commands, test counts,
  versions, hashes, timings, platform evidence, and intentional limitations.
- `R3-F30-004`: Rocket 3.0 is not declared complete until all atomic IDs are
  `ACCEPTED` or explicitly removed by an approved replacement decision.

## 6. Foundation scope and packet sequencing

The following kernel/tool packets establish internal behavior before their
designated public-integration packets:

| Provisional slice | Atomic coverage | Allowed now | Deferred integration |
| --- | --- | --- | --- |
| Geometry kernel | F12 geometry math | Standalone types and pure tests | Public module/API, SDK registration |
| Color kernel | F13 conversion/math | Deterministic values and tests | Final constructors/names, renderer conversion |
| Hit testing | F15 containment | Pure rectangle/circle helpers | Pointer/native input integration |
| Layout kernel | F19 algorithms | Pure deterministic layout cases | UI context and public layout API |
| VirtualCanvas kernel | F17 transform math | Pure viewport/coordinate mapping | Render targets, window/input connection |
| Theme/style data | F20 value/merge rules | Internal data model and tests | Final hierarchy, names, constructors |
| Widget-state kernel | F18 IDs plus F25 eviction | ID composition, duplicate detection, bounds | Public `Context`, input/focus integration |
| Reduced-motion kernel | F06 reduced policy | Pure policy/transition state | Public timelines and UI integration |
| Comparator kernel | F27 raw RGBA metrics | Synthetic buffers and artifacts model | PNG, capture, CMake/CTest, CI |
| Performance/golden schema | F26/F27 metrics | Deterministic schema and synthetic checks | Final budgets, scenes, platform goldens |

The table does not prohibit a later packet from touching any Rocket subsystem.
It keeps the foundation packets small; public ownership, native integration,
and repository-wide registration occur in their designated packets.

## 7. Acceptance model

Each implementation packet must:

1. Read `AGENTS.md`, `docs/PROJECT_CONTEXT.md`, this requirements file, and the
   companion implementation plan.
2. Confirm `master`, the main Rocket checkout, and clean upstream state.
3. Push any existing committed Rocket 3.0 checkpoint before editing.
4. Limit itself to one or two feature groups named by the packet.
5. Write focused failing tests before implementation where executable behavior
   is involved.
6. Run the focused tests required by that packet, then all broader checks its
   risk classification requires.
7. Update atomic traceability and maturity without claiming public acceptance.
8. Commit the packet's files, evidence, and rotated handoff prompt with its
   scoped message, then push `master` to `origin`.
9. Stop after the push; a later chat handles the next packet. If the push fails,
   do not begin another packet until the existing commit is pushed or the
   blocker is resolved.

Foundation work is audited selectively by WP09, then receives the full public
API, supported-target, compatibility, bootstrap, packaging, documentation,
performance, and visual-acceptance gates.
