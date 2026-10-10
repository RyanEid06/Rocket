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
the matching stage-zero host, toolchain and runtime libraries, standard library, target metadata,
dependency licenses, installation instructions, checksums, and release metadata.
Keep `stage0/` with the SDK: version 3.0.0 uses this matching host for debugging,
structured IDE commands, and package security operations. It is part of the
released runtime and must not be removed or replaced with an older compiler.
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

## Configure the SDK in RocketIDE on Windows

1. Install and open [RocketIDE](https://ryaneid06.github.io/Rocket-RocketIDE/).
2. Choose **Tools > Rocket SDK Settings...**.
3. Beside **Rocket compiler**, click **Browse...** and select this branch's `platforms\windows-x64\bin\rocketc.exe`. If you downloaded the SDK ZIP instead, select `Rocket-SDK-3.0.0-windows-x64\bin\rocketc.exe` in your extracted folder.
4. Beside **Language server**, click **Browse...** and select `bin\rocket-lsp.exe` from that same SDK. Keep every SDK directory, including `stage0`, together.
5. Click **Save**, then choose **Tools > Validate Rocket Environment**. Open **View > Output** and look for `Environment validation passed.` and compiler version `rocketc 3.0.0`.
6. Open a writable project folder with **File > Open Folder...**, open or save a `.rocket` file, then use **Build > Check Rocket Target** and **Run > Run Rocket Target**.

If you move the SDK, select its new paths again. See the [first-program and debugging guide](https://github.com/RyanEid06/Rocket-RocketIDE/blob/main/docs/GETTING_STARTED.md) for a complete example.
