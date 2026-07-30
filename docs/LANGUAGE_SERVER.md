# Rocket Language Server Protocol 0.1

This document specifies the first Rocket 1.7 professional-tooling milestone.
It is a tooling contract and does not change Rocket source syntax, type
semantics, generated code, or runtime ABI v1.

## Process and transport

`rocket-lsp` is a standalone Windows x64 executable distributed beside
`rocketc`. It communicates over standard input and standard output using the
Language Server Protocol 3.17 base protocol and `Content-Length` framing.
Protocol logs and human-readable diagnostics must never be written to standard
output because that stream is reserved for framed protocol messages.

The initial server has protocol version `0.1` and supports:

- `initialize`, `initialized`, `shutdown`, and `exit`;
- `textDocument/didOpen`, `textDocument/didChange`,
  `textDocument/didSave`, and `textDocument/didClose`;
- full-document synchronization (`TextDocumentSyncKind.Full`);
- deterministic `textDocument/publishDiagnostics` notifications containing
  stable `Rdddd` codes from the ordinary Rocket frontend.

Unknown requests receive JSON-RPC `-32601`. Malformed envelopes and parameters
receive the applicable JSON-RPC error without terminating the server. A clean
`exit` after `shutdown` returns zero; an `exit` without `shutdown` returns one.
Input messages larger than 16 MiB are rejected before allocation.

## Document and diagnostic model

Open documents are keyed by their URI and retain the newest integer version.
Stale `didChange` notifications are ignored. A change must contain exactly one
full-document replacement; ranged incremental edits are not accepted in
protocol 0.1.

Each accepted open, change, or save runs Rocket lexing, parsing, and, when the
document is a self-contained module, semantic analysis. The analyzer treats an
editor document as a library unit so a partial module is not required to define
`main`. Diagnostics use one-based Rocket source locations internally and are
converted to zero-based LSP ranges. Columns and range lengths are measured in
UTF-16 code units as required by LSP. Until the compiler records precise end
locations, a diagnostic range covers the token at its start location or one
code unit when no token is available.

Protocol 0.1 deliberately limits live semantic analysis to self-contained
documents. Documents containing imports still receive lexical and parser
diagnostics, but multi-file semantic analysis waits for the incremental project
graph and in-memory overlay work. Closing a document publishes an empty
diagnostic set so clients remove stale problems.

## Compatibility and security

The server accepts only JSON-RPC 2.0 messages and bounded headers. URIs and
source text are treated as data; the language server does not execute builds,
programs, package scripts, or shell commands. The initial server never writes
source files and does not fetch dependencies or use the network.

The protocol is editor-neutral. VS Code integration may launch the same
executable, but no editor-specific extension API is part of this contract.

## Deferred Phase 17 work

Later compatible protocol versions will add incremental multi-package analysis,
hover and signature information, completion, definition/reference navigation,
rename, semantic tokens, code actions and imports, documentation generation,
native debugging, machine-readable test/build output, benchmarking, profiling,
coverage, and evaluation of an incremental-AOT REPL.
