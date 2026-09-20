#!/usr/bin/env python3
"""Enforce the Rocket 3.5 WP1 production-runtime ownership boundary."""

from __future__ import annotations

import argparse
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True, type=Path)
    arguments = parser.parse_args()
    root = arguments.source_dir.resolve()

    adapter = root / "src" / "raylib" / "rocket_raylib_adapter.cpp"
    header = root / "src" / "raylib" / "rocket_raylib_adapter.h"
    native_module = root / "stdlib" / "rocket" / "raylib" / "native.rocket"
    for path in (adapter, header, native_module):
        require(path.is_file(), f"missing production runtime file: {path}")

    showcase_native = root / "examples" / "raylib_showcase" / "native"
    require(
        not showcase_native.exists() or not any(showcase_native.rglob("*")),
        "raylib showcase still owns native runtime files",
    )
    require(
        not (root / "examples" / "raylib_showcase" / "src" / "rocket_raylib_adapter.rocket").exists(),
        "raylib showcase still owns a generated native binding",
    )

    duplicate_declarations: list[str] = []
    for source in root.rglob("*.rocket"):
        if source == native_module or "out" in source.relative_to(root).parts:
            continue
        if "extern fn rlv_" in source.read_text(encoding="utf-8"):
            duplicate_declarations.append(source.relative_to(root).as_posix())
    require(
        not duplicate_declarations,
        f"Raylib host symbols are declared outside the canonical native module: {duplicate_declarations}",
    )

    forbidden_import = "examples.raylib_showcase.src"
    forbidden_path = "examples/raylib_showcase/src"
    supported_roots = (
        root / "stdlib",
        root / "examples" / "rocket3_graphics_ui",
        root / "tests" / "fixtures",
    )
    violations: list[str] = []
    for supported in supported_roots:
        for source in supported.rglob("*.rocket"):
            text = source.read_text(encoding="utf-8")
            if forbidden_import in text or forbidden_path in text:
                violations.append(source.relative_to(root).as_posix())
    require(not violations, f"supported sources import showcase code: {violations}")

    top_cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    require(
        "src/raylib/rocket_raylib_adapter.cpp" in top_cmake,
        "top-level build does not compile the production-owned adapter",
    )
    require(
        "examples/raylib_showcase/native/rocket_raylib_adapter.cpp" not in top_cmake,
        "top-level build still compiles an example-owned adapter",
    )
    example_cmake = (
        root / "examples" / "raylib_showcase" / "CMakeLists.txt"
    ).read_text(encoding="utf-8")
    require(
        "add_library(rocket_raylib_adapter" not in example_cmake,
        "showcase independently compiles the production adapter",
    )

    compatibility = (
        root / "examples" / "raylib_showcase" / "src" / "rocket_raylib.rocket"
    ).read_text(encoding="utf-8")
    require(
        "import rocket.raylib.native" in compatibility
        and "Deprecated Rocket 1.4 compatibility surface" in compatibility,
        "legacy wrapper is not an explicit compatibility client of the production module",
    )

    package_source = (
        root / "scripts" / "phase19_package.py"
    ).read_text(encoding="utf-8")
    for marker in (
        "install_game_runtime",
        "rocket_raylib_adapter.lib",
        "librocket_raylib_adapter.a",
        "RAYLIB-LICENSE.txt",
        "rocket35_game_runtime_package",
    ):
        require(marker in package_source, f"release package wiring omits {marker}")

    print("Rocket 3.5 WP1 runtime ownership policy passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
