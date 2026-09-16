# Rocket 3.0 atomic traceability

This matrix closes all **167 atomic requirements** in
`ROCKET_3_0_GRAPHICS_UI_REQUIREMENTS.md`. Every listed ID is `ACCEPTED` for
Rocket 3.0.0. Packet-level commands, counts, hashes, and commit provenance stay
in the implementation plan; the release-wide platform evidence is in
`RELEASE_3_0.md`.

Common target evidence is GitHub Actions native run `35079104907` and visual
run `35079104860` on Windows x64, Linux x64, Linux ARM64, and macOS ARM64.
Earlier packet maturity labels describe the state at that packet boundary;
WP34 promoted F02-F29 to `ACCEPTED`, and WP35 closes GOV, F01, and F30.

| Atomic IDs | Owner | Implementation | Tests | Documentation | Target coverage and evidence | State |
| --- | --- | --- | --- | --- | --- | --- |
| R3-GOV-001, R3-GOV-002, R3-GOV-003, R3-GOV-004, R3-GOV-005, R3-GOV-006, R3-GOV-007 | WP00, WP09, WP35 | `docs/DECISIONS.md`, requirements and plan | `phase19_release_tooling` governance checks | `CHARTER.md`, `DOCUMENTATION_STATUS.md`, `RELEASE_3_0.md` | Repository-wide; WP34/WP35 release evidence | ACCEPTED |
| R3-ISO-001, R3-ISO-002, R3-ISO-003, R3-ISO-004, R3-ISO-005, R3-ISO-006, R3-ISO-007 | WP00, WP09, WP34 | `CMakePresets.json`, `dependencies/manifest.json`, `.github/workflows/phase19-native.yml` | release-tooling, package, target, relocation, cross-SDK tests | `TARGETS.md`, `PHASE_19_AUDIT.md`, implementation plan | Four native hosts plus four cross paths; run `35079104907` | ACCEPTED |
| R3-F01-001, R3-F01-002, R3-F01-003, R3-F01-004 | WP00, WP09, WP35 | Requirements, plan, decision and documentation authority chain | `phase19_release_tooling` documentation closure | `DOCUMENTATION_STATUS.md`, `ROADMAP.md`, `PROJECT_CONTEXT.md` | Repository-wide governance; WP35 checkpoint | ACCEPTED |
| R3-F02-001, R3-F02-002, R3-F02-003, R3-F02-004, R3-F02-005, R3-F02-006, R3-F02-007, R3-F02-008, R3-F02-009, R3-F02-010, R3-F02-011, R3-F02-012 | WP10, WP11A | `src/`, `compiler/src/main.rocket`, formatter and LSP | named-argument stage0/self-host/parity/formatter/LSP/target suites | `SPEC.md`, `TOOLING.md`, syntax dictionary | Four native hosts; WP34 288/288 matrices | ACCEPTED |
| R3-F03-001, R3-F03-002, R3-F03-003, R3-F03-004, R3-F03-005, R3-F03-006, R3-F03-007 | WP11 | Stage0 and self-host callable normalization | default-argument stage0/self-host/parity/diagnostic suites | `SPEC.md`, syntax dictionary | Four native hosts; WP34 matrices | ACCEPTED |
| R3-F04-001, R3-F04-002, R3-F04-003, R3-F04-004, R3-F04-005, R3-F04-006, R3-F04-007, R3-F04-008 | WP12 | Compiler-owned `std.math` in stage0/self-host | scalar vectors, runtime, LLVM, target-surface tests | `STDLIB.md`, syntax dictionary | Direct numeric execution on all four native hosts | ACCEPTED |
| R3-F05-001, R3-F05-002, R3-F05-003, R3-F05-004 | WP13 | `stdlib/rocket/motion.rocket` | easing endpoint, overshoot, parity and target tests | `STDLIB.md`, syntax dictionary | Four native hosts | ACCEPTED |
| R3-F06-001, R3-F06-002, R3-F06-003, R3-F06-004, R3-F06-005, R3-F06-006 | WP06, WP13 | `stdlib/rocket/motion.rocket` | tween/timeline/reduced-motion stage0/self-host suites | `STDLIB.md`, `BOOK.md` | Four native hosts | ACCEPTED |
| R3-F07-001, R3-F07-002, R3-F07-003, R3-F07-004, R3-F07-005 | WP14 | `rocket.raylib.safe`, native adapter | safe-geometry, lifetime, native-adapter, target suites | `FFI_GUIDE.md`, `RAYLIB_TUTORIAL.md` | Four native hosts and capability failures | ACCEPTED |
| R3-F08-001, R3-F08-002, R3-F08-003, R3-F08-004 | WP15 | graphics texture/filter surface and adapter | texture/filter stage0/self-host/native tests | `STDLIB.md`, syntax dictionary | Four native hosts | ACCEPTED |
| R3-F09-001, R3-F09-002, R3-F09-003, R3-F09-004, R3-F09-005 | WP16 | checked render targets and scoped graphics state | render-target, scope, screenshot and cleanup suites | `STDLIB.md`, syntax dictionary | Four native hosts | ACCEPTED |
| R3-F10-001, R3-F10-002, R3-F10-003, R3-F10-004 | WP17 | checked shader resources/uniforms | shader deterministic/native/target suites | `STDLIB.md`, syntax dictionary | Four native hosts | ACCEPTED |
| R3-F11-001, R3-F11-002, R3-F11-003, R3-F11-004 | WP18 | display quality, DPI, MSAA and window transitions | display/native/screenshot suites | `STDLIB.md`, syntax dictionary | Four native hosts | ACCEPTED |
| R3-F12-001, R3-F12-002, R3-F12-003, R3-F12-004 | WP01, WP19 | `stdlib/rocket/graphics.rocket` | graphics-core stage0/self-host/target suites | `STDLIB.md`, syntax dictionary | Four native hosts | ACCEPTED |
| R3-F13-001, R3-F13-002, R3-F13-003, R3-F13-004, R3-F13-005 | WP02, WP19 | public `rocket.graphics.Color` | color conversion/clamping/parity suites | `STDLIB.md`, syntax dictionary | Four native hosts | ACCEPTED |
| R3-F14-001, R3-F14-002, R3-F14-003, R3-F14-004 | WP20 | `rocket.graphics.shapes` | shapes/input stage0/self-host/native/target suites | `STDLIB.md`, `BOOK.md` | Four native hosts | ACCEPTED |
| R3-F15-001, R3-F15-002, R3-F15-003, R3-F15-004 | WP01, WP20 | `rocket.graphics.input` and safe hit testing | input mapping and outside-viewport suites | `STDLIB.md`, syntax dictionary | Four native hosts | ACCEPTED |
| R3-F16-001, R3-F16-002, R3-F16-003, R3-F16-004, R3-F16-005, R3-F16-006 | WP22 | public typography and native text adapter | typography measurement/wrapping/clipping suites | `STDLIB.md`, `BOOK.md` | Four native hosts | ACCEPTED |
| R3-F17-001, R3-F17-002, R3-F17-003, R3-F17-004, R3-F17-005 | WP03, WP21 | `rocket.graphics.canvas` | VirtualCanvas mapping/render/input/screenshot suites | `STDLIB.md`, syntax dictionary | Four native hosts | ACCEPTED |
| R3-F18-001, R3-F18-002, R3-F18-003, R3-F18-004, R3-F18-005 | WP05, WP23 | `rocket.ui` context/frame/response/IDs | UI context lifecycle, focus, modal and misuse suites | `STDLIB.md`, syntax dictionary | Four native hosts | ACCEPTED |
| R3-F19-001, R3-F19-002, R3-F19-003, R3-F19-004, R3-F19-005, R3-F19-006 | WP03, WP24 | `rocket.ui.layout` | Row/Column/Grid/Stack/Anchor and invalid-layout suites | `STDLIB.md`, syntax dictionary | Four native hosts | ACCEPTED |
| R3-F20-001, R3-F20-002, R3-F20-003, R3-F20-004, R3-F20-005 | WP04, WP25 | `rocket.ui.theme`, `rocket.ui.styles` | theme/style state and validation suites | `STDLIB.md`, syntax dictionary | Four native hosts | ACCEPTED |
| R3-F21-001, R3-F21-002, R3-F21-003, R3-F21-004 | WP26 | `rocket.ui.controls` | controls interaction/keyboard/focus/modal suites | `STDLIB.md`, `BOOK.md` | Four native hosts | ACCEPTED |
| R3-F22-001, R3-F22-002, R3-F22-003, R3-F22-004 | WP27 | `rocket.ui.containers` | container/dialog/overlay/tooltip/toast suites | `STDLIB.md`, `BOOK.md` | Four native hosts | ACCEPTED |
| R3-F23-001, R3-F23-002, R3-F23-003, R3-F23-004, R3-F23-005 | WP28 | showcase `src.rocket_assets` reference module | asset-store path, bound, cleanup, relocation and native suites | `STDLIB.md`, showcase README, syntax dictionary | Four native hosts | ACCEPTED |
| R3-F24-001, R3-F24-002, R3-F24-003, R3-F24-004, R3-F24-005 | WP29 | unified compiler/public errors and lease-first lifetimes | error/lifetime stage0/self-host/native/target suites | `DIAGNOSTICS.md`, `STDLIB.md` | Four native hosts | ACCEPTED |
| R3-F25-001, R3-F25-002, R3-F25-003, R3-F25-004 | WP05, WP30 | bounded UI, measurement and asset state | capacity/eviction/100000-ID stress/cleanup suites | `ROCKET_3_0_WAVE_C_CALIBRATION.json` | Four native hosts; calibrated evidence | ACCEPTED |
| R3-F26-001, R3-F26-002, R3-F26-003, R3-F26-004 | WP08, WP31 | performance instrumentation and enforced budgets | performance budget and evidence-schema tests | `ROCKET_3_0_WP31_PERFORMANCE_BUDGETS.json` | WP34 four-host matrices | ACCEPTED |
| R3-F27-001, R3-F27-002, R3-F27-003, R3-F27-004, R3-F27-005, R3-F27-006, R3-F27-007 | WP07, WP08, WP32 | visual capture, comparison, goldens and CI | canonical scene, PNG, tolerance and guarded-update tests | visual workflow and syntax dictionary | Four visual jobs; run `35079104860` | ACCEPTED |
| R3-F28-001, R3-F28-002, R3-F28-003, R3-F28-004 | WP33 | focused public examples and premium showcase | compile/runtime/package/relocation/visual example checks | `BOOK.md`, example READMEs | Four native hosts | ACCEPTED |
| R3-F29-001, R3-F29-002, R3-F29-003, R3-F29-004, R3-F29-005 | WP09, WP34 | target/compiler/bootstrap/package/compatibility workflows | complete 288/288 and stage0 209/209 native matrices, cross execution | `TARGETS.md`, implementation plan, `RELEASE_3_0.md` | Run `35079104907`; all required jobs green | ACCEPTED |
| R3-F30-001, R3-F30-002, R3-F30-003, R3-F30-004 | WP00, WP35 | release identity, synchronized docs, traceability and packaging metadata | focused release-tooling/version/link/stale-status checks | `RELEASE_3_0.md`, `MIGRATION_3_0.md`, this matrix, documentation ledger | Release tag `v3.0.0`; WP34 evidence reused without rerunning full matrices | ACCEPTED |

No atomic ID is removed or replaced. Historical packet records retain their
then-current maturity wording, while the live status documents and this matrix
record final release acceptance.
