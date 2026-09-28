# WP06 incremental-test prerequisite

Base: c2268ca1c241c8d20777000364bc1488fa6cb1ee (includes BUG-003).

The original incremental test binary reproduced 32 failed assertions. A fresh
isolated MSVC Release build with LLVM disabled reproduced the failures while
temporarily recording every workspace-symbol response. There were 33 paired
comparisons: 32 differed only in `data.rocketGeneration`, and the empty-workspace
pair was identical. Names, kinds, ordered results, source URIs, ranges, and all
other fields matched. Independent sessions have different snapshot clocks.

The test oracle now normalizes only this numeric generation field while retaining
the complete ordered semantic response comparison. Dedicated live-session tests
assert exact generations 2 then 3 on both edited and cached symbols, no generation
change on read-only queries, and generation 3 on a filtered cached symbol.
No compiler/LSP production code changed.

Validation: affected CTest gate 6/6, exit 0, zero skips: language_server,
analysis_queue, language_server_analysis, language_server_incremental,
workspace_state, formatter. Incremental test duration 38.31 s; gate 41.83 s.
Diff whitespace check passed. Independent read-only review found no actionable
issues. This closes the specific inherited prerequisite, not a full Rocket
compiler/ASAN/release gate or any RocketIDE GUI acceptance.

Raw evidence is retained at
`C:\Users\Administrator\Desktop\Projects\RocketIDE-Build\wp06-20260927\evidence`:
`incremental-original-red.log`, `incremental-instrumented-red.log`,
`incremental-response-comparison.json`, `prerequisite-six-tests.log`.
The WP06 report retains the integration dependency and evidence manifest.
