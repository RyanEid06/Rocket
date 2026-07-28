# Rocket for Visual Studio Code

This extension folder provides `.rocket` syntax highlighting, indentation and
bracket behavior, snippets, and the `$rocket` problem matcher for coded
compiler diagnostics.

For local development, copy or link this directory into the VS Code extensions
directory as `rocket-lang.rocket-language-1.0.0`, then reload VS Code. The
repository tasks in `.vscode/tasks.json` use the Debug compiler build.

The extension deliberately contains no language server. Completion, rename,
and cross-file navigation remain post-self-hosting work rather than pretending
that TextMate highlighting provides semantic IDE support.
