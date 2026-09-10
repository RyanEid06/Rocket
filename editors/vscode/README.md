# Rocket for Visual Studio Code

This extension folder provides `.rocket` syntax highlighting, indentation and
bracket behavior, snippets, the `$rocket` problem matcher, and Phase 17 live
compiler diagnostics through `rocket-lsp`.

For local development, copy or link this directory into the VS Code extensions
directory as `rocket-lang.rocket-language-1.7.0`, then reload VS Code. Set
`rocket.languageServer.path` to the built `rocket-lsp.exe` when it is not on
`PATH`. The
repository tasks in `.vscode/tasks.json` use the Debug compiler build.

Protocol 1.0 provides incremental multi-package analysis with unsaved overlays,
stable coded diagnostics, completion and automatic imports, hover and signature
help, cross-file definition/references/rename, semantic tokens, and safe code
actions. The extension is only a client: the same server works with any LSP
3.17 editor, and opening source never runs builds or package code.

The shared server also describes the accepted Rocket 3 Wave B named/default
argument and labeled-enum metadata, including the bundled graphics/UI
signatures. Wave C does not fork the editor protocol. Use the current dictionary
and status ledger in `docs/ROCKET_3_0_SYNTAX_DICTIONARY.md` and
`docs/DOCUMENTATION_STATUS.md` rather than the historical 1.7 label above.
