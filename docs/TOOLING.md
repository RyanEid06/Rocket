# Rocket Tooling and Packages - Draft 0.9

## Create and use a package

```powershell
rocketc new hello
rocketc check hello
rocketc run hello
rocketc test hello
rocketc fmt hello --check
```

`rocketc new` creates the stable layout:

```text
hello/
  rocket.toml
  src/
    main.rocket
  tests/
    smoke_test.rocket
```

The manifest is intentionally small TOML:

```toml
[package]
name = "hello"
version = "0.1.0"
entry = "src/main.rocket"

[test]
directory = "tests"
```

Names start with a letter or underscore. Entry and test paths are relative and
cannot escape the package root. Package builds keep generated objects and
executables in `<package>/.rocketc`. Imports resolve from the package root, so a
test may use `import src.math` even though its file lives under `tests/`.

Standalone `.rocket` files remain supported. When no input is supplied,
package-aware commands use the current directory.

## Test runner

Every `.rocket` file under the manifest's test directory is a separate native
test program and must define `fn main() -> Int`. Exit 0 passes; any other exit,
compile failure, or runtime failure fails. Files are discovered recursively in
lexical path order and generated directories are ignored.

This convention keeps tests ordinary Rocket programs with no hidden exception
or reflection mechanism:

```rocket
import src.math

fn main() -> Int:
    if math.doubled(3) == 6:
        return 0
    return 1
```

The runner reports each path followed by `PASS` or `FAIL`, prints a final count,
and exits nonzero if any test failed. It uses the same LLVM or preserved Stage 0
backend as `run`.

## Formatter

`rocketc fmt <file-or-directory>` writes canonical source. `--check` makes no
changes and exits 1 if any file differs, which is suitable for CI. Formatting
uses four-space indentation, canonical token spacing, LF newlines, one final
newline, and preserves line comments and literal contents. It is idempotent and
refuses lexically invalid input rather than guessing around a broken literal or
indentation structure.

## Other commands

`check`, `build`, `run`, `emit-ir`, and `emit-asm` accept either a standalone
source, a package directory, or a `rocket.toml` path. Program arguments follow
`--`. `rocketc --version` prints the compiler version.

## VS Code

`editors/vscode` contains the Rocket TextMate grammar, language configuration,
snippets, and `$rocket` diagnostic matcher. Copy or link it into the VS Code
extensions directory as `rocket-lang.rocket-language-0.8.0`, reload the editor,
and use the tasks in `.vscode/tasks.json` for check, run, test, and format check.
This is honest syntax/editor support; semantic language-server features remain
future work.

## Compiler packaging

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\package-compiler.ps1 -Configuration Release
```

The script runs the canonical build/tests and creates a checksummed archive
under `out/package`. The compiler prefers a sibling `rocket_runtime.lib` and
the bundled `toolchain/clang.exe`/`lld-link.exe`, making the developer package relocatable. Before Rocket
1.0, native linking still requires an activated MSVC Build Tools/Windows SDK
environment; Phase 10 owns the fully self-contained distribution promise.
