# Rocket 3.0 Graphics, UI, and Language-Ergonomics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: use
> `superpowers:test-driven-development` for implementation packets and
> `superpowers:verification-before-completion` before every checkpoint. Use
> `superpowers:subagent-driven-development` only when the owner explicitly asks
> for subagents. Each work packet is executed in a fresh chat and contains no
> more than two feature groups.

**Goal:** Deliver Rocket 3.0's additive language ergonomics and complete,
safe, portable, game-oriented graphics/UI stack without contaminating active
Phase 19 work.

**Architecture:** Work proceeds on an isolated provisional branch while Phase
19 is active. Only deterministic internal kernels and standalone tooling are
allowed before the final Phase 19 release. After Phase 19 is accepted, selected
provisional commits are promoted onto a fresh integration branch, public APIs
are frozen, and compiler/native/platform/release work proceeds in narrow
vertical packets.

**Tech stack:** Rocket, C++20 permanent stage0, Rocket self-hosted compiler,
LLVM 22.1.6, raylib 6.0, CMake/Ninja, native target SDKs inherited from the
final Phase 19 release.

**Spec:** `docs/ROCKET_3_0_GRAPHICS_UI_REQUIREMENTS.md`

## Global constraints

- Rocket 3.0 is the target release. The predecessor is called **the final Phase
  19 release** until its accepted tag and version exist.
- Preserve every valid predecessor program and runtime ABI v1 unless the owner
  separately approves a specific Rocket 3.0 incompatibility decision.
- Preserve C++20 stage0, self-host equivalence, and deterministic stage2/stage3
  bootstrap.
- Do not merge Rocket 3.0 into `master`, modify Phase 19 evidence, rebuild
  frozen outputs, retag releases, or change release artifacts while Phase 19 is
  active.
- Do not introduce cloud, paid, subscription, API-key, or per-use graphics
  dependencies. raylib 6.0 remains the renderer.
- Do not migrate Scroll2Roll in this plan.
- Run builds and tests sequentially. Do not overlap Phase 19 validation. Stop
  and report a process over 4 GiB or one that grows continuously.
- A packet handles one or two feature groups, updates its traceability rows,
  rotates the current next-chat prompt, commits those changes together, and
  stops.

---

## 1. Current provisional baseline

| Field | Recorded value |
| --- | --- |
| Provisional branch | `codex/rocket-3-provisional` |
| Linked worktree | `C:\Users\Administrator\Desktop\Projects\Rocket\out\worktrees\rocket-3-provisional` |
| Starting Phase 19 commit | `5314abf9da7ed0be2daa5b6978744c93b8020b27` |
| Starting commit subject | `Fix Phase 19 cross SDK library layouts` |
| Starting worktree state | clean before these planning files |
| Phase 19 state | active; release version and final tag intentionally unresolved |
| Provisional generated output | `out/rocket3-provisional/` inside the linked worktree |
| Merge policy | never merge provisional work to `master` while Phase 19 is active |

The starting SHA is provenance, not a claim that Phase 19 is complete. Before
every provisional packet, refresh the active overlap with:

```powershell
git -C 'C:\Users\Administrator\Desktop\Projects\Rocket' status --short
git -C 'C:\Users\Administrator\Desktop\Projects\Rocket' rev-parse master
git -C 'C:\Users\Administrator\Desktop\Projects\Rocket' diff --name-only 5314abf..master
git -C 'C:\Users\Administrator\Desktop\Projects\Rocket\out\worktrees\rocket-3-provisional' status --short
```

If the intended packet touches any newly changed Phase 19 file or alters a
Phase 19 contract/test/package/release surface, reclassify it `RED` and stop.

## 2. Maturity and isolation states

### Isolation

- `GREEN`: standalone deterministic code or tooling; no Phase 19-owned files,
  global registration, SDK inclusion, native binding, or release effect.
- `YELLOW`: reads or depends on Phase 19 behavior but does not modify it. It may
  be specified or tested read-only, but is not promoted while Phase 19 is active.
- `RED`: changes the compiler, runtime, standard-library registration, native
  adapters, SDK/module lookup, top-level build/test matrix, target behavior,
  packaging, release docs, evidence, hashes, or any Phase 19-owned path.

### Maturity

- `PROVISIONAL`: tested internal kernel or tool on the isolated branch; public
  API and release support are not claimed.
- `INTEGRATION-READY`: selectively promoted onto the final Phase 19 baseline,
  audited, and compatibility checked.
- `PUBLIC`: final Rocket 3.0 names, signatures, ownership, errors, docs, and
  target support are approved.
- `ACCEPTED`: complete tests, bootstrap, packaging, performance, visual,
  documentation, and supported-target evidence pass.

## 3. Planned file ownership

### Stable planning documents

- `docs/ROCKET_3_0_GRAPHICS_UI_REQUIREMENTS.md`: stable WHAT/WHY, atomic IDs,
  semantics, and acceptance requirements.
- `docs/ROCKET_3_0_GRAPHICS_UI_IMPLEMENTATION_PLAN.md`: live HOW/WHEN, packet
  status, evidence, checkpoints, and integration state.

### Provisional files permitted before Phase 19 completion

The first implementation packet creates the package skeleton. Later packets
add only their named files:

```text
experiments/rocket3_foundation/
    rocket.toml
    README.md
    src/
        main.rocket
        geometry.rocket
        color.rocket
        hit_testing.rocket
        layout.rocket
        virtual_canvas.rocket
        theme.rocket
        widget_state.rocket
        reduced_motion.rocket
    tests/
        geometry_test.rocket
        color_test.rocket
        hit_testing_test.rocket
        layout_test.rocket
        virtual_canvas_test.rocket
        theme_test.rocket
        widget_state_test.rocket
        reduced_motion_test.rocket

experiments/rocket3_visual_compare/
    README.md
    synthetic fixture and comparator-kernel sources chosen by its packet
```

These directories are explicitly non-SDK, non-release, and non-public. They are
not registered in top-level CMake/CTest or added to packaged standard modules
while Phase 19 is active. Focused checks use the existing compiler directly and
write only below the provisional output directory.

### Post-Phase-19 public ownership

The integration audit confirms exact repository locations before promotion.
The intended module boundaries are:

```text
stdlib/std/math
stdlib/rocket/motion
stdlib/rocket/raylib/safe
stdlib/rocket/graphics/{geometry,color,shapes,textures,typography,input,virtual_canvas}
stdlib/rocket/ui/{context,layout,theme,styles,controls,containers}
stdlib/rocket/assets
tools/rocket-visual-compare
```

If the final Phase 19 SDK uses a different bundled-module layout, the
integration packet updates these paths once, before any public promotion.

## 4. Per-packet execution protocol

Every new chat must perform this sequence:

- [ ] Read `AGENTS.md`, `docs/PROJECT_CONTEXT.md`, both Rocket 3.0 documents,
  and only the existing code/specifications relevant to the packet.
- [ ] Verify the worktree path, branch, clean starting state, and current
  Phase 19 SHA.
- [ ] Refresh GREEN/YELLOW/RED classification before editing.
- [ ] Confirm no Phase 19 build/test processes are active before running a
  Rocket command.
- [ ] Write focused failing tests for the packet's behavior.
- [ ] Run the tests and capture the expected failure.
- [ ] Implement only the packet's named feature(s).
- [ ] Re-run focused tests and relevant formatting/static checks.
- [ ] Update this plan's packet status and atomic traceability evidence.
- [ ] After all required checks pass, select the lowest-numbered incomplete
  packet whose dependencies are complete and whose isolation state permits it
  to run.
- [ ] Replace the completed packet's label and fenced block under
  `Current next-chat prompt` with a complete prompt for that selected packet.
  If no implementation packet is eligible, install a status-only holding
  prompt that waits for or audits the condition that blocks the next packet.
- [ ] Run `git diff --check` and review the scoped diff.
- [ ] Commit only the packet's files and the plan/status/prompt rotation with
  the prescribed message.
- [ ] Stop. Do not begin the next packet in the same chat.

Prompt rotation is a success-only handoff. If implementation, verification, or
the checkpoint commit fails, do not advance the packet label or replace its
prompt. Record the blocker outside the fenced prompt when useful, leave the
same packet as current, and stop. The permanent launcher in section 10 never
changes during ordinary packet work.

Every replacement prompt must remain self-contained: repeat the exact worktree
and branch, required document reads, clean/Phase-19 overlap checks, packet-only
scope and exclusions, test and generated-output boundaries, resource-safety
rules, checkpoint message, success-only rotation, and stop condition. The small
launcher is a pointer to this full operational prompt, not a substitute for it.

## 5. Packet index

`WP00` is this approved planning checkpoint. `WP01` through `WP08` are the only
implementation packets eligible while Phase 19 is active. Every later packet
is `RED` until `WP09` accepts the final Phase 19 baseline.

| Packet | Feature groups | Maximum scope | Current state |
| --- | --- | --- | --- |
| WP00 | F01, F30 planning | Requirements and live plan only | COMPLETE / PROVISIONAL |
| WP01 | F12, F15 | Provisional geometry and hit-testing kernels | COMPLETE / PROVISIONAL |
| WP02 | F13 | Provisional color kernel | COMPLETE / PROVISIONAL |
| WP03 | F19, F17 | Provisional layout and VirtualCanvas math | READY / GREEN |
| WP04 | F20 | Provisional theme/style data | READY / GREEN |
| WP05 | F18, F25 | Provisional widget IDs and bounded state | READY / GREEN |
| WP06 | F06 | Provisional reduced-motion state kernel only | BLOCKED BY WP05 / GREEN |
| WP07 | F27 | Raw-RGBA comparator kernel and synthetic fixtures | READY / GREEN |
| WP08 | F26, F27 | Provisional metric/golden schemas and synthetic budgets | BLOCKED BY WP07 / GREEN |
| WP09 | F01, F29 | Final Phase 19 integration audit and selective promotion | WAIT FOR PHASE 19 / RED |
| WP10 | F02 | Named arguments | WAIT FOR WP09 / RED |
| WP11 | F03 | Default arguments | WAIT FOR WP10 / RED |
| WP12 | F04 | Complete `std.math` | WAIT FOR WP11 / RED |
| WP13 | F05, F06 | Easing plus complete motion/timelines | WAIT FOR WP12 / RED |
| WP14 | F07 | Safe raylib geometry expansion | WAIT FOR WP09 / RED |
| WP15 | F08 | Advanced textures/filtering | WAIT FOR WP14 / RED |
| WP16 | F09 | Render targets, clipping, blending | WAIT FOR WP15 / RED |
| WP17 | F10 | Safe shader subset | WAIT FOR WP16 / RED |
| WP18 | F11 | Window/display/rendering quality | WAIT FOR WP16 / RED |
| WP19 | F12, F13 | Public graphics core types and Color | WAIT FOR WP11, WP12 / RED |
| WP20 | F14, F15 | Public shapes and input/hit testing | WAIT FOR WP14, WP19 / RED |
| WP21 | F17 | Integrated `VirtualCanvas` | WAIT FOR WP16, WP18, WP20 / RED |
| WP22 | F16 | Typography | WAIT FOR WP15, WP19 / RED |
| WP23 | F18 | Public UI context/response/IDs | WAIT FOR WP20, WP21 / RED |
| WP24 | F19 | Public layout integration | WAIT FOR WP23 / RED |
| WP25 | F20 | Public themes/styles | WAIT FOR WP22, WP24 / RED |
| WP26 | F21 | Controls | WAIT FOR WP23, WP25 / RED |
| WP27 | F22 | Containers/dialogs/transient UI | WAIT FOR WP16, WP26 / RED |
| WP28 | F23 | Typed asset store | WAIT FOR WP15, WP17, WP22 / RED |
| WP29 | F24 | Unified errors and lifetime hardening | WAIT FOR WP17, WP27, WP28 / RED |
| WP30 | F25 | Bounded-state/cache integration | WAIT FOR WP22, WP27, WP28 / RED |
| WP31 | F26 | Performance instrumentation and budgets | WAIT FOR WP30 / RED |
| WP32 | F27 | Visual scenes, image I/O, goldens, CI | WAIT FOR WP17, WP31 / RED |
| WP33 | F28 | Focused examples and premium showcase | WAIT FOR WP32 / RED |
| WP34 | F29 | Full compiler/platform/compatibility acceptance | WAIT FOR WP33 / RED |
| WP35 | F30 | Documentation, release, traceability closure | WAIT FOR WP34 / RED |

## 6. Provisional work packets

### WP00 - Requirements and implementation-plan checkpoint

**Features:** F01 and F30 planning only.

**Files:**

- Create: `docs/ROCKET_3_0_GRAPHICS_UI_REQUIREMENTS.md`
- Create: `docs/ROCKET_3_0_GRAPHICS_UI_IMPLEMENTATION_PLAN.md`

**Acceptance:**

- [x] Exactly 30 feature groups and 148 unique atomic requirements exist.
- [x] Every original graphics/UI/language requirement maps to a feature group.
- [x] GREEN/YELLOW/RED and maturity rules are explicit.
- [x] Work packets contain at most two feature groups.
- [x] The permanent launcher, prompt-rotation contract, and initial WP01 prompt
  are included verbatim below.
- [x] `git diff --check` passes with only the two planning files staged.

**Checkpoint:** `docs: plan isolated Rocket 3.0 graphics and UI work`

### WP01 - Provisional geometry and hit testing

**Features:** F12 geometry subset and F15 pure hit testing.

**Files:**

- Create: `experiments/rocket3_foundation/rocket.toml`
- Create: `experiments/rocket3_foundation/README.md`
- Create: `experiments/rocket3_foundation/src/main.rocket`
- Create: `experiments/rocket3_foundation/src/geometry.rocket`
- Create: `experiments/rocket3_foundation/src/hit_testing.rocket`
- Create: `experiments/rocket3_foundation/tests/geometry_test.rocket`
- Create: `experiments/rocket3_foundation/tests/hit_testing_test.rocket`
- Modify: this plan's packet status and traceability evidence only.

**Provisional interfaces:**

```text
Vec2(x: Float, y: Float)
Size(width: Float, height: Float)
Rect(x: Float, y: Float, width: Float, height: Float)

vec2_add(left, right) -> Vec2
vec2_subtract(left, right) -> Vec2
vec2_scale(value, factor) -> Vec2
rect_is_finite(value) -> Bool
rect_has_nonnegative_size(value) -> Bool
rect_normalized(value) -> Rect
rect_translate(value, offset) -> Rect
rect_intersection(left, right) -> Option[Rect]
point_in_rect(point, bounds) -> Bool
point_in_circle(point, center, radius) -> Bool
```

These names are local to the provisional package and are not Rocket 3.0 public
API commitments.

**Behavior:**

- Rect containment includes left/top and excludes right/bottom.
- A zero-area or non-finite rectangle contains no point.
- `rect_normalized` moves a negative extent to the opposite edge and returns a
  nonnegative size; it does not authorize negative drawing geometry.
- Rect intersection returns `None` for zero-area contact.
- Circle containment includes the circumference, returns false for a negative
  or non-finite radius, and compares squared distance without square root.

**Tests:** exact vectors cover positive/negative coordinates, edges/corners,
zero sizes, normalization, disjoint/touching/overlapping intersections, circle
center/circumference/outside, negative radius, and large finite coordinates.

**Focused commands:** use the existing compiler from the provisional checkout
to check/test only this package and direct all generated state to
`out/rocket3-provisional/wp01`. Do not add top-level CMake/CTest registration.

**Checkpoint:** `feat: add provisional Rocket 3 geometry kernels`

**WP01 evidence (2026-08-29):** The isolated `rocket3_foundation_provisional`
package provides local `Vec2`, `Size`, and `Rect` values plus the F12 geometry
math and F15 pure hit-testing kernels. The required red run failed because
`src.geometry` and `src.hit_testing` did not yet exist. The focused native
suite then passed `geometry_test.rocket` and `hit_testing_test.rocket` (2/2),
with `ROCKET_ARTIFACT_ROOT` set below `out/rocket3-provisional/wp01`; the
focused formatter check also passed. This is PROVISIONAL kernel evidence only:
there is no public API, SDK registration, native input integration, renderer,
or final non-finite-input constructor surface.

### WP02 - Provisional color kernel

**Feature:** F13.

**Files:** create `src/color.rocket` and `tests/color_test.rocket` inside the
provisional package; update only this plan's evidence.

**Scope:** RGBA channel model; RGB/RGBA/hex/HSV construction; alpha replacement;
mix/lerp; lighten/darken; saturate/desaturate; deterministic clamping and hue
wrapping. Use stable test vectors and return `Result` for malformed hex. Do not
bind raylib color values or freeze public constructor spelling.

**Checkpoint:** `feat: add provisional Rocket 3 color kernel`

**WP02 evidence (2026-08-29):** The isolated
`rocket3_foundation_provisional` package now provides a local RGBA `Color`
kernel with deterministic RGB/RGBA, optional-`#` six-digit `RRGGBB` and
eight-digit `RRGGBBAA` hex construction (six digits default alpha to `1.0`),
and HSV construction; alpha replacement; clamped mix/lerp, lighten/darken, and
saturation adjustment; and `Result` failures for malformed hex. Non-finite
channels clamp deterministically (`+infinity` to `1.0`, negative infinity and
NaN to `0.0`); non-finite hue becomes `0.0`, while finite hue uses bounded
binary range reduction. The required red run failed because `src.color` did
not yet exist. The final focused native package suite passed `color_test.rocket`,
`geometry_test.rocket`, and `hit_testing_test.rocket` (3/3), with
`ROCKET_ARTIFACT_ROOT` set below `out/rocket3-provisional/wp02`; the focused
formatter check also passed. This remains PROVISIONAL kernel evidence only: no
raylib conversion, renderer, public SDK module, final constructor spelling, or
named/default-argument surface is frozen.

### WP03 - Provisional layout and VirtualCanvas mathematics

**Features:** F19 and the pure F17 subset.

**Files:** create `layout.rocket`, `virtual_canvas.rocket`, and their focused
tests in the provisional package.

**Scope:** nine anchors; fixed/fill/content/percentage sizing calculations;
Insets, padding, margin, and gap math; deterministic Row/Column/Grid/Stack
placement; aspect-preserving viewport, scale, bars, and reversible coordinate
mapping. No render textures, window input, native calls, or public UI types.

**Checkpoint:** `feat: add provisional Rocket 3 layout and canvas kernels`

### WP04 - Provisional theme and style data

**Feature:** F20.

**Files:** create `theme.rocket` and `theme_test.rocket`.

**Scope:** internal semantic tokens, style-state data, deterministic override/
merge resolution, and validation. Do not freeze final module names,
constructors, style hierarchy, inheritance, or named/default call surfaces.

**Checkpoint:** `feat: add provisional Rocket 3 theme data`

### WP05 - Provisional widget IDs and bounded state

**Features:** F18 ID subset and F25.

**Files:** create `widget_state.rocket` and `widget_state_test.rocket`.

**Scope:** deterministic hierarchical ID composition, duplicate detection,
frame-seen tracking, bounded capacity, unseen-frame eviction, active/focus
cleanup, and a 100,000-ID stress fixture. Capacity values remain measured
configuration, not public constants. No native input or public `Context` API.

**Checkpoint:** `feat: add provisional Rocket 3 widget state kernel`

### WP06 - Provisional reduced-motion kernel

**Feature:** F06 reduced-motion subset.

**Files:** create `reduced_motion.rocket` and `reduced_motion_test.rocket`.

**Scope:** classify essential/nonessential transitions; snap nonessential
motion to the final state; define deterministic progress for allowed reduced
transitions; test zero/negative duration and large deltas. Do not implement the
public timeline API or arbitrary state mutation.

**Checkpoint:** `feat: add provisional Rocket 3 reduced motion policy`

### WP07 - Provisional visual-comparator kernel

**Feature:** F27 raw-data subset.

**Files:**

- Create: `experiments/rocket3_visual_compare/CMakeLists.txt`
- Create: `experiments/rocket3_visual_compare/README.md`
- Create: `experiments/rocket3_visual_compare/src/comparator.h`
- Create: `experiments/rocket3_visual_compare/src/comparator.cpp`
- Create: `experiments/rocket3_visual_compare/tests/comparator_tests.cpp`

The experiment is a standalone C++20 target with no external image dependency,
is configured directly from its own directory, and writes build/test output to
`out/rocket3-provisional/wp07`. It is not included by the repository's top-level
CMake project.

**Scope:** equal-sized raw RGBA buffers; ignored per-channel delta; mean absolute
error; changed-pixel ratio; difference/heat values; changed-region bounds; and
deterministic error results for invalid dimensions or buffer lengths. PNG,
raylib capture, top-level build registration, approved goldens, and CI are not
part of this packet.

**Checkpoint:** `feat: add provisional Rocket 3 visual comparator kernel`

### WP08 - Provisional performance and golden schemas

**Features:** F26 plus F27 infrastructure subset.

**Files:** add versioned schema definitions, synthetic fixtures, and focused
tests only within the provisional comparator/foundation areas.

**Scope:** deterministic scene/metric identity; environment metadata; allocation,
cache, upload, switch, and FFI counters; comparison thresholds as measured data;
failure-artifact manifest; explicit golden approval/update record. Do not set
final budgets or register global performance/visual tests.

**Checkpoint:** `test: define provisional Rocket 3 visual performance evidence`

## 7. Post-Phase-19 packets

Each packet below receives its own new chat and a packet-specific detailed TDD
checklist after `WP09` refreshes exact final paths and commands.

### WP09 - Final baseline audit and selective promotion

**Features:** F01 and F29.

Record the final Phase 19 tag/version/commit, clean status, targets, toolchain,
runtime ABI, SDK/module layout, packages, raylib, tests, release evidence, and
frozen hashes. Create a fresh `codex/rocket-3-integration` branch from that tag.
Review every provisional commit against the final architecture, then cherry-pick
only valid commits. Rework or discard the rest. Run focused compatibility checks
before marking promoted kernels `INTEGRATION-READY`.

**Checkpoint:** `chore: establish final Rocket 3 integration baseline`

### WP10 - Named arguments

**Feature:** F02. Implement the complete grammar-to-tooling vertical slice in
stage0 and self-host, including metadata, diagnostics, formatter, LSP/editor
support where present, tests, specifications, and deterministic bootstrap.

**Checkpoint:** `feat: add named arguments`

### WP11 - Default arguments

**Feature:** F03. Implement declaration checking, evaluation order, generic and
cross-module behavior, positional normalization before MIR ABI calls, tooling,
negative tests, specifications, and deterministic bootstrap.

**Checkpoint:** `feat: add default arguments`

### WP12 - Complete `std.math`

**Feature:** F04. Implement the full required function inventory, target/domain
semantics, stage0/self-host parity, runtime/backend work only where necessary,
numeric vectors, docs, compatibility, and bootstrap evidence.

**Checkpoint:** `feat: add complete standard math module`

### WP13 - Easing and complete motion

**Features:** F05 and F06. Promote reduced-motion kernels, implement every
easing family, Float/Vec2/Color tweens, delay/sequence/parallel/repeat/yoyo, and
the convenience constructors with explicit application-state ownership.

**Checkpoint:** `feat: add Rocket motion and easing`

### WP14 - Safe raylib geometry

**Feature:** F07. Expand the reviewed native adapter and safe Rocket module for
every required shape; add token/state/geometry validation, deterministic test
backend behavior, native tests, package generation, and all-target acceptance.

**Checkpoint:** `feat: expand safe raylib geometry`

### WP15 - Advanced textures and filtering

**Feature:** F08. Add source/destination/pivot/rotation/tint drawing, filtering
capabilities/fallbacks, resource validation, deterministic backend tests, and
native target evidence.

**Checkpoint:** `feat: add advanced safe texture drawing`

### WP16 - Render targets, clipping, and blending

**Feature:** F09. Add checked render textures and scoped target/scissor/blend
state machines with nesting, cleanup, compositing, and native tests.

**Checkpoint:** `feat: add safe render scopes`

### WP17 - Safe shaders

**Feature:** F10. Add checked shader loading/failure/unload, reviewed uniforms,
scoped use, render-target integration, deterministic tests, and supported-target
capability behavior.

**Checkpoint:** `feat: add safe shader support`

### WP18 - Window and rendering quality

**Feature:** F11. Add DPI/framebuffer/monitor APIs, MSAA4x configuration,
resizing, fullscreen/borderless transitions, screenshots, and target tests.

**Checkpoint:** `feat: add display quality controls`

### WP19 - Public graphics types and Color

**Features:** F12 and F13. Promote accepted kernels into final `rocket.graphics`
modules; freeze exact constructors, parameter names/defaults, ownership, errors,
documentation, and public compatibility tests.

**Checkpoint:** `feat: add Rocket graphics core types`

### WP20 - Public shapes and input/hit testing

**Features:** F14 and F15. Implement ergonomic shapes over the safe backend and
connect pointer states/hit testing to real windows while preserving logical
coordinate behavior and outside-viewport safety.

**Checkpoint:** `feat: add Rocket shapes and input helpers`

### WP21 - Integrated `VirtualCanvas`

**Feature:** F17. Connect promoted math to render targets, clipping, pointer
mapping, screenshots, resizing, fullscreen, and DPI across supported targets.

**Checkpoint:** `feat: add Rocket virtual canvas`

### WP22 - Typography

**Feature:** F16. Implement actual font measurement, baseline/bounds,
horizontal/vertical alignment, wrapping/multiline/clipping/ellipsis, bounded
caching, visual scenes, and invalidation.

**Checkpoint:** `feat: add Rocket typography`

### WP23 - Public UI context and interaction

**Feature:** F18. Promote stable IDs into `Context`, `UiFrame`, and `Response`;
add frame lifecycle, pointer, keyboard, focus, disabled state, modal capture,
duplicate-ID, and misuse detection.

**Checkpoint:** `feat: add Rocket UI context`

### WP24 - Public layout

**Feature:** F19. Promote Row/Column/Grid/Stack/Anchor, sizing, Insets/SafeArea,
spacing, alignment, invalid-layout handling, and deterministic layout tests.

**Checkpoint:** `feat: add Rocket UI layout`

### WP25 - Public themes and styles

**Feature:** F20. Freeze semantic tokens, style objects, state resolution,
constructors/defaults, docs, and compatibility behavior.

**Checkpoint:** `feat: add Rocket UI themes and styles`

### WP26 - Controls

**Feature:** F21. Implement text/image/separator/badge/pill/button/icon-button
behavior with centralized pointer/keyboard/focus/disabled/modal logic.

**Checkpoint:** `feat: add Rocket UI controls`

### WP27 - Containers and transient UI

**Feature:** F22. Implement panels, clipping/shadows, dialogs, overlays,
tooltips, toasts, stacking, focus capture, and nested-scope tests.

**Checkpoint:** `feat: add Rocket UI containers`

### WP28 - Typed asset store

**Feature:** F23. Implement cached typed texture/font/sound/music/shader loading,
lookup/borrowing, path security, stale references, dependency-safe cleanup,
relocation, and target tests.

**Checkpoint:** `feat: add Rocket asset store`

### WP29 - Unified error and lifetime hardening

**Feature:** F24. Audit all prior public layers against the required compile-time,
`Result`, and contract-failure taxonomy; close missing error/lifetime tests
without adding new product scope.

**Checkpoint:** `fix: harden Rocket graphics and UI contracts`

### WP30 - Bounded state and caches

**Feature:** F25. Integrate and calibrate widget/measurement/resource caches,
100,000-ID stress, deterministic eviction, focused-state cleanup, and selective
invalidation.

**Checkpoint:** `perf: bound Rocket UI state and caches`

### WP31 - Performance budgets

**Feature:** F26. Add full instrumentation, warm-up/steady-state scenes,
environment records, calibrated budgets, and regression enforcement.

**Checkpoint:** `perf: enforce Rocket graphics and UI budgets`

### WP32 - Visual regression system

**Feature:** F27. Add image I/O/capture, all canonical scenes, strict Windows
goldens, portability subsets/metrics, review-only golden updates, CI, and failure
artifacts.

**Checkpoint:** `test: add Rocket visual regression suite`

### WP33 - Examples and showcase

**Feature:** F28. Add every focused example and the neutral premium card-table
showcase using only public APIs; validate packaging, relocation, and visuals.

**Checkpoint:** `docs: add Rocket 3 graphics and UI examples`

### WP34 - Full compatibility and platform acceptance

**Feature:** F29. Run fresh Debug/Release, stage0, stage1-stage3, conformance,
compatibility, stdlib, native, ownership, package, application, hardening,
performance, visual, installation, relocation, and supported-target matrices.
Record exact commands/counts/hashes/timings; fix real defects without weakening
gates.

**Checkpoint:** `test: accept Rocket 3 across supported targets`

### WP35 - Documentation and Rocket 3.0 release

**Feature:** F30. Complete every required specification/reference/migration/
release/example update, close atomic traceability, list intentional limitations,
build reproducible packages, verify checksums/provenance/signing-where-present,
and tag only after a clean accepted release state.

**Checkpoint:** `release: complete Rocket 3.0`

## 8. Atomic traceability summary

This table is the group-level index. Each packet expands its atomic IDs with
file/test/doc/evidence links when executed.

| Feature | Owning packet(s) | Pre-Phase-19 state | Final acceptance packet |
| --- | --- | --- | --- |
| F01 Governance | WP00, WP09 | documented | WP35 |
| F02 Named arguments | WP10 | RED | WP34 |
| F03 Default arguments | WP11 | RED | WP34 |
| F04 `std.math` | WP12 | RED | WP34 |
| F05 Easing | WP13 | RED | WP34 |
| F06 Motion/reduced motion | WP06, WP13 | kernel GREEN | WP34 |
| F07 Safe geometry backend | WP14 | RED | WP34 |
| F08 Textures/filtering | WP15 | RED | WP34 |
| F09 Render targets/scopes | WP16 | RED | WP34 |
| F10 Shaders | WP17 | RED | WP34 |
| F11 Display quality | WP18 | RED | WP34 |
| F12 Graphics core types | WP01, WP19 | WP01 PROVISIONAL geometry kernel: `experiments/rocket3_foundation/src/geometry.rocket`; focused native test and formatter evidence recorded above | WP34 |
| F13 Color | WP02, WP19 | WP02 PROVISIONAL color kernel: `experiments/rocket3_foundation/src/color.rocket`; focused native package suite and formatter evidence recorded above | WP34 |
| F14 Shape API | WP20 | RED | WP34 |
| F15 Input/hit testing | WP01, WP20 | WP01 PROVISIONAL pure hit kernel: `experiments/rocket3_foundation/src/hit_testing.rocket`; focused native test and formatter evidence recorded above | WP34 |
| F16 Typography | WP22 | RED | WP34 |
| F17 VirtualCanvas | WP03, WP21 | math kernel GREEN | WP34 |
| F18 UI context/IDs | WP05, WP23 | ID kernel GREEN | WP34 |
| F19 Layout | WP03, WP24 | kernel GREEN | WP34 |
| F20 Themes/styles | WP04, WP25 | data kernel GREEN | WP34 |
| F21 Controls | WP26 | RED | WP34 |
| F22 Containers/transient UI | WP27 | RED | WP34 |
| F23 Asset store | WP28 | RED | WP34 |
| F24 Errors/lifetimes | WP29 | RED | WP34 |
| F25 Bounded state | WP05, WP30 | kernel GREEN | WP34 |
| F26 Performance | WP08, WP31 | schema GREEN | WP34 |
| F27 Visual regression | WP07, WP08, WP32 | comparator/schema GREEN | WP34 |
| F28 Examples/showcase | WP33 | RED | WP34 |
| F29 Platform/compatibility | WP09, WP34 | RED | WP34 |
| F30 Docs/release/traceability | WP00, WP35 | planning only | WP35 |

## 9. Current next-chat prompt

This is the single mutable handoff slot. It must contain exactly one current
packet label and one fenced `text` prompt. A successful packet replaces both
with the next eligible packet before committing. A failed or blocked packet
does not rotate this slot. Do not preserve completed prompts here; Git history
is their archive.

**Current packet:** WP03 - Provisional layout and VirtualCanvas mathematics

```text
Work only in this existing isolated worktree:
C:\Users\Administrator\Desktop\Projects\Rocket\out\worktrees\rocket-3-provisional

Set that path as the working directory first. Then read AGENTS.md,
docs/PROJECT_CONTEXT.md, docs/ROCKET_3_0_GRAPHICS_UI_REQUIREMENTS.md, and
docs/ROCKET_3_0_GRAPHICS_UI_IMPLEMENTATION_PLAN.md.

Verify that the branch is codex/rocket-3-provisional and begin from a clean
worktree. Phase 19 is still active on master. Refresh the Phase 19 overlap from
the recorded base SHA before editing. Do not modify master, Phase 19 evidence,
compiler/runtime/stdlib registration, native adapters, top-level CMake/CTest,
SDK/package outputs, version files, tags, or release artifacts. If WP03 now
touches a Phase-19-owned file or contract, classify it RED and stop with the
evidence.

Execute only WP03: provisional layout and VirtualCanvas mathematics (F19 and
the pure F17 subset). Use test-driven development and the exact provisional
behavior in the plan. Create only
experiments/rocket3_foundation/src/layout.rocket,
experiments/rocket3_foundation/src/virtual_canvas.rocket, and their focused
tests in the same provisional package, plus this plan's WP03-only status,
traceability evidence, and prompt rotation. Keep all APIs explicitly
provisional; do not freeze final Rocket 3.0 constructors, named/default
arguments, module layout, renderer conversion, or public SDK surface.

Before running Rocket commands, confirm no Phase 19 build/test processes are
active. Run commands sequentially, write generated state only below
out/rocket3-provisional/wp03 inside this worktree, never automatically retry a
timeout, and stop/report if a task process exceeds 4 GiB or keeps growing.

Implement and test nine anchors; fixed/fill/content/percentage sizing
calculations; Insets, padding, margin, and gap math; deterministic
Row/Column/Grid/Stack placement; and aspect-preserving viewport, scale, bars,
and reversible coordinate mapping. Do not add render textures, window input,
native calls, public UI types, or later-packet work. Run focused checks, update
WP03's packet-index state and atomic traceability evidence, then follow the
prompt-rotation contract in section 4. The next eligible packet will normally
be WP04: replace the current packet label and this entire fenced block with a
complete WP04 prompt. Run git diff --check, review the scoped diff, and commit
the WP03 files plus plan/status/prompt rotation together with:

feat: add provisional Rocket 3 layout and canvas kernels

Only rotate after every required WP03 check passes. If implementation,
verification, or commit fails, leave WP03 as the current prompt and report the
blocker. After a successful commit, stop and report the exact files, tests,
results, commit, remaining intentional provisional limitations, and the packet
prepared for the next chat. Do not begin WP04 in the same chat.
```

## 10. Permanent reusable launcher

The owner may send this exact message at the start of every Rocket 3.0 packet
chat. Do not customize it for individual packets; section 9 supplies the
changing scope.

```text
Work only in the existing Rocket 3.0 provisional worktree. Read AGENTS.md and
both Rocket 3.0 planning documents, then execute exactly the "Current next-chat
prompt" in the implementation plan. After successful completion, replace it
with the next eligible packet's prompt, commit everything, and stop.
```
