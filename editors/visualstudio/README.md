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

For the simplest visible run, leave `rocketc.exe` selected beside the green Run
button and start it. The repository-owned launch configuration supplies the
demo package argument, repository working directory, and Visual Studio
debug-console behavior. `open-visualstudio.ps1` refreshes it from the tracked
template into Visual Studio's ignored `.vs` workspace state. The demo prints a small
collection, struct, function, and recursive Fibonacci result (`55`).

This extension does not claim semantic IntelliSense, go-to-definition, rename,
references, or Rocket-aware debugging inside Visual Studio. Rocket's completed
editor-neutral language server and CodeView/PDB debugging contracts remain
available to IDE integrations that connect them.
