# Rocket 3.0 Graphics, UI, and Language-Ergonomics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: use
> `superpowers:test-driven-development` for implementation packets and
> `superpowers:verification-before-completion` before every checkpoint. Use
> `superpowers:subagent-driven-development` only when the owner explicitly asks
> for subagents. Each work packet is executed in a fresh chat and contains no
> more than two feature groups.

**Goal:** Deliver Rocket 3.0's additive language ergonomics and complete, safe,
portable, game-oriented graphics/UI stack on the accepted Rocket 2.1 baseline.

**Architecture:** Rocket 3.0 work proceeds directly on `master`, which contains
the accepted Rocket 2.1 portability baseline. Foundation kernels stay intentionally internal until their
named public-integration packets; compiler, native, SDK, platform, package, and
release work are available whenever a packet owns them. WP09 audits the rebased
foundation before public promotion, and all work remains in narrow vertical
packets.

**Tech stack:** Rocket, C++20 permanent stage0, Rocket self-hosted compiler,
LLVM 22.1.6, raylib 6.0, CMake/Ninja, native target SDKs inherited from Rocket
2.1.

**Spec:** `docs/ROCKET_3_0_GRAPHICS_UI_REQUIREMENTS.md`

## Global constraints

- Rocket 3.0 is the target release. Its accepted predecessor is Rocket 2.1 at
  `19596db860d4105d2226c98be2693edc5632aaf0`.
- Preserve every valid predecessor program and runtime ABI v1 unless the owner
  separately approves a specific Rocket 3.0 incompatibility decision.
- Preserve C++20 stage0, self-host equivalence, and deterministic stage2/stage3
  bootstrap.
- Phase 19 is complete. A packet may modify any Rocket subsystem it owns,
  including compiler, runtime, standard-library, native, SDK, build, package,
  or release code; it must still preserve predecessor compatibility and meet its
  named validation scope.
- Do not introduce cloud, paid, subscription, API-key, or per-use graphics
  dependencies. raylib 6.0 remains the renderer.
- Do not migrate Scroll2Roll in this plan.
- Run builds and tests sequentially. Stop and report a process over 4 GiB or one
  that grows continuously.
- A packet handles one or two feature groups, updates its traceability rows,
  rotates the current next-chat prompt, commits those changes together, pushes
  `master` to `origin`, and stops.

---

## 1. Current integrated baseline

| Field | Recorded value |
| --- | --- |
| Active branch | `master` |
| Active checkout | `C:\Users\Administrator\Desktop\Projects\Rocket` |
| Accepted Rocket 2.1 baseline | `19596db860d4105d2226c98be2693edc5632aaf0` (`Refresh Phase 19 roadmap and context`) |
| Phase 19 state | complete by owner direction on 2026-08-29; Rocket 2.1 portability accepted |
| Branch integration state | Rocket 3.0 foundation commits fast-forwarded into `master` on 2026-08-29 |
| Packet generated output | `out/rocket3-provisional/` inside the active checkout |
| Delivery policy | every successful packet commits and pushes `master` to `origin` |

The accepted baseline is provenance for Rocket 3.0 compatibility. Before every
packet, verify the branch is clean and still contains the accepted baseline:

```powershell
git -C 'C:\Users\Administrator\Desktop\Projects\Rocket' status --short --branch
git -C 'C:\Users\Administrator\Desktop\Projects\Rocket' merge-base --is-ancestor 19596db HEAD
```

The accepted Phase 19 baseline no longer blocks files or tests. Use the packet's
scope and risk classification to select the appropriate compatibility,
bootstrap, platform, package, or release checks.

## 2. Maturity and isolation states

### Isolation

- `GREEN`: standalone deterministic code or tooling with focused tests.
- `YELLOW`: change that crosses an existing subsystem and requires focused
  regression coverage in that subsystem.
- `RED`: compiler/bootstrap, ABI, supported-target, package, or release change
  that requires the packet's full acceptance evidence before handoff.

### Maturity

- `PROVISIONAL`: tested internal kernel or tool; public
  API and release support are not claimed.
- `INTEGRATION-READY`: audited against the accepted Rocket 2.1 baseline and
  compatibility checked.
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

### Foundation files and generated output

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

These directories are explicitly non-SDK, non-release, and non-public until
their named public-integration packets. They are not registered in top-level
CMake/CTest or added to packaged standard modules until those packets own that
registration. Focused checks write only below the packet output directory.

### Intended public ownership

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

WP09 confirms the accepted Rocket 2.1 SDK layout before public promotion and
updates these locations only when the current repository requires it.

## 4. Per-packet execution protocol

Every new chat must perform this sequence:

- [ ] Read `AGENTS.md`, `docs/PROJECT_CONTEXT.md`, both Rocket 3.0 documents,
  and only the existing code/specifications relevant to the packet.
- [ ] Verify the active checkout, `master`, clean starting state, and accepted
  Rocket 2.1 baseline.
- [ ] Verify `master` has no unpushed checkpoint; push any existing
  committed work before editing.
- [ ] Refresh GREEN/YELLOW/RED classification before editing.
- [ ] Confirm no Rocket build/test processes from this task are active before
  running a Rocket command.
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
  the prescribed message, then push `master` to `origin`.
- [ ] Stop after a successful push. Do not begin the next packet in the same
  chat.

Prompt rotation is a success-only handoff. If implementation or verification
fails, do not advance the packet label or replace its prompt. Record the blocker
outside the fenced prompt when useful, leave the same packet as current, and
stop. If the checkpoint commit succeeds but push fails, do not begin a new
packet: preserve the committed rotation and resolve the push before continuing.
The permanent launcher in section 10 never changes during ordinary packet work.

Every replacement prompt must remain self-contained: repeat the exact checkout
and branch, required document reads, clean/baseline checks, packet-only scope
and exclusions, test and generated-output boundaries, resource-safety rules,
recommended model and reasoning effort, checkpoint message, success-only
rotation, push, and stop condition. The small launcher is a pointer to this full
operational prompt, not a substitute for it.

## 5. Packet index

`WP00` is the planning checkpoint, `WP01` through `WP08` are complete
foundation packets, and WP09 completed their integration audit. WP10, WP11,
the mandatory suffixed successor WP11A, and WP12 are now complete, making WP13
the lowest-numbered eligible packet. Suffixed packets sort immediately after
their numeric packet.

| Packet | Feature groups | Maximum scope | Current state |
| --- | --- | --- | --- |
| WP00 | F01, F30 planning | Requirements and live plan only | COMPLETE / PROVISIONAL |
| WP01 | F12, F15 | Provisional geometry and hit-testing kernels | COMPLETE / PROVISIONAL |
| WP02 | F13 | Provisional color kernel | COMPLETE / PROVISIONAL |
| WP03 | F19, F17 | Provisional layout and VirtualCanvas math | COMPLETE / PROVISIONAL |
| WP04 | F20 | Provisional theme/style data | COMPLETE / PROVISIONAL |
| WP05 | F18, F25 | Provisional widget IDs and bounded state | COMPLETE / PROVISIONAL |
| WP06 | F06 | Provisional reduced-motion state kernel only | COMPLETE / PROVISIONAL |
| WP07 | F27 | Raw-RGBA comparator kernel and synthetic fixtures | COMPLETE / PROVISIONAL |
| WP08 | F26, F27 | Provisional metric/golden schemas and synthetic budgets | COMPLETE / PROVISIONAL |
| WP09 | F01, F29 | Rocket 2.1 baseline audit and foundation integration | COMPLETE / INTEGRATION-READY |
| WP10 | F02 | Named arguments | COMPLETE / GREEN |
| WP11 | F03 | Default arguments | COMPLETE / GREEN |
| WP11A | F02 | Complete named-callable parity | COMPLETE / GREEN |
| WP12 | F04 | Complete `std.math` | COMPLETE / LOCAL-GREEN; target-lab R3-F04-008 pending |
| WP13 | F05, F06 | Easing plus complete motion/timelines | READY / RED |
| WP14 | F07 | Safe raylib geometry expansion | READY / RED |
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

## 6. Foundation work packets

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

**WP03 evidence (2026-08-29):** The isolated
`rocket3_foundation_provisional` package now provides local, deterministic F19
layout and pure F17 VirtualCanvas kernels. Layout covers the nine anchor
positions; fixed, fill, content, and percentage sizing; Insets for padding and
margin composition; gap-aware Row, Column, and Grid item placement; and Stack
placement. VirtualCanvas computes an aspect-preserving viewport, scale,
letterbox/pillarbox bars, logical-to-physical conversion, and an explicit
outside-viewport `None` result for physical-to-logical mapping; invalid or
non-finite dimensions return `Err` before division. The required red run failed
because `src.layout` and `src.virtual_canvas` did not yet exist. The final
focused native package suite passed `layout_test.rocket`,
`virtual_canvas_test.rocket`, and the prior geometry/color/hit-testing tests
(5/5), with `ROCKET_ARTIFACT_ROOT` set below
`out/rocket3-provisional/wp03`; the focused formatter check also passed. This
remains PROVISIONAL kernel evidence only: there is no public UI API, native
window/input connection, render texture, SDK registration, renderer conversion,
or final constructor/named/default-argument surface.

### WP04 - Provisional theme and style data

**Feature:** F20.

**Files:** create `theme.rocket` and `theme_test.rocket`.

**Scope:** internal semantic tokens, style-state data, deterministic override/
merge resolution, and validation. Do not freeze final module names,
constructors, style hierarchy, inheritance, or named/default call surfaces.

**Checkpoint:** `feat: add provisional Rocket 3 theme data`

**WP04 evidence (2026-08-29):** The main `master` checkout now provides a
local, explicitly provisional F20 theme kernel in
`experiments/rocket3_foundation/src/theme.rocket`. `TokenSet` stores semantic
color, spacing, radius, typography-size, and motion-duration values;
`TokenOverride` represents explicit per-token state overrides; and
`StyleStates` stores normal, hovered, pressed, disabled, and focused data.
Merge resolution is deterministic and preserves base values for omitted
overrides. Validation rejects non-finite or negative numeric tokens, invalid
color channels, and invalid active override values; state resolution rejects
unknown state identifiers. The required red run failed because `src.theme`
did not exist. The final focused native package suite passed
`theme_test.rocket`, `color_test.rocket`, `geometry_test.rocket`,
`hit_testing_test.rocket`, `layout_test.rocket`, and
`virtual_canvas_test.rocket` (6/6), with `ROCKET_ARTIFACT_ROOT` set below
`out/rocket3-provisional/wp04`; the focused formatter check and
`git diff --check` also passed. This remains PROVISIONAL data-kernel evidence:
there are no public constructors, named/default arguments, inheritance,
renderer conversion, native calls, UI controls, or SDK registration.

### WP05 - Provisional widget IDs and bounded state

**Features:** F18 ID subset and F25.

**Files:** create `widget_state.rocket` and `widget_state_test.rocket`.

**Scope:** deterministic hierarchical ID composition, duplicate detection,
frame-seen tracking, bounded capacity, unseen-frame eviction, active/focus
cleanup, and a 100,000-ID stress fixture. Capacity values remain measured
configuration, not public constants. No native input or public `Context` API.

**Checkpoint:** `feat: add provisional Rocket 3 widget state kernel`

**WP05 evidence (2026-08-29, corrected):** The main `master` checkout provides
a local, explicitly provisional F18/F25 widget-state kernel in
`experiments/rocket3_foundation/src/widget_state.rocket`. Hierarchical IDs
encode byte-length-prefixed parent and child boundaries, so embedded separators
or raw parent strings cannot alias a different hierarchy. Active/focused state retains and validates the complete
`(path, hash)` identity rather than a hash alone, including deliberate hash
collisions. Capacity is validated from 1 through 100,000 and registrations
beyond capacity are rejected. The bulk stress helper now builds a balanced set
of 100,000 distinct deterministic IDs, rejects a same-frame re-registration as
a duplicate, and uses balanced concatenation again during full-frame retention;
it does not fabricate repeated path/hash pairs. Corrective regressions first
failed for ambiguous slash/raw-parent composition, missing full-identity state
behavior, and the false bulk-duplicate claim. The final focused native package suite
passed `widget_state_test.rocket`, `theme_test.rocket`, `color_test.rocket`,
`geometry_test.rocket`, `hit_testing_test.rocket`, `layout_test.rocket`, and
`virtual_canvas_test.rocket` (7/7), with `ROCKET_ARTIFACT_ROOT` set below
`out/rocket3-provisional/wp05/finalfix`; the focused formatter check and
`git diff --check` passed. This remains PROVISIONAL kernel evidence only: there
is no public `Context`, `UiFrame`, `Response`, native input, keyboard/focus
system, modal capture, or public SDK surface.

### WP06 - Provisional reduced-motion kernel

**Feature:** F06 reduced-motion subset.

**Files:** create `reduced_motion.rocket` and `reduced_motion_test.rocket`.

**Scope:** classify essential/nonessential transitions; snap nonessential
motion to the final state; define deterministic progress for allowed reduced
transitions; test zero/negative duration and large deltas. Do not implement the
public timeline API or arbitrary state mutation.

**Checkpoint:** `feat: add provisional Rocket 3 reduced motion policy`

**WP06 evidence (2026-08-29):** The main `master` checkout now provides a
local, explicitly provisional F06 reduced-motion policy in
`experiments/rocket3_foundation/src/reduced_motion.rocket`. `Policy` keeps
reduced-motion mode and the explicit essential-transition allowance;
`Transition` and `Sample` keep application state value-owned. Nonessential
transitions snap to their final value in reduced mode, while allowed essential
transitions use deterministic linearly clamped progress. Zero/negative or
non-finite durations complete at the final value, and negative or very large
elapsed deltas are bounded. The required red run failed because
`src.reduced_motion` did not yet exist. The final focused native package suite
passed `reduced_motion_test.rocket` plus the prior foundation tests (8/8), and
the focused formatter check passed with `ROCKET_ARTIFACT_ROOT` set below
`out/rocket3-provisional/wp06`. This remains PROVISIONAL kernel evidence only:
there is no public timeline API, easing family, tween constructor, arbitrary
object mutation, native integration, or public SDK surface.

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

**WP07 evidence (2026-08-29):** The standalone C++20 experiment in
`experiments/rocket3_visual_compare/` provides a provisional raw-RGBA
comparator with equal-dimension and buffer-length validation, an ignored
per-channel threshold, mean absolute error, changed-pixel ratio, maximum
channel delta, raw difference bytes, thresholded heat bytes, and changed-region
bounds. Invalid dimensions and lengths return deterministic explicit errors.
The required red configure failed because `src/comparator.cpp` did not yet
exist. The final sequential Ninja build succeeded and CTest passed
`comparator_tests` (1/1, 100%) under `out/rocket3-provisional/wp07`; no
external image dependency, raylib capture, top-level CMake/CTest registration,
golden references, or CI was added. This remains PROVISIONAL comparator-kernel
evidence only: PNG/image I/O, capture, approved goldens, budgets, and final
visual-regression integration are deferred. The later corrective regression
replaced `assert`-only checks with always-active test checks: an intentionally
wrong bounds expectation failed in a Release (`NDEBUG`) build, and both Debug
and Release CTest runs then passed 1/1 with the correct expectation.

### WP08 - Provisional performance and golden schemas

**Features:** F26 plus F27 infrastructure subset.

**Files:** add versioned schema definitions, synthetic fixtures, and focused
tests only within the provisional comparator/foundation areas.

**Scope:** deterministic scene/metric identity; environment metadata; allocation,
cache, upload, switch, and FFI counters; comparison thresholds as measured data;
failure-artifact manifest; explicit golden approval/update record. Do not set
final budgets or register global performance/visual tests.

**Checkpoint:** `test: define provisional Rocket 3 visual performance evidence`

**WP08 evidence (2026-08-29):** The standalone C++20 comparator experiment now
provides versioned value-only evidence schemas in
`experiments/rocket3_visual_compare/src/evidence_schema.h` and `.cpp`. The
schemas define deterministic scene and metric identities, environment metadata,
allocation/cache/upload/state-switch/FFI counters, measured (not final-budget)
thresholds, explicit failure-artifact manifest entries, and a golden approval
record whose synthetic fixture defaults to `Proposed` with automatic updates
disabled. `evidence_schema_tests.cpp` covers deterministic identity,
environment fields, all counters, threshold pass/fail boundaries, manifest
entries, and golden approval state. The required red configure/build first
failed because `src/evidence_schema.cpp` did not yet exist. The final sequential
MSVC/Ninja build and CTest run passed `comparator_tests` and
`evidence_schema_tests` (2/2); the focused provisional foundation package
regression passed all 8 tests after the identical command was rerun with
narrow elevation for the known `clang.exe: permission denied` linker issue; and
the focused formatter check passed with `ROCKET_ARTIFACT_ROOT` below
`out/rocket3-provisional/wp08`. This remains PROVISIONAL schema evidence only:
there are no final performance budgets, PNG/image capture, global visual or
performance registration, automatic golden updates, CI gates, or public SDK
integration.

## 7. Public integration and release packets

Each packet below receives its own new chat and a packet-specific detailed TDD
checklist after its dependencies establish the exact current paths and commands.

### WP09 - Rocket 2.1 baseline audit and foundation integration

**Features:** F01 and F29.

Record the accepted Rocket 2.1 commit, clean status, targets, toolchain, runtime
ABI, SDK/module layout, packages, raylib, tests, release evidence, and frozen
hashes. Review each rebased foundation commit against current architecture;
retain, rework, or discard it based on interface and compatibility evidence. Run
focused compatibility checks before marking valid kernels `INTEGRATION-READY`.

**Checkpoint:** `chore: establish final Rocket 3 integration baseline`

**WP09 evidence (2026-08-29):** The audit began from clean, synchronized
`master` at `08cdb92ca509b4f8063ae9adb2cc26ccf9188557`, with accepted Rocket
2.1 baseline `19596db860d4105d2226c98be2693edc5632aaf0` confirmed as an
ancestor. The permanent C++20 stage0 remains in `src/`, the Rocket-written
compiler remains `compiler/src/main.rocket`, runtime ABI v1 remains declared
by `src/runtime.h` and implemented by `src/runtime.cpp`, virtual standard
modules remain compiler/runtime owned with the authored `std.testing` module
under `stdlib/`, and target packaging remains in
`scripts/phase19_package.py` and `scripts/package-compiler.ps1`. The reviewed
raylib 6.0 surface remains the primitive adapter and safe Rocket package under
`examples/raylib_showcase`; none of the Rocket 3 foundation experiments is
registered in top-level CMake, the SDK, or a release package.

Fresh dependency verification passed with Git 2.54.0.windows.1, CMake 4.3.2,
Ninja 1.13.1, Clang/LLVM 22.1.6, MSVC 19.51.36256 x64, and raylib 6.0. A fresh
sequential Release configure/build completed 97/97 steps below
`out/rocket3-provisional/wp09`. Its `rocketc 2.1.0` reported native
`windows-x64` and triple `x86_64-pc-windows-msvc`; the runtime ABI v1 test
passed 1/1; and the focused compatibility/package/provenance selection passed
20/20 tests. That selection covered Phase 19 release tooling, target queries
and stage0/self-host parity; Rocket 2.0 compatibility and build-cache behavior;
Phase 16 dependency, registry, and self-hosted package workflows; runtime; and
the stage0/self-hosted raylib adapter, reference package, and generated-binding
parity. The complete fresh Release matrix then passed 222/222 tests in 88.82
seconds with zero failures.

The foundation package passed all 8/8 native tests plus its formatter check
with artifacts below the WP09 root. The standalone raw-RGBA comparator and
evidence-schema experiment passed 2/2 CTest tests in both fresh Debug and
Release builds, preserving always-active Release checks. Review of every
rebased foundation commit produced the following disposition:

| Foundation packet | Reviewed commits | WP09 disposition |
| --- | --- | --- |
| WP01 geometry/hit testing | `d1d5be5` | Retain / INTEGRATION-READY pure kernels; public graphics types and native input remain deferred. |
| WP02 Color | `8538712` | Retain / INTEGRATION-READY deterministic value kernel; renderer conversion and final constructors remain deferred. |
| WP03 layout/VirtualCanvas | `eb7c771` | Retain / INTEGRATION-READY pure layout and mapping kernels; window, render-target, DPI, and pointer integration remain deferred. |
| WP04 theme/style data | `5d09776` | Retain / INTEGRATION-READY value and resolution rules; public hierarchy and constructors remain deferred. |
| WP05 widget state | `4def1c9`, `da85f6a`, `875eb39` | Retain / INTEGRATION-READY corrected full-identity, bounded-state kernel; public context, input, focus, and calibrated retention remain deferred. |
| WP06 reduced motion | `bc5c559` | Retain / INTEGRATION-READY policy kernel; easing, tweens, and timelines remain deferred. |
| WP07 comparator | `28cc31a`, `f4bb026` | Retain / INTEGRATION-READY raw-RGBA tool kernel with Release-active checks; image I/O, capture, and CI remain deferred. |
| WP08 evidence schemas | `ee73c35` | Retain / INTEGRATION-READY versioned synthetic schema; final budgets, platform goldens, and automatic integration remain deferred. |

The immutable Rocket 2.0 Windows x64 consumer SDK under
`out/package/rocket-2.0.0-windows-x64` verified all 927 checksum records and
runtime ABI v1 provenance. Its compiler, runtime, and language-server hashes
remain `d2009a5dd0e6745fff75c902c53323dc72a4fb9ff3c8bea83360fdc675bf618e`,
`94b0fcf3620a2127d5783a128b01a273fac331ad464240e308fc0f23c215630f`,
and `cb47089d26831b000442d12d66129eb25470f20bf736951713a218b38c352406`.
The historical `out/build/windows-release` hashes in the Phase 19 safety table
identify the former Rocket 2.0 build outputs; that mutable build tree now
reports Rocket 2.1.0 and is not the frozen consumer SDK. The accepted Rocket
2.1 Windows package verified all 954 checksum records, provenance for compiler
`7ebfc94924b50d3320236b89e34df67b8114cafd543f12297397e2165307b24f`,
runtime `ded59834e44c07f1b0ce5355c27d367ce7cd72baa2eb1aa786366a12e7c8d1f2`,
and stage0 `18be13779c554af0aaeca7de40481ed4dfd9894671f1e65f928220d724575928`,
and the 278,930,687-byte archive SHA-256
`ccc8a1a7ba33bbd6f0dd0ecfadfa341d589204aee182476e9f08cb25b34fedcc`.

The accepted production targets remain `windows-x64`
(`x86_64-pc-windows-msvc`), `linux-x64`
(`x86_64-unknown-linux-gnu`), `linux-arm64`
(`aarch64-unknown-linux-gnu`), and `macos-arm64`
(`arm64-apple-macosx`). Their complete native/cross acceptance remains the
observed Phase 19 evidence recorded in `docs/PHASE_19_AUDIT.md`; WP09 adds
fresh Windows compatibility evidence, not a replacement all-target release
matrix. All retained Rocket 3 work remains internal and INTEGRATION-READY only:
no public names, SDK registration, ABI change, final cache/retention values,
final performance budgets, renderer/window integration, or visual goldens are
claimed.

### WP10 - Named arguments

**Feature:** F02. Implement the complete grammar-to-tooling vertical slice in
stage0 and self-host, including metadata, diagnostics, formatter, LSP/editor
support where present, tests, specifications, and deterministic bootstrap.

**Checkpoint:** `feat: add named arguments`

**Completed evidence (2026-08-30):** The permanent C++20 stage0 and the
Rocket-written compiler now parse dedicated named-argument nodes, retain public
parameter names in HIR/interface metadata, bind positional-plus-named calls,
and preserve receiver/callee plus written-argument evaluation order while
normalizing operands before the unchanged runtime ABI v1/backend ABI. Direct
functions, generic functions, methods, extern functions, and struct
constructors are covered. Closure values, standard-library intrinsics, enum
constructors, and built-in functions are explicit documented/tested
exclusions. Stable tests cover reordered, unknown-with-suggestion, duplicate,
missing, positional/named-conflicting, wrong-typed, and positional-after-named
calls; formatter, LSP signature help/cross-file calls, and documentation search
metadata carry the same syntax and names.

Fresh focused parser/HIR/MIR/formatter/language-server tests passed `5/5`.
The final WP10 stage0/self-hosted matrices passed `3/3` in Debug and `3/3` in
Release, including the self-host compiler fixture. The fresh full Debug suite
passed `224/224` in 128.61 seconds and the fresh full Release suite passed
`224/224` in 116.12 seconds. The coherent LLVM-disabled Release stage0 and
predecessor-compatibility selection passed `17/17` in 9.68 seconds. The
isolated Windows x64 Release bootstrap produced stage1 through stage3 below
`out/rocket3-provisional/wp10`; all six stage1-stage3 lexer/parser self-tests,
the stage3 HIR/MIR checks, and the stage3 WP10 matrix passed. Stage2 and stage3
LLVM IR matched at SHA-256
`0494ec1b44ff163d17045c83c564a6489cc0a3ffcd9d7a6a64c6e9d7a7e3559a`.
Native toolchain working set remained below 1 GiB, and every generated artifact
remained inside the packet output root.

### WP11 - Default arguments

**Feature:** F03. Implement declaration checking, evaluation order, generic and
cross-module behavior, positional normalization before MIR ABI calls, tooling,
negative tests, specifications, and deterministic bootstrap.

**Checkpoint:** `feat: add default arguments`

**Completed evidence (2026-08-30):** The permanent C++20 stage0 and the
Rocket-written compiler now parse and retain canonical default expressions for
ordinary functions and methods, enforce required-before-defaulted declaration
order, type-check defaults in isolated declaration scope, bind legal earlier
parameters, specialize generic defaults, and transport public defaults across
module boundaries. Explicit positional or named arguments override defaults;
omitted required arguments remain errors. Calls evaluate receiver/callee,
written arguments left-to-right, and omitted defaults in parameter order before
pre-MIR positional normalization, preserving runtime ABI v1 and backend ABI.
Lambda, callback, trait-declaration, enum-payload, extern, and struct-field
defaults remain explicit parser-tested exclusions. Formatter output, LSP
signature help, package documentation/search JSON, specifications, and the
supported editor-facing metadata expose the same declaration syntax.

The focused parser/HIR/MIR/formatter/language-server RED baseline failed `4/5`
before implementation, with only syntax-preserving formatter behavior already
passing; the final focused selection passed `5/5`. The repaired HIR regression
selects `main` by symbol identity instead of assuming function-vector order and
passed 20 consecutive native runs without an application fault. The final
stage0/self-hosted predecessor and internal parity selection passed `8/8`.
Fresh full Debug and Release suites each passed `226/226`. The coherent
LLVM-disabled MSVC Release stage0 and predecessor-compatibility selection passed
`18/18`, extending WP10's gate with the WP11 matrix. The isolated Windows x64
Release bootstrap produced stage1 through stage3 below
`out/rocket3-provisional/wp11`; all six lexer/parser self-tests, both stage3
HIR/MIR checks, and the stage3 WP10 and WP11 matrices passed. Stage2 and stage3
LLVM IR matched at SHA-256
`4aa87fe969ff42d8806c938a24106d2a14bad91a76f23cbda063ae27ed8eb210`.
No task process crossed the 4 GiB guard, and all generated WP11 state occupied
5.692 GiB inside the packet output root.

### WP11A - Complete named-callable parity

**Feature:** F02 completion after F03. Remove every callable-category exclusion
recorded by WP10 without folding this work into WP11. Add stable named arguments
for standard-library intrinsics, compiler built-ins, closure values, and
immediately invoked lambdas. Extend enum variant declarations with explicit
labeled payload syntax; labeled variants accept positional, named, and mixed
construction, while legacy anonymous payloads remain valid and positional-only
and a single variant may not mix labeled and anonymous entries.

Carry compiler-owned intrinsic/built-in names, closure parameter names, and
public enum-payload labels through the permanent C++20 stage0, Rocket self-host,
cross-module metadata, formatter, LSP signature help, documentation generator,
and supported editor integrations. Reuse WP10 diagnostics, typo suggestions,
evaluation-order rules, and pre-MIR positional normalization. Preserve runtime
ABI v1 and backend ABI behavior. Test every newly supported callable in
positional, all-named, reordered, and positional-then-named forms plus unknown,
duplicate, missing, conflicting, wrong-typed, and anonymous-enum rejection
cases. Run predecessor compatibility, LLVM-disabled stage0, Debug/Release,
stage0/self-host parity, and deterministic stage0-to-stage3 bootstrap evidence.
Default arguments for enum payloads and the other exclusions retained by F03
remain outside WP11A.

**Dependency:** WP11 is complete. WP11A is the only eligible successor to WP11
and blocks WP12.

**Generated output:** `out/rocket3-provisional/wp11a`

**Checkpoint:** `feat: complete named callable parity`

**Completed evidence (2026-08-31):** The permanent C++20 stage0 and the
Rocket-written compiler now bind named arguments for closure values,
immediately invoked lambdas, all 231 registered standard intrinsics, the
`print` built-in, and explicitly labeled enum payload constructors. Labeled
variants accept positional, all-named, reordered, and positional-then-named
construction and carry public labels across package modules; legacy anonymous
payloads remain positional-only, and mixed labeled/anonymous entries fail with
a stable diagnostic. The compiler-owned callable-name inventories agree across
stage0, self-host, LSP signature help, and documentation search metadata.

HIR/MIR preserve callee/receiver and written-operand left-to-right evaluation
before positional normalization, leaving runtime ABI v1 and backend ABI
unchanged. Focused parser/HIR/MIR/formatter/language-server plus WP10/WP11/WP11A
stage0/self-host compatibility passed `12/12`. Fresh full Debug and Release
suites each passed `228/228`. The LLVM-disabled MSVC Release stage0 and
predecessor-compatibility selection passed `19/19`. The isolated Windows x64
Release bootstrap produced stage1 through stage3 below the WP11A output root;
all six lexer/parser self-tests, both stage3 HIR/MIR checks, and the stage3
WP10/WP11/WP11A matrices passed. Stage2 and stage3 LLVM IR matched at SHA-256
`d6a8e980c386837045a0e84ad997ac3024149663e697ed76593d16be968c632f`.
No task process crossed the 4 GiB guard, and all generated WP11A state occupied
5.112 GiB inside the packet output root. Enum-payload, lambda, callback,
trait-declaration, extern, and struct-field defaults remain the intentional F03
exclusions.

### WP12 - Complete `std.math`

**Feature:** F04. Implement the full required function inventory, target/domain
semantics, stage0/self-host parity, runtime/backend work only where necessary,
numeric vectors, docs, compatibility, and bootstrap evidence.

**Dependency:** WP11A is complete. WP12 is the lowest-numbered eligible packet
and blocks WP13 until its local implementation gate is complete; native target
laboratory confirmation for R3-F04-008 remains a WP34/F29 acceptance item.

**Generated output:** `out/rocket3-provisional/wp12`

**Checkpoint:** `feat: add complete standard math module`

**Completed evidence (2026-09-02):** The permanent C++20 stage0 and the
Rocket-written compiler now expose the complete final `std.math` inventory:
constant functions; Float and explicit Int scalar helpers; rounding and
fractional operations; roots, powers, logarithms, and exponentials;
trigonometry and degree/radian conversion; and interpolation, smooth-step, and
bounded-motion helpers. Compiler-owned stable parameter names drive named calls,
LSP signature help, and package/documentation metadata. The runtime and both
LLVM and LLVM-disabled lowering paths preserve runtime ABI v1 and backend ABI.
The F04 specification records deterministic IEEE-754/domain, invalid-range,
integer-overflow, rounding, signed-zero, exact-endpoint, tolerance, and
no-overshoot rules.

The required pre-implementation RED fixture failed with unknown `std.math`
functions. Repair coverage first reproduced the formerly unsafe large-opposite
endpoint paths, then the final stage0 and self-hosted matrices each passed all
`76/76` numeric/domain vectors. The named public-surface, package-documentation,
formatter, and language-server signature-help gates passed, as did the fresh
four-target source/lowering surface gate. Fresh full Debug and Release suites
each passed `231/231` with no failure or timeout marker. The LLVM-disabled MSVC
Release stage0 WP10/WP11/WP11A/WP12 predecessor selection passed `4/4`; its
LLVM/self-host-only cases are intentionally not a fallback acceptance gate.
The locally available supported-target query/self-host parity evidence passed
`2/2`. The isolated Windows x64 Release bootstrap produced stage1 through
stage3 below the WP12 output root; all six lexer/parser self-tests, both stage3
HIR/MIR checks, and the stage3 WP10/WP11/WP11A/WP12 matrices passed. Stage2 and
stage3 LLVM IR matched at SHA-256
`bac28a1ae6bb945ae92686e0d6aea9441bc86f58e8e06c5b316f187cbd669ef7`.
No task process crossed the 4 GiB guard.

WP12 is complete and LOCAL-GREEN for the Windows implementation, source
surface, and cross-target lowering gates that this packet can execute. It
unblocks WP13. Native numeric execution on Linux x64, Linux arm64, and macOS
arm64 remains explicitly pending target-laboratory acceptance for R3-F04-008;
this packet does not misrepresent that pending evidence as cross-target GREEN.

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

| Feature | Owning packet(s) | Foundation state | Final acceptance packet |
| --- | --- | --- | --- |
| F01 Governance | WP00, WP09 | WP09 INTEGRATION-READY baseline: accepted Rocket 2.1 ancestry, target/toolchain/ABI/SDK/package/raylib layout, frozen-package hashes, and focused compatibility evidence recorded above | WP35 |
| F02 Named arguments | WP10, WP11A | GREEN: all function/method/extern/struct, closure/IIFE, registered standard-intrinsic, built-in, and labeled-enum named calls; public and compiler-owned names, cross-module enum labels, deterministic diagnostics, written evaluation order, pre-MIR positional ABI normalization, formatter/LSP/docs/editor parity, Debug/Release `228/228`, LLVM-disabled `19/19`, and matching stage2/stage3 IR `d6a8e980c386837045a0e84ad997ac3024149663e697ed76593d16be968c632f` | WP34 |
| F03 Default arguments | WP11 | GREEN: ordinary function/method defaults in stage0 and self-host; declaration-context and earlier-parameter binding, generic specialization, written-before-default evaluation, pre-MIR ABI normalization, cross-module metadata, stable diagnostics/exclusions, formatter/LSP/docs parity, Debug/Release `226/226`, LLVM-disabled `18/18`, and matching stage2/stage3 IR `4aa87fe969ff42d8806c938a24106d2a14bad91a76f23cbda063ae27ed8eb210` | WP34 |
| F04 `std.math` | WP12 | LOCAL-GREEN: final Float/Int standard-module surface, source-stable parameter names, stage0/self-host/runtime/LLVM parity, LSP/docs metadata, documented IEEE-754 and range semantics, 76-vector Windows stage0/self-host matrices, Debug/Release `231/231`, LLVM-disabled predecessor `4/4`, target source/lowering evidence `2/2`, and matching stage2/stage3 IR `bac28a1ae6bb945ae92686e0d6aea9441bc86f58e8e06c5b316f187cbd669ef7`; native Linux/macOS numeric confirmation of R3-F04-008 is pending WP34/F29 target-lab acceptance | WP34 |
| F05 Easing | WP13 | RED | WP34 |
| F06 Motion/reduced motion | WP06, WP13 | WP09 retained the WP06 reduced-motion policy as an INTEGRATION-READY internal kernel: `experiments/rocket3_foundation/src/reduced_motion.rocket`; fresh native package and formatter evidence recorded above | WP34 |
| F07 Safe geometry backend | WP14 | RED | WP34 |
| F08 Textures/filtering | WP15 | RED | WP34 |
| F09 Render targets/scopes | WP16 | RED | WP34 |
| F10 Shaders | WP17 | RED | WP34 |
| F11 Display quality | WP18 | RED | WP34 |
| F12 Graphics core types | WP01, WP19 | WP09 retained the WP01 geometry kernel as an INTEGRATION-READY internal kernel: `experiments/rocket3_foundation/src/geometry.rocket`; fresh native package and formatter evidence recorded above | WP34 |
| F13 Color | WP02, WP19 | WP09 retained the WP02 color kernel as an INTEGRATION-READY internal kernel: `experiments/rocket3_foundation/src/color.rocket`; fresh native package and formatter evidence recorded above | WP34 |
| F14 Shape API | WP20 | RED | WP34 |
| F15 Input/hit testing | WP01, WP20 | WP09 retained the WP01 pure hit kernel as an INTEGRATION-READY internal kernel: `experiments/rocket3_foundation/src/hit_testing.rocket`; fresh native package and formatter evidence recorded above | WP34 |
| F16 Typography | WP22 | RED | WP34 |
| F17 VirtualCanvas | WP03, WP21 | WP09 retained the WP03 mapping math as an INTEGRATION-READY internal kernel: `experiments/rocket3_foundation/src/virtual_canvas.rocket`; fresh native package and formatter evidence recorded above | WP34 |
| F18 UI context/IDs | WP05, WP23 | WP09 retained the corrected WP05 full-identity kernel as an INTEGRATION-READY internal kernel: `experiments/rocket3_foundation/src/widget_state.rocket`; fresh native package and formatter evidence recorded above | WP34 |
| F19 Layout | WP03, WP24 | WP09 retained the WP03 layout math as an INTEGRATION-READY internal kernel: `experiments/rocket3_foundation/src/layout.rocket`; fresh native package and formatter evidence recorded above | WP34 |
| F20 Themes/styles | WP04, WP25 | WP09 retained the WP04 value/resolution data as an INTEGRATION-READY internal kernel: `experiments/rocket3_foundation/src/theme.rocket`; fresh native package and formatter evidence recorded above | WP34 |
| F21 Controls | WP26 | RED | WP34 |
| F22 Containers/transient UI | WP27 | RED | WP34 |
| F23 Asset store | WP28 | RED | WP34 |
| F24 Errors/lifetimes | WP29 | RED | WP34 |
| F25 Bounded state | WP05, WP30 | WP09 retained the corrected WP05 bounded-state kernel as an INTEGRATION-READY internal kernel: `experiments/rocket3_foundation/src/widget_state.rocket`; capacity/retention calibration remains deferred | WP34 |
| F26 Performance | WP08, WP31 | WP09 retained the WP08 synthetic evidence schema as INTEGRATION-READY internal tooling: `experiments/rocket3_visual_compare/src/evidence_schema.h`; fresh Debug/Release CTest evidence recorded above and final budgets remain deferred | WP34 |
| F27 Visual regression | WP07, WP08, WP32 | WP09 retained the WP07 raw-RGBA comparator and WP08 evidence schema as INTEGRATION-READY internal tooling: `experiments/rocket3_visual_compare/src/comparator.h` and `src/evidence_schema.h`; fresh Debug/Release CTest evidence recorded above | WP34 |
| F28 Examples/showcase | WP33 | RED | WP34 |
| F29 Platform/compatibility | WP09, WP34 | WP09 INTEGRATION-READY baseline: four accepted target identities, observed Phase 19 native/cross evidence, and fresh Windows target/self-host/compatibility/package/raylib checks recorded above; full Rocket 3 acceptance remains RED | WP34 |
| F30 Docs/release/traceability | WP00, WP35 | planning only | WP35 |

## 9. Current next-chat prompt

This is the single mutable handoff slot. It must contain exactly one current
packet label and one fenced `text` prompt. A successful packet replaces both
with the next eligible packet before committing. A failed or blocked packet
does not rotate this slot. Do not preserve completed prompts here; Git history
is their archive.

**Current packet:** WP13 - Easing and complete motion

```text
Work only in the main Rocket checkout:
C:\Users\Administrator\Desktop\Projects\Rocket

Set that path as the working directory first. Read AGENTS.md,
docs/PROJECT_CONTEXT.md, docs/ROCKET_3_0_GRAPHICS_UI_REQUIREMENTS.md, and
docs/ROCKET_3_0_GRAPHICS_UI_IMPLEMENTATION_PLAN.md. Verify `master`, a clean
checkout, and accepted baseline `19596db860d4105d2226c98be2693edc5632aaf0`.
Phase 19 and WP09 through WP12 are complete; WP13 is the lowest-numbered
eligible packet. The retained Rocket 3 foundations remain internal
INTEGRATION-READY inputs, not public APIs. Push any preceding committed Rocket
3 checkpoint before editing.

Execute only WP13, F05 and F06: make the final public `rocket.motion` module.
Implement the complete Linear, Quad, Cubic, Quart, Sine, Back, Bounce, and
Elastic easing families (In, Out, and InOut where required), with Float progress
and exact 0/1 endpoints but no implicit intermediate clamping. Implement typed
Float, Vec2, and Color tweens; delay, sequence, parallel, repeat, and yoyo
timelines; fade, move, slide, scale, rotate, pulse, and color-transition
conveniences; and explicit application-owned state only. Promote the retained
reduced-motion kernel into the final policy: nonessential motion snaps to its
final state, and any essential reduced transitions are specifically documented.
Define zero/negative duration, repeat counts, cancellation/completion, large
deltas, yoyo boundaries, overshoot, and deterministic cleanup. Do not start
WP14 or add graphics, UI, raylib, assets, visual-regression, or Scroll2Roll
work.

Use TDD: add focused positive, endpoint, symmetry, overshoot, cancellation,
reduced-motion, and failure tests before production code, and capture their RED
baseline. Implement matching permanent C++20 stage0 and Rocket-written compiler
behavior, module/import discovery, HIR/MIR and public metadata, LLVM and
LLVM-disabled lowering, runtime work only where necessary, formatter, LSP,
documentation/search metadata, and supported editor behavior. Preserve valid
Rocket 2.1/WP10/WP11/WP11A/WP12 programs, runtime ABI v1, and backend ABI.

Before Rocket commands, confirm no task build/test process is active. Run all
commands sequentially; put all generated state only below
`out/rocket3-provisional/wp13`; never automatically retry a timeout; stop and
report a task process that exceeds 4 GiB or continues growing rapidly. Inspect
current build guidance and estimate combined matrix disk use before configuring;
ask the owner before any operation that could exceed 20 GiB.

Run focused compiler/runtime/module/docs/formatter/LSP/cross-target checks, then
the RED packet's predecessor compatibility, LLVM-disabled stage0, full
Debug/Release, supported-target evidence, and deterministic stage0-to-stage3
bootstrap without weakening gates. Only if all pass, update WP13 evidence and
traceability and rotate section 9 to a complete self-contained WP14 prompt.
Run `git diff --check`, review only the packet diff, and commit all WP13 work
with exactly:

feat: add Rocket motion and easing

Push `master` to `origin` after the commit and stop; do not begin WP14. If any
implementation or required validation fails, leave WP13 current, do not rotate,
and report the blocker.
```

## 10. Permanent reusable launcher

The owner may send this exact message at the start of every Rocket 3.0 packet
chat. Do not customize it for individual packets; section 9 supplies the
changing scope.

```text
Work only in the main Rocket checkout on `master`. Read AGENTS.md and
both Rocket 3.0 planning documents, then execute exactly the "Current next-chat
prompt" in the implementation plan. After successful completion, replace it
with the next eligible packet's prompt, commit everything, push `master` to
origin, and verify `git status --short --branch` reports no unpushed commits.
Do not stop after a local-only commit. If the push fails, do not begin the next
packet: report the push blocker and preserve the committed handoff. Stop only
after the push has succeeded. If this launcher is invoked with `/goal`, treat
it as permission for exactly this one current packet only: after the intended
packet's push and final synchronization check succeed, call `update_goal` with
status `complete` immediately and stop. The rotated next-packet prompt is a
handoff for a future chat, not approval to start that packet, and `/goal` must
never be used to infer that approval.
```

## 11. Packet model routing

Select the recommended model before sending the permanent launcher. The current
prompt repeats the applicable choice so the user does not need to infer it from
the packet number.

| Model and reasoning effort | Packets |
| --- | --- |
| GPT-5.6 Luna / Medium | WP04, WP06, WP08 |
| GPT-5.6 Terra / Medium | WP01, WP02, WP07 |
| GPT-5.6 Terra / High | WP03, WP05, WP12, WP13, WP15, WP20, WP25, WP26, WP33 |
| GPT-5.6 Sol / High | WP10, WP14, WP16-WP19, WP21, WP22, WP24, WP27, WP30-WP32, WP35 |
| GPT-5.6 Sol / XHigh | WP09, WP11, WP11A, WP23, WP28, WP29, WP34 |

Use `Max` only after an `XHigh` attempt produces a specific unresolved
correctness problem. If Luna fails once for a technical reason, retry that
packet with Terra Medium; if Terra High cannot resolve a design or correctness
issue after one serious attempt, continue with Sol High instead of repeatedly
retrying the lower tier.
