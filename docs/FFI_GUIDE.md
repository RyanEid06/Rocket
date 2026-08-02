# Rocket 2.0 FFI Guide

Rocket's supported FFI is the frozen Windows x64 C-compatible subset from
Phase 13. Put target inputs in `[native.windows-x64]`, generate low-level Rocket
declarations with `rocketc bind`, and generate a C consumer header with
`rocketc emit-header`.

Supported boundaries include fixed-width scalar values, `rocket_bool`, `void`,
pointers, primitive native structs, opaque handles, synchronous non-storing
callbacks, integer constants, and C-compatible exported functions. Variadics,
C++ APIs, unions, bitfields, flexible arrays, dynamic library discovery,
unreviewed calling conventions, and callbacks retained after the call are not
supported.

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

Static Rocket libraries require consumers to link the matching
`rocket_runtime.lib`; dynamic Rocket libraries embed the runtime. ABI v1 is
frozen for Rocket 2.x. See `SPEC.md`, `TOOLING.md`, and `COMPILER_ARCHITECTURE.md` for the
normative layouts, mangling, linker order, and generated-header contract.
