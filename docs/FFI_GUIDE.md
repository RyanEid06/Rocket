# Rocket 2.1 FFI Guide

Rocket's portable FFI is the Rocket 2.0-compatible C subset on each Phase 19
production target. Put target inputs in the matching `[native.<alias>]`
section, generate low-level Rocket declarations with `rocketc bind`, and
generate a C consumer header with `rocketc emit-header`. The target aliases,
native-input selection, and host/target policy are normative in `TARGETS.md`.

Supported boundaries include fixed-width scalar values, `rocket_bool`, `void`,
pointers, primitive native structs, opaque handles, synchronous non-storing
callbacks, integer constants, and C-compatible exported functions. Variadics,
C++ APIs, unions, bitfields, flexible arrays, unreviewed calling conventions,
and callbacks retained after the call are not supported. The portable surface
uses the target's ordinary C ABI only: Windows x64 uses its MSVC C ABI, Linux
x64/Linux ARM64 use the GNU C ABI, and macOS ARM64 uses the Apple C ABI.
Broader calling conventions are optional enhancements, not hidden Phase 19
requirements. Rocket supports producing and consuming explicit static and
dynamic native products; arbitrary runtime discovery/loading of a foreign
library is intentionally not a safe Rocket API. Internal platform providers may
load an OS library to implement a documented standard-library function without
exposing its handle or symbol lookup to safe Rocket.

## Wrapper rules

1. Keep generated declarations separate from handwritten code.
2. Make the smallest possible region `unsafe:`.
3. Validate pointers, lengths, statuses, and ownership before returning to safe
   Rocket; translate recoverable errors to `Option` or `Result`.
4. Model a native handle with one owner and exactly one documented destructor.
5. Never claim `Send` or `Share` for pointers, callbacks, or opaque handles.
6. Ensure buffers and callback captures outlive the complete synchronous call.
7. Pin native libraries and headers as reviewed package inputs and exercise the
   wrapper in native, negative, sanitizer, and relocation tests.

Static Rocket libraries require consumers to link the matching target runtime
(`rocket_runtime.lib` on Windows and `librocket_runtime.a` on Linux/macOS);
dynamic Rocket libraries embed the runtime. ABI v1 is frozen for Rocket 2.x.
The suffixes are `.dll/.lib` on Windows, `.so/.a` on Linux, and `.dylib/.a` on
macOS. See `SPEC.md`, `TOOLING.md`, and `COMPILER_ARCHITECTURE.md` for the
normative layouts, mangling, linker order, and generated-header contract.
