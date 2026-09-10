# Rocket Documentation Consistency Refresh Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Audit all 61 Markdown/text/config-like explanatory files and bring every current-facing Rocket document into agreement with the accepted Rocket 2.1 baseline, accepted Rocket 3 Wave B, and ready Wave C queue without rewriting historically accurate release or audit records.

**Architecture:** Keep documentation in two explicit tiers. Current guidance describes the live product and points to one Rocket 3 dictionary; versioned releases, audits, migrations, and syntax dictionaries remain historical contracts and receive only status labeling where their old claims could be mistaken for current status. A status ledger records the disposition of every explanatory file, while live references describe only APIs and paths that exist in this checkout.

**Tech Stack:** Markdown and plain-text documentation, PowerShell/`rg` read-only consistency checks, and Git diff validation. No compiler, runtime, test, CMake, or generated-output changes.

**Spec:** `docs/ROCKET_3_0_GRAPHICS_UI_REQUIREMENTS.md`, `docs/ROCKET_3_0_GRAPHICS_UI_IMPLEMENTATION_PLAN.md`, `docs/SPEC.md`, `docs/STDLIB.md`, and `docs/PROJECT_CONTEXT.md`.

## Global Constraints

- Rocket 3.0 work is additive on the accepted Rocket 2.1 portability baseline and preserves valid predecessor source and runtime ABI v1.
- `compiler/src/main.rocket` remains the production compiler; C++ remains the permanent reproducible stage0 plus runtime/native/tooling/test infrastructure.
- Wave B is accepted on the integrated Windows baseline; Wave C begins with Eddy's WP25 -> WP26 -> WP27 -> WP30 queue and WP29 remains deferred until the Wave C barrier.
- Historical documents retain the facts that were true at their recorded release or audit date; do not rewrite their evidence into current status.
- The accepted typed asset store is currently implemented at `examples/raylib_showcase/src/rocket_assets.rocket` and imported as `src.rocket_assets`; do not document `rocket.assets` or `stdlib/rocket/assets` as an available module until code exists there.
- Tests, build files, generated output, dependency archives, and source implementation files are out of scope unless a read-only check verifies a documented path.
- Do not start any operation expected to consume more than 20 GiB of disk space.

---

### Task 1: Create the documentation disposition ledger

**Files:**
- Create: `docs/DOCUMENTATION_STATUS.md`
- Read-only inventory: all 61 files returned by `rg --files -g '*.md' -g '*.txt' -g '*.rst' -g '*.adoc' -g '!out/**' -g '!.git/**'`

**Interfaces:**
- Consumes: the repository file inventory, current `master` status, and the authority rules in `docs/PROJECT_CONTEXT.md` and the Rocket 3 plan.
- Produces: a dated disposition table naming every explanatory file as `CURRENT`, `VERSIONED`, `HISTORICAL`, `EXAMPLE`, `EXPERIMENT`, `CONFIGURATION`, or `POLICY`, plus the authority for its claims.

- [x] **Step 1: Capture the complete inventory.** Run `rg --files -g '*.md' -g '*.txt' -g '*.rst' -g '*.adoc' -g '!out/**' -g '!.git/**' | Sort-Object` from the repository root and confirm the count is 61.
- [x] **Step 2: Write the ledger authority rules.** Record audit date `2026-09-10`, baseline `master` at `fe948e98070d3d61b6ea02cdd0dfc787fcdae6fa`, the live-status chain (`README.md` -> `docs/ROADMAP.md` -> `docs/PROJECT_CONTEXT.md` -> Rocket 3 plan/requirements), and the rule that historical evidence is not rewritten.
- [x] **Step 3: Add one disposition row per inventory file.** Explicitly call out stale `SECURITY.md`, historical Phase 19 wording, the legacy raylib showcase, and the `rocket.assets` versus `src.rocket_assets` mismatch.
- [x] **Step 4: Verify ledger coverage.** Count the inventory and the unique backticked document paths listed in the ledger; the ledger must state any intentionally excluded test fixture or generated file.

### Task 2: Refresh live product, governance, and reference documents

**Files:**
- Modify: `README.md`, `SECURITY.md`, `CONTRIBUTING.md`
- Modify: `docs/CHARTER.md`, `docs/BOOK.md`, `docs/PROJECT_CONTEXT.md`, `docs/ROADMAP.md`
- Modify: `docs/SELF_HOSTING.md`, `docs/SPEC.md`, `docs/STDLIB.md`, `docs/TOOLING.md`
- Modify: `docs/FFI_GUIDE.md`, `docs/PACKAGES.md`, `docs/PACKAGE_AUTHOR_GUIDE.md`
- Modify: `docs/LANGUAGE_SERVER.md`, `docs/CONCURRENCY.md`, `docs/DEBUGGING.md`
- Modify: `docs/ROCKET_3_0_GRAPHICS_UI_IMPLEMENTATION_PLAN.md`, `docs/ROCKET_3_0_GRAPHICS_UI_REQUIREMENTS.md`

**Interfaces:**
- Consumes: accepted Wave B evidence and actual public Rocket modules under `stdlib/rocket/`.
- Produces: current-facing prose that distinguishes completed 2.1, accepted-but-not-final Wave B, ready Wave C, and the frozen 2.0 SDK kept for compatibility.

- [x] **Step 1: Correct root navigation.** Keep README's Wave B evidence, link `DOCUMENTATION_STATUS.md` and the new Rocket 3 dictionary, state that Rocket 3.0 is not a final release, and preserve the Scroll2Roll 2.0 SDK note.
- [x] **Step 2: Correct support policy.** Change `SECURITY.md` from “Phase 19 is deferred” to the accepted Rocket 2.1 release line and four-target status, while labeling Wave B as development work.
- [x] **Step 3: Correct contribution and charter boundaries.** Separate frozen 2.x contracts from additive Rocket 3 work and retain the C++ stage0/Rocket production boundary.
- [x] **Step 4: Make the Book current.** Change the learning-path status to Rocket 2.1 plus accepted Wave B, add a graphics/UI chapter for the current modules, and link normative details to the dictionary, `SPEC.md`, and `STDLIB.md`.
- [x] **Step 5: Reconcile normative references.** Update status/introduction notes in the listed references so they identify accepted Wave B additions, queued Wave C work, and the new dictionary without erasing versioned contract semantics.
- [x] **Step 6: Refresh self-hosting and project status.** Add current bootstrap evidence and the explicit Rocket-source production compiler statement to `SELF_HOSTING.md`; keep `PROJECT_CONTEXT.md` and `ROADMAP.md` authoritative and record the actual `src.rocket_assets` location.
- [x] **Step 7: Reconcile the Rocket 3 status records.** Update the implementation plan and requirements preambles to point at the published `master` status commit `fe948e98070d3d61b6ea02cdd0dfc787fcdae6fa`, distinguish the underlying `6bb9841` integration commit from the publication barrier, and state that the `rocket.assets` namespace is planned while the accepted Wave B reference implementation is `src.rocket_assets`.

### Task 3: Add the current Rocket 3.0 syntax and API dictionary

**Files:**
- Create: `docs/ROCKET_3_0_SYNTAX_DICTIONARY.md`
- Modify: `README.md`, `docs/BOOK.md`, `docs/SPEC.md`, `docs/STDLIB.md`
- Modify: `docs/ROCKET_1_0_SYNTAX_DICTIONARY.md`, `docs/ROCKET_2_0_SYNTAX_DICTIONARY.md`

**Interfaces:**
- Consumes: callable rules in `docs/SPEC.md`, signatures in `stdlib/rocket/*.rocket`, and asset-store signatures in `examples/raylib_showcase/src/rocket_assets.rocket`.
- Produces: one copyable current reference for the implemented Wave B surface without changing historical dictionary contracts.

- [x] **Step 1: Write the status preamble.** State accepted Wave B, queued Wave C, and “not a final Rocket 3.0 release”; link the ledger, requirements, implementation plan, and historical dictionaries.
- [x] **Step 2: Document callable syntax exactly.** Include positional, named, positional-then-named, default parameters, labeled enums, closures, immediately-invoked lambdas, built-ins, intrinsics, evaluation order, declaration-order normalization, public-name commitments, diagnostics, and all documented default exclusions.
- [x] **Step 3: Document accepted modules.** Cover `std.math`, `rocket.motion`, `rocket.graphics`, `rocket.graphics.shapes`, `rocket.graphics.input`, `rocket.graphics.canvas`, `rocket.ui`, `rocket.ui.layout`, and `rocket.raylib.safe` using signatures present in the checkout.
- [x] **Step 4: Document the asset store honestly.** Describe `src.rocket_assets` under the showcase package, its typed references, cleanup/path rules, and its distinction from the planned `rocket.assets` namespace; do not imply a standard-library module.
- [x] **Step 5: Link the dictionary.** Add it to README, Book, current spec/library introductions, and historical dictionary preambles as the current implemented-surface destination.

### Task 4: Clarify tutorials, examples, editors, and experiments

**Files:**
- Modify: `docs/RAYLIB_TUTORIAL.md`
- Modify: `examples/raylib_showcase/README.md`, `examples/raylib_showcase/assets/about.txt`
- Modify: `experiments/rocket3_foundation/README.md`, `experiments/rocket3_visual_compare/README.md`
- Modify: `editors/vscode/README.md`, `editors/visualstudio/README.md`

**Interfaces:**
- Consumes: actual example package names, current Rocket 3 module paths, and the plan's provisional/integration-ready terminology.
- Produces: truthful guidance that does not present a Rocket 1.4 compatibility showcase or provisional experiment as the canonical Wave B API.

- [x] **Step 1: Label the legacy scaffold.** At the top of the tutorial and showcase README, identify sections using legacy `src.rocket_raylib` versus current Rocket 3 modules and link the new dictionary.
- [x] **Step 2: Correct asset prose.** Identify `src.rocket_assets` and its `rocket.toml` package context; preserve its 1.4 compatibility facts without calling it `rocket.assets`.
- [x] **Step 3: Clarify experiment disposition.** State which kernels are provisional/internal, which public modules supersede them, and where Wave C resumes; retain the visual comparator's non-production boundary.
- [x] **Step 4: Refresh editor guidance.** Add the current named/default syntax, shared LSP metadata, dictionary, and status-ledger links without changing extension code.

### Task 5: Mark historical records without rewriting history

**Files:**
- Modify only preambles of `docs/PHASE_18_AUDIT.md`, `docs/PHASE_19_AUDIT.md`, `docs/PHASE_20_AUDIT.md`
- Modify only preambles of `docs/RELEASE_1_0.md`, `docs/RELEASE_1_1.md`, `docs/RELEASE_1_2.md`, `docs/RELEASE_1_3.md`, `docs/RELEASE_1_4.md`, `docs/RELEASE_1_5.md`, `docs/RELEASE_1_6.md`, `docs/RELEASE_1_8.md`, `docs/RELEASE_2_0.md`, `docs/RELEASE_2_1.md`
- Modify only preambles of `docs/MIGRATION_1_8.md`, `docs/MIGRATION_2_0.md`, `docs/MIGRATION_2_1.md`
- Modify only preambles of `docs/ROCKET_1_1_SYNTAX_DICTIONARY.md`, `docs/ROCKET_1_2_SYNTAX_DICTIONARY.md`, `docs/ROCKET_1_3_SYNTAX_DICTIONARY.md`, `docs/ROCKET_1_4_SYNTAX_DICTIONARY.md`, `docs/ROCKET_1_5_SYNTAX_DICTIONARY.md`, `docs/ROCKET_1_8_SYNTAX_DICTIONARY.md`, `docs/ROCKET_2_0_SYNTAX_DICTIONARY.md`
- Read-only review: `docs/DECISIONS.md`, `docs/COMPILER_ARCHITECTURE.md`, and other versioned references

**Interfaces:**
- Consumes: ledger classifications and each file's recorded release/audit date.
- Produces: unmistakable historical labeling plus links to current references, with no altered test counts, hashes, dates, or past decisions.

- [x] **Step 1: Add a uniform historical banner.** Place a short blockquote after each title or existing historical preamble directing readers to the ledger, roadmap, project context, and Rocket 3 dictionary.
- [x] **Step 2: Preserve all historical content outside the banner.** Keep phrases such as “Phase 19 portability was deferred” when explicitly scoped to the old audit date and keep old target statements unchanged.
- [x] **Step 3: Verify decision chronology.** Leave accepted entries in `docs/DECISIONS.md` unchanged; add only a non-semantic introductory link if the audit shows a past decision could be mistaken for a live queue.

### Task 6: Run documentation-only verification and prepare handoff

**Files:**
- Read-only verification over all modified files and all new link targets.

**Interfaces:**
- Consumes: the updated live references, dictionary, ledger, and historical banners.
- Produces: evidence that current docs have no stale Phase 19/deferred-support claim, all new links resolve, the actual asset-store path is used, and no tests/build/source files changed.

- [x] **Step 1: Check formatting and stale claims.** Run `git diff --check` and search current docs for `phase 19 (is )?deferred`, `windows x64 is the only supported target`, `rocket.assets`, and `stdlib/rocket/assets`. Any remaining old-status match must be in an explicitly historical or intended-future section.
- [x] **Step 2: Verify new relative links.** Resolve every new link with PowerShell `Test-Path` from its containing directory and confirm the README, Book, and current references reach both new documents.
- [x] **Step 3: Confirm scope.** Run `git status --short --branch` and `git diff --name-only`; the diff must contain documentation/status files only and no `tests/`, `src/`, `compiler/`, `stdlib/`, `CMakeLists.txt`, generated output, or dependency artifacts/configuration. `dependencies/README.md` is an explicitly scoped explanatory-file exception.
- [x] **Step 4: Record results.** Add the audit date, baseline SHA, changed-file list, stale-match review, link result, and scope result to `DOCUMENTATION_STATUS.md`. Do not claim a compiler/test pass from this documentation-only change.
- [x] **Step 5: Prepare handoff.** Report the audited categories, corrected claims, preserved historical records, actual asset-store path, Rocket/C++ boundary, and PowerShell commands for the user to review, commit, push `master`, verify `git ls-remote`, and keep only the desired remote branch state.
