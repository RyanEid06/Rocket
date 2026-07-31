# Incremental-AOT REPL Evaluation

Phase 17 evaluates, but does not promote to a language guarantee, an
incremental-AOT REPL in `scripts/repl-prototype.ps1`. Each accepted expression
is accumulated in a generated `main`, compiled with the normal Rocket frontend
and AOT backend, then run as a fresh process. Generated source and native
artifacts stay under `out/repl-prototype`.

This model gives ordinary Rocket diagnostics, dependency/toolchain behavior,
and deterministic native execution. Expression state lasts for the interactive
session because prior expressions are replayed. `:reset` clears it and `:quit`
ends it. A command-line `-Expression` list provides a noninteractive acceptance
path; `-Measure` emits `rocket-repl-evaluation-1` JSON with end-to-end latency.

The experiment rejected a JIT for Rocket 1.7. Startup includes parse, type,
MIR, object, link, and process launch (roughly one second on the Phase 17 Windows
reference run), and replay means side effects run again. Definitions,
redefinition, durable mutable values, and native-resource lifetime are not safe
enough for a supported REPL yet. Locked package dependencies can be used only
by extending the generated session into a package. These limits are explicit
rather than hidden behind an incompatible interpreter or second type system.

The next viable experiment is declaration-cell caching with one generated
module per cell and an explicit state ABI. A JIT remains optional until that
design demonstrates materially lower latency without weakening ownership,
bootstrap reproducibility, or diagnostic equivalence.
