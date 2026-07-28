# Rocket Master Roadmap

Rocket language development comes first. Casino development begins only after Rocket is self-hosted and Rocket 1.0 is released.

1. **Stabilize** - Clean the repository, verify builds/tests, update documentation, and create the first baseline commit. **Completed.**
2. **Core syntax** - Add assignment, loops, primitive types, operators, ranges, and stronger diagnostics. **Completed.**
3. **Compiler architecture** - Introduce typed HIR and MIR so every backend receives clean, verified instructions. **Completed.**
4. **LLVM backend** - Replace C++ transpilation with genuine LLVM IR, optimization, object generation, and native linking. **Completed.**
5. **Runtime** - Implement UTF-8 strings, arrays, slices, ARC memory management, and safe runtime errors. **Completed.**
6. **Type system** - Add structs, enums, generics, pattern matching, `Option`, `Result`, and error propagation. **Completed.**
7. **Modules and libraries** - Add imports, files, collections, JSON, CSV, randomness, processes, time, and practical APIs. **Completed.**
8. **Tooling** - Build the formatter, test runner, diagnostic catalog, VS Code support, packaging, and documentation. **Completed.**
9. **Self-hosting** - Rewrite the Rocket compiler in Rocket and verify the `stage0 -> stage1 -> stage2 -> stage3` bootstrap. **Completed.**
10. **Rocket 1.0** - Freeze the language, run conformance/performance tests, and publish the self-contained compiler. **Completed.**
11. **Graphics platform** - Bind raylib and add safe Rocket APIs for windows, drawing, input, audio, and assets.
12. **Casino engines** - Build and test Blackjack, Slots, and Video Poker logic entirely in Rocket.
13. **Casino v1** - Create the graphical lobby, shared wallet, profiles, saves, animations, audio, and distributable application.

See `PROJECT_CONTEXT.md` for the current implementation state, locked decisions, handoff requirements, and next task.
