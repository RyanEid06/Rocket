# Rocket 3.5 canonical game runtime foundation

Rocket 3.5 has one Raylib backend. Its source and ABI header live under
`src/raylib/`; the repository build produces `rocket_raylib_adapter` and the
pinned Raylib 6.0 static library from that production location. Examples do not
own, copy, or independently compile the adapter.

Application code imports `rocket.raylib.safe` and the higher-level
`rocket.graphics` modules. `rocket.raylib.native` is the generated internal ABI
surface and is not an application API. The safe layer owns the public
`Window`, `Frame`, render-target, scissor, and font tokens and returns
`Result[..., String]` for fallible operations. Frames and scopes are
single-use; stale, double-ended, and incorrectly nested operations fail
deterministically.

## Native package contract

A game package declares the production libraries for its target in
`rocket.toml`. For example, Windows x64 uses:

```toml
[native.windows-x64]
libraries = "rocket_raylib_adapter.lib;raylib.lib;opengl32.lib;winmm.lib;gdi32.lib;user32.lib;shell32.lib;ole32.lib"
```

Linux x64 and ARM64 use
`librocket_raylib_adapter.a;libraylib.a;GL;m;pthread;dl;rt;X11`; macOS ARM64
uses the same static-library names plus the Cocoa, IOKit, CoreVideo, and OpenGL
frameworks. A released SDK installs the adapter and Raylib library under
`lib/`, the ABI header under `include/rocket/raylib/`, and the Raylib license
under `licenses/`. Both Rocket compiler implementations search the installed
SDK `lib/` directory, so a relocated SDK does not need the repository build
tree or an example-local native manifest.

Package-relative asset paths remain an application/package concern. The native
runtime never assumes that the process working directory is the repository.
WP5's bundled `rocket.assets` resolves relative paths against an explicit
package root; see `ROCKET_3_5_ASSETS.md`.

## Compatibility policy

`examples/raylib_showcase/src/rocket_raylib.rocket` remains temporarily as a
deprecated Rocket 1.4 compatibility wrapper. It imports the production
`rocket.raylib.native` module and exists only to keep previously copied example
code testable while its texture/display, effects, and audio features move to
the canonical safe surface in WP2-WP4. It is not bundled as a standard-library
module, is not copied into the released SDK, and must not be imported by new
supported examples. Its separate generated adapter binding and native C++ copy
have been removed.

## Verification

The WP1 policy test rejects example-owned adapter sources, independent example
adapter builds, and imports from the showcase source tree in production or
supported sample code. Existing native lifetime suites continue to prove
window/frame/scope cleanup and deterministic stale-handle errors. Release
packaging verification builds and runs a stdlib-only game package from a
sanitized relocated SDK, then exercises window, frame, drawing, resize, close,
and duplicate-close/end behavior through its headless test harness.
