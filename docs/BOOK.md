# The Rocket Book

This compact book is the supported learning path for the completed Rocket 2.1
baseline and the accepted Rocket 3 Wave B surface. Wave C is ready but Rocket
3.0 is not a final release; the linked specifications remain normative when a
tutorial explanation is abbreviated. The current-surface index is
`ROCKET_3_0_SYNTAX_DICTIONARY.md` and the complete disposition of repository
documentation is in `DOCUMENTATION_STATUS.md`.

## 1. Build and run

Create a package with `rocketc new hello`, then use `rocketc check hello`,
`rocketc run hello`, `rocketc test hello`, and `rocketc fmt hello --check`.
A minimal program is:

```rocket
fn main() -> Int:
    let greeting = "Hello from Rocket"
    print(greeting)
    return 0
```

## 2. Values and control flow

Use inferred immutable `let`, explicit mutable `var`, indentation blocks,
`if`/`else`, `while`, integer ranges, and typed functions. Expected failure is
represented by `Option[T]` or `Result[T, E]`; postfix `?` propagates a matching
failure without exceptions.

## 3. Data and abstraction

`Array`, retained `Slice`, tuples, ordered maps/sets, structs, enums, exhaustive
`match`, generics, methods, traits, lambdas, and iterators provide the regular
application surface. Persistent values are the default; use controlled mutation
or `UniqueBuffer[T]` when an algorithm needs a unique mutable builder.

## 4. Modules and packages

Put source under `src/`, declare public APIs with `pub`, and import package
modules by dotted names. Commit `rocket.toml` and `rocket.lock`; never commit
`.rocketc`. Read `PACKAGE_AUTHOR_GUIDE.md` and `PACKAGES.md` before publishing.

## 5. Ownership and concurrency

ARC owns managed values deterministically. Break potential strong cycles with
`Weak[T]`. Rocket derives `Send` and `Share`; it does not accept programmer
assertions that an unsafe native value is thread-safe. `async fn` returns a
typed task, `await` consumes it, structured groups join children, and all public
queues/executors have explicit bounds. See `CONCURRENCY.md` and
`MIGRATION_1_8.md`.

## 6. Native applications

Keep raw native declarations in a generated low-level module, call them only in
small `unsafe:` regions, and expose safe `Result`-returning wrappers. Resource
tokens need exactly-one cleanup. Start with `FFI_GUIDE.md`; the raylib adapter
and Orbital Workshop example demonstrate a substantial wrapper and application.

## 7. Graphics and UI (Rocket 3 Wave B)

Use `rocket.graphics` for value-owned `Vec2`, `Rect`, `Color`, typography, and
`VirtualCanvas` data. `rocket.graphics.shapes`, `rocket.graphics.input`, and
`rocket.graphics.canvas` provide typed drawing, pointer mapping, clipping,
render-target, resize, and screenshot helpers through the safe
`rocket.raylib.safe` boundary. `rocket.ui` provides bounded immediate-mode
frames, stable widget IDs, focus, modal, disabled, and response state;
`rocket.ui.layout` provides pure Row, Column, Grid, Stack, and Anchor placement.
`rocket.motion` supplies value-owned easing, tweens, timelines, and reduced
motion. These modules are ordinary bundled Rocket source and keep native handles
out of the normal application surface.

The accepted typed asset store is currently the reference-package module
`examples/raylib_showcase/src/rocket_assets.rocket`, imported as
`src.rocket_assets`. It owns typed texture, font, sound, music, and shader
references with package-rooted paths and explicit cleanup. The planned
`rocket.assets` namespace is not available in this checkout.

## 8. Tools and reference

Use `TOOLING.md` for the LSP, formatter, debugger, coverage, profiles, and
benchmarks; `STDLIB.md` for stable APIs; `DIAGNOSTICS.md` for stable codes;
`SPEC.md` for the language; and
`ROCKET_3_0_SYNTAX_DICTIONARY.md` for the accepted Wave B additions. The
completed Rocket 2.1 target contract is in `TARGETS.md`, with its evidence in
`PHASE_19_AUDIT.md`; versioned release and migration files are historical
contracts and are indexed by `DOCUMENTATION_STATUS.md`.
