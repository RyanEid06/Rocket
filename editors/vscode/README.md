# Rocket for Visual Studio Code

This extension folder provides `.rocket` syntax highlighting, indentation and
bracket behavior, snippets, the `$rocket` problem matcher, and Phase 17 live
compiler diagnostics through `rocket-lsp`.

For local development, copy or link this directory into the VS Code extensions
directory as `rocket-lang.rocket-language-1.7.0`, then reload VS Code. Set
`rocket.languageServer.path` to the built `rocket-lsp.exe` when it is not on
`PATH`. The
repository tasks in `.vscode/tasks.json` use the Debug compiler build.

Protocol 0.1 provides live lexical, syntax, and self-contained-file semantic
diagnostics. Completion, hover, rename, cross-file navigation, and incremental
multi-package analysis remain later Phase 17 work.
