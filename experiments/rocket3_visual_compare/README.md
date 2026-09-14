# Rocket 3 performance and visual acceptance tooling

WP31 promotes the performance evidence schema and deterministic scene into the
top-level Rocket build and release test matrix. The final budgets, calibration
method, environment, and seven-sample evidence are recorded in
`docs/ROCKET_3_0_WP31_PERFORMANCE_BUDGETS.json`.

This standalone C++20 experiment compares equal-sized raw RGBA buffers and
defines versioned, value-only evidence schemas for performance and golden
records. The raw comparator and golden approval portion remains provisional
until WP32 adds image I/O, capture, approved references, and CI integration.
Automatic golden updates remain forbidden.

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
