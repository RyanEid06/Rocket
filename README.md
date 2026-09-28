# Rocket consumer SDK

This branch is a ready-to-use Rocket SDK for Windows x64. It is built from
Rocket master commit 1f6ba76f16f3246095d5d573c28d825d8b9367e3. The
compiler reports version 3.0.0, and the included standard library contains the
Rocket 3.5 game APIs present in that source commit. This is a source snapshot,
not a separately versioned Rocket 3.5 release.

The SDK includes the production compiler, its Clang/LLD toolchain, native
runtime and linker libraries, standard library, target metadata, and required
licenses. It leaves out compiler source, tests, examples, editor integrations,
and bootstrap tools.

Install Git LFS before cloning so bin/clang.exe is downloaded as an executable.
There is no source build step. From PowerShell:

    .\bin\rocketc.exe --version
    .\bin\rocketc.exe check .\path\to\app.rocket
    .\bin\rocketc.exe run .\path\to\app.rocket
    .\bin\rocketc.exe build .\path\to\app.rocket

For a package, pass the directory containing rocket.toml instead of a single
Rocket source file. The compiler locates its bundled runtime and tools relative
to bin. SHA256SUMS.txt covers every distributed file except itself.

See [the named roadmap](ROADMAP.md) for the game-platform milestones and
remaining release checks. Master remains the source of truth for Rocket
development; this branch is the consumer distribution for Windows x64.
