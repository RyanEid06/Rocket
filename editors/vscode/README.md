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
