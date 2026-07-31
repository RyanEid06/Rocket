# Rocket Language Server Protocol 1.0

`rocket-lsp` is Rocket 1.7's standalone, editor-neutral LSP 3.17 server. It
uses `Content-Length` framing over standard input/output; logs go only to
standard error. `rocket-lsp --version` prints `rocket-lsp 1.0.0`.

## Safety and bounds

Opening a source file performs lexical, parser, semantic, and project-graph
analysis only. It never runs a build, package script, native program, registry
request, or other source-controlled command. Locked dependencies are opened as
inert source in offline mode.

- protocol message: 16 MiB; header: 16 KiB;
- open document: 4 MiB;
- project defaults: 4,096 files and 64 MiB of source;
- one workspace edit: 1,024 edits;
- malformed frames, invalid JSON-RPC, stale versions, invalid UTF-16 ranges,
  oversized content, and requests before `initialize` receive bounded errors.

Each client runs an independent process. Requests within a process are handled
in arrival order. `$/cancelRequest` suppresses queued work and returns LSP
`RequestCancelled` (`-32800`); analysis generations suppress results computed
for stale document versions. The server reports elapsed milliseconds, analyzed
bytes/files, generation, and invalidation counts without source text through
`rocket/analysisStatus`. `rocket/projectStatus` returns the current bounds and
index size on request.

## Project model

Protocol 1.0 reuses `ModuleLoader`, the package manifest/lock resolver, the
compiler lexer/parser, HIR symbols, and Rocket types. It does not maintain a
second type system. A workspace graph contains the root package, exact locked
dependencies, standard modules, and rootless open files. Open documents are
path-keyed in-memory overlays and therefore win over disk without being saved.
Changes invalidate the affected graph generation; only bounded workspace roots
are scanned. Incomplete files retain recoverable lexical/AST symbols so editor
features remain available without manufacturing successful type results.

## Lifecycle and synchronization

The lifecycle is `initialize`, optional `initialized`, ordinary requests and
notifications, `shutdown`, then `exit`. The server negotiates UTF-16 positions
and incremental synchronization (`textDocumentSync.change = 2`). Full-content
changes remain accepted as the bounded compatibility baseline. Versions must
increase. `workspace/didChangeConfiguration` accepts:

- `rocket.maximumProjectFiles` (1..4096),
- `rocket.maximumProjectBytes` (1 MiB..64 MiB),
- `rocket.telemetry` (boolean).

Workspace-folder changes rebuild the bounded graph. Closing a document removes
its overlay and publishes an empty diagnostics array.

## Capabilities

- deterministic semantic completion, qualified completion, and automatic
  import edits for a unique visible public declaration;
- Markdown hover with Rocket type/signature, documentation, and a
  `rocket-doc://` versioned-documentation link;
- signature help with active parameter and available compiler signatures;
- resolved definition, references, prepare-rename, and conflict-checked
  workspace rename;
- semantic token full/delta responses for keywords, values, declarations,
  parameters, properties, types, traits/interfaces, functions/methods, native
  declarations, strings, and numbers;
- stable quick fixes for `R4002` missing names plus whole-document formatter
  actions. Imports are emitted only when resolution is unique, and repeated
  action requests are byte-stable; once applied, the import action disappears;
- workspace symbols and compiler diagnostics retaining their stable `Rdddd`
  codes.

Rename refuses keywords, standard/native declarations, locked dependency
sources, invalid identifiers, and conflicts. Edits are limited to writable
workspace/open files. Textual matches that did not resolve to the selected HIR
symbol are not edited.

## Client neutrality and validation

`tests/language_server_tests.cpp` is an editor-independent in-process LSP
client. It covers lifecycle, framing, malformed/oversized messages, UTF-16,
incremental and stale changes, overlays, incomplete code, navigation, rename,
completion, imports, hover, signatures, tokens, actions, cancellation, telemetry,
and latency. This is the required non-VS-Code client. The dependency-free VS
Code extension is a separate consumer and its transport/provider tests live in
`editors/vscode/test/client.test.js`.

Protocol 1.0 deliberately remains LSP rather than a Rocket-specific editor
protocol. New fields and custom `rocket/*` methods must be additive and bounded;
breaking behavior requires a new protocol version.
