# Rocket 3 performance and visual acceptance tooling

WP31 promotes the performance evidence schema and deterministic scene into the
top-level Rocket build and release test matrix. The final budgets, calibration
method, environment, and seven-sample evidence are recorded in
`docs/ROCKET_3_0_WP31_PERFORMANCE_BUDGETS.json`.

WP32 completes the visual-regression surface with deterministic canonical
scenes, lossless PNG I/O, approved PPM references, per-channel and aggregate
metrics, changed-pixel bounds, difference images, heatmaps, and native
render-target capture. Windows runs the full strict suite; other supported
hosts run the portable deterministic subset and structural checks.

Golden references are review-only and are never overwritten by a test. To
prepare an explicitly approved update for review, build
`rocket_phase32_golden_tool` and provide all approval fields:

```powershell
rocket_phase32_golden_tool --output <candidate-directory> `
  --approval-id <review-id> --approved-by <reviewer> --approved-at <yyyy-mm-dd>
```

The committed approvals and calibrated exact tolerances live in
`tests/visual/goldens/manifest.json`. Platform-specific references or looser
tolerances require evidence and an explicit manifest review.

Configure and test it with generated state below the packet boundary:

```powershell
cmake -S experiments/rocket3_visual_compare -B out/rocket3-provisional/wp08/build -G Ninja
cmake --build out/rocket3-provisional/wp08/build
ctest --test-dir out/rocket3-provisional/wp08/build --output-on-failure
```

The final integrated performance gate is the top-level CTest named
`rocket3_performance_budgets`. It runs 30 warm-up frames before measuring 120
steady-state frames and fails on allocations, temporary strings, layout
recomputation, repeated measurement, asset lookup, FFI, render-target switch,
texture upload, state/cache growth, or timing regressions.

The final integrated visual gate is the top-level CTest named
`rocket3_visual_regression`. Its artifacts are written beneath
`out/rocket3-provisional/wp32/<configuration>/visual`.
