# Rocket consumer SDK

This branch contains the minimal ready-to-use Rocket 3.0.0 SDK for Windows x64.
It is built from the unchanged master branch at 1f6ba76. The SDK includes the
production Rocket compiler, its Clang/LLD toolchain, native runtime and linker
libraries, standard library, target metadata, and required licenses.

Clone with Git LFS installed so bin/clang.exe is downloaded as a real executable.
The SDK is ready to use after checkout; there is no source build step.

From PowerShell:

    .\bin\rocketc.exe --version
    .\bin\rocketc.exe check .\path\to\app.rocket
    .\bin\rocketc.exe run .\path\to\app.rocket
    .\bin\rocketc.exe build .\path\to\app.rocket

For a package, pass the directory containing rocket.toml instead of an .rocket
file. The compiler locates its bundled runtime and tools relative to bin.
SHA256SUMS.txt covers the files in this branch. To verify a file, compare its
SHA-256 digest with the matching line in that manifest.

Scope: Windows x64 only. Source code, tests, examples, editor integrations,
bootstrap tools, and release engineering files stay on master.
