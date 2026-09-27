# Rocket Documentation Status

**Audit date:** 2026-09-16
**Baseline:** Rocket 3.0.0 source release over WP34/F29 accepted implementation tree
`a9e7ea261f25c4438eb3594312a79e88da99eda0`. Full native, cross-target,
destination-execution, package/relocation, bootstrap, and visual acceptance is
green. WP35 release documentation, migration, version identity, and atomic
traceability closure is complete.
**Current tracked explanatory inventory:** 68 Markdown, text, RST, or ADOC
files, including the three Rocket 3.0 release-closure documents.

This ledger is the index for the documentation consistency pass. It separates live
guidance from versioned contracts and historical evidence so an old sentence can
be true for its release date without being mistaken for the current project
status.

## Authority chain

For current status, use the documents in this order:

1. [`README.md`](../README.md) for the public summary and supported workflow.
2. [`docs/ROADMAP.md`](ROADMAP.md) for milestone status and the next wave.
3. [`docs/PROJECT_CONTEXT.md`](PROJECT_CONTEXT.md) for accepted evidence,
   boundaries, and handoff details.
4. [The Rocket 3 requirements](ROCKET_3_0_GRAPHICS_UI_REQUIREMENTS.md) and
   [implementation plan](ROCKET_3_0_GRAPHICS_UI_IMPLEMENTATION_PLAN.md) for
   packet-level scope and maturity.
5. [The Rocket 3.0 release](RELEASE_3_0.md),
   [migration guide](MIGRATION_3_0.md), and
   [atomic traceability matrix](ROCKET_3_0_TRACEABILITY.md) for the released
   contract and evidence closure.

The current implemented syntax and API destination is
[`ROCKET_3_0_SYNTAX_DICTIONARY.md`](ROCKET_3_0_SYNTAX_DICTIONARY.md). The
versioned release, migration, audit, decision, and 1.x/2.0 dictionary files
below retain their recorded facts; a historical banner points readers here.

## Current baseline

- Rocket 2.1 portability (Phase 19) is complete on the four documented
  production target rows. The frozen Rocket 2.0 Windows SDK remains available
  for Scroll2Roll compatibility.
- Rocket 3.0.0 is released from the WP34/F29 implementation SHA above. GitHub
  Actions native run `35079104907` passed four native hosts, four supported
  cross builds, and four destination-execution jobs; visual run `35079104860`
  passed all four target rows. WP35 maps all 167 atomic requirements and closes
  the release documentation without rerunning those immutable matrices.
- The production compiler is `compiler/src/main.rocket`. C++ is the permanent
  reproducible stage0 plus runtime, native-adapter, language-server, and test
  infrastructure.
- Rocket 3.0's accepted typed asset-store implementation was the showcase
  reference module `src.rocket_assets`. Rocket 3.5 bundles `rocket.assets`;
  `ROCKET_3_5_ASSETS.md` documents the current ownership and path contract.

## Disposition table

| Path | Class | Disposition in this pass | Authority or note |
| --- | --- | --- | --- |
| `AGENTS.md` | POLICY | Reviewed, unchanged | Repository safety constraint |
| `CMakeLists.txt` | CONFIGURATION | Reviewed, unchanged | Build/test wiring; not explanatory prose |
| `CONTRIBUTING.md` | CURRENT | Updated | Current contribution and compatibility boundary |
| `dependencies/README.md` | CURRENT | Updated | Toolchain setup and current status links |
| `docs/BOOK.md` | CURRENT | Updated | Current learning path |
| `docs/CHARTER.md` | CURRENT | Updated | Governance and roadmap boundary |
| `docs/COMPILER_ARCHITECTURE.md` | CURRENT | Updated | Architecture plus current compiler boundary |
| `docs/CONCURRENCY.md` | VERSIONED/CURRENT | Updated | 1.8 contract remains current under later releases |
| `docs/DEBUGGING.md` | CURRENT | Updated | Current native-debugging status |
| `docs/DECISIONS.md` | HISTORICAL | Reviewed, chronology preserved | Decision journal is historical by design |
| `docs/DIAGNOSTICS.md` | CURRENT | Updated | Includes current Rocket 3 callable diagnostics |
| `docs/FFI_GUIDE.md` | VERSIONED/CURRENT | Updated | 2.1 FFI plus Rocket 3 safe boundary |
| `docs/LANGUAGE_SERVER.md` | VERSIONED/CURRENT | Updated | Protocol contract and current syntax metadata |
| `docs/MIGRATION_1_8.md` | HISTORICAL | Banner added | Versioned migration instructions |
| `docs/MIGRATION_2_0.md` | HISTORICAL | Banner added | Versioned migration instructions |
| `docs/MIGRATION_2_1.md` | VERSIONED/CURRENT | Banner added | Portability migration remains valid |
| `docs/MIGRATION_3_0.md` | VERSIONED/CURRENT | Added | Additive Rocket 2.1-to-3.0 migration guidance |
| `docs/PACKAGE_AUTHOR_GUIDE.md` | CURRENT | Updated | Current package author workflow |
| `docs/PACKAGES.md` | VERSIONED/CURRENT | Updated | 2.1 package contract and Rocket 3 note |
| `docs/PHASE_18_AUDIT.md` | HISTORICAL | Banner added | Frozen audit evidence |
| `docs/PHASE_19_AUDIT.md` | HISTORICAL | Banner added | Completed portability evidence |
| `docs/PHASE_20_AUDIT.md` | HISTORICAL | Banner added | “Deferred at audit time” is preserved |
| `docs/PROJECT_CONTEXT.md` | CURRENT | Updated | Current evidence and handoff |
| `docs/RAYLIB_TUTORIAL.md` | CURRENT/EXAMPLE | Updated | Legacy scaffold labeled; current modules linked |
| `docs/RELEASE_1_0.md` | HISTORICAL | Banner added | Frozen release contract |
| `docs/RELEASE_1_1.md` | HISTORICAL | Banner added | Frozen release contract |
| `docs/RELEASE_1_2.md` | HISTORICAL | Banner added | Frozen release contract |
| `docs/RELEASE_1_3.md` | HISTORICAL | Banner added | Frozen release contract |
| `docs/RELEASE_1_4.md` | HISTORICAL | Banner added | Frozen release contract |
| `docs/RELEASE_1_5.md` | HISTORICAL | Banner added | Frozen release contract |
| `docs/RELEASE_1_6.md` | HISTORICAL | Banner added | Frozen release contract |
| `docs/RELEASE_1_8.md` | HISTORICAL | Banner added | Old Phase 19-forward wording preserved |
| `docs/RELEASE_2_0.md` | HISTORICAL | Banner added | Old “did not perform Phase 19” wording preserved |
| `docs/RELEASE_2_1.md` | VERSIONED/CURRENT | Banner added | Portability release contract |
| `docs/RELEASE_3_0.md` | VERSIONED/CURRENT | Added | Rocket 3.0.0 release evidence and limitations |
| `docs/REPL.md` | EXPERIMENT | Updated | Explicitly non-guaranteed REPL experiment |
| `docs/ROADMAP.md` | CURRENT | Updated | Current milestone authority |
| `docs/ROCKET_1_0_SYNTAX_DICTIONARY.md` | HISTORICAL | Banner/link updated | Frozen 1.0 compatibility reference |
| `docs/ROCKET_1_1_SYNTAX_DICTIONARY.md` | HISTORICAL | Banner added | Frozen 1.1 compatibility reference |
| `docs/ROCKET_1_2_SYNTAX_DICTIONARY.md` | HISTORICAL | Banner added | Frozen 1.2 compatibility reference |
| `docs/ROCKET_1_3_SYNTAX_DICTIONARY.md` | HISTORICAL | Banner added | Frozen 1.3 compatibility reference |
| `docs/ROCKET_1_4_SYNTAX_DICTIONARY.md` | HISTORICAL | Banner added | Frozen 1.4 compatibility reference |
| `docs/ROCKET_1_5_SYNTAX_DICTIONARY.md` | HISTORICAL | Banner added | Frozen 1.5 compatibility reference |
| `docs/ROCKET_1_8_SYNTAX_DICTIONARY.md` | HISTORICAL | Banner added | Frozen 1.8 compatibility reference |
| `docs/ROCKET_2_0_SYNTAX_DICTIONARY.md` | HISTORICAL | Banner/link updated | Frozen 2.0 syntax reference |
| `docs/ROCKET_3_0_GRAPHICS_UI_IMPLEMENTATION_PLAN.md` | CURRENT | Updated | WP35 release closure and final packet status |
| `docs/ROCKET_3_0_GRAPHICS_UI_REQUIREMENTS.md` | CURRENT | Updated | Accepted release status and intended namespace |
| `docs/ROCKET_3_0_TRACEABILITY.md` | VERSIONED/CURRENT | Added | All 167 atomic requirements mapped to accepted evidence |
| `docs/ROCKET_3_0_WAVE_C_CALIBRATION.json` | EVIDENCE | Added | Machine-checked Wave C capacity/retention measurements and rationale |
| `docs/SELF_HOSTING.md` | CURRENT | Updated | Production Rocket compiler and stage0 boundary |
| `docs/SPEC.md` | VERSIONED/CURRENT | Updated | Normative grammar plus accepted Rocket 3 additions |
| `docs/STDLIB.md` | VERSIONED/CURRENT | Updated | Stable modules plus accepted Rocket 3 modules |
| `docs/TARGETS.md` | VERSIONED/CURRENT | Updated | Completed Phase 19 target contract |
| `docs/TOOLING.md` | VERSIONED/CURRENT | Updated | Released 3.0 tooling and syntax metadata |
| `editors/visualstudio/README.md` | CURRENT | Updated | Current client and LSP surface |
| `editors/vscode/README.md` | CURRENT | Updated | Current client and LSP surface |
| `examples/raylib_showcase/assets/about.txt` | EXAMPLE | Updated | Legacy compatibility label |
| `examples/raylib_showcase/CMakeLists.txt` | CONFIGURATION | Reviewed, unchanged | Example build wiring |
| `examples/raylib_showcase/README.md` | EXAMPLE | Updated | Legacy package and asset-store location |
| `examples/raylib_showcase/THIRD_PARTY_NOTICES.md` | POLICY | Reviewed, unchanged | License/provenance notice |
| `experiments/rocket3_foundation/README.md` | EXPERIMENT | Updated | Provisional kernels versus public modules |
| `experiments/rocket3_visual_compare/CMakeLists.txt` | CONFIGURATION | Reviewed, unchanged | Experiment build wiring |
| `experiments/rocket3_visual_compare/README.md` | EXPERIMENT | Updated | Provisional non-production boundary |
| `README.md` | CURRENT | Updated | Public status and navigation |
| `SECURITY.md` | POLICY/CURRENT | Updated | Supported release and target policy |

## Process records

The implementation plan is
[`docs/superpowers/plans/2026-09-10-documentation-refresh.md`](superpowers/plans/2026-09-10-documentation-refresh.md).
This ledger is itself a process record added after the 61-file baseline
inventory; neither process file is counted in that baseline.

## Verification record

The documentation-only diff was reviewed on 2026-09-10.

- **Coverage:** all 61 baseline inventory paths were reviewed. Fifty-six
  tracked explanatory files were modified; five baseline paths were intentionally
  unchanged (`AGENTS.md`, `CMakeLists.txt`, the showcase CMake file, the
  third-party notice, and the visual-compare CMake file). Three new Markdown
  process/reference files were added: this ledger, the Rocket 3 dictionary, and
  the implementation plan.
- **Formatting:** `git diff --check` passed. Git's normal Windows LF-to-CRLF
  warnings are line-ending notices, not whitespace errors.
- **Links:** the changed/new Markdown and text set contained 128 path-like
  Markdown links; a containing-directory `Test-Path` check found 0 missing
  targets.
- **Dictionary cross-check:** the public-function inventory across
  `stdlib/rocket/**/*.rocket` and the accepted reference package contains 284
  names; all 284 names are present in `ROCKET_3_0_SYNTAX_DICTIONARY.md`.
  Source remains authoritative for the complete signature and implementation
  behavior.
- **Stale-status review:** no current-facing document retains the old deferred-
  support or Windows-only claim. Remaining old wording is confined to the
  explicitly historical `PHASE_20_AUDIT.md`,
  `RELEASE_1_0.md`, and `RELEASE_1_8.md`; `rocket.assets` and
  `stdlib/rocket/assets` mentions were labeled unavailable or intended-future
  at the WP35 release, with `src.rocket_assets` as the 3.0 reference package.
  Rocket 3.5 WP5 now bundles `rocket.assets`.
- **Scope:** no compiler, Rocket source, standard-library source, tests, build
  wiring, generated output, or dependency artifact changed. The only
  dependency-path change is the explanatory `dependencies/README.md` file
  listed in the disposition table.

Compiler, runtime, and test results are not claimed by this prose-only change.

## WP35 release-closure verification

The WP35 release closure was verified on 2026-09-16 without repeating WP34's
full accepted matrices.

- **Release identity:** the focused Windows Release configure/build completed,
  the built compiler reported `rocketc 3.0.0`, and
  `phase19_release_tooling|cli_version` passed `2/2`.
- **Atomic traceability:** all 167 unique requirement IDs are present in
  `ROCKET_3_0_TRACEABILITY.md`; no requirement is omitted.
- **Documentation links:** the 36 changed/new Markdown files passed a local
  path-target check with zero missing targets.
- **Script syntax:** five changed Python files and two changed PowerShell files
  passed parser-only syntax checks.
- **Formatting:** `git diff --check` passed; line-ending notices are normal
  Windows checkout warnings.
- **Scope:** WP35 updates release identity, packaging/workflow metadata,
  release-focused tests, and current-facing documentation. It reuses the
  immutable WP34 native/visual acceptance evidence instead of rerunning the
  full Debug/Release and cross-platform matrices.
