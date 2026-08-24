# Rocket Target and Portability Contract 2.1

Phase 19 adds explicit target selection and the first portable Rocket release.
This contract is additive to the frozen Rocket 2.0 language and runtime ABI v1:
valid Rocket 2.0 source remains valid, the C++20 compiler remains permanent
`stage0`, and the Rocket-written compiler must make the same target decisions.

The word **host** means the machine on which `rocketc` is executing. The word
**target** means the platform for which it emits an artifact. A compiler must
never infer target behavior from the host after target normalization.

## Required production targets

| Alias | Canonical LLVM triple | OS | Architecture | Environment | Pointer width | Endianness |
| --- | --- | --- | --- | --- | --- | --- |
| `windows-x64` | `x86_64-pc-windows-msvc` | `windows` | `x64` | `msvc` | 64 | little |
| `linux-x64` | `x86_64-unknown-linux-gnu` | `linux` | `x64` | `gnu` | 64 | little |
| `linux-arm64` | `aarch64-unknown-linux-gnu` | `linux` | `arm64` | `gnu` | 64 | little |
| `macos-arm64` | `arm64-apple-macosx` | `macos` | `arm64` | `apple` | 64 | little |

The alias and canonical triple are accepted anywhere a target is requested and
normalize to the table row. Spelling is ASCII case-sensitive. All other values
produce `R6001`, except `windows-arm64` and
`aarch64-pc-windows-msvc`, which are recognized evaluation targets and produce
`R6002` until the Windows ARM64 acceptance matrix has passed.

When `--target` is omitted, the compiler detects the native host row. An
unrecognized native host is an `R6002` error; it is not silently treated as one
of the production targets. `rocketc target` prints the normalized alias and
`rocketc target --verbose` prints host, target, triple, OS, architecture,
environment, pointer width, endianness, features, and whether execution is
native or cross-compiled.

## Deterministic target diagnostics

Target failures have stable categories in both compilers:

| Code | Meaning |
| --- | --- |
| `R6001` | the requested target name or triple is unknown |
| `R6002` | the target is recognized but is not a supported production target |
| `R6003` | a required compiler, linker, sysroot, SDK, runtime, or target-native input is unavailable |
| `R6004` | the host/target operation or native execution path is unsupported |
| `R6005` | target-conditioned manifest configuration is invalid or ambiguous |

Diagnostics name normalized targets and explicit missing inputs. They must not
depend on ambient `PATH`, absolute checkout paths, locale, or discovery order.

## Conditional source selection

Rocket 2.1 performs target selection at the package/module boundary before
lexing or type checking inactive source. The portable source tree remains the
default. A manifest may overlay it for a specific target:

```toml
[target.linux-arm64]
source-root = "src/targets/linux-arm64"
entry = "main.rocket"
test-directory = "tests"
```

`source-root`, `entry`, and `test-directory` are relative, normalized package
paths and may not escape the package. The selected target root has precedence
over the portable package root for the same logical module. Modules that exist
only in the portable root remain available. A module may not resolve to two
files in the same precedence tier. Inactive roots are excluded before source
discovery limits, parsing, semantic analysis, caching, and diagnostics.

The target section is optional. An absent section means the portable source
tree is used. Unknown target sections, duplicate normalized sections, invalid
paths, or a selected entry that does not exist produce `R6005`. Dependencies
apply their own target section using the same normalized compilation target.

This mechanism deliberately adds no lexical preprocessor and no conditional
expression to the frozen grammar. Platform-neutral interfaces belong in the
portable tree; target overlays provide the platform implementation.

## Target-scoped native inputs

Native inputs remain explicit capabilities and are selected by target:

```toml
[native.linux-x64]
include-directory = "native/include"
library-directory = "native/linux-x64/lib"
library = "example"
runtime-file = "native/linux-x64/libexample.so"
```

The accepted keys and security rules are those in `PACKAGES.md`. The four
production aliases are valid section names. A native section for a different
target is inert and is not inspected, hashed, linked, copied, or granted a
capability. A required input missing from the selected section is `R6003`.

## Compile-time target queries

`std.target` exposes immutable values for the selected target, never the host:

```rocket
std.target.alias() -> String
std.target.triple() -> String
std.target.os() -> String
std.target.architecture() -> String
std.target.environment() -> String
std.target.pointer_width() -> Int
std.target.endianness() -> String
std.target.has_feature(name: String) -> Bool
```

The baseline feature names are `threads`, `dynamic-libraries`, `dwarf`,
`codeview`, `sse2`, and `neon`. Unknown well-formed feature names return
`false`. Windows x64 has `threads`, `dynamic-libraries`, `codeview`, and
`sse2`; Linux x64 has `threads`, `dynamic-libraries`, `dwarf`, and `sse2`;
Linux ARM64 and macOS ARM64 have `threads`, `dynamic-libraries`, `dwarf`, and
`neon`. Target values are compiler-provided constants and do not consult the
running operating system.

## Artifacts, caches, and installation

All non-explicit artifacts are target-qualified under
`.rocketc/targets/<alias>/`. Cache identities include the normalized triple,
target features, code-generation options, compiler/runtime hashes, complete
active package inputs, dependency identities, and selected native inputs.
Inactive target files do not affect the key.

| Target OS | Executable suffix | Dynamic library | Static library | Object |
| --- | --- | --- | --- | --- |
| Windows | `.exe` | `.dll` | `.lib` | `.obj` |
| Linux | none | `.so` | `.a` | `.o` |
| macOS | none | `.dylib` | `.a` | `.o` |

An installed native SDK bundles the matching `rocketc`, runtime, standard
library, linker/toolchain components, target metadata, licenses, and checksum
manifest. A normal native compile after installation must not require an
unrelated system compiler. Relocation may not rely on the original checkout.

## Toolchains and cross-compilation

Toolchain discovery is an explicit function of `(host, target)`. Native SDKs
use their bundled compiler, linker, runtime, and standard library. Cross builds
must name a target SDK/sysroot through a documented option or environment
variable; ambient system headers and libraries are never silently substituted.

Phase 19 supports these production paths:

- every target compiling itself natively;
- Windows x64 to Linux x64 and Linux ARM64 with an explicit Linux target SDK;
- Linux x64 to Linux ARM64 with an explicit Linux ARM64 target SDK;
- Linux x64 to Windows x64 with an explicit Windows target SDK.

Other combinations fail with `R6004`. In particular, Apple SDK licensing and
linker requirements mean macOS artifacts are linked and packaged only on a
macOS host. `run` and native portions of `test` require `host == target`; a
cross-built artifact is never executed implicitly or through an undeclared
emulator.

## Platform behavior

Portable APIs use `/` as their logical path separator and convert at the OS
boundary. Filesystem, environment, wall and monotonic time, process, sockets,
HTTP/TLS, concurrency, async I/O, dynamic loading, debugger/source information,
FFI, installation, and package operations must implement the contracts in their
own specifications on all four production targets. A platform limitation must
be stated in the relevant API contract and returned as a `Result`; it may not be
hidden behind different observable behavior.

Windows emits PE/COFF and CodeView/PDB information, Linux emits ELF and DWARF,
and macOS emits Mach-O and DWARF. Native C interoperability uses the platform C
ABI. Broader calling conventions are optional only where `FFI_GUIDE.md`
explicitly classifies them as outside the portable C surface.

## Acceptance

A target is production-supported only after a native host has completed clean
Debug and Release builds; stage0 through stage3 bootstrap determinism; complete
compiler, conformance, standard-library, package, FFI, compatibility,
concurrency/async, debugger, and substantial-application suites; native
installation; reproducible packaging; checksum verification; and sanitized
relocation. Cross-compilation or a workflow definition is not native evidence.

Exact commands, test counts, versions, hashes, timings, and results are recorded
in `PHASE_19_AUDIT.md`. The numbered roadmap closes only after all four rows have
observed native evidence.
