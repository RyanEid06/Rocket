# Rocket Tooling and Packages 1.3

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

[build]
kind = "executable"
name = "hello"
```

Names start with a letter or underscore. Entry and test paths are relative and
cannot escape the package root. Package builds keep generated objects and
executables in `<package>/.rocketc`. Imports resolve from the package root, so a
test may use `import src.math` even though its file lives under `tests/`.

Standalone `.rocket` files remain supported. When no input is supplied,
package-aware commands use the current directory.

## Native inputs and library products

`[build].kind` is `executable`, `static-library`, or `dynamic-library`.
`[build].name` is the artifact basename. For backward compatibility, an
executable package without `[build]` still writes `.rocketc/main.exe`; a library
without an explicit name uses the package name. Library builds emit `.lib` or
`.dll` plus a deterministic `.h` header and do not synthesize `main`.

Windows x64 native inputs are explicit and target-scoped:

```toml
[native.windows-x64]
libraries = "vendor_math.lib;vendor_handles.lib"
library-search = "native/lib"
headers = "native/vendor.h"
```

Lists use semicolons inside one quoted value. Search and header paths are
package-relative, containment-checked, and deterministic. Named libraries are
resolved in listed search directories, then the package root, and passed to the
native linker in manifest order. Headers are validated inputs; Rocket never
implicitly parses C during a normal build.

Generate an importable low-level binding module and a C consumer header with:

```powershell
rocketc bind native/vendor.h --output native_bindings.rocket
rocketc emit-header . --output .rocketc/my_library.h
```

`bind` intentionally accepts only the frozen C subset: `int64_t`, `double`,
`uint8_t`, `rocket_bool`, `void`, pointers, one-line named struct typedefs,
opaque struct typedefs, function-pointer typedefs, integer `#define` constants,
and ordinary or `ROCKET_API` function prototypes. Output declarations are
`pub extern` so a safe wrapper module can import them. Preprocessor conditionals,
unions, bitfields, arrays, variadics, calling-convention attributes, C++ APIs,
and macro expressions are rejected or ignored rather than guessed.

Keep generated modules low-level and put policy in a handwritten wrapper. The
wrapper owns the smallest possible unsafe region and translates documented C
status values into Rocket's checked values:

```rocket
import native_bindings

pub fn validate(value: Int) -> Result[Int, String]:
    unsafe:
        if native_bindings.vendor_status(value) == 0:
            return Ok(value)
    return Err("vendor rejected the value")
```

An owned handle follows the same rule: acquire it inside the wrapper, release it
exactly once with the documented C destructor, never use it afterward, and do
not rely on Rocket ARC to manage it. A wrapper must not let a synchronous native
callback or pointer escape beyond the lifetime promised by the C API.

Static Rocket libraries do not embed `rocket_runtime.lib`; a native consumer
must link the matching runtime when an exported function reaches runtime
services. Dynamic Rocket libraries embed the runtime and produce an import
library beside the DLL. Native library packages cannot be passed to `run`.

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
indentation structure. The compiler accepts both LF and CRLF input; repository
attributes keep Rocket sources and golden diagnostics at canonical LF on every
host.

## Other commands

`check`, `build`, `run`, `emit-ir`, `emit-asm`, and `emit-header` accept either a standalone
source, a package directory, or a `rocket.toml` path. Program arguments follow
`--`. `rocketc --version` prints the compiler version.

In an LLVM-disabled stage0 build, generated C++ is compiled by the exact C++
compiler recorded during CMake configuration. On the supported Windows workflow
this is the activated MSVC compiler; stage0 does not require a separate `g++`
installation.

## VS Code

`editors/vscode` contains the Rocket TextMate grammar, language configuration,
snippets, and `$rocket` diagnostic matcher. Copy or link it into the VS Code
extensions directory as `rocket-lang.rocket-language-1.0.0`, reload the editor,
and use the tasks in `.vscode/tasks.json` for check, run, test, and format check.
This is honest syntax/editor support; semantic language-server features remain
future work.

## Visual Studio 2026

The supported Windows IDE is the purple Visual Studio Community 2026. Build and
install the repository-owned language extension once:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\package-visualstudio-extension.ps1
```

Open `out/visualstudio/Rocket.Language.VisualStudio.vsix`, then start the IDE
with Rocket's pinned build environment:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\open-visualstudio.ps1
```

The extension recognizes `.rocket` files and supplies syntax coloring,
comments, brackets, and indentation. Visual Studio loads the repository's CMake
project; its CMake Targets View exposes `rocket_demo_check`,
`rocket_demo_run`, and `rocket_demo_test`. These run the actual compiler against
`examples/visualstudio_demo`.

Semantic IntelliSense, go-to-definition, rename, references, live Rocket
diagnostics, and Rocket-aware debugging require the language-server and debugger
work planned for Phase 17.

## Compiler packaging

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\package-compiler.ps1 -Configuration Release
```

The script runs the Release test matrix and deterministic bootstrap, packages
the Rocket-written stage3 compiler, and creates a checksummed archive under
`out/package`. `bin/rocketc.exe` locates the bundled runtime, Clang/LLD,
compiler-rt resources, and static MSVC/UCRT/Windows SDK libraries relative to
its executable. The verification step clears development toolchain variables,
changes to an isolated working directory, then checks, builds, runs, and
directly executes a native Rocket program.

The preserved C++ bootstrap compiler is distributed separately as
`stage0/rocketc-stage0.exe`; it is not the production `rocketc` command.
