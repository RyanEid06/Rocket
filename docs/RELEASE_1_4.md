# Rocket 1.4 Release Contract

Rocket 1.4 validates the Rocket 1.3 native interoperability surface through a
substantial raylib application. Existing Rocket 1.0-1.3 source keeps its
meaning, runtime ABI v1 and the C ABI remain unchanged, and C++20 remains the
reproducible stage0 compiler.

## Pinned platform

- Windows x64 with the repository-pinned MSVC, Ninja, LLVM, Clang, and LLD.
- raylib 6.0 tag commit `dbc56a87da87d973a9c5baa4e7438a9d20121d28`.
- Source archive size 52,545,040 bytes and SHA-256
  `2b3ee1e2120c7a0796b33062c7e9a694dd8a8caa56a96319ac8c8ecf54a90d0b`.
- Static raylib and adapter linkage. Downloaded sources, generated bindings,
  native libraries, executables, and bundles remain outside Git.

## Safe API surface

The supported wrapper provides window creation/shutdown, single-use drawing
frames, background/rectangle/circle/text drawing, default and loaded fonts,
texture loading/drawing/scaling/unloading, keyboard and mouse input, frame and
elapsed timing, audio-device lifecycle, sound file loading, procedural tones,
play/stop/volume, synchronous callbacks, and checked error translation.

raylib structures, C pointers, and strings do not cross into application code.
Window, frame, texture, font, audio, and sound tokens are validated before every
native operation. The wrapper owns all explicit unsafe regions and maps native
status codes to `Result`. Resource cleanup order is deterministic and tested.

## Reference application and packaging

`examples/raylib_showcase` is a non-casino event-driven Rocket application. It
contains a moving textured orbiter, keyboard/mouse control, persistent trail
state, collection growth/clear behavior, text-file and texture loading,
callback-driven animation, timing, procedural audio with silent fallback, and
ordered shutdown. No handwritten C++ application behavior exists.

The repository supplies Debug/Release validation, a reusable application
scaffold command, and a static Windows bundle command. Bundles contain the
executable, assets, instructions, checksums, notices, and upstream raylib
license. A headless deterministic backend validates the same native ABI on
machines without display or audio devices.

## Limitations

Rocket 1.4 supports one window and one audio device at a time. Cleanup is
explicit because Rocket does not yet have linear types, language destructors,
or weak references. Native rendering is validated only on Windows x64. The safe
surface is intentionally narrower than the complete raylib API: shaders,
render textures, cameras, models, video, stored callbacks, raw pixel buffers,
custom audio streams, dynamic raylib loading, and non-Windows backends remain
future work.

## Release gate

A Rocket 1.4 artifact is releasable only after Debug and Release LLVM matrices,
Debug and Release LLVM-disabled stage0 matrices, the full conformance and
performance gates, the native raylib validation suite, packaging validation,
and deterministic `stage0 -> stage1 -> stage2 -> stage3` bootstrap pass.
Stage2/stage3 compiler IR and generated raylib bindings must be byte-identical.

## Validated milestone

The completed Phase 14 branch passed 111/111 tests in each LLVM Debug and
Release matrix, 71/71 tests in each LLVM-disabled stage0 matrix, 10/10 focused
raylib tests in each configuration, 63 conformance cases, and all eight
performance gates. A clean scaffold build and the Release packaging workflow
also passed.

The deterministic bootstrap produced byte-identical stage2 and stage3 LLVM IR
with SHA-256
`5e5a33f1a38ac2192ee71b972e79bfc67f1e5e85ba6e0fbe19ffe63fdfe7e407`.
