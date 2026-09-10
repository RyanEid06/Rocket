# Rocket 3.0 Graphics, UI, and Language-Ergonomics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: use
> `superpowers:test-driven-development` for implementation packets and
> `superpowers:verification-before-completion` before every checkpoint. Use
> `superpowers:subagent-driven-development` only when the owner explicitly asks
> for subagents. Each work packet is executed in a fresh chat and contains no
> more than two feature groups.

**Goal:** Deliver Rocket 3.0's additive language ergonomics and complete, safe,
portable, game-oriented graphics/UI stack on the accepted Rocket 2.1 baseline.

**Architecture:** `master` is the single accepted integration baseline. Starting
after completed WP16, Ryan and Eddy implement dependency-safe packets concurrently
on isolated per-wave branches created from the same pushed `origin/master`
checkpoint. Foundation kernels stay intentionally internal until their named
public-integration packets; compiler, native, SDK, platform, package, and release
work remain owned by the packet that requires them. Ordinary packets prove their
own focused and directly affected surfaces; expensive repository-wide regression,
bootstrap, package, and integration matrices run once at defined wave barriers
instead of being repeated after every packet.

**Tech stack:** Rocket, C++20 permanent stage0, Rocket self-hosted compiler,
LLVM 22.1.6, raylib 6.0, CMake/Ninja, native target SDKs inherited from Rocket
2.1.

**Spec:** `docs/ROCKET_3_0_GRAPHICS_UI_REQUIREMENTS.md`

**Documentation status:** The published Wave B baseline and the disposition of
current, historical, example, and provisional documents are indexed by
`docs/DOCUMENTATION_STATUS.md`; the copyable accepted syntax/API surface is in
`docs/ROCKET_3_0_SYNTAX_DICTIONARY.md`.

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
- A packet handles one or two feature groups and remains independently
  reviewable. During a parallel wave it commits and pushes only to its owner's
  wave branch after focused verification; shared roadmap/traceability state is
  reconciled once by the integration owner at the wave barrier. `master` changes
  only when an integration barrier has passed.

---

## 1. Current integrated baseline

| Field | Recorded value |
| --- | --- |
| Integration branch | `master` |
| Accepted Rocket 2.1 baseline | `19596db860d4105d2226c98be2693edc5632aaf0` (`Refresh Phase 19 roadmap and context`) |
| Phase 19 state | complete by owner direction on 2026-08-29; Rocket 2.1 portability accepted |
| Rocket 3 shared baseline | Wave B integration `6bb9841e185e948a9135a63bdcceeec4e5a8314a` published to `master` in status commit `fe948e98070d3d61b6ea02cdd0dfc787fcdae6fa` |
| WP16 implementation owner | Eddy |
| Packet generated output | packet-local paths below `out/rocket3-provisional/`; never share generated state between developer checkouts |
| Parallel delivery policy | Ryan and Eddy push isolated wave branches; Ryan integrates only at a defined barrier after required verification |

The accepted Rocket 2.1 baseline remains compatibility provenance. The Wave B
barrier verified the integrated tree at `6bb9841e185e948a9135a63bdcceeec4e5a8314a`,
and the status commit was published to `master` at
`fe948e98070d3d61b6ea02cdd0dfc787fcdae6fa`. At the start of each wave, record
the exact accepted `origin/master`
SHA with `git rev-parse origin/master`; both developer branches for that wave
must be created from that same SHA.

Before a lane starts a packet, verify all of the following:

```powershell
git status --short --branch
git fetch origin
git rev-parse origin/master
git merge-base --is-ancestor 19596db HEAD
```

`master` is an integration baseline, not a simultaneous development branch.
Neither Ryan nor Eddy implements ordinary wave packets directly on `master`. If
they work in one local clone, use separate Git worktrees; if they use independent
clones, use the same branch names and common-baseline rules below.

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
# Planned namespace; current accepted reference package:
examples/raylib_showcase/src/rocket_assets.rocket
tools/rocket-visual-compare
```

WP09 confirms the accepted Rocket 2.1 SDK layout before public promotion and
updates these locations only when the current repository requires it. The
Wave B asset-store implementation is intentionally package-local today; a
future packet may promote it to `rocket.assets` after the source path exists.

## 4. Parallel packet execution protocol

The remaining Rocket 3 work is scheduled by dependency and conflict surface, not
by the lowest numerical WP alone. WP numbers remain stable requirement/traceability
identifiers; the schedule in section 5A is the execution authority from WP17
through WP34.

### 4.1 Shared rules for Ryan and Eddy

Every packet chat must perform this sequence:

- [ ] Read `AGENTS.md`, `docs/PROJECT_CONTEXT.md`, both Rocket 3.0 planning
  documents, the packet definition, and only the code/specifications relevant to
  that packet.
- [ ] Identify the human owner from section 5A and use only that owner's current
  wave branch. Never implement a packet assigned to the other owner unless the
  plan is first changed at an integration barrier.
- [ ] Fetch `origin`, verify the branch descends from the current wave baseline,
  and verify all packet dependencies are already in that baseline or were
  completed earlier in the same owner's lane. Cross-lane unmerged work is never a
  valid dependency.
- [ ] Confirm no Rocket build/test process from this task is active before a
  Rocket command and keep generated state inside the packet's checkout under
  `out/rocket3-provisional/wpNN`.
- [ ] Use TDD: add focused failing tests, run the RED case, then implement only
  the packet's named feature groups.
- [ ] Run an incremental build of the changed/affected targets. Do not clean and
  rebuild the repository merely from habit.
- [ ] Run the packet's focused tests plus directly affected subsystem, formatter,
  documentation/search, package, target-surface, fallback, self-host, or native
  checks when that packet actually touches those surfaces.
- [ ] Do not automatically run the complete repository Debug+Release+bootstrap+
  package matrix after an ordinary packet. Those expensive gates belong to the
  wave barrier unless section 4.4 says the packet forces an immediate barrier.
- [ ] Run `git diff --check` and review the packet-only diff.
- [ ] Commit the packet independently using its existing checkpoint subject. Add
  a commit body recording `Owner`, `Packet`, focused commands/results, affected
  regression commands/results, and `Lane state: LANE-GREEN`.
- [ ] Push the owner's wave branch. Never push an ordinary in-wave packet directly
  to `master`.
- [ ] Continue to the next packet in the same owner's wave only if section 5A
  explicitly places it next in that lane and all of its dependencies are already
  satisfied in that lane/baseline.

A failed focused build/test stops that lane immediately. Do not stack later
packets on known-broken work. Investigate and fix the root cause before continuing.
The other lane may continue only if it is genuinely independent of the failure.

### 4.2 Scheduling states

These are execution states and do not replace the requirements document's
PROVISIONAL/INTEGRATION-READY/PUBLIC/ACCEPTED maturity terminology.

- `BLOCKED`: at least one required dependency is absent from the lane baseline.
- `READY`: all dependencies are integrated and no active cross-lane contract
  collision exists.
- `ACTIVE-RYAN`: Ryan is implementing the packet.
- `ACTIVE-EDDY`: Eddy is implementing the packet.
- `LANE-GREEN`: focused and directly affected verification passed on the owner's
  branch; full wave integration has not yet been claimed.
- `WAVE-GREEN`: the combined wave passed the barrier and is present in the new
  common `master` baseline.

### 4.3 Shared-file and documentation ownership

Parallelism must not turn the live roadmap itself into a merge-conflict hotspot.
During an active wave:

- Ryan and Eddy do **not** edit
  `docs/ROCKET_3_0_GRAPHICS_UI_IMPLEMENTATION_PLAN.md`,
  `docs/ROCKET_3_0_GRAPHICS_UI_REQUIREMENTS.md`, `docs/PROJECT_CONTEXT.md`,
  `docs/ROADMAP.md`, or the root `README.md` for status/handoff updates.
- Packet-specific source documentation, generated/search metadata, focused tests,
  and feature-local docs may change when required by the packet.
- Ryan is the roadmap integration owner. At the wave barrier he reads both lanes'
  commits and verification evidence, updates packet status/traceability/global
  handoff once, and includes that documentation update in the integration commit.
- Eddy reviews the integrated roadmap/status diff for factual correctness before
  the barrier is considered closed.

Some remaining packets necessarily append to shared implementation files such as
the raylib adapter/header, root CMake registration, or bundled-module loader.
The wave schedule eliminates dependency/semantic collisions, but a mechanical Git
conflict can still occur in these files. During a wave, each owner must make only
the minimal WP-specific additions and must not perform unrelated refactors of a
shared file. Ryan resolves any mechanical merge conflict on the integration
branch by preserving both packet contracts and rerunning both packets' focused
tests before the full barrier matrix.

### 4.4 Immediate-barrier rule

A packet forces an early full barrier before either developer continues if its
actual implementation changes a foundational contract more broadly than the
packet definition predicted, including:

- parser/semantic/HIR/MIR/backend conventions used by later packets;
- runtime ABI v1 or backend ABI;
- general bundled-module resolution/security behavior;
- global graphics resource-lifetime representation;
- the core public `rocket.graphics` or `rocket.ui` lifecycle contract in a way
  that invalidates the current wave's other packet assumptions.

Do not weaken the packet or bypass the barrier to preserve the schedule.

### 4.5 Wave integration barrier

Ryan is the primary integration operator. At each full barrier:

1. Fetch both pushed owner branches and verify every scheduled packet has a
   LANE-GREEN commit with its prescribed checkpoint subject.
2. Create a clean `rocket3/integration-wave-<letter>` branch from the exact wave
   baseline.
3. Merge Ryan's and Eddy's wave branches without squashing away individual WP
   commits.
4. Resolve only genuine integration conflicts; do not redesign features during
   the merge. Re-run the focused tests for every packet involved in a conflict.
5. Run the full Windows/local Debug and Release repository suites appropriate to
   the current development stage.
6. Run the eligible LLVM-disabled/predecessor compatibility matrix.
7. Run deterministic stage0 -> stage1 -> stage2 -> stage3 bootstrap/self-host
   acceptance and compare the required stage2/stage3 evidence.
8. Run package/docs/search/formatter/LSP/target-surface/relocation checks required
   by the combined wave.
9. Run relevant graphics/UI native, deterministic-backend, visual, and resource
   cleanup checks introduced so far.
10. Keep native Linux x64, Linux ARM64, and macOS ARM64 final target-laboratory
    acceptance deferred to WP34 unless a packet explicitly requires earlier
    native evidence. Never mislabel cross-compilation as native evidence.
11. Update the two Rocket 3 planning documents and global handoff once with the
    integrated evidence.
12. Run `git diff --check`, review the integration diff, merge the accepted
    integration branch to `master`, push `master`, and record the new baseline
    SHA.

Only after this sequence passes may the next wave's branches be created.

## 5. Packet index

`WP00` through `WP16` are completed history. WP16 was completed by Eddy and is
the starting common baseline for the parallel schedule. From WP17 onward, packet
numbers are stable identifiers rather than a requirement to execute every lower
number first. A packet may run when its real dependencies are satisfied and
section 5A assigns it to the current owner/wave.

| Packet | Feature groups | Maximum scope | Current state | Planned owner | Wave |
| --- | --- | --- | --- | --- | --- |
| WP00 | F01, F30 planning | Requirements and live plan only | COMPLETE / PROVISIONAL | historical | complete |
| WP01 | F12, F15 | Provisional geometry and hit-testing kernels | COMPLETE / PROVISIONAL | historical | complete |
| WP02 | F13 | Provisional color kernel | COMPLETE / PROVISIONAL | historical | complete |
| WP03 | F19, F17 | Provisional layout and VirtualCanvas math | COMPLETE / PROVISIONAL | historical | complete |
| WP04 | F20 | Provisional theme/style data | COMPLETE / PROVISIONAL | historical | complete |
| WP05 | F18, F25 | Provisional widget IDs and bounded state | COMPLETE / PROVISIONAL | historical | complete |
| WP06 | F06 | Provisional reduced-motion state kernel only | COMPLETE / PROVISIONAL | historical | complete |
| WP07 | F27 | Raw-RGBA comparator kernel and synthetic fixtures | COMPLETE / PROVISIONAL | historical | complete |
| WP08 | F26, F27 | Provisional metric/golden schemas and synthetic budgets | COMPLETE / PROVISIONAL | historical | complete |
| WP09 | F01, F29 | Rocket 2.1 baseline audit and foundation integration | COMPLETE / INTEGRATION-READY | historical | complete |
| WP10 | F02 | Named arguments | COMPLETE / GREEN | historical | complete |
| WP11 | F03 | Default arguments | COMPLETE / GREEN | historical | complete |
| WP11A | F02 | Complete named-callable parity | COMPLETE / GREEN | historical | complete |
| WP12 | F04 | Complete `std.math` | COMPLETE / LOCAL-GREEN; target-lab R3-F04-008 pending | historical | complete |
| WP13 | F05, F06 | Easing plus complete motion/timelines | COMPLETE / LOCAL-GREEN; target-lab acceptance pending WP34/F29 | historical | complete |
| WP14 | F07 | Safe raylib geometry expansion | COMPLETE / LOCAL-GREEN; target-lab acceptance pending WP34/F29 | historical | complete |
| WP15 | F08 | Advanced textures/filtering | COMPLETE / LOCAL-GREEN; target-lab acceptance pending WP34/F29 | historical | complete |
| WP16 | F09 | Render targets, clipping, blending | COMPLETE / LOCAL-GREEN | Eddy | baseline |
| WP17 | F10 | Safe shader subset | COMPLETE / WAVE-GREEN | Ryan | A |
| WP18 | F11 | Window/display/rendering quality | COMPLETE / WAVE-GREEN | Ryan | A |
| WP19 | F12, F13 | Public graphics core types and Color | COMPLETE / WAVE-GREEN | Eddy | A |
| WP20 | F14, F15 | Public shapes and input/hit testing | COMPLETE / WAVE-GREEN (`ac01cfb`) | Ryan | B |
| WP21 | F17 | Integrated `VirtualCanvas` | COMPLETE / WAVE-GREEN (`f82582e`) | Ryan | B |
| WP22 | F16 | Typography | COMPLETE / WAVE-GREEN (`158bc6f`) | Eddy | B |
| WP23 | F18 | Public UI context/response/IDs | COMPLETE / WAVE-GREEN (`6e2971a`) | Ryan | B |
| WP24 | F19 | Public layout integration | COMPLETE / WAVE-GREEN (`56f4498`) | Ryan | B |
| WP25 | F20 | Public themes/styles | READY / RED; Wave B accepted and WP25 is unblocked | Eddy | C |
| WP26 | F21 | Controls | BLOCKED until Eddy WP25 / RED | Eddy | C |
| WP27 | F22 | Containers/dialogs/transient UI | BLOCKED until Eddy WP26 / RED | Eddy | C |
| WP28 | F23 | Typed asset store | COMPLETE / WAVE-GREEN (`778de291`) | Eddy | B |
| WP29 | F24 | Unified errors and lifetime hardening | BLOCKED until Wave C integrates WP27/WP28; intentionally scheduled after WP30 | Ryan | D |
| WP30 | F25 | Bounded-state/cache integration | BLOCKED until Eddy WP27 plus integrated WP22/WP28 / RED | Eddy | C |
| WP31 | F26 | Performance instrumentation and budgets | BLOCKED until Wave C integrates WP30 / RED | Eddy | D |
| WP32 | F27 | Visual scenes, image I/O, goldens, CI | BLOCKED until Eddy WP31 and integrated WP17 / RED | Eddy | D |
| WP33 | F28 | Focused examples and premium showcase | BLOCKED until Eddy WP32 / RED | Eddy | D |
| WP34 | F29 | Full compiler/platform/compatibility acceptance | BLOCKED until Wave D integrates WP33 / RED | Ryan + Eddy | FINAL |
| WP35 | F30 | Documentation, release, traceability closure | WAIT FOR WP34 / RED | release owner after WP34 | post-parallel |

## 5A. Ryan/Eddy dependency-safe execution schedule

This schedule is now authoritative for WP17-WP34. It intentionally executes
WP28 before WP25 and WP30 before WP29. Those reorderings satisfy the existing
DAG, keep each wave internally independent across developers, and reduce
cross-cutting merge conflicts. They do not change any feature requirement or WP
number.

### Common starting baseline

- `master` contains completed WP16.
- WP16 owner: **Eddy**.
- At Wave A start, both developers run `git fetch origin` and record the exact
  `origin/master` SHA containing `feat: add safe render scopes`.
- Ryan creates/pushes `rocket3/ryan-wave-a` from that SHA.
- Eddy creates/pushes `rocket3/eddy-wave-a` from that SHA.

### Wave A - backend quality plus public graphics foundation

**Ryan lane**

1. **WP17 - Safe shaders**
2. **WP18 - Window and rendering quality**

WP17 and WP18 deliberately stay in the same lane because both extend the
reviewed raylib adapter/safe wrapper and therefore have a high same-file conflict
surface. WP18 may consume WP17 only if its implementation naturally uses shader
capabilities; it does not require Eddy's unmerged work.

**Eddy lane**

1. **WP19 - Public graphics types and Color**

WP19 is independent of WP17/WP18 at the requirements level and primarily owns
public graphics value types/Color promotion and their compiler/module/public API
integration. When implementing WP19, prefer a deterministic, security-preserving
bundled-module registration/resolution structure that does not require every
later `rocket.graphics`/`rocket.ui` module to add another ad-hoc hard-coded
loader special case. Do not broaden imports outside the existing standard-library
security model merely for convenience.

**Wave A barrier:** FULL. Merge both lanes and run section 4.5. The resulting
`master` is the sole baseline for Wave B.

### Wave B - graphics/UI foundation in two independent chains

Create `rocket3/ryan-wave-b` and `rocket3/eddy-wave-b` from the Wave A accepted
`master` SHA.

**Ryan lane**

1. **WP20 - Public shapes and input/hit testing**
2. **WP21 - Integrated `VirtualCanvas`**
3. **WP23 - Public UI context and interaction**
4. **WP24 - Public layout**

The chain is intentionally kept on Ryan's branch because WP21 depends on WP20,
WP23 depends on WP20+WP21, and WP24 depends on WP23. No cross-lane merge is
needed inside the wave.

**Eddy lane**

1. **WP22 - Typography**
2. **WP28 - Typed asset store**

WP28 depends on WP17, which is already in the Wave A baseline, and WP22, which
Eddy completes earlier in the same lane. It does not depend on Ryan's Wave B
packets.

**Conflict discipline for Wave B:** Ryan owns shape/input/VirtualCanvas/UI-context/
layout changes; Eddy owns typography and typed asset/resource-store changes. Both
may need minimal additions in the shared raylib adapter/header or root CMake
registration. Do not refactor shared adapter state, error codes, loader structure,
or build layout merely for style. Make only packet-specific additions; the
integration branch owns mechanical reconciliation.

**Wave B barrier:** FULL. This barrier integrates six packets and is mandatory
before WP25 begins.

### Wave C - serial public-UI completion and cache integration

Create `rocket3/eddy-wave-c` from the Wave B accepted `master` SHA. Ryan does not
start another implementation packet during this wave; he may review Eddy's pushed
commits, but must not duplicate or pre-implement blocked WP29 work. The apparent
idle lane is intentional: the remaining public UI chain is dependency-serial, and
forcing fake parallelism here would create more rework than it saves.

**Eddy lane**

1. **WP25 - Public themes and styles**
2. **WP26 - Controls**
3. **WP27 - Containers and transient UI**
4. **WP30 - Bounded state and caches**

WP30 is deliberately scheduled before WP29. Its existing dependencies are WP22,
WP27, and WP28, all satisfied by this point. Landing cache/state integration
first allows WP29's later cross-cutting hardening audit to include the final cache
layer rather than forcing two developers to modify the same UI/state files in
parallel.

**Wave C barrier:** FULL. Ryan integrates Eddy's lane, runs section 4.5, and
creates the common baseline that unlocks WP29 and WP31.

### Wave D - hardening versus performance/visual/examples

Create `rocket3/ryan-wave-d` and `rocket3/eddy-wave-d` from the Wave C accepted
`master` SHA.

**Ryan lane**

1. **WP29 - Unified error and lifetime hardening**

Because WP30 is now already integrated, WP29 audits/hardens the final bounded
state/cache layer as part of the public graphics/UI lifetime taxonomy. Ryan must
not add new product scope; this remains the existing F24 closure packet.

**Eddy lane**

1. **WP31 - Performance budgets**
2. **WP32 - Visual regression system**
3. **WP33 - Examples and showcase**

This chain is dependency-serial inside Eddy's lane and does not require WP29.
WP31 owns calibrated instrumentation/budgets, WP32 consumes those final integrated
surfaces for visual acceptance, and WP33 consumes the accepted visual/public API
surface for examples/showcase.

**Wave D conflict discipline:** WP29 may touch many production contracts, while
WP31-WP33 should keep instrumentation, visual tooling, fixtures, CI, examples,
and showcase work localized. If WP31 needs production instrumentation hooks, keep
them minimal and do not refactor error/lifetime behavior owned by WP29. A genuine
contract collision triggers section 4.4.

**Wave D barrier:** FULL. After it passes, every implementation packet through
WP33 is in one accepted common baseline.

### FINAL - WP34 shared full compatibility/platform acceptance

WP34 is not an ordinary wave and must not use the reduced per-packet test policy.
It runs the full F29 acceptance contract. Ryan and Eddy work from the same WP33
integration baseline but divide evidence, not product semantics.

**Ryan primary responsibilities**

- integration owner and defect-fix coordinator;
- Windows x64 full Debug/Release acceptance;
- stage0, self-host, stage1-stage3 deterministic bootstrap and compiler parity;
- predecessor compatibility/runtime ABI/package/application/hardening matrices;
- Windows packaging, installation, relocation, checksum/provenance evidence;
- final merge of verified WP34 defect fixes and F29 traceability.

**Eddy primary responsibilities**

- Linux x64, Linux ARM64, and macOS ARM64 native target-laboratory coordination
  and evidence where actual native hosts are available;
- supported-target graphics capability/failure behavior;
- non-Windows visual portability subsets and structural/numerical checks;
- independent review of F29 requirements and Ryan's Windows/bootstrap evidence.

Cross-compilation, workflow configuration, or emulation does not count as native
host evidence where F29 requires a native host. If a required target host is not
available, record WP34 as blocked rather than fabricating acceptance.

Ryan and Eddy may run independent WP34 matrices simultaneously, but no test result
may be silently skipped, weakened, or replaced. WP34 becomes COMPLETE only after
the original F29 contract is satisfied and the combined evidence is integrated.

### Wave summary

| Wave | Ryan | Eddy | Barrier |
| --- | --- | --- | --- |
| A | WP17 -> WP18 | WP19 | FULL |
| B | WP20 -> WP21 -> WP23 -> WP24 | WP22 -> WP28 | FULL |
| C | review/integration support only | WP25 -> WP26 -> WP27 -> WP30 | FULL |
| D | WP29 | WP31 -> WP32 -> WP33 | FULL |
| FINAL | WP34 Windows/compiler/integration evidence | WP34 non-Windows/platform/review evidence | F29 FULL ACCEPTANCE |

This reduces the routine full-repository regression cadence from once per packet
to four pre-WP34 wave barriers, while still requiring focused correctness after
every packet. WP34 then performs the comprehensive final acceptance that earlier
LOCAL-GREEN packets intentionally defer.

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

**WP13 evidence (2026-09-02):** Added bundled public source module
`stdlib/rocket/motion.rocket` and stage0/self-host import resolution for
`rocket.motion`. The immutable API covers every required easing family,
Float/Vec2/Color tween sampling, motion policy, delay/sequence/parallel/repeat/
yoyo/cancel timelines, and all required conveniences. The required fixture was
RED before the module existed. Afterwards, focused stage0/self-host tests,
formatter, named-surface diagnostics, and four-target source/lowering checks
passed; fresh Debug and Release CTest suites passed `234/234` each; the
LLVM-disabled stage0/predecessor selection passed `5/5`; and a deterministic
Windows bootstrap passed stage1/stage2/stage3 lexer and parser self-tests, HIR/
MIR checks, WP10/WP11/WP11A/WP12/WP13 fixture checks, and the stage3 20-case
motion run. Stage2/stage3 IR matched at SHA-256
`5383af22c8e6cb8049a2dc180a80295dfee494dba04f434a1f701a0ff4139f9c`.
Generated state stayed under `out/rocket3-provisional/wp13` (4.944 GiB), below
the 20 GiB operation guard. This packet is LOCAL-GREEN for Windows and
source/lowering checks; native non-Windows target-laboratory acceptance remains
WP34/F29 work.

### WP14 - Safe raylib geometry

**Feature:** F07. Expand the reviewed native adapter and safe Rocket module for
every required shape; add token/state/geometry validation, deterministic test
backend behavior, native tests, package generation, and all-target acceptance.

**Checkpoint:** `feat: expand safe raylib geometry`

**WP14 evidence (2026-09-02):** Expanded the reviewed primitive raylib adapter
and its safe Rocket wrapper with every F07 rectangle, circle/ellipse/ring/
sector, line, triangle, polygon, and Bezier operation. Safe calls reject
non-finite values and defined negative geometry before the native boundary;
the native layer repeats the checks, preserves frame tokens, and copies curve
points through short-lived integer-token buffers rather than exposing native
structures or pointers. The deterministic backend reports geometry call counts
and live point-buffer counts. Focused positive, validation, boundary, stale-
token, deterministic, and cleanup tests were RED on the missing symbols, then
passed `10/10` in Debug and Release across native, stage0, self-host, package,
and generated-binding parity. Fresh full Debug and Release suites passed
`234/234` each; the LLVM-disabled predecessor/WP14 selection passed `8/8`;
stage0 and self-host package checks passed on all four supported targets; and
stage0/self-host documentation each exposed 191 public items with stable search
metadata. The deterministic Windows bootstrap passed 184 validation cases and
matched stage2/stage3 IR at SHA-256
`5383af22c8e6cb8049a2dc180a80295dfee494dba04f434a1f701a0ff4139f9c`.
The checksummed Windows showcase archive is 1,570,523 bytes with SHA-256
`bbfc71e07d3b86db9009f0bf1d80f3f0734202a8d5d5e0bff66a9d1446b12c58`.
Generated WP14 state occupied 3.932 GiB, below both resource guards. WP14 is
LOCAL-GREEN; native non-Windows target-laboratory execution remains WP34/F29.

### WP15 - Advanced textures and filtering

**Feature:** F08. Add source/destination/pivot/rotation/tint drawing, filtering
capabilities/fallbacks, resource validation, deterministic backend tests, and
native target evidence.

**WP15 evidence (2026-09-07):** Added value-based `Rect`/`Point` texture
drawing with finite/float-range, source-region, frame-token, texture-token,
window-ownership, and tint validation. Point, bilinear, trilinear, and 4x/8x/
16x anisotropic filters expose capability queries and explicit fallback results;
trilinear generation creates mipmaps before selecting the mode, while native
anisotropy queries the active OpenGL extension and maximum level. The
deterministic backend simulates anisotropy limits and covers stale handles,
invalid regions, non-finite/overflowing values, flipping, cleanup, and fallback
selection. The showcase package reports `6 passed; 0 failed` under stage0,
self-hosted, Debug, Release, and the 184-case Windows bootstrap. Full Debug and
Release CTest suites passed `235/235`; the LLVM-disabled predecessor selection
passed `11/11`; stage0/stage3 documentation each generated two matching files
with 220 public API items. Native non-Windows target-lab execution remains
WP34/F29 acceptance and is not claimed by this Windows packet.

**Checkpoint:** `feat: add advanced safe texture drawing`

### WP16 - Render targets, clipping, and blending

**Feature:** F09. Add checked render textures and scoped target/scissor/blend
state machines with nesting, cleanup, compositing, and native tests.

**WP16 evidence (2026-09-07):** Added checked, value-owned render textures and
strictly nested render-target, scissor, and reviewed blend scopes across the
portable primitive adapter and safe Rocket module. The implementation supports
virtual-resolution rendering, compositing, screenshot export, UI layering and
transition primitives, and shader-pass preparation without exposing native
structures, pointers, or backend-owned handles. Both wrapper and native
boundary reject invalid dimensions/regions, non-finite or float-overflowing
values, stale state, wrong-window use, self-sampling, and invalid scope order;
explicit frame abort unwinds every live scope deterministically.

The required adapter/package tests were RED on the missing WP16 surface before
implementation. Focused deterministic and hidden-window native coverage passed,
the showcase package passed `7/7`, generated bindings matched the checked-in
source, and formatter/docs/search/cross-target checks passed. Fresh full Debug
and Release suites each passed `237/237`. The eligible LLVM-disabled matrix
passed `182/182` (the seven LLVM/self-host-native cases unavailable by design
were excluded). The deterministic Windows x64 bootstrap passed `184` validation
cases; stage2/stage3 IR matched at SHA-256
`5383af22c8e6cb8049a2dc180a80295dfee494dba04f434a1f701a0ff4139f9c`.
The checksummed Windows showcase archive is 1,535,687 bytes with SHA-256
`b942c38c00c4ed4111f030fdfd54284f29d55cc2a4d7b0e618687ccbfe5a5083`.
Native non-Windows target-laboratory execution remains WP34/F29 acceptance.

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

**Wave A acceptance (WP17-WP19):** the owner commits `000b8c86`, `badf152`,
and `b8aff97` were preserved through non-squash merges and integrated at
`0a946eeae664bdabd9bbe5a4897c005b7812b962`. Fresh full Debug and Release
suites passed `244/244` each; the eligible LLVM-disabled matrix passed
`187/187`. The exact Release bootstrap and the independent Phase 19 bootstrap
both produced identical stage2/stage3 IR at SHA-256
`8c24bfc2e00ccc2242ffe932d7bb8bb02796665abd2f6e1bb4078a2bd9f3862e`;
the Phase 19 run passed 184 validation cases and the raylib package passed
`9/9`. Focused WP17-WP19 native/deterministic, stage0/self-host,
formatter/docs/search, LSP, and four-target source/lowering gates passed `9/9`
in Debug and Release. Hardening passed `17/17` in each configuration, and the
standalone Rocket 2.1 conformance runner passed `90/90` after its stale 2.0
identity and missing native-toolchain environment were corrected. The
checksummed relocated Windows SDK passed with 960 files; its 279,286,807-byte
archive has SHA-256
`357928f5dadc3ccc2f3f67248d4b85a349ead04781e7bd15f8c6218ac723804e`.
Native Linux x64, Linux ARM64, and macOS ARM64 execution remains deferred to
WP34/F29 target-laboratory acceptance.

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

### Wave B acceptance (WP20-WP24, WP22, WP28)

The six scheduled Wave B packets were integrated without squashing their owner
commits on `rocket3/integration-wave-b` at `6bb9841e185e948a9135a63bdcceeec4e5a8314a`
(Ryan merge `e435e9d`, Eddy merge parent `778de291`). The integration resolved
the shared CMake registrations mechanically and fixed one platform-specific
asset-store edge case: a missing in-root final path component now reports
`RLV_ERR_NOT_FOUND`, while traversal and absolute paths remain
`RLV_ERR_PATH_ESCAPE`.

Fresh Windows evidence on the integrated tree:

- The complete Debug and Release CTest suites each passed `267/267`. This
  includes the six packet stage0, self-hosted, and target-surface gates, the
  WP20/WP21/WP22/WP28 native adapter checks, package/docs/search/formatter/LSP,
  and relocation coverage.
- Release bootstrap passed stage0 -> stage1 -> stage2 -> stage3 with matching
  stage2/stage3 LLVM IR SHA-256
  `1aa7c6d4c15b0ccf4a00b4445827dddb821aea3553d814788d0e4e57ade93edf`.
- Rocket 2.1 predecessor conformance passed `90` cases; compatibility passed
  `11` release-line cases; application validation resolved `32` packages and
  passed all `11` application checks.
- The integrated native adapter selection passed `4/4` (WP20, WP21, WP22,
  WP28), and the focused target-surface selection passed `6/6`. Native Linux
  x64, Linux ARM64, and macOS ARM64 execution remains deferred to WP34/F29
  target-laboratory acceptance.

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
| F05 Easing | WP13 | LOCAL-GREEN: bundled `rocket.motion` has every required easing family, exact endpoints, unclamped intermediate progress, deterministic Bounce, intentional Back/Elastic overshoot, stage0/self-host parity, Debug/Release `234/234`, LLVM-disabled predecessor `5/5`, four-target source/lowering checks, and matching stage2/stage3 IR `5383af22c8e6cb8049a2dc180a80295dfee494dba04f434a1f701a0ff4139f9c`; native non-Windows target-lab acceptance remains WP34/F29 | WP34 |
| F06 Motion/reduced motion | WP06, WP13 | LOCAL-GREEN: public value-owned Float/Vec2/Color tweens, explicit reduced-motion policy, delay/sequence/parallel/repeat/yoyo/cancel timelines, all convenience constructors, and defined zero/negative/repeat/large-delta behavior; WP06 remains a superseded internal kernel, while WP13 carries the final public contract and the same validation evidence | WP34 |
| F07 Safe geometry backend | WP14 | LOCAL-GREEN: complete reviewed primitive adapter and safe value API for all required geometry, pre-native validation, stale-frame and token-buffer cleanup, deterministic backend counts, Debug/Release `234/234`, focused `10/10`, LLVM-disabled `8/8`, four-target stage0/self-host checks, 191-item docs/search parity, and matching stage2/stage3 IR `5383af22c8e6cb8049a2dc180a80295dfee494dba04f434a1f701a0ff4139f9c`; native non-Windows target-lab acceptance remains WP34/F29 | WP34 |
| F08 Textures/filtering | WP15 | LOCAL-GREEN: checked source/destination/pivot/rotation/tint drawing, explicit filter capability and fallback behavior, mipmap-backed trilinear filtering, context-based anisotropy queries, deterministic/native tests, Debug/Release `235/235`, LLVM-disabled `11/11`, documentation parity, and deterministic bootstrap `184` cases; native non-Windows target-lab acceptance remains WP34/F29 | WP34 |
| F09 Render targets/scopes | WP16 | LOCAL-GREEN: checked render textures; value-based target/scissor/blend scopes; strict LIFO, stale-token, wrong-window, self-sampling, cleanup, compositing, screenshot, deterministic/native coverage; Debug/Release `237/237`, LLVM-disabled eligible `182/182`, bootstrap `184` with matching IR | WP34 |
| F10 Shaders | WP17 | WAVE-GREEN: checked shader resources, reviewed uniforms, scoped render-target integration, deterministic/native coverage, and Wave A barrier evidence recorded above | WP34 |
| F11 Display quality | WP18 | WAVE-GREEN: DPI/framebuffer/monitor APIs, MSAA4x configuration, resizing/fullscreen/borderless transitions, screenshots, deterministic/native coverage, and Wave A barrier evidence recorded above | WP34 |
| F12 Graphics core types | WP01, WP19 | WAVE-GREEN: the WP01 kernel is promoted into final public `rocket.graphics` value types with frozen constructors/named parameters, stage0/self-host/formatter/LSP/docs/target parity, and Wave A barrier evidence recorded above | WP34 |
| F13 Color | WP02, WP19 | WAVE-GREEN: the WP02 kernel is promoted into final public `rocket.graphics.Color` with defined parsing/conversion/clamping/interpolation behavior, stage0/self-host parity, and Wave A barrier evidence recorded above | WP34 |
| F14 Shape API | WP20 | PUBLIC / WAVE-GREEN: public shapes and input helpers passed the combined Wave B Debug/Release, stage0/self-host, target-surface, native, formatter/docs/search, and package gates; final cross-target acceptance remains WP34 | WP34 |
| F15 Input/hit testing | WP01, WP20 | PUBLIC / WAVE-GREEN: pointer states, hit testing, logical-coordinate behavior, and outside-viewport safety passed the combined Wave B gates; final cross-target acceptance remains WP34 | WP34 |
| F16 Typography | WP22 | PUBLIC / WAVE-GREEN: bounded measurement/alignment/wrapping/clipping behavior and typography native/target-surface gates passed the combined Wave B evidence; final cross-target acceptance remains WP34 | WP34 |
| F17 VirtualCanvas | WP03, WP21 | PUBLIC / WAVE-GREEN: integrated logical mapping, render-target, clipping, pointer, resize/fullscreen/DPI, and screenshot behavior passed the combined Wave B gates; final cross-target acceptance remains WP34 | WP34 |
| F18 UI context/IDs | WP05, WP23 | PUBLIC / WAVE-GREEN: `Context`, `UiFrame`, `Response`, stable IDs, lifecycle, interaction, focus, disabled, modal, duplicate-ID, and misuse checks passed the combined Wave B gates; final cross-target acceptance remains WP34 | WP34 |
| F19 Layout | WP03, WP24 | PUBLIC / WAVE-GREEN: Row/Column/Grid/Stack/Anchor sizing, Insets/SafeArea, spacing, alignment, invalid-layout, deterministic target-surface, and self-host checks passed the combined Wave B gates; final cross-target acceptance remains WP34 | WP34 |
| F20 Themes/styles | WP04, WP25 | WP09 retained the WP04 value/resolution data as an INTEGRATION-READY internal kernel: `experiments/rocket3_foundation/src/theme.rocket`; fresh native package and formatter evidence recorded above | WP34 |
| F21 Controls | WP26 | RED | WP34 |
| F22 Containers/transient UI | WP27 | RED | WP34 |
| F23 Asset store | WP28 | PUBLIC / WAVE-GREEN: typed texture/font/sound/music/shader lookup, path security, duplicate/missing handling, relocation, cleanup, and native adapter checks passed the combined Wave B gates; final cross-target acceptance remains WP34 | WP34 |
| F24 Errors/lifetimes | WP29 | RED | WP34 |
| F25 Bounded state | WP05, WP30 | WP09 retained the corrected WP05 bounded-state kernel as an INTEGRATION-READY internal kernel: `experiments/rocket3_foundation/src/widget_state.rocket`; capacity/retention calibration remains deferred | WP34 |
| F26 Performance | WP08, WP31 | WP09 retained the WP08 synthetic evidence schema as INTEGRATION-READY internal tooling: `experiments/rocket3_visual_compare/src/evidence_schema.h`; fresh Debug/Release CTest evidence recorded above and final budgets remain deferred | WP34 |
| F27 Visual regression | WP07, WP08, WP32 | WP09 retained the WP07 raw-RGBA comparator and WP08 evidence schema as INTEGRATION-READY internal tooling: `experiments/rocket3_visual_compare/src/comparator.h` and `src/evidence_schema.h`; fresh Debug/Release CTest evidence recorded above | WP34 |
| F28 Examples/showcase | WP33 | RED | WP34 |
| F29 Platform/compatibility | WP09, WP34 | WAVE-GREEN Windows evidence: Wave B Debug/Release `267/267`, deterministic bootstrap with matching stage2/stage3 IR, Rocket 2.1 conformance `90`, predecessor compatibility `11`, application validation, package/relocation, and native raylib checks all passed; native non-Windows target-lab acceptance remains WP34 | WP34 |
| F30 Docs/release/traceability | WP00, WP35 | planning only | WP35 |

## 9. Current parallel handoff

This section replaces the former single mutable next-packet slot. It is updated
only by Ryan during a successful full wave barrier, after both lane branches have
been integrated and verified. Ordinary packet chats never rotate this section.

**Current common baseline:** Wave B is accepted on the integrated tree at
`6bb9841e185e948a9135a63bdcceeec4e5a8314a` and published to `master` in
status commit `fe948e98070d3d61b6ea02cdd0dfc787fcdae6fa`, preserving Ryan WP20
`ac01cfb5d63db1ff5b930a82d06c66b9181e6ba7`, WP21
`f82582e8638f082da6b754e98edfe5a2f4e60ed2`, WP23
`6e2971a0bf4669089074c772b3c683302977e681`, WP24
`56f4498bfb4f6909b0f633020710dba10275281b`, and Eddy WP22
`158bc6fc80923f5b358efd906a40d392723fa7bb` plus WP28
`778de29110dac6c29c99c4eabdd5a4221931db85`. The published `master` SHA above
is the source for the Wave C branch.

**Current wave:** Wave C

**Ryan current lane:**

1. Review/integration support only; no Ryan implementation packet is assigned.
2. Do not begin WP29 before the Wave C barrier.
3. Stop at the Wave C barrier.

**Eddy current lane:**

1. **NEXT: WP25 - Public themes and styles - READY / RED**
2. WP26 - Controls - BLOCKED until WP25
3. WP27 - Containers/dialogs/transient UI - BLOCKED until WP26
4. WP30 - Bounded-state/cache integration - BLOCKED until WP27
5. Stop at the Wave C barrier.

**Wave B barrier:** COMPLETE. The integrated Windows Debug/Release, bootstrap,
predecessor, application, package/relocation, native, and focused packet gates
are recorded in the acceptance block above. Native non-Windows target-laboratory
acceptance remains deferred to WP34/F29.

**Wave C launch condition:** satisfied by this accepted Wave B barrier. Create
`rocket3/eddy-wave-c` from the accepted `master` SHA and implement only the
serial Eddy queue above. Ryan remains review/integration support only.

When a barrier succeeds, Ryan replaces only the current baseline/wave/lane block
above with the next wave's exact queues from section 5A and records the new master
SHA. Completed packet definitions/evidence remain in their existing sections; Git
history is not used as a substitute for the live status table.

## 10. Permanent reusable launchers

Use the launcher matching the human developer. These launchers are stable; the
current wave and packet order come from sections 5A and 9.

### Ryan launcher

```text
You are Ryan's Rocket 3 implementation worker. Work only on the packet currently
assigned to Ryan in sections 5A and 9 of
docs/ROCKET_3_0_GRAPHICS_UI_IMPLEMENTATION_PLAN.md. Read AGENTS.md,
docs/PROJECT_CONTEXT.md, docs/ROCKET_3_0_GRAPHICS_UI_REQUIREMENTS.md, and the
implementation plan first. Use the model and reasoning effort assigned to the
current WP in section 11. Fetch origin, use Ryan's branch for the current wave,
and verify it descends from the exact common master baseline recorded in section
9. Never implement Eddy's packet and never consume Eddy's unmerged branch.

Use TDD. For each Ryan packet, run the RED focused test, implement only that WP,
run an incremental affected-target build, the packet-focused tests, and directly
affected subsystem/formatter/docs/package/target/fallback/self-host/native checks.
Do not run the complete repository Debug+Release+bootstrap matrix after an
ordinary packet unless section 4.4 forces an immediate barrier. If any focused
verification fails, stop Ryan's lane and fix the root cause before continuing.

Do not edit the shared Rocket 3 plan/requirements/global status documents during
the active wave. Commit each WP independently with its prescribed checkpoint
subject and a body recording Owner: Ryan, Packet, commands/results, and Lane
state: LANE-GREEN; push Ryan's wave branch. Continue only to the next Ryan packet
explicitly listed in the same wave. At the wave boundary stop implementation.
Ryan is the integration owner: after both lanes are pushed LANE-GREEN, perform
section 4.5 on a clean integration branch, update shared plan/traceability once,
push the accepted master baseline, and stop. Do not begin the next wave in the
same chat.
```

### Eddy launcher

```text
You are Eddy's Rocket 3 implementation worker. Work only on the packet currently
assigned to Eddy in sections 5A and 9 of
docs/ROCKET_3_0_GRAPHICS_UI_IMPLEMENTATION_PLAN.md. Read AGENTS.md,
docs/PROJECT_CONTEXT.md, docs/ROCKET_3_0_GRAPHICS_UI_REQUIREMENTS.md, and the
implementation plan first. Use the model and reasoning effort assigned to the
current WP in section 11. Fetch origin, use Eddy's branch for the current wave,
and verify it descends from the exact common master baseline recorded in section
9. Never implement Ryan's packet and never consume Ryan's unmerged branch.

Use TDD. For each Eddy packet, run the RED focused test, implement only that WP,
run an incremental affected-target build, the packet-focused tests, and directly
affected subsystem/formatter/docs/package/target/fallback/self-host/native checks.
Do not run the complete repository Debug+Release+bootstrap matrix after an
ordinary packet unless section 4.4 forces an immediate barrier. If any focused
verification fails, stop Eddy's lane and fix the root cause before continuing.

Do not edit the shared Rocket 3 plan/requirements/global status documents during
the active wave. Commit each WP independently with its prescribed checkpoint
subject and a body recording Owner: Eddy, Packet, commands/results, and Lane
state: LANE-GREEN; push Eddy's wave branch. Continue only to the next Eddy packet
explicitly listed in the same wave. At the wave boundary stop implementation and
report the pushed branch/commits and exact verification evidence to Ryan. Do not
merge to master, rotate the global handoff, take Ryan's work, or begin the next
wave until Ryan publishes the accepted barrier baseline.
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
