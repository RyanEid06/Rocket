# Rocket Documentation Status

**Audit date:** 2026-09-10
**Baseline:** `master` at `fe948e98070d3d61b6ea02cdd0dfc787fcdae6fa`
**Baseline inventory:** 61 Markdown, text, RST, ADOC, or configuration-like explanatory files before this ledger and the implementation plan were added.

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

The current implemented syntax and API destination is
[`ROCKET_3_0_SYNTAX_DICTIONARY.md`](ROCKET_3_0_SYNTAX_DICTIONARY.md). The
versioned release, migration, audit, decision, and 1.x/2.0 dictionary files
below retain their recorded facts; a historical banner points readers here.

## Current baseline

- Rocket 2.1 portability (Phase 19) is complete on the four documented
  production target rows. The frozen Rocket 2.0 Windows SDK remains available
  for Scroll2Roll compatibility.
- Rocket 3 Wave B is accepted on the integrated Windows baseline and published
  to `master` at the baseline SHA above. Wave C is ready with Eddy's
  WP25 -> WP26 -> WP27 -> WP30 queue; WP29 remains deferred until the Wave C
  barrier.
- The production compiler is `compiler/src/main.rocket`. C++ is the permanent
  reproducible stage0 plus runtime, native-adapter, language-server, and test
  infrastructure.
- The accepted typed asset-store implementation is the reference package module
  `examples/raylib_showcase/src/rocket_assets.rocket`, imported as
  `src.rocket_assets`. The `rocket.assets` / `stdlib/rocket/assets` names in
  the provisional Rocket 3 design are not available modules in this checkout.

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
| `docs/ROCKET_3_0_GRAPHICS_UI_IMPLEMENTATION_PLAN.md` | CURRENT | Updated | Published Wave B baseline and asset path |
| `docs/ROCKET_3_0_GRAPHICS_UI_REQUIREMENTS.md` | CURRENT | Updated | Wave B status and intended namespace |
| `docs/SELF_HOSTING.md` | CURRENT | Updated | Production Rocket compiler and stage0 boundary |
| `docs/SPEC.md` | VERSIONED/CURRENT | Updated | Normative grammar plus accepted Rocket 3 additions |
| `docs/STDLIB.md` | VERSIONED/CURRENT | Updated | Stable modules plus accepted Rocket 3 modules |
| `docs/TARGETS.md` | VERSIONED/CURRENT | Updated | Completed Phase 19 target contract |
| `docs/TOOLING.md` | VERSIONED/CURRENT | Updated | 2.1 tooling plus Wave B syntax metadata |
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
  `stdlib/rocket/assets` mentions are labeled unavailable or intended-future,
  with `src.rocket_assets` identified as the current reference package.
- **Scope:** no compiler, Rocket source, standard-library source, tests, build
  wiring, generated output, or dependency artifact changed. The only
  dependency-path change is the explanatory `dependencies/README.md` file
  listed in the disposition table.

Compiler, runtime, and test results are not claimed by this prose-only change.
