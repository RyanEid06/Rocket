# FINAL-WP01A - Responsive analysis infrastructure

## Entry and scope

Approved roadmap: RocketIDE/docs/ROCKETIDE_FINAL_ROADMAP_IMPLEMENTATION.md, read completely.
Rocket AGENTS.md read completely. Only FINAL-WP01A authorized.
2026-09-17 fetch/prune and live refs verified: master/origin/master
10f295dd000b93fe50d254be21fedffd17892aeb; Eddie local/tracking/live
2a03b7358399d70efac5a9bb056851c0ff3521d4. Initial main worktree clean.
Existing detached worktrees out/w/a and out/worktrees/rocket3-ryan-wave-a
at fe2a85e499c5107752bca31837d09bcd607d393d preserved.
Dedicated branch codex/rocketide-final-wp01-lsp, worktree
out/w/lsp (original out/worktrees/rocketide-final-wp01-lsp retained as a junction).
Final build directories are dedicated
Rocket/out/wp01a/{release,debug,stage0-release,stage0-debug,asan}. Initial build/logs
under the worktree out/final-wp01a are retained. The long initial build path caused
four unchanged package assertions to fail; the identical executable passed when
run from Rocket/out/wp01a-test. Final matrices use a short junction with files
physically inside this worktree out/final-wp01a/matrix to satisfy workflow guards
and Windows path limits; no package behavior/test expectations were weakened.
No upstream until first push. Shared pinned dependencies reused through a junction without changes;
initial configure failed for missing ignored raylib, resolved with installed junction.
RocketIDE accepted automated entry baseline supplied by user/roadmap: 350 passed,
0 failed/skipped, Release 0 warnings/errors, win-x64 publish/assets succeeded.
No RocketIDE files changed or verification rerun. Manual smoke remains deferred;
the follow-up attempt below records direct observation and an SDK configuration
blocker, not a Computer Use bridge failure.

## Design and execution plan

Existing rebuildSnapshot synchronously discovers sources, parses each source,
loads flattened module graphs and analyzes each root. Symbols and occurrences own
values; no temporary AST/HIR pointers may escape. invalidatedFiles estimates roots.

1. Observe unchanged-save regression failing against existing implementation.
2. Test a one-worker, one-pending-input queue, immutable inputs, generation gate,
   stale suppression, cancellation, bounded shutdown and output serialization.
3. Run analysis privately; publish only under the same gate as protocol acceptance.
4. Cache discovery by explicit epoch; invalidate for folders, configuration,
   manifest/project and watched create/delete, not normal source edits.
5. Instrument real parse/semantic work including repeated graph passes. No
   dependency-aware reuse (reserved for FINAL-WP01B).
6. Pace existing protocol fixtures as interactive clients and preserve assertions.
7. Run focused and repeated tests, affected regressions, full Windows Debug/Release
   LLVM-on/off compiler/LSP/native CTest matrices and focused sanitizer.
8. Format and review complete diff, record exact results and limitations, commit
   prescribed subject, push branch, prove SHA equality and provide handoff only.

Build configurations run sequentially with existing dependencies reused. Observe
AGENTS.md 20 GiB operation bound; never reset/discard or stage unrelated paths.

## Implementation ownership and telemetry

The queue owns one running and one replaceable pending job. Protocol mutation,
generation acceptance, and snapshot/diagnostic publication share a gate. Every
job captures document text/version, configuration and discovery epoch by value.
Discovery inventories and dependency roots are immutable shared snapshots; only
the analysis worker owns the cache pointer/counters. Temporary compiler AST/HIR
objects remain local to the job. No dependency-aware reuse is introduced.

Compiler checkpoints are opt-in thread-local scopes. Cancellation unwinds lexer,
parser, module loader, source discovery and HIR state normally. Shutdown rejects
publication, cancels queued/running work and waits at most 2 seconds for the
worker. Cooperative work joins; noncooperative OS/foreign work uses an explicitly
reported bounded fallback with shared state, disabled publication and a nonzero
server exit. No compiler owners are destroyed beneath a running job.

A dedicated output writer serializes complete Content-Length frames and log
records outside the analysis/acceptance gate. Enqueue never waits for transport
capacity: exceeding 64 MiB or 1024 queued records fails output and cancels analysis.
The borrowed stream writer is joined; a caller-supplied blocking stream can delay
final transport drain. The two-second deadline covers analysis work, not arbitrary
streambuf I/O or an already executing synchronous request handler. Pending semantic
requests are bounded at 256 and remain in a session-owned registry until execution.
Callbacks release the gate between requests. Cancel responds immediately; newer
source state returns ContentModified for obsolete waiting requests.

Telemetry definitions:
- generation: the published snapshot; requestedGeneration: newest accepted input.
- files/bytes: unique sources admitted to the snapshot's configured bounds.
- reparsedFiles: actual Parser::parseModule invocations, including repeated loader
  parsing across roots; invalidatedFiles: distinct normalized paths reparsed.
- semanticallyAnalyzedFiles: loaded module contributions to actual flattened HIR
  passes, with repeated roots/imports counted for each pass. This is not a claim
  of per-module incremental semantic reuse.
- elapsedMilliseconds: complete analysis-job wall time, before publication.
- discoveryCacheHits/Misses: cumulative worker lookups/attempts as of publication.
- started/completed/stale/cancelled/coalesced/failedGenerations: live queue counters.
  Stale and cancelled may overlap; coalesced counts jobs replaced before starting.

Defined discovery invalidations: initialization/new session; workspace-folder
events; manifest/lock open/change/save; watched Rocket create/delete or
manifest/lock modification; explicit rename; configuration change (including
toolchain configuration). Normal Rocket open/change/close and watched source
modification reuse inventory. Toolchain executables/stdlib are process-scoped;
a client changing SDK restarts the server. Unchanged Rocket saves, with or without
text, do not schedule work. Manifest saves reload persisted project metadata.

## Verification host and scope

Windows 11 Pro 10.0.26200 x64; Intel Core i7-13620H (10 cores, 16 threads);
16,857,817,088 bytes RAM. Pinned LLVM 22.1.6 and Ninja 1.13.1; MSVC 14.51.36231.
Dedicated configurations run sequentially, with two compilation jobs and one
CTest job. Existing measured Debug/stage0 Debug build sizes (4.43/1.11 GiB) and
reused dependencies keep the planned operation below the AGENTS.md 20 GiB bound.
Free disk before final matrix: 102,596,079,616 bytes.

2026-09-22 live refresh: master/local tracking/live remain the approved baseline.
Eddie's remote advanced independently to fb5b46f9c98d1c2e621cdc68b92196706c9ffeac;
his local branch/worktree remains 2a03b7358399d70efac5a9bb056851c0ff3521d4 (behind 2).
No Eddie commits were incorporated. Rocket master, Eddie's worktree and RocketIDE
were clean on the final scope check. No RocketIDE files were modified.

## Evidence

- Original LSP CTest baseline 1/1 passed.
- Test-first red: unchanged save; synchronous queue responsiveness/shutdown;
  absent work counters; discovery invalidation; unanswered failed/cancelled
  semantic requests; newly persisted manifest save.
- Final focused targets: 3/3 passed. Final affected compiler/LSP/native targets:
  12/12 passed, 0 failed/skipped (affected-final.xml).
- Final queue and protocol concurrency targets: 100 repetitions each, 200 passed,
  0 failures/timeouts (repeat-final.log); each protocol run includes 256-request
  edit/shutdown races, malformed-frame interleaving and blocked-output handling.
- Review regressions: a blocked output stream previously prevented edit acceptance
  and analysis cancellation (red observed); opaque callbacks lost pending request
  completions at edit/shutdown (original fault restored temporarily, red observed,
  corrected source restored in finally). Both corrected fixtures pass.
- Full diff reviewed for scope, races, generation ordering, AST/HIR ownership,
  fallback lifetime and output serialization. Two findings above fixed; final
  independent review has no remaining must-fix findings.
- Changed tracked C++ lines and all new C++ files pass pinned clang-format;
  git diff --check passes. No broad reformat or unrelated modifications.
- Initial affected matrix: 12/13 passed; package failed four assertions due to
  long working paths, then the exact executable passed from a short path.
- MANUAL SMOKE DEFERRED. Follow-up attempt on 2026-09-22 used the Computer Use
  skill and @oai/sky: launch_app of RocketIDE/artifacts/verify-win-x64/RocketIDE.exe,
  select the returned window, get_window_state, then Tools > Validate Rocket
  Environment. The window was visible and restored the existing Rocket workspace
  and test.rocket. Status: "Rocket SDK: not found", "LSP: offline". Validation
  displayed "Rocket environment validation found problems"; Output reported
  "Problem: rocketc.exe was not found" and "Problem: rocket-lsp.exe was not found".
  Thus the GUI bridge works, but the published IDE's current SDK discovery is not
  configured for a usable toolchain. No source defect was established by this
  environment failure. RocketIDE settings/source were not changed in this
  Rocket-only checkpoint. Check/Build/Run/Test/Stop, real-LSP navigation/refactoring/
  formatting, search/replace with dirty buffers, recovery/external change and the
  full native-debugger sequence remain unverified. Repeat them at the earliest
  manual checkpoint after configuring the real SDK. The attempt occurred during
  final verification; it does not retroactively satisfy a before-feature smoke
  sequence and is not counted as GUI acceptance.

Final Windows x64 matrix (2026-09-22/23), all five builds successful:

| Configuration | Passed | Failed | Skipped | Total |
| --- | ---: | ---: | ---: | ---: |
| LLVM Release | 289 | 1 | 0 | 290 |
| LLVM Debug | 289 | 1 | 0 | 290 |
| Stage0 Release (LLVM off) | 210 | 1 | 0 | 211 |
| Stage0 Debug (LLVM off) | 210 | 1 | 0 | 211 |
| AddressSanitizer focused LSP | 3 | 0 | 0 | 3 |

The four full suites total 998 passed, 4 failed, 0 skipped out of 1002 test
executions. Each failure is the same unchanged baseline compatibility assertion
below. All four include passing language_server, analysis_queue,
language_server_analysis and phase17_editor_neutral_lsp. The sanitizer run used
ROCKETC_ENABLE_SANITIZERS=ON and MSVC /fsanitize=address. No sanitizer finding was
reported. No ThreadSanitizer or non-Windows platform run is claimed.

Raw configure/build logs, CTest logs and JUnit files are under
out/final-wp01a/matrix/evidence (also accessible at Rocket/out/wp01a/evidence).
Completed evidence was retained across interruptions. The final Release and
stage0 Debug suites were restarted only after their previous process handles
were confirmed missing; the complete results above supersede partial logs.

Reproduction uses dependencies/activate.ps1 followed by these dedicated presets:
windows-release, windows-debug, phase19-windows-x64-stage0-release,
phase19-windows-x64-stage0-debug, and windows-asan, each with -B pointing to its
own directory under Rocket/out/wp01a. Build with cmake --build <directory> -j 2;
run full configurations with ctest --test-dir <directory> -j 1 --output-on-failure.
The ASAN build/test selects only rocketc_language_server_tests,
rocketc_analysis_queue_tests and rocketc_language_server_analysis_tests.

Supplemental diagnostic: 11/11 compatibility cases passed for each of the four
full configurations (44/44). The ignored runner
out/final-wp01a/compatibility-diagnostic.py imports the original workflow and
changes only its compiler-version expectation to the actual 3.0.0; it asserts
the old expectation was exactly 2.1.0. All remaining cases execute unchanged.
Logs are compatibility-diagnostic-<configuration>.log. This additional evidence
does not turn the original CTest failures into passes or change tracked tests.


Known baseline gate: tests/portable_workflow_test.py:130 expects rocketc 2.1.0,
while CMakeLists.txt at the required base declares 3.0.0. The actual compiler
reports 3.0.0. This assertion is unchanged and must not be reported as passing.
No compatibility expectation was weakened for WP01A. Exact reproduction from
this worktree (after sourcing dependencies/activate.ps1):

```powershell
ctest --test-dir C:/Users/Administrator/Desktop/Projects/Rocket/out/wp01a/release -R '^phase20_compatibility$' --output-on-failure
```

The same named test fails in all four full configurations. Evidence: full-release.log/full-release.xml
and the corresponding full-debug, full-stage0-release and full-stage0-debug log/XML pairs in the evidence directory. The logged failure
is "compiler-version did not match '^rocketc 2\\.1\\.0$'" followed by "rocketc 3.0.0".
Baseline inspection used git show 10f295dd000b93fe50d254be21fedffd17892aeb:tests/portable_workflow_test.py
and the same commit's CMakeLists.txt; the mismatch predates this checkpoint.

The WP01B prompt is docs/FINAL-WP01B-HANDOFF.md; it has not been executed.
