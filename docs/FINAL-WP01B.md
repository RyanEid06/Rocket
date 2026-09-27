# FINAL-WP01B - Dependency-aware LSP analysis

## Entry and scope

Executed only FINAL-WP01B under the canonical RocketIDE Final Roadmap and
`docs/FINAL-WP01B-HANDOFF.md`. Rocket `AGENTS.md`, the complete roadmap, and the
WP01A handoff/evidence were read before implementation. After fetch/prune, the
dedicated `out/w/lsp` worktree, `codex/rocketide-final-wp01-lsp` branch, upstream,
and live remote all matched WP01A commit
`4c6ffacd4277b988d2aba73f8d5c658b50bb1d14` with 0 ahead/behind. Rocket
`master` and `origin/master` were
`10f295dd000b93fe50d254be21fedffd17892aeb`. Eddie's separate Rocket 3.5
local worktree was `2a03b7358399d70efac5a9bb056851c0ff3521d4`, while
his live remote was `fb5b46f9c98d1c2e621cdc68b92196706c9ffeac`; none of
that work was incorporated. RocketIDE main and live remote matched
`062b78fe2d12675b86fd44ccf3c0e24e98b81d01` and RocketIDE was read only.

WP01A's accepted RocketIDE automated baseline remains 350/350. GUI/manual
acceptance was deferred by the absent real Rocket SDK executables in that
installation; WP01B makes no GUI smoke claim.

## Compiler ownership and reuse

- `WorkspaceState` owns immutable, normalized parsed sources with source
  content/hash, editor version, tokens, AST, parser diagnostics, document
  structure, and a canonical exported interface record/hash. Source equality
  accompanies the hash. The interface record covers imports and public
  signatures, including generic parameters, constraints, native flags,
  callback shapes, enum payloads, and trait methods; private bodies are omitted.
- `DependencyGraph` records the actual compiler-loaded closure per analysis
  root and reverse edges. Changed sources invalidate affected roots, while
  unrelated root symbols, occurrences, and diagnostics are reused.
- The tooling-only cached loader receives a deep AST clone before module
  namespacing and semantic analysis. CLI loader overloads retain their original
  full-analysis path. Semantic/HIR owners are local to the worker; cached
  results retain values, not pointers into temporary compiler state.
- Discovery epoch changes, incomplete/limited inventories, and analysis
  cancellation clear semantic reuse. An incomplete inventory uses the ordinary
  compiler loader. Parser-error roots use the original parser recovery path.
- Parser-derived `documentSymbol` and indentation-token `foldingRange` are
  exposed through real LSP methods. `workspace/symbol` uses LSP SymbolKind;
  `rocket/projectStatus` counters reflect actual parse and semantic work.

## Test-first and review evidence

Red tests preceded implementation for one-source reparse/semantic reuse and
missing document symbols/folds. The large-source fixture exposed a valid bare
`return` HIR occurrence-indexing null dereference; a minimal regression was
added before fixing it. A large leaf-edit performance assertion exposed
repeated incomplete declaration work; per-source summary reuse fixed that
case. Manifest and burst fixtures compare the final incremental publication
with clean full analysis.

All fixtures compare incremental and clean-full diagnostics, workspace symbols,
and document symbols. Coverage includes one file, ordinary package, unrelated
modules, a deep chain, fan-in/fan-out, public interface, private body, import
and manifest changes, 48 unrelated modules, a rapid 30-edit burst, and the
581,431-byte `compiler/src/main.rocket` plus a leaf. Dependency tests assert
one reparse and only affected root graph contributions. Cancellation review
found that a partial worker cache could survive an interrupted generation; the
worker now clears it on any exception before the next generation.

Review found missing public type parameters in the first interface record.
`workspace_state` was added test-first: its public-type-parameter assertion
failed, then passed after the record was made complete. The record does not
drive selective invalidation; reverse dependency closure stays conservative.

The first full Debug run found a test harness limit in the new large-source
fixture: its short settle poll expired during cold Debug analysis and the
uncaught test exception produced Windows exit status `0xc0000409`. A bounded
180-second settle is used only for sources above 500,000 bytes; the subsequent
focused Debug fixture passed. The document-structure builder now indexes
source lines, tokens by line, and fold ends once per parse, rather than
rescanning them for each declaration. Final full matrices use those changes.

## Verification and limitations

Reference machine: Windows 11 Pro x64, Intel Core i7-13620H (10 cores, 16
threads), 16,857,817,088 bytes RAM, pinned LLVM 22.1.6 and Ninja 1.13.1;
hardware details carried from the WP01A evidence, with the CPU checked live.
Direct CMake, build, and CTest commands source `dependencies/activate.ps1`.
The dedicated short `Rocket/out/wp01b` path is a junction whose build files
are physically inside `out/w/lsp/out/wp01b/matrix`, matching the WP01A
workflow guard layout. Configurations run sequentially with two build jobs and
one CTest job. No planned operation approaches the 20 GiB repository limit.

The first Release run used a physical build directory outside the worktree;
five workflow tests rejected that location. After moving only the WP01B build
tree inside the worktree and preserving its short path as a junction, four
focused reruns passed. `phase20_compatibility` then showed the preexisting
version mismatch: its test expects `rocketc 2.1.0`, while this compiler reports
`rocketc 3.0.0`. Its failure is recorded, not treated as a pass.

Final matrix counts, repeated concurrency counts, timings, and raw counters
are recorded below. The branch synchronization proof is reported in the
handoff after the push.

### Release timing and counters

Run directly from the Release build with the pinned toolchain. Raw status
responses from the final test source are in
`out/wp01b/evidence/benchmark-release-final-source.log` (ignored build
evidence); times are the server's `elapsedMilliseconds` per published
generation, not wall-clock times for the entire protocol fixture. An earlier
idle run recorded 10,917 ms cold and 71 ms warm for the same large fixture;
the final run was slower, so the table uses the final run's values.

| Fixture | Sources/bytes | Cold or warm ms | Reparsed | Semantic graph contributions | Invalidated |
| --- | ---: | ---: | ---: | ---: | ---: |
| One file, warm edit | 1 / 32 | 0 | 1 | 1 | 1 |
| 48 unrelated modules, warm edit | 48 / 1,536 | 5 | 1 | 1 | 1 |
| Large source, cold | 1 / 581,431 | 39,393 | 1 | 1 | 1 |
| Large source plus leaf, warm leaf edit | 2 / 581,462 | 261 | 1 | 1 | 1 |

The warm normal edits are below 250 ms and the large-workspace leaf edit is
below one second on this host. Cold analysis of the large source is much
slower and is reported separately.

### Matrix results

The final full Windows Release suite built successfully and completed
291 passed, 1 failed, 0 skipped out of 292 in 258.81 seconds
(`out/wp01b/evidence/full-release-wp01b.log` and `.xml`). The sole failure is
`phase20_compatibility` with the preexisting 2.1.0 versus 3.0.0 version
expectation. The four focused LSP tests (including the new incremental test),
compiler front end, native, self hosted, and application validation tests all
passed in this configuration.

The final Windows Debug suite built successfully and completed 291 passed,
1 failed, 0 skipped out of 292 in 687.63 seconds
(`out/wp01b/evidence/full-debug-wp01b.log` and `.xml`). Its sole failure is the
same `phase20_compatibility` version mismatch. The new incremental fixture
passed in the complete Debug run.

The final Stage0 Release suite (LLVM off) built successfully and completed
212 passed, 1 failed, 0 skipped out of 213 in 456.00 seconds
(`out/wp01b/evidence/full-stage0-release-wp01b.log` and `.xml`). Its sole
failure is the same compatibility version mismatch. An earlier run lost its
process at test 135 and produced no final XML; only the completed rerun is
counted here.

The final Stage0 Debug suite (LLVM off) built successfully and completed
212 passed, 1 failed, 0 skipped out of 213 in 858.10 seconds
(`out/wp01b/evidence/full-stage0-debug-wp01b.log` and `.xml`). Its sole
failure is the same compatibility version mismatch. The large incremental
protocol fixture passed in 331.40 seconds and the interface record test
passed. Its earlier run failed a Release-only timing assertion at 1,340 ms;
the final fixture applies that performance target only in optimized builds.

The affected Release regression set passed 14/14 in 50.38 seconds
(`out/wp01b/evidence/affected-release-wp01b.log` and `.xml`). The analysis
queue and language server analysis tests each passed 100 consecutive
repetitions, 200/200 total, with no failures in 40.90 seconds
(`out/wp01b/evidence/concurrency-repeat-wp01b.log`).

The first five-target ASAN run passed four targets and stack-overflowed while
opening the 581,431-byte reference source in the incremental fixture. An
isolated `rocketc check compiler/src/main.rocket` using the ASAN compiler also
stack-overflowed in its original full-analysis path (`asan-plain-large-check.log`).
A targeted 32 MiB test stack removed the immediate overflow, but the large
source did not settle within 600 seconds; that run passed four targets and
failed the incremental target (`focused-asan-restack.log`). The final ASAN
incremental target retains the graph/edit equivalence fixtures but excludes
only this large-source case. The four complete non-ASAN matrices cover it.
The large-source ASAN blocker is not claimed to pass. The final five-target
focused ASAN suite passed 5/5 in 78.55 seconds
(`out/wp01b/evidence/focused-asan-final.log` and `.xml`).
