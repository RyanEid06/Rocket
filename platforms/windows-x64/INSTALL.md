# Rocket SDK 3.0.0 for Windows x64

Extract this entire ZIP into a directory you can read and execute. Keep the
`bin`, `lib`, `include`, `stdlib`, `share`, `licenses`, and `stage0` directories together.
The SDK includes its compiler, language server, LLVM tools and runtime; it is
separate from the RocketIDE Windows application.
The matching `stage0` compiler is required at runtime for debugging, structured
IDE commands, and package security operations. Do not remove or replace it.

From PowerShell in the extracted directory:

```powershell
.\bin\rocketc.exe --version
.\bin\rocketc.exe target --verbose
.\bin\rocketc.exe check C:\path\to\hello.rocket
.\bin\rocketc.exe run C:\path\to\hello.rocket
.\bin\rocketc.exe build C:\path\to\hello.rocket --debug --message-format=json
```

In RocketIDE, select `bin\rocketc.exe` and `bin\rocket-lsp.exe` under
Tools > Rocket SDK Settings. `SHA256SUMS.txt` verifies the included files.
`RELEASE.json` records the source and original release archive identities.
