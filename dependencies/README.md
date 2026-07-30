# Reproducible Developer Toolchain

This directory describes the tools needed to build the compiler. Large archives and extracted programs are deliberately ignored by Git.

## One-time setup

Open PowerShell and run:

```powershell
.\dependencies\bootstrap.ps1
.\dependencies\verify.ps1
```

The bootstrap downloads the exact LLVM, Ninja, and raylib releases recorded in
`manifest.json`, resumes downloads where the upstream server permits it, and
verifies SHA-256 hashes before extraction. raylib source is pinned to tag 6.0
and commit `dbc56a87da87d973a9c5baa4e7438a9d20121d28`; it is built statically by
the Phase 14 CMake targets and is never committed.

The host also needs CMake, Git, and the Microsoft C++ workload with a Windows SDK. `verify.ps1` reports any missing system component. Regular users of a future packaged `rocketc` release will not need this developer toolchain.

## Activating a shell

```powershell
. .\dependencies\activate.ps1
```

This loads the Microsoft x64 build environment and prepends the pinned LLVM and Ninja tools to `PATH` for the current PowerShell session.

## Licensing

LLVM is distributed under Apache License 2.0 with LLVM exceptions. Ninja is
distributed under the Apache License 2.0. raylib is distributed under the
zlib/libpng license. Their upstream license files remain in their extracted
distributions. The manifest URLs are the authoritative upstream artifacts.
