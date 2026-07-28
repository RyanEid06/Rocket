# Rocket for Visual Studio 2026

This repository-owned VSIX gives Visual Studio Community 2026 basic, honest
Rocket support:

- `.rocket` file recognition and TextMate syntax coloring;
- line comments, bracket matching/closing, and indentation behavior; and
- CMake targets named `rocket_demo_check`, `rocket_demo_run`, and
  `rocket_demo_test` for the example package.

Build the VSIX from the repository root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\package-visualstudio-extension.ps1
```

Install `out/visualstudio/Rocket.Language.VisualStudio.vsix`, then launch the
repository with its pinned LLVM, Ninja, and MSVC environment:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\open-visualstudio.ps1
```

Visual Studio opens the repository as its existing CMake project. In CMake
Targets View, build `rocket_demo_check`, `rocket_demo_run`, or
`rocket_demo_test` to check, run, or test the real package under
`examples/visualstudio_demo`.

This extension does not claim semantic IntelliSense, go-to-definition, rename,
references, or Rocket-aware debugging. Those require the language server and
debugger work planned for Phase 17.
