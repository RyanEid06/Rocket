# Rocket Security Policy

## Supported release

The latest completed Rocket 2.1 portability release is the supported line.
Rocket 1.x and 2.0 remain source-compatibility inputs, and the frozen Rocket
2.0 Windows x64 SDK remains available to consumers such as Scroll2Roll. The
accepted Rocket 2.1 target rows are Windows x64, Linux x64, Linux ARM64, and
macOS ARM64; security fixes are released against the current 2.1 line.

Rocket 3.0 is still in development. Wave B is accepted on the Windows
integration baseline, while Wave C and the final non-Windows Rocket 3 target
laboratory work remain in progress. Do not treat Wave B documentation or
provisional graphics/UI experiments as a released security-support promise.

## Reporting a vulnerability

Do not publish an exploitable report in a public issue. Use the repository
host's private security-advisory channel and include the affected version,
reproducer, impact, and any known mitigation. If private advisories are not
available, contact the repository maintainers privately before disclosure.

Maintainers acknowledge a complete report within three business days, provide
an initial severity and remediation plan within seven business days, and
coordinate a disclosure date with the reporter. A release may ship sooner when
active exploitation or a simple safe mitigation warrants it.

## Release and hardening policy

- Official artifacts are stable-channel, clean-tree builds signed with an
  explicitly supplied Windows signing certificate.
- Every distribution includes `RELEASE-PROVENANCE.json`, exact SHA-256 sums,
  and a detached CMS signature over those sums.
- `scripts/verify-distribution.ps1` verifies every listed file, provenance,
  checksum signature, binary Authenticode signatures for official artifacts,
  relocation, dependency locking, and representative native execution.
- Parser/package fuzzing, bounded-input tests, sanitizer builds, compatibility,
  bootstrap, conformance, performance, and application validation are release
  gates. Minimized reproducers must not contain confidential source.
- Package archives and dependency caches remain inert data: no package build
  scripts are executed.

Security fixes receive an `Rdddd` diagnostic or protocol/version change when a
stable machine contract is affected. An incompatible fix requires a recorded
decision and migration guidance; safety takes priority over compatibility when
the two cannot both be preserved.
