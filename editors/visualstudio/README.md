# Rocket for Visual Studio 2026

This repository-owned VSIX gives Visual Studio Community 2026 basic, honest
Rocket support:

- `.rocket` file recognition and TextMate syntax coloring;
- line comments, bracket matching/closing, and indentation behavior; and
- CMake targets named `rocket_demo_check`, `rocket_demo_run`, and
  `rocket_demo_test` for the example package.

## Fresh-clone setup

These steps are deliberately repository-relative. Do not replace them with a
username, drive letter, or hard-coded clone directory.

1. Install Visual Studio Community 2026 with the **Desktop development with
   C++** workload and CMake support.
2. Open PowerShell in the repository root and install/verify Rocket's pinned
   dependencies:

   ```powershell
   .\dependencies\bootstrap.ps1
   powershell.exe -NoProfile -ExecutionPolicy Bypass `
     -File .\dependencies\verify.ps1
   ```

3. Build the repository-owned extension:

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass `
     -File .\scripts\package-visualstudio-extension.ps1
   ```

4. Open `out/visualstudio/Rocket.Language.VisualStudio.vsix` and install it for
   Visual Studio Community 2026. Restart Visual Studio if the installer asks.
5. Launch the repository through the supported wrapper:

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass `
     -File .\scripts\open-visualstudio.ps1
   ```

6. Wait for CMake configuration to finish. Leave `rocketc.exe` selected beside
   the green Run button and start it. A successful demo ends with `55` and exit
   code `0`.

For a Codex-assisted setup, tell Codex to read `AGENTS.md`,
`docs/PROJECT_CONTEXT.md`, and this file, then perform the fresh-clone steps and
verify the visible Visual Studio run. Codex must keep `.vs`, `out`, `.rocketc`,
and `dependencies/installed` out of Git; they are local/generated state.

Visual Studio opens the repository as its existing CMake project. In CMake
Targets View, build `rocket_demo_check`, `rocket_demo_run`, or
`rocket_demo_test` to check, run, or test the real package under
`examples/visualstudio_demo`.

For the simplest visible run, leave `rocketc.exe` selected beside the green Run
button and start it. The repository-owned launch configuration supplies the
demo package argument, repository working directory, and Visual Studio
debug-console behavior. `open-visualstudio.ps1` refreshes it from the tracked
template into Visual Studio's ignored `.vs` workspace state. The demo prints a
small collection, struct, function, and recursive Fibonacci result (`55`).

This extension does not claim semantic IntelliSense, go-to-definition, rename,
references, or Rocket-aware debugging inside Visual Studio. Rocket's completed
editor-neutral language server and CodeView/PDB debugging contracts remain
available to IDE integrations that connect them.
