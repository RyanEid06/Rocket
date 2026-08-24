# Reproducible Developer Toolchain

This directory describes the tools needed to build the compiler. Large archives and extracted programs are deliberately ignored by Git.

## One-time setup

On every supported host, Python 3.11 or newer installs the exact native row
from `manifest.json`:

```text
python3 dependencies/bootstrap.py
python3 dependencies/verify.py
```

The PowerShell entry points remain available on Windows:

```powershell
.\dependencies\bootstrap.ps1
.\dependencies\verify.ps1
```

The bootstrap detects `windows-x64`, `linux-x64`, `linux-arm64`, or
`macos-arm64`, downloads that row's official LLVM 22.1.6 and Ninja 1.13.1
archives plus shared raylib 6.0 source, resumes downloads where the server
permits it, and verifies both the exact byte count and SHA-256 before
extraction. raylib remains pinned to tag 6.0 and commit
`dbc56a87da87d973a9c5baa4e7438a9d20121d28`.

The host also needs CMake, Git, a C++20 compiler, and its native platform SDK.
Windows uses the MSVC workload and Windows SDK. Linux developer builds need
glibc, curl, OpenSSL, ICU, X11/OpenGL, and ALSA development packages. macOS
uses the installed Apple SDK plus curl, OpenSSL, and ICU. These are developer
inputs; a packaged Rocket SDK supplies its own Rocket compiler and LLVM tools.

## Activating a shell

```powershell
. .\dependencies\activate.ps1
```

On Linux and macOS:

```sh
. ./dependencies/activate.sh
```

This loads the Microsoft x64 build environment and prepends the pinned LLVM and Ninja tools to `PATH` for the current PowerShell session.

## Licensing

LLVM is distributed under Apache License 2.0 with LLVM exceptions. Ninja is
distributed under the Apache License 2.0. raylib is distributed under the
zlib/libpng license. Their upstream license files remain in their extracted
distributions. The manifest URLs are the authoritative upstream artifacts.
