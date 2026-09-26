# FINAL-WP01 acceptance: interface-aware LSP invalidation

## Root cause and repair

The original incremental worker cached compiler results by analysis root. It
parsed an edited dependency once but then reanalyzed every root in that
dependency's reverse closure, even when the exported interface was unchanged.
For a library and one consumer, a private body edit therefore reported three
semantic graph contributions. The exported interface record existed but did
not participate in semantic invalidation.

The worker now compares the exact old and new exported interface records.
It analyzes the edited root first and compares its compiler-derived symbols,
occurrences, and diagnostics with the previous result. If those are equal,
consumer roots remain cached. If a private edit changes only the edited
module's semantic contribution, the compiler lowers just that module's bodies
inside an unchanged declaration graph, and the worker replaces only that
source's contribution in affected cached roots. Equivalent prior contexts
share one new source projection across fan-out roots. Changed public
declarations, compiler errors, incomplete filtered lowering, and exported
generic or associated/native constant bodies use conservative full analysis.
The ordinary compiler path remains authoritative.

Parsed source owners are retained for current inventory files or live loaded
dependencies. The worker prunes them only when an inventory source disappears
or root/dependency ownership changes, avoiding a full cache sweep per edit.
Rootless watched changes conservatively invalidate discovery so unopened
imports are reloaded. The unused document-version cache was removed.
References and rename edits are ordered by source location so replacing a
per-source contribution does not change response ordering.

## Test coverage

`language_server_incremental` asserts exact semantic-work counters and
compares diagnostics, workspace symbols, and document symbols to a clean full
session for private and public body changes, public signatures, added and
removed exports, struct and enum shape changes, a four-module chain, fan-in,
fan-out, an unopened watched dependency, and a 239-file workspace. It also
compares references and rename edits. Long-session tests close 120 rootless
documents and eight more rootless documents with on-disk imports, then assert
no parsed owners remain. `workspace_state` checks interface stability,
500 parse/erase cycles, and unchanged versus changed dependency ownership.

The Release queue and language-server-analysis fixtures passed 100 consecutive
runs each. Together with the existing LSP protocol tests, they cover
coalescing, cancellation, stale publication, bounded shutdown, serialized
output, unchanged saves, and close behavior. The compiler regression suites
remain in the full gate.

## Release telemetry

The values after this change are from the repeatable
`rocketc_language_server_incremental_tests` Release probe. They are per
published warm generation, not whole-test wall time. The pre-fix two-file
private result was observed in the red acceptance run. The deep-chain and
239-file pre-fix contribution counts are derived from the old reverse-closure
algorithm, not separate timed runs. The earlier 239-file acceptance probe
edited an unrelated leaf and already observed one contribution; this probe
adds a consumer of the edited library to exercise interface invalidation.
The raw final responses are in the ignored build evidence at
`out/wp01b/matrix/evidence/wp01-completion-release-final.log`.

| Case | Before semantic contributions | After files | Reparsed | Invalidated | After semantic contributions | After ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Two-file private body | 3 | 2 | 1 | 1 | 1 | 0 |
| Two-file public signature | 3 | 2 | 1 | 1 | 3 | 1 |
| Four-module private chain | 10 (derived) | 4 | 1 | 1 | 1 | 1 |
| Four-module public chain | 10 (derived) | 4 | 1 | 1 | 10 | 1 |
| 239-file private body with consumer | 3 (derived) | 239 | 1 | 1 | 1 | 3 |

The same probe observed a private body that adds a local at two semantic
contributions, compared with one when only a literal changes. In an
eight-consumer fan-out, a private literal edit required one contribution,
a local edit two, and a public interface edit 17. Release warm private edits
meet the under-250-ms common-edit and under-one-second large-workspace targets
on this host. Debug builds do not enforce a timing threshold.

## Verification and remaining limit

The complete Windows Release gate reports 291 passed and one failed out of
292. The sole failure is the existing `phase20_compatibility` expectation of
`rocketc 2.1.0` while the compiler reports `3.0.0`. Compiler, LSP, package,
and application regressions pass. The final Debug queue, LSP analysis,
incremental LSP, and workspace-state fixtures pass 4/4. Changed C++ lines
pass pinned LLVM 22.1.6 `git-clang-format`, and `git diff --check` passes.
This compatibility version expectation still blocks declaring the
integration gate green.
