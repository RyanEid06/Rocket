# Rocket 3.5 Roadmap Implementation

> **For agentic workers:** Execute this roadmap work-package by work-package. Do not start a later work package until the current package's automated tests, native tests, examples, documentation, and acceptance gates are green. Prefer test-driven implementation and small, reviewable commits.

**Goal:** Finish Rocket 3.x as a coherent, production-ready game-development platform so Scroll2Roll can begin immediately afterward without private native calls, copied compatibility wrappers, missing rendering/audio/asset APIs, or game-specific engine workarounds.

**Architecture:** Rocket 3.5 is a fresh finalization cycle built on Rocket 3.0, not a new language generation. Proven functionality currently split across stdlib, native adapters, and the historical Raylib showcase is consolidated into one supported game platform. Each work package must leave behind a production API, tests, migrated examples, and documentation; the roadmap ends only when a polished Scroll2Roll-style vertical slice works entirely through those canonical APIs.

**Tech Stack:** Rocket compiler/runtime, Rocket standard library, Rocket package system, Raylib 6.0, C++ native adapter layer, CMake/CTest, existing Rocket 3 graphics/UI modules and test infrastructure.

**Baseline:** Rocket 3.0 is complete against its published WP00-WP35 contract and is tagged at `v3.0.0` (`d8a2f8e`). Existing canonical modules include `rocket.graphics`, `rocket.graphics.canvas`, `rocket.graphics.input`, `rocket.graphics.shapes`, `rocket.motion`, `rocket.raylib.safe`, `rocket.ui`, `rocket.ui.controls`, `rocket.ui.containers`, `rocket.ui.layout`, `rocket.ui.styles`, and `rocket.ui.theme`. Mature but non-canonical functionality still exists under `examples/raylib_showcase`.

**Roadmap numbering:** Rocket 3.5 intentionally restarts at **WP1**. Treat this as a fresh implementation cycle built on the completed Rocket 3.0 baseline, with its own independent work-package numbering. The old roadmap is historical context; this roadmap is the final game-platform hardening pass before Scroll2Roll development begins.

**Implementation owner:** Eddie owns WP1-WP7. Publishing this roadmap authorizes the plan and its handoff; it does not authorize another implementation lane to start or to modify the frozen `v3.0.0` tag.

**Branch policy:** Eddie must perform and push all WP1-WP7 work only on the dedicated `rocket35/eddie` branch, using a separate worktree or checkout from Ryan's compiler lane. Do not commit, push, merge, or fast-forward Rocket 3.5 implementation work directly to `master`. Integration into `master` requires a later, explicit user-authorized review after the concurrent lanes are complete.

## Repository audit and disposition (2026-09-17)

This roadmap is **accepted with the corrections recorded in this document**. It is needed for Scroll2Roll readiness, but it must not be described as proof that the Rocket 3.0 release or its prior work packages were falsely completed. Rocket 3.0 deliberately shipped a narrower contract; Rocket 3.5 promotes the remaining proven example-local capabilities and adds the missing product-level pieces.

The audit found:

- The repository has one Raylib C++ adapter implementation, but top-level CMake builds it from `examples/raylib_showcase/native`, so production infrastructure is physically owned by an example.
- `rocket.raylib.safe` is bundled but does not expose the showcase's complete texture/filter, blend, shader, timing, or audio surface.
- `rocket.assets` does not exist as a bundled module. Rocket 3.0 release and migration documentation explicitly identify `examples/raylib_showcase/src/rocket_assets.rocket` as a reference-package module.
- The native asset store already loads and cleans up music, but the public runtime has no complete streamed-music playback/update/pause/resume/stop lifecycle.
- Rocket UI has public state, layout, style, control, and container models, while the premium example still renders reusable button/panel visuals manually because no `rocket.ui.render` module exists.
- The focused Windows Release evidence used for this audit passed 16/16 texture, render-scope, shader, display, typography, asset-store, visual-regression, and graphics-example tests. This proves the source behavior worth promoting; it does not make those example-local APIs canonical.

### Bundled-module and SDK rule

Any new bundled module in this roadmap, including `rocket.assets`, `rocket.ui.render`, or an approved `rocket.game` facade, must be registered in both compiler implementations (`src/module_loader.cpp` and `compiler/src/main.rocket`). It must pass stage0, self-hosted, target-surface, documentation/search, formatter, and packaged-SDK relocation checks. A source file merely existing under `stdlib/` is not sufficient.

The production Raylib adapter and pinned Raylib libraries must also be obtainable by an ordinary external Rocket package through the released SDK/package flow on every supported target. No final acceptance may depend on the repository build tree, an example-local native manifest, or a manually copied library directory.

---

## 0. Release Definition

Rocket 3.5 is complete only when all of the following are true:

- A normal game imports supported standard-library modules only; it does not copy files from `examples/raylib_showcase`.
- The canonical API supports the complete Scroll2Roll rendering stack: shapes, images/textures, texture filtering, render textures, clipping, blending, shaders, fonts/text layout, logical-resolution canvases, DPI-aware output, screenshots, and animation.
- The canonical API supports the complete Scroll2Roll audio stack: audio-device lifetime, short sound effects, continuously streamed background music, playback state, volume, pause/resume/stop, and cleanup.
- Assets are managed through a canonical `rocket.assets` module with typed handles, caching, duplicate-name protection, path/package-root handling, deterministic cleanup, and explicit failures.
- Rocket UI can render commonly reused game widgets without every application manually rebuilding their visuals from primitives.
- There is one documented, supported game-facing API path. Historical compatibility APIs are removed, archived, or clearly marked internal/test-only.
- No Scroll2Roll acceptance code uses private adapter functions, undocumented host symbols, direct C++/Raylib calls, or source-file copying.
- Existing Rocket 3 behavior remains compatible unless a migration is explicitly documented.
- The full supported test matrix is green.
- A polished Scroll2Roll-style casino vertical slice passes the final visual, interaction, resize/DPI, audio, resource-lifetime, and performance gates.

### Non-goals

Rocket 3.5 does **not** redesign the Rocket language, type system, package manager, compiler architecture, or LSP incremental-analysis model. Those are separate projects. Rocket 3.5 also does not attempt to become Unity, Unreal, Qt, or a scene-editor framework. The objective is a clean and capable 2D game/application runtime suitable for Scroll2Roll.

---

# WP1 — Canonical Game Runtime Foundation

## Objective

Remove the current architectural split where production-quality Raylib functionality lives partly in `stdlib/rocket/raylib/safe.rocket` and partly in the historical `examples/raylib_showcase` wrapper. Move the existing backend out of the example and establish one production-owned native backend plus one supported Rocket-facing runtime surface before adding more features.

## Current problem

`stdlib/rocket/raylib/safe.rocket` contains the modern safe lifecycle and Rocket 3 shape/text/render-target features, while `examples/raylib_showcase/src/rocket_raylib.rocket` still contains mature texture, filtering, blending, shader, audio, timing, and related game APIs. Both wrappers call the same C++ adapter today, but the example acts as a second Rocket-facing SDK surface instead of merely demonstrating the official runtime.

## Primary files

**Create / promote:**
- `src/raylib/rocket_raylib_adapter.cpp`
- `src/raylib/rocket_raylib_adapter.h`
- `stdlib/rocket/game.rocket` if an application-level facade is needed after API review

**Modify:**
- `stdlib/rocket/raylib/native.rocket`
- `stdlib/rocket/raylib/safe.rocket`
- `CMakeLists.txt`
- `scripts/phase19_package.py` and release/package wiring for the native adapter and pinned Raylib libraries
- package/build wiring that currently references the showcase adapter or requires an example-local native manifest
- `examples/raylib_showcase/CMakeLists.txt`
- `examples/raylib_showcase/src/rocket_raylib_adapter.rocket`
- native adapter tests under `tests/native/`

**Source material to migrate, not duplicate:**
- `examples/raylib_showcase/native/rocket_raylib_adapter.cpp`
- `examples/raylib_showcase/native/rocket_raylib_adapter.h`
- `examples/raylib_showcase/src/rocket_raylib.rocket`

## Requirements

1. Move the existing Raylib adapter implementation to a production source location owned by Rocket, not by an example. This is a relocation and decomposition task, not permission for a clean-room rewrite.
2. Preserve the existing safe handle/lifetime model for `Window`, `Frame`, render targets, fonts, scopes, and future resources.
3. Establish one naming convention for public game APIs. Avoid maintaining a "safe API" and a second unrelated "showcase API" indefinitely.
4. Keep host/native symbols internal. User code consumes Rocket modules, never C++ adapter details.
5. Maintain explicit `Result[..., String]` failures for operations that can fail.
6. Keep nested frame/render/scissor/shader/blend scope validation deterministic; invalid scope order must fail rather than silently corrupt state.
7. Update examples so they import the production modules instead of maintaining their own runtime implementation.
8. Add a compatibility/deprecation strategy for any public example API that users may already have copied.
9. No behavior should depend on current working directory when the package root can be resolved explicitly.
10. Split production adapter code into focused files if the migrated adapter becomes too large to review safely. Suggested boundaries: window/frame, drawing, textures/render-targets, shaders/blending, typography, audio.
11. Define and test how external packages link the adapter and pinned Raylib libraries from a relocated release SDK on Windows x64, Linux x64, Linux ARM64, and macOS ARM64.

## Tests

- Native adapter lifetime tests remain green after relocation.
- A package using only stdlib imports can open a window, begin/end a frame, draw, resize, and close.
- Invalid/double-close/double-end operations produce deterministic errors.
- An example build must not compile its own independent copy of the runtime adapter.
- Add a source-policy test that rejects imports from `examples/raylib_showcase/src` inside production stdlib/examples intended as supported game samples.
- A minimal game must build and run from a relocated packaged SDK without reading the repository build tree or `examples/raylib_showcase`.

## Exit gate

WP1 is complete when the sole Raylib backend is production-owned and release-packaged, all supported examples consume it, and the historical showcase no longer acts as a second SDK. From this point onward, every Rocket 3.5 feature must land on the canonical production surface first.

---

# WP2 — Production Rendering Pipeline: Textures, Canvas, and Display

## Objective

Make images and texture-based game rendering first-class in the canonical Rocket API. Scroll2Roll must be able to render card artwork, chips, table textures, icons, UI imagery, backgrounds, and off-screen scenes without legacy wrappers or pixel-generation hacks.

## Primary files

**Modify / extend:**
- `stdlib/rocket/raylib/safe.rocket`
- `stdlib/rocket/raylib/native.rocket`
- `stdlib/rocket/graphics.rocket`
- `stdlib/rocket/graphics/canvas.rocket`
- production Raylib adapter files created in WP1

**Promote behavior from:**
- `examples/raylib_showcase/src/rocket_raylib.rocket`
- `examples/raylib_showcase/tests/texture_pro_test.rocket`
- `examples/raylib_showcase/tests/display_quality_test.rocket`
- `examples/raylib_showcase/tests/render_scopes_test.rocket`

**Add canonical tests/fixtures under:**
- `tests/fixtures/rocket35_texture_package/`
- `tests/rocket35_texture_test.cmake`
- `tests/native/`

## Required public capabilities

### Texture lifetime

- `Texture` typed handle.
- `load_texture(window, path)`.
- `unload_texture(texture)`.
- Width/height queries.
- Safe rejection of stale or already-unloaded handles.

### Drawing

- Basic texture draw.
- Scaled texture draw.
- Source-rectangle to destination-rectangle draw.
- Origin/pivot and rotation.
- Tint/color multiplication.
- Flipping through source geometry or an explicit supported mechanism.
- Drawing from render textures through the same coherent geometry model.

### Filtering

Promote and canonicalize:
- point/nearest filtering;
- bilinear filtering;
- trilinear filtering where supported;
- anisotropic filtering capability detection;
- max-anisotropy query;
- fallback behavior when a requested mode is unavailable;
- getter/setter for texture filtering.

Do not expose magic integers as the preferred user API if Rocket can express a typed enum/constant surface consistently.

### Display/game loop essentials

Canonicalize the game-loop functions required by real applications:
- clear frame/background;
- `window_should_close` or equivalent close-request query;
- target FPS / frame-rate policy where Raylib owns timing;
- delta/frame time;
- elapsed application time;
- framebuffer and window size queries;
- resize handling;
- fullscreen and borderless control;
- DPI-aware dimensions;
- screenshot/image capture needed for visual regression and game screenshots.

### Canvas / virtual resolution

The existing virtual-canvas system must support texture-heavy scenes with:
- fixed logical resolution;
- letterbox/pillarbox scaling;
- deterministic mouse/pointer coordinate conversion;
- nearest or smooth output scaling selectable according to art style;
- resizing without invalid logical input mapping;
- render-target recreation or revision handling when necessary.

## Acceptance tests

1. Load real card/table/chip image assets from a package-relative path.
2. Render sub-regions of an atlas using source rectangles.
3. Rotate and scale card textures around a chosen origin.
4. Switch between point and bilinear filtering and validate state.
5. Render an entire logical scene into a render texture and present it at multiple window sizes.
6. Verify mouse hit coordinates remain correct after resize and high-DPI scaling.
7. Save deterministic screenshots for visual comparison.
8. Repeatedly load/unload resources and close the window under sanitizing/lifetime tests without leaked or stale handles.

## Exit gate

A Rocket application can reproduce a complete texture-heavy 2D casino table without any API from `examples/raylib_showcase` and without generating artwork pixel-by-pixel in Rocket code.

---

# WP3 — Rendering Effects: Blend Modes, Shaders, and Post-Processing

## Objective

Promote the remaining production rendering tools needed for premium visual polish: blend scopes, shaders, shader uniforms, post-processing, and safe capability fallback.

## Primary files

**Modify:**
- `stdlib/rocket/raylib/safe.rocket`
- `stdlib/rocket/raylib/native.rocket`
- production adapter from WP1

**Promote behavior/tests from:**
- `examples/raylib_showcase/src/rocket_raylib.rocket`
- `examples/raylib_showcase/tests/shader_test.rocket`
- `examples/raylib_showcase/tests/render_scopes_test.rocket`

**Add:**
- `tests/fixtures/rocket35_render_effects_package/`
- `tests/rocket35_render_effects_test.cmake`

## Required capabilities

### Blending

Canonical support for the useful Raylib blend modes already proven by the showcase layer, including at minimum:
- alpha;
- additive;
- multiplied;
- premultiplied alpha;
- supported add/subtract color modes where backend/platform support is reliable.

Expose blending as a scoped lifetime:

`begin_blend(frame, mode) -> Result[BlendScope, String]`

`end_blend(scope) -> Result[Bool, String]`

Incorrect nesting, stale frames, or double-ending a scope must fail explicitly.

### Shaders

Canonical support for:
- capability query;
- load shader from files;
- load shader from memory/source;
- unload;
- named uniform lookup;
- float uniform;
- integer uniform;
- vec2 uniform;
- color/vec4-equivalent uniform needed by the existing backend;
- scoped shader begin/end.

Shader compilation/link errors must preserve useful backend diagnostics rather than collapse to a generic failure.

### Post-processing

Document and test the supported pattern:

1. draw logical scene to render texture;
2. begin shader;
3. draw render texture to output;
4. end shader.

This is sufficient for casino effects such as glow, tinting, subtle vignette, highlight pulses, grayscale/desaturation, and transition effects without requiring a scene graph.

## Tests

- Each supported uniform type round-trips through the native adapter.
- Missing or invalid shader produces a controlled error.
- Unsupported shader platform/capability is detectable before use.
- Blend and shader scopes cannot be illegally nested/ended across frames.
- Render-target + shader + canvas composition survives window resize.
- Existing graphics and UI tests remain unchanged/green when no shader or custom blend is active.

## Exit gate

A Scroll2Roll scene can use textures, render-to-texture, blending, and shader-based polish through documented stdlib APIs only.

---

# WP4 — Production Audio and Streamed Music

## Objective

Finish the canonical audio stack. Rocket must support short UI/game sound effects and continuously streamed casino background music with correct lifetime and frame-update behavior.

## Primary files

**Modify:**
- `stdlib/rocket/raylib/safe.rocket`
- `stdlib/rocket/raylib/native.rocket`
- production adapter from WP1

**Promote/extend behavior from:**
- `examples/raylib_showcase/src/rocket_raylib.rocket`
- `examples/raylib_showcase/tests/audio_stress_test.rocket`
- existing native adapter audio tests

**Add:**
- `tests/fixtures/rocket35_audio_package/`
- `tests/rocket35_audio_test.cmake`

## Required capabilities

### Audio device

- typed `AudioDevice` lifetime;
- open/close;
- ready/status query;
- deterministic failure for use after close.

### Sound effects

- typed `Sound` handle;
- load;
- play;
- stop;
- playing-state query if supported cleanly;
- volume;
- pitch if backend support is stable and already available or trivial to expose;
- unload;
- repeated playback without leaking native resources.

### Streamed music

Add a complete typed `Music` lifecycle:
- load music stream;
- play;
- per-frame/update-stream call;
- pause;
- resume;
- stop;
- playing-state query;
- volume;
- optional looping control if backend behavior can be represented reliably;
- unload.

The API documentation must explicitly state that streamed music requires update calls while playing and show where this belongs in a normal game loop.

### Application behavior

- Multiple sound effects may coexist with music.
- Sound/music failures must not crash the application.
- Closing the audio device with live resources must either safely clean them up through the owning system or reject the invalid ordering according to one documented rule; it must never leave ambiguous ownership.
- Asset-store integration in WP5 must reuse these canonical handles instead of defining a second audio representation.

## Tests

- Open/close audio repeatedly.
- Load/play/stop/unload sound repeatedly.
- Stream music across many update ticks.
- Pause/resume and volume changes.
- Invalid resource order produces deterministic errors.
- Audio stress test runs through many resource cycles.
- Headless/CI behavior is controlled: tests that need an unavailable real audio device must use the established adapter testing seam rather than becoming flaky.

## Exit gate

Scroll2Roll can run background casino music and overlapping interface/game sound effects exclusively through supported Rocket APIs with deterministic cleanup.

---

# WP5 — Canonical Asset System: `rocket.assets`

## Objective

Promote the proven asset-store idea from the showcase into a real standard-library module and make it the normal resource-management path for games.

## Primary files

**Create:**
- `stdlib/rocket/assets.rocket`
- supporting focused modules under `stdlib/rocket/assets/` only if needed, e.g. `types.rocket` or `paths.rocket`

**Promote/refactor from:**
- `examples/raylib_showcase/src/rocket_assets.rocket`

**Modify:**
- `src/module_loader.cpp`
- `compiler/src/main.rocket`
- stdlib documentation/search and CMake registration
- packaged-SDK and relocation checks
- `examples/raylib_showcase/src/showcase.rocket`
- `examples/rocket3_graphics_ui/examples/premium_card_table.rocket`
- relevant docs

**Tests:**
- promote `examples/raylib_showcase/tests/asset_store_test.rocket`
- promote `examples/raylib_showcase/tests/missing_asset_test.rocket`
- extend `tests/rocket3_asset_store_test.cmake`
- extend `tests/rocket3_asset_store_target_surface_test.cmake`

## Required API model

Provide typed references for at least:
- texture;
- font;
- sound;
- music;
- shader.

The store must:
- have an explicit package/root path;
- normalize and resolve asset paths consistently;
- cache successfully loaded resources by logical asset name;
- reject or explicitly define duplicate logical names;
- reject wrong-type lookups;
- return controlled missing-file/load errors;
- prevent use-after-cleanup;
- clean up all owned resources exactly once;
- support graphics-only applications without forcing audio initialization;
- support full graphics+audio stores when sound/music are required.

### Ownership rule

Make one ownership contract explicit and enforce it:

- The `AssetStore` owns native resources it loads.
- Typed `*Ref` values are borrowed references/handles, not independent owners.
- `cleanup(store)` invalidates all references produced by that store.
- Borrowing from an invalid/stale reference returns an error.

### Ergonomics

Loading should be concise enough for real games. The public surface should not require a game to manually keep parallel arrays of paths, handles, and cleanup callbacks.

Do not add a giant content pipeline, hot-reload daemon, asset editor, or serialization system to this roadmap. Scroll2Roll needs reliable runtime asset loading and ownership, not an engine editor.

## Tests

- Cache identity for repeated loads of the same logical asset.
- Missing file.
- Duplicate logical name.
- Wrong-type lookup.
- Store at capacity.
- Cleanup with all supported resource types loaded.
- Double cleanup.
- Borrow after cleanup.
- Graphics-only store.
- Full graphics/audio store.
- Package-root behavior from a working directory different from the project root.

## Exit gate

A normal Rocket game can centralize textures, fonts, sounds, music, and shaders in `rocket.assets`, and the old showcase-local asset-store implementation is deleted or reduced to a compatibility shim scheduled for removal.

---

# WP6 — Rendered Game UI Layer

## Objective

Close the gap between Rocket 3's strong UI state/layout/style system and actual pixels on screen. Keep the UI lightweight and game-oriented, but provide canonical rendering helpers for the widgets every application otherwise rebuilds by hand.

## Existing foundation

Reuse instead of replace:
- `stdlib/rocket/ui.rocket`
- `stdlib/rocket/ui/layout.rocket`
- `stdlib/rocket/ui/styles.rocket`
- `stdlib/rocket/ui/theme.rocket`
- `stdlib/rocket/ui/controls.rocket`
- `stdlib/rocket/ui/containers.rocket`
- `stdlib/rocket/motion.rocket`

## Primary files

**Create:**
- `stdlib/rocket/ui/render.rocket`

**Modify as required:**
- `src/module_loader.cpp`
- `compiler/src/main.rocket`
- `stdlib/rocket/ui.rocket`
- `stdlib/rocket/ui/controls.rocket`
- `stdlib/rocket/ui/containers.rocket`
- `stdlib/rocket/ui/styles.rocket`
- `stdlib/rocket/ui/theme.rocket`
- `examples/rocket3_graphics_ui/examples/layout_and_controls.rocket`
- `examples/rocket3_graphics_ui/examples/premium_card_table.rocket`

**Tests:**
- new `tests/fixtures/rocket35_ui_render_package/`
- new `tests/rocket35_ui_render_test.cmake`
- visual goldens under `tests/visual/goldens/`

## Required rendered primitives

At minimum provide reusable drawing for:

### Panel / card container

- background fill;
- border;
- corner radius;
- optional shadow/elevation representation supported by the current renderer;
- theme/style input.

### Button

- idle;
- hover;
- pressed;
- disabled;
- focus/keyboard-visible state if already represented by the input/UI model;
- label typography/alignment;
- optional icon/image if this can be kept small and clean.

The renderer consumes the result/state from the existing control logic; it must not invent a second interaction state machine.

### Text label

- text style/theme role;
- alignment;
- wrapping/overflow/clipping using the existing typography system.

### Image widget

- texture reference;
- fit/fill/stretch policy sufficient for UI artwork;
- tint;
- clipping to bounds where requested.

### Progress/value bar

Useful for loading/progress/status displays and sufficiently general to justify inclusion.

### Scrollable/clipped container

Only if the current container/input system already has the required scrolling model. Do not create a new complex retained-mode UI framework merely to check a box. If scrolling is not part of the current state model, keep this out of WP6 and document it as unnecessary for Scroll2Roll.

## Styling rules

- Theme remains data; rendering consumes theme/style values.
- No hardcoded casino palette inside stdlib.
- Widgets must scale correctly through the virtual canvas and DPI model.
- Default styles must be usable but easy to override.
- Games can still bypass widget rendering and draw completely custom controls when desired.

## Visual acceptance

Create deterministic visual fixtures for:
- buttons in idle/hover/pressed/disabled states;
- panel/card container;
- label wrapping/alignment;
- image fit behavior;
- a small composed layout.

Update `premium_card_table.rocket` so at least its reusable button/panel UI uses the canonical renderer instead of reconstructing every visual primitive manually. Custom playing-card/table art may remain deliberately custom.

## Exit gate

Rocket provides a real rendered game-UI layer without becoming a heavyweight desktop widget toolkit. Common game menus/HUD/panels/buttons no longer require boilerplate primitive drawing.

---

# WP7 — Final Consolidation and Scroll2Roll Readiness Gate

## Objective

Finish Rocket 3.5 by removing duplication, documenting one supported path, cleaning release debt, and proving the complete stack with a realistic casino vertical slice.

## Part A — Public API consolidation

Perform an API inventory of:
- `stdlib/rocket/graphics*`
- `stdlib/rocket/raylib/*`
- `stdlib/rocket/assets*`
- `stdlib/rocket/ui*`
- `stdlib/rocket/motion.rocket`
- any remaining `examples/raylib_showcase/src/rocket_raylib*.rocket`

For every public function/type in the old showcase wrapper, classify it as:
1. canonical in stdlib;
2. intentionally unsupported, with reason;
3. test-only/internal;
4. obsolete duplicate.

There must be no unexplained fourth surface that developers need to discover themselves.

### Preferred developer experience

A Scroll2Roll source file should be able to use obvious supported imports such as:

```rocket
import rocket.graphics
import rocket.motion
import rocket.raylib.safe
import rocket.assets
import rocket.ui
import rocket.ui.render
```

If WP1 introduces `rocket.game` as a small facade, it may simplify common setup, but it must not duplicate the underlying modules or hide ownership rules. Do not add the facade unless it measurably improves game startup code.

## Part B — Cleanup

### Remove or relocate dead/duplicated code

- Remove showcase runtime copies after all examples use the production backend.
- Retire only files made obsolete by the canonical migration, and first prove that no build, test, documentation, or package path references them. Unrelated compiler, LSP, and historical cleanup is out of scope.
- Keep `experiments/rocket3_visual_compare` until equivalent visual-regression infrastructure exists elsewhere; it is currently functional test infrastructure, not dead code.
- Relocate active test infrastructure out of misleading historical/example directories when practical.

### Build hygiene

- Reduce warnings introduced or exposed by touched code.
- Do not fold unrelated filesystem, cryptography, compiler, or LSP maintenance into Rocket 3.5; record independently discovered debt as separately owned follow-up work.
- Split giant CMake sections only where Rocket 3.5 additions would otherwise make them harder to maintain. Avoid a full build-system rewrite.

## Part C — Documentation

Create/update documentation covering:
- creating a game window and frame loop;
- virtual resolution and DPI;
- shapes and texture drawing;
- texture filters;
- render targets;
- shaders and blending;
- fonts/text layouts;
- audio and streamed music;
- asset-store ownership;
- UI state + rendered widgets;
- cleanup order;
- package-relative asset paths;
- a short "Building a 2D Game with Rocket 3.5" path.

The examples must match the documented API exactly.

## Part D — Scroll2Roll vertical slice

Create a dedicated acceptance example, not the full casino product.

**Suggested path:**
- `examples/scroll2roll_vertical_slice/`

### Scene requirements

Build one high-quality casino table/menu scene using only canonical Rocket 3.5 APIs.

It must demonstrate:
- logical 1920x1080-style design resolution or another explicitly chosen fixed logical resolution;
- window resizing and aspect-ratio preservation;
- high-DPI correctness;
- textured table/background;
- real card-front/card-back artwork or deterministic repository-owned test artwork;
- chips/icons/images;
- rounded shapes and borders;
- shadows/elevation treatment;
- premium typography;
- rendered UI buttons/panels;
- hover/pressed interaction;
- card/chip animation using `rocket.motion`;
- render target composition;
- at least one blend effect;
- at least one shader/post-process effect with capability fallback;
- sound effect playback;
- continuously streamed background music;
- asset loading through `rocket.assets`;
- screenshot capture for comparison;
- clean exit with all resources released.

### Hard prohibition

The vertical slice fails acceptance if it:
- imports files from `examples/raylib_showcase/src`;
- calls private/native adapter functions directly;
- contains copied C++/Raylib glue;
- implements missing engine functionality locally that belongs in the stdlib/runtime;
- generates visual assets pixel-by-pixel merely because image loading is inadequate;
- relies on undefined resource-destruction order.

If the vertical slice exposes a missing generally useful engine capability, fix Rocket 3.5 first and then resume the slice. Do not hide engine gaps inside the example.

## Part E — Performance and stability gates

Rocket 3.5 is not accepted merely because screenshots look good.

### Runtime stability

- Run the vertical slice repeatedly through startup/shutdown cycles.
- Resize/fullscreen-toggle repeatedly.
- Exercise shader/blend/render-target nesting repeatedly.
- Load/unload resource-heavy scenes repeatedly in test fixtures.
- Verify no stale-handle or double-destruction failures.

### Rendering performance

Define and record a repeatable benchmark configuration for the vertical slice. The goal is not an artificial engine benchmark; it is proof that normal Scroll2Roll-style rendering does not create pathological frame cost.

At minimum track:
- average frame time;
- high-percentile frame time or a simple worst-frame diagnostic during the controlled benchmark;
- resource count;
- draw/render-target usage;
- debug versus release distinction.

Do not bake a hardware-specific universal FPS promise into the language. Instead establish a reference-machine baseline and reject major regressions against that baseline.

### Compiler/build sanity

The vertical slice must compile through the normal Rocket package flow without custom one-off build scripts beyond the supported example integration. Rocket 3.5 does not need to solve the separate LSP incremental-analysis problem here.

## Part F — Final verification matrix

Before declaring Rocket 3.5 complete, run:

1. compiler tests;
2. package/build tests;
3. Rocket 3 graphics tests;
4. shapes/input tests;
5. UI context/layout/theme/control/container tests;
6. new texture/display tests;
7. new shader/blend tests;
8. new audio/music tests;
9. canonical asset-store tests;
10. rendered-widget tests;
11. native Raylib adapter tests;
12. visual regression tests/goldens;
13. showcase example after migration to canonical APIs;
14. premium card-table example;
15. Scroll2Roll vertical-slice acceptance test;
16. stage0/self-hosted bundled-module parity and target-surface checks;
17. relocated packaged-SDK build/run tests that do not use the repository build tree;
18. native Debug/Release and visual acceptance on Windows x64, Linux x64, Linux ARM64, and macOS ARM64, plus the supported cross-build matrix already defined by the repository release process.

Record exact pass/fail counts and platform/toolchain versions in the release evidence. A previous green run is not sufficient for final acceptance.

## Exit gate

WP7, and therefore Rocket 3.5, is complete only when the Scroll2Roll vertical slice proves the complete game stack works through one canonical API, all automated/release gates are green, and no engine-level workaround remains inside the acceptance example.

At that point Rocket 3.5 becomes the frozen baseline for starting the real Scroll2Roll casino. Any later engine change should be driven by a concrete product requirement or genuine defect, not by unfinished foundational rendering/audio/UI work.

---

# Work-Package Dependency Order

```text
WP1  Canonical Game Runtime Foundation
  ↓
WP2  Production Rendering Pipeline
  ├───────────────┐
  ↓               ↓
WP3  Rendering    WP4  Audio / Music
     Effects
  └───────┬───────┘
          ↓
WP5  Canonical Asset System
          ↓
WP6  Rendered Game UI Layer
          ↓
WP7  Final Consolidation + Scroll2Roll Readiness
```

**Sequencing rule:** WP1 and WP2 are foundational and must land first. After WP2, WP3 and WP4 may run in parallel. Both must be complete before WP5 because `rocket.assets` owns shader, sound, and music resources. WP6 uses the settled rendering and asset surfaces and therefore follows WP5. WP7 is strictly last and is the only work package allowed to declare Rocket 3.5 release-ready.

---

# Commit / Review Policy

Each work package should be split into small commits that preserve a green build whenever possible. Recommended review boundaries:

- native adapter/API types;
- Rocket-facing wrapper surface;
- tests;
- example migration;
- docs.

Do not combine unrelated compiler/LSP work with Rocket 3.5 game-runtime commits.

Every Rocket 3.5 commit and push must remain on `rocket35/eddie`; `master` is an integration target, not Eddie's working branch.

For migrated behavior, first preserve existing tests or create equivalent failing canonical tests, then move/implement behavior, then remove the duplicate implementation only after the new path is verified.

---

# Definition of “Rocket Is Finished Enough for Scroll2Roll”

After Rocket 3.5, the following should be true without qualification:

**Rendering:** Rocket can draw all required casino geometry, real images, rotated/scaled cards, gradients, text, clipping, render targets, blended effects, and shaders at a stable logical resolution across resize/DPI changes.

**Audio:** Rocket can play UI/game sounds and continuously streamed background music with explicit, safe resource lifetime.

**Assets:** Rocket has one typed asset manager for textures, fonts, sounds, music, and shaders.

**UI:** Rocket provides both low-level custom drawing and reusable rendered game widgets tied to the existing layout/style/theme/control model.

**Architecture:** The historical showcase is an example, not a secret second SDK. Production functionality lives in production modules.

**Developer experience:** A developer can start a game from documented stdlib imports and does not need to understand C++ adapter internals.

**Proof:** A polished Scroll2Roll-style vertical slice exists and uses no workarounds outside the canonical Rocket API.

If all seven work packages and the final verification matrix pass, do **not** create another broad Rocket roadmap before starting Scroll2Roll. Start the casino. Future Rocket work should be product-driven, bug-driven, platform-driven, or performance-driven.
