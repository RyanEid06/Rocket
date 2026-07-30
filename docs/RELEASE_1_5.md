# Rocket 1.5 Release Contract

Rocket 1.5 completes the production standard-library milestone on the
self-hosted Rocket 1.4 foundation. Rocket 1.0-1.4 source keeps its meaning, no
grammar or C ABI change is introduced, runtime ABI v1 remains stable, and the
C++20 compiler remains the reproducible stage0.

## Supported platform

- Windows x64 with the repository-pinned MSVC, Ninja, LLVM, Clang, and LLD.
- Synchronous host integration through Windows CNG, WinVerifyTrust, WinHTTP,
  Winsock, the Windows Compression API, and the system `winsqlite3` service.
- UTF-8 Rocket strings and explicit Windows path conversion at host boundaries.

## Standard-library surface

Rocket 1.5 adds buffered binary streams; big-endian codecs; Unicode scalar,
normalization, and practical grapheme operations; a bounded Thompson-NFA regular
expression engine; secure randomness, SHA-256, HMAC-SHA-256, constant-time
comparison, and offline Authenticode verification; DNS, TCP, HTTP/HTTPS client,
and bounded HTTP server foundations; UTC/calendar/time-zone helpers; logging,
command-line and configuration parsing; XPRESS Huffman compression; validated
data-only ustar archives; parameterized SQLite access; and a testing library
with fixtures, expected failures, filtering, temporary resources, and coverage
hooks.

The complete signatures, limits, error behavior, security boundaries, and
blocking behavior are normative in `STDLIB.md`. Expected data and host failures
cross the public surface as `Option` or `Result`. Resource APIs use checked
process-local tokens and reject reuse after close or cancellation.

## Security and determinism

Network and stream operations are synchronous and bounded. Socket and HTTP
operations require explicit timeouts; HTTPS never exposes a certificate-bypass
switch. Regular expressions do not use recursive backtracking. Archive readers
validate checksums and safe relative names and never extract host paths.
SQLite values use bound parameters. Cryptographic randomness is separate from
the deterministic `std.random` API, and certificate verification performs no
surprise network retrieval.

Archives, testing coverage reports, package test discovery, and standard-library
fixtures have deterministic ordering. `std.testing` is an ordinary bundled
Rocket source module over a narrow private host boundary. Generated artifacts,
databases, temporary files, downloaded dependencies, and build products remain
outside Git.

## Compatibility and limitations

Rocket 1.5 is additive. It does not add asynchronous I/O, locale-sensitive
Unicode segmentation, capture substitution, TLS server termination, cookie or
redirect policy APIs, typed SQLite values, migrations, ZIP/gzip compatibility,
or automatic compiler coverage instrumentation. Those omissions are explicit
and do not weaken the documented safe foundations.

## Release gate

A Rocket 1.5 artifact is releasable only after Debug and Release LLVM matrices,
Debug and Release LLVM-disabled stage0 matrices, all Phase 15 isolated
integration and failure-path tests, the full conformance and performance gates,
relocation/package validation, and deterministic
`stage0 -> stage1 -> stage2 -> stage3` bootstrap pass. Stage2 and stage3 compiler
IR must be byte-identical, and the stage0 and self-hosted compilers must accept
and run the same Phase 15 conformance programs.

## Validated milestone

The completed Phase 15 branch passes 131/131 tests in both pinned LLVM Debug
and Release configurations and 91/91 applicable tests in both LLVM-disabled
stage0 configurations. The focused Phase 15 suite passes 18/18 in each backend,
the Rocket 1.5 conformance suite passes 72 cases, and all eight performance
budgets pass.

The deterministic Release bootstrap checks and runs the Phase 15 fixtures
through the generated compiler and produces byte-identical stage2/stage3 LLVM
IR with SHA-256
`7e0d139180b692ccaf265768154bd210a4c4da4b00b08db737e7f9aacce67418`.
