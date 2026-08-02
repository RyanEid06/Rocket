# The Rocket Book

This compact book is the supported learning path for Rocket 2.0. The linked
specifications remain normative when a tutorial explanation is abbreviated.

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

## 7. Tools and reference

Use `TOOLING.md` for the LSP, formatter, debugger, coverage, profiles, and
benchmarks; `STDLIB.md` for APIs; `DIAGNOSTICS.md` for stable codes; `SPEC.md`
for the language; and `RELEASE_2_0.md` for security, compatibility, and release
trust. Rocket 2.0 supports Windows x64; Phase 19 portability is deferred.
