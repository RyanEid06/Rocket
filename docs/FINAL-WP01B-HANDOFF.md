# FINAL-WP01B handoff prompt (not executed)

Execute only RocketIDE Final Roadmap checkpoint FINAL-WP01B — Dependency-aware
reuse and protocol completion. This is a prepared prompt; FINAL-WP01A does not
authorize starting it.

Read Rocket AGENTS.md and the complete canonical roadmap first:
C:/Users/Administrator/Desktop/Projects/RocketIDE/docs/ROCKETIDE_FINAL_ROADMAP_IMPLEMENTATION.md.
Work only in Rocket. Do not modify RocketIDE or incorporate Eddie's Rocket 3.5
work. Preserve unrelated changes; never reset/discard or use git add -A.

Use codex/rocketide-final-wp01-lsp, starting at its published FINAL-WP01A commit
with subject "add responsive LSP analysis scheduling". Fetch/prune and verify
local HEAD, upstream and live remote equal the SHA in the FINAL-WP01A delivery.
Stop on an unexpected mismatch. Record master and Eddie refs without changing
them. Use the dedicated out/w/lsp worktree and a separate WP01B build directory.
The old out/worktrees/rocketide-final-wp01-lsp path is a junction. Source
dependencies/activate.ps1 before direct CMake/build/CTest. Respect the AGENTS.md
20 GiB operation bound and do not share master/Eddie build outputs.

Read docs/FINAL-WP01A.md for exact evidence, limitations and ownership contracts.
Do not claim its known baseline/environment failures passed. RocketIDE's accepted
350/350 automated baseline is distinct from deferred GUI/manual acceptance.

Implement FINAL-WP01B test-first:
- Extract focused workspace-state and dependency-graph components.
- Maintain normalized module identity, source version/hash, tokens/AST, imports,
  reverse dependencies, exported interface fingerprint, semantic results and
  diagnostics behind explicit compiler ownership boundaries.
- Reparse changed modules, invalidate the required reverse-dependency closure,
  reuse unrelated work, and retain a safe full-analysis fallback.
- Compare incremental and clean-full diagnostics/symbols for every fixture.
- Audit/test workspace/symbol and rocket/projectStatus; implement authoritative
  documentSymbol and foldingRange if absent using compiler/parser structures.
- Preserve generations, coalescing, stale suppression, cancellation, request
  registry ownership, output serialization and unchanged-save no-op.

Cover one file, ordinary packages, many unrelated modules, deep chains, fan-in/
fan-out, bursts, private body/public interface changes, manifest/import edits and
a source comparable to compiler/src/main.rocket. Record reference hardware,
cold/warm timings, SHAs and raw counters. Target typical feedback below 250 ms
and a large-workspace leaf edit with a small affected closure below one second.
Prove unrelated modules are not reparsed/semantically analyzed and results equal
clean full analysis. Run real protocol tests for symbols/folding, repeat
concurrency checks and run the complete relevant compiler/LSP/native matrix.

Format and review all changes for scope, races and AST/HIR lifetimes. Record exact
pass/fail/skipped counts and blockers. Stage explicit paths. Commit with exactly:
implement dependency-aware LSP analysis

Push the branch and prove local/upstream/live SHA equality and 0/0 before reporting
completion. Follow the roadmap integration gate only after full review and
verification. Do not start RocketIDE FINAL-WP02 or Eddie's lane.
