# Rocket Native Debugging

Rocket emits target-native source information for every LLVM executable or
dynamic-library build. Windows x64 emits CodeView/PDB; Linux x64 and Linux
ARM64 emit ELF/DWARF; macOS ARM64 emits Mach-O/DWARF. `--debug` selects
unoptimized lowering; the default retains optimized debug locations. The
adjacent `*.rocket.map.json` file uses schema `rocket-source-map-1` and records
the optimization mode, native Rocket symbol, declaration, and executable source
locations.

The workflow is editor neutral:

1. build with `rocketc build <path> --debug` or omit `--debug` to inspect an
   optimized build;
2. give the selected target's native debugger the executable and its adjacent
   CodeView/PDB or DWARF information;
3. map the logical `rocket:\source\<file>.rocket` record to the workspace file
   using the sidecar's `source` field;
4. set a source breakpoint or a symbol breakpoint such as
   `rocket_fn_<qualified-name>_<symbol-id>`, launch, and use the debugger's
   ordinary threads, stack, scopes, and variables requests.

Rocket locals and functions are emitted in the selected native debug format.
On Windows this is CodeView local/constant data and `S_GPROC32`; on DWARF
targets it is standard DWARF function, line, and local information. MIR
instruction and terminator locations produce line-table entries, so runtime
panic helpers inherit the precise call-site location. The sidecar symbol table
is also the stable symbolization input for sampling profiles. Optimized
variables can be constant-folded or unavailable, as in other native optimized
languages; their source line and enclosing Rocket frame remain present.

Absolute checkout directories are intentionally excluded from Rocket DI files
(`rocket:\source` is used) so debug information does not invalidate reproducible
native artifacts. The sidecar performs workspace resolution instead.

## Visual Studio Community 2026

The repository's version-2 VSIX connects this editor-neutral contract to Visual
Studio's native Windows debugger. **Debug Rocket Project** discovers the active
package (or standalone source), builds it with `--debug`, and refuses to launch
unless the expected `.exe`, adjacent `.pdb`, and adjacent
`.rocket.map.json` all exist. Every sidecar source is resolved and checked
before `IVsDebugger4` launches the native-only engine. The Rocket Output pane
shows the executable, PDB, sidecar, and each logical-to-workspace source mapping.

The extension creates the Rocket executable itself with `CREATE_SUSPENDED`,
`CREATE_NO_WINDOW`, an inherited environment, and redirected output/error
pipes. It then asks `IVsDebugger4` to attach the native-only engine to that
process ID before resuming the primary thread. The extension recognizes and
continues only the attach-generated `ntdll` break, leaving Rocket source
breakpoints stopped. Consequently the native engine never takes Visual Studio's
console/terminal launch path, and program output remains in the Rocket Output
pane. Existing Rocket source breakpoints bind
through the CodeView file/line records; ordinary Visual Studio stepping, Call
Stack, Threads, and Locals windows consume the native records described above.
Stop ends both the native-debug session and the hidden process. The extension
does not claim values that the PDB does not represent, and optimized builds can
still have unavailable or folded locals.

The no-terminal contract also means the VSIX debug workflow is non-interactive:
stdin is connected to `NUL`, while stdout and stderr are captured in Visual
Studio. Program arguments and application-level file or GUI input remain
available.

The frozen Rocket 2.0 DI contract stores `path.filename()` under
`rocket:\source`. Consequently, two compiled source files that have the same
basename cannot be mapped unambiguously even though the sidecar retains both
absolute sources. The VSIX detects this case and reports it before launch. No
compiler or language contract is changed to implement the IDE integration.

Run `scripts/debugging.ps1`. It builds the same multi-function fixture both
optimized and unoptimized, checks PDB source/file/line/function/variable
records with pinned `llvm-pdbutil`, verifies both sidecar modes, validates a
panic source location, and writes `out/debugging/report.json` with hashes.

The Visual Studio integration remains Windows x64-specific. On Linux and macOS,
the same source map accompanies standard DWARF artifacts and may be used by
native debugger front ends; Rocket does not mandate an editor-specific debug
adapter. Platform acceptance, including actual debug-information inspection,
is recorded in `PHASE_19_AUDIT.md` rather than inferred from this contract.
