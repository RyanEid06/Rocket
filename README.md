# Rocket consumer SDK

This branch contains ready-to-use Rocket 3.0.0 SDK packages for Windows x64,
Linux x64, Linux ARM64, and macOS Apple Silicon ARM64. They originate from
Rocket source commit `1f6ba76f16f3246095d5d573c28d825d8b9367e3`.
The compiler reports 3.0.0; the included standard library has the game APIs
present in that source commit.

| Target | Consumer package |
| --- | --- |
| Windows x64 | `platforms/windows-x64/` |
| Linux x64 | `platforms/Rocket-SDK-3.0.0-linux-x64.tar.xz` |
| Linux ARM64 | `platforms/Rocket-SDK-3.0.0-linux-arm64.tar.xz` |
| macOS Apple Silicon ARM64 | `platforms/Rocket-SDK-3.0.0-macos-arm64.tar.xz` |

Each package contains production compiler and language-server launchers,
toolchain and runtime libraries, standard library, target metadata,
dependency licenses, installation instructions, checksums, and release metadata.
Extract a complete Unix archive and follow its `INSTALL.md`. The Windows SDK
is unpacked in `platforms/windows-x64/`; keep all files together and run
`platforms/windows-x64/bin/rocketc.exe --version` from a Windows shell.
On Ubuntu or Debian, install `libc6-dev libcurl4-openssl-dev` for native builds.
On macOS, install Xcode Command Line Tools and use an active macOS SDK.

The same SDK archives are available from the [published release](https://github.com/RyanEid06/Rocket-RocketIDE/releases/tag/v3.0.0).
`SHA256SUMS.txt` covers distributed files in this branch. Install Git LFS
before cloning so large binaries and archives are fetched as real files:

```sh
git lfs install
git clone --branch consumer --single-branch https://github.com/RyanEid06/Rocket.git
```

`master` retains Rocket developer source and tests. RocketIDE is a separate
Windows application; configure this SDK in its Tools > Rocket SDK Settings.
