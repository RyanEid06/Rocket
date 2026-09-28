# Rocket consumer SDK

This branch is a ready-to-use Rocket SDK for Windows x64, Linux x64, Linux
ARM64, and macOS Apple Silicon ARM64. All platform packages come from Rocket
master commit 1f6ba76f16f3246095d5d573c28d825d8b9367e3. The compiler reports
version 3.0.0, and the included standard libraries contain the Rocket 3.5 game
APIs present in that source commit. This is a source snapshot, not a separately
versioned Rocket 3.5 release.

Each platform package includes its production compiler, required native
toolchain and runtime libraries, standard library, target metadata, and
licenses. All platform packages are grouped under `platforms/`. The Windows
SDK is unpacked in `platforms/windows-x64/`; the Linux and macOS packages are
the official, complete release archives kept intact so their platform tools,
setup instructions, provenance, and work-package acceptance records stay
together. This branch adds no standalone example projects.

| Target | Package |
| --- | --- |
| Windows x64 | `platforms/windows-x64/` |
| Linux x64 | `platforms/rocket-3.0.0-linux-x64.tar.xz` |
| Linux ARM64 | `platforms/rocket-3.0.0-linux-arm64.tar.xz` |
| macOS Apple Silicon ARM64 | `platforms/rocket-3.0.0-macos-arm64.tar.xz` |

The three archives are the verified Rocket 3.0.0 release packages. Their
SHA-256 hashes are listed in `SHA256SUMS.txt` and match the published release.
Extract a platform archive with `tar -xf <archive>`; its root folder contains
`PACKAGE.md` with platform prerequisites and setup instructions.

Install Git LFS before cloning so the compiler tools and platform archives are
downloaded as real files. Clone the branch with:

    git lfs install
    git clone --branch consumer --single-branch https://github.com/RyanEid06/Rocket.git

There is no source build step. From PowerShell, run the Windows SDK with:

    .\platforms\windows-x64\bin\rocketc.exe --version
    .\platforms\windows-x64\bin\rocketc.exe check .\path\to\app.rocket
    .\platforms\windows-x64\bin\rocketc.exe run .\path\to\app.rocket
    .\platforms\windows-x64\bin\rocketc.exe build .\path\to\app.rocket

For a package, pass the directory containing rocket.toml instead of a single
Rocket source file. The compiler locates its bundled runtime and tools relative
to bin. SHA256SUMS.txt covers every distributed file except itself.

See [the named roadmap](ROADMAP.md) for the game-platform milestones and
remaining release checks. Master remains the source of truth for Rocket
development; this branch contains the ready-to-use consumer SDK packages.
