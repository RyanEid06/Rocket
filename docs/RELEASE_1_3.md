# Rocket 1.3 Release Contract

Rocket 1.3 is an additive release over Rocket 1.2. Existing Rocket 1.0-1.2
source keeps its meaning, runtime ABI v1 remains stable, and the C++20 compiler
remains the reproducible stage0 bootstrap.

## Language and ABI surface

- Lexical `unsafe:` blocks are required around every imported native call.
- `extern fn` imports and `export fn` exports use one frozen Windows x64 C ABI:
  `Int` is `int64_t`, `Float` is `double`, `Bool` is an 8-bit `rocket_bool`,
  `Char` is `uint8_t`, and `Unit` is `void`.
- `extern const` represents a primitive compile-time literal, not native storage.
- `Pointer[T]` and `extern opaque` values are non-owning, non-ARC native
  pointers. Rocket provides no pointer construction, dereference, arithmetic,
  or general `null` value.
- `extern struct` records supported C layout information but may cross the ABI
  only behind a pointer.
- `extern callback` accepts an exactly matching top-level noncapturing Rocket
  function. The foreign callee may invoke it synchronously during that call
  and may not retain it.
- Native declarations and exports are non-generic. Managed Rocket values,
  by-value aggregates, closures, `Option`, and `Result` do not cross the ABI.

Rocket ARC never retains or releases native pointers or opaque handles. A C API
that returns ownership must document its release function, and a Rocket safe
wrapper must translate status values and pointer/length string conventions into
ordinary checked Rocket values. Nullability, allocation, aliasing, and callback
lifetime violations remain the native contract's responsibility.

## Library and package production

`[build].kind` selects `executable`, `static-library`, or `dynamic-library`, and
`[build].name` selects the native basename. Windows x64 native libraries,
search directories, and validated header inputs are declared explicitly under
`[native.windows-x64]` and preserve manifest order.

Library builds may omit `main`, export unmangled C wrappers, and emit a
deterministic consumer header. Static libraries leave runtime linkage to the
consumer; dynamic libraries embed the matching Rocket runtime and emit a DLL
and import library. `rocketc bind` deterministically converts the documented C
header subset into public low-level Rocket declarations, while
`rocketc emit-header` emits the C surface of a Rocket package.

## Compatibility and limitations

The supported release target and native ABI are Windows x64 with the pinned
MSVC, Ninja, LLVM, Clang, and LLD toolchain. Rocket 1.3 does not support other C
calling conventions, variadics, unions, bitfields, C++ ABIs, by-value native
structs, direct Rocket strings, stored or capturing callbacks, dynamic symbol
loading, arbitrary pointer operations, package registries, or non-Windows
targets. The internal Rocket runtime ABI remains version 1 and is not a public C
interface.

## Release gate

A 1.3 artifact is releasable only when complete Debug and Release LLVM matrices,
LLVM-disabled stage0 matrices, Phase 13 bidirectional native consumers,
stage0/self-hosted generation parity, conformance and performance gates pass,
and `stage0 -> stage1 -> stage2 -> stage3` produces byte-identical stage2/stage3
LLVM IR. Generated headers/bindings must be byte deterministic. Generated,
downloaded, bootstrap, package, native, and `.rocketc` artifacts remain outside
Git.
