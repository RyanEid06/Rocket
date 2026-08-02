# Contributing to Rocket

Rocket accepts focused compiler, runtime, standard-library, package, tooling,
test, and documentation changes. Read `docs/PROJECT_CONTEXT.md`, the relevant
specification, and `docs/CHARTER.md` before editing.

## Change contract

1. Describe the user-facing problem and compatibility effect.
2. Update the normative specification before treating new behavior as stable.
3. Implement matching C++ stage0 and Rocket-written compiler behavior where
   the compiler surface is affected.
4. Add positive, negative, diagnostic, ownership, security, and performance
   coverage proportional to the risk.
5. Run the focused tests plus Debug and Release matrices. Compiler changes also
   require deterministic stage0-to-stage3 bootstrap validation.
6. Update reference documentation, migration notes, the decision journal when
   policy changes, and `PROJECT_CONTEXT.md` when a milestone completes.

Use the pinned Windows x64 toolchain and keep `out/`, `.rocketc/`, downloads,
keys, certificates, and generated artifacts out of Git. Do not weaken resource
bounds, signature checks, safe ownership, locked dependencies, or diagnostic
stability merely to accept malformed input. New syntax must earn its cost; the
Rocket 2.0 grammar and public contracts are frozen for compatible 2.x releases.

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\dependencies\verify.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Debug
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Release
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\hardening.ps1
```

Report vulnerabilities through the private process in `SECURITY.md`, not a
public issue. Contributions must be reviewable, deterministic, and covered by
the repository's existing license terms.
