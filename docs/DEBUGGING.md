# Rocket Native Debugging

Rocket 1.7 emits native CodeView records and a PDB for every LLVM executable or
dynamic-library build. `--debug` selects unoptimized lowering; the default
retains optimized debug locations. The adjacent `*.rocket.map.json` file uses
schema `rocket-source-map-1` and records the optimization mode, native Rocket
symbol, declaration, and executable source locations.

The workflow is editor neutral:

1. build with `rocketc build <path> --debug` or omit `--debug` to inspect an
   optimized build;
2. give any Windows x64 CodeView debugger the `.exe` and adjacent `.pdb`;
3. map the logical `rocket:\source\<file>.rocket` record to the workspace file
   using the sidecar's `source` field;
4. set a source breakpoint or a symbol breakpoint such as
   `rocket_fn_<qualified-name>_<symbol-id>`, launch, and use the debugger's
   ordinary threads, stack, scopes, and variables requests.

Rocket locals are emitted as CodeView local/constant records and user functions
as `S_GPROC32`. MIR instruction and terminator locations produce line-table
entries, so runtime panic helpers inherit the precise call-site location. The
sidecar symbol table is also the stable symbolization input for sampling
profiles. Optimized variables can be constant-folded or unavailable, as in
other native optimized languages; their source line and enclosing Rocket frame
remain present.

Absolute checkout directories are intentionally excluded from Rocket DI files
(`rocket:\source` is used) so debug information does not invalidate reproducible
native artifacts. The sidecar performs workspace resolution instead.

Run `scripts/debugging.ps1`. It builds the same multi-function fixture both
optimized and unoptimized, checks PDB source/file/line/function/variable
records with pinned `llvm-pdbutil`, verifies both sidecar modes, validates a
panic source location, and writes `out/debugging/report.json` with hashes.

Current platform boundary: CodeView/PDB and the validation workflow target
Windows x64. Rocket does not ship or lock users into an editor-specific debug
adapter; Visual Studio, WinDbg, and other CodeView-capable DAP front ends can
consume the same executable contract.
