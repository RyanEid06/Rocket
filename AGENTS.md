# Rocket Repository Guidance

Before planning or implementing Rocket work, read `docs/PROJECT_CONTEXT.md` and the relevant language specification files in `docs/`.

- Rocket language development is the priority; do not begin casino implementation before Rocket is self-hosted.
- Preserve the C++ compiler as the reproducible `stage0` bootstrap compiler.
- Target Windows x64 first and use the pinned MSVC/Ninja/LLVM toolchain.
- Keep generated artifacts and downloaded dependencies out of Git.
- Every language feature must include specification updates, parser/type/MIR/backend work as needed, tests, and documentation.
- Run the relevant build and test commands before handing work off.
