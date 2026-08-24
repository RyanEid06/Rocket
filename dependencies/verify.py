#!/usr/bin/env python3
"""Verify Rocket's installed native developer dependencies."""

from __future__ import annotations

import json
import re
import shutil
import subprocess
from pathlib import Path

from bootstrap import host_alias


ROOT = Path(__file__).resolve().parent


def command(path: Path, *arguments: str) -> str:
    result = subprocess.run(
        [str(path), *arguments], text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, check=False
    )
    if result.returncode != 0:
        raise SystemExit(f"dependency check failed ({result.returncode}): {path}\n{result.stdout}")
    return result.stdout.splitlines()[0] if result.stdout else "ok"


def main() -> int:
    manifest = json.loads((ROOT / "manifest.json").read_text(encoding="utf-8"))
    selected = host_alias()
    packages = manifest["platforms"][selected]
    installed = ROOT / "installed"
    suffix = ".exe" if selected == "windows-x64" else ""
    llvm = installed / packages["llvm"]["installDirectory"]
    ninja = installed / packages["ninja"]["installDirectory"]
    raylib = installed / manifest["shared"]["raylib"]["installDirectory"]
    checks = {
        "ninja": (ninja / ("ninja" + suffix), "--version"),
        "clang": (llvm / "bin" / ("clang" + suffix), "--version"),
        "llvm-config": (llvm / "bin" / ("llvm-config" + suffix), "--version"),
    }
    for name, (path, argument) in checks.items():
        if not path.is_file():
            raise SystemExit(f"missing installed dependency: {path}")
        print(f"{name:12} {command(path, argument)}")
    cmake = shutil.which("cmake")
    git = shutil.which("git")
    if not cmake or not git:
        raise SystemExit("CMake and Git must be installed")
    print(f"{'cmake':12} {command(Path(cmake), '--version')}")
    print(f"{'git':12} {command(Path(git), '--version')}")
    header = raylib / "src" / "raylib.h"
    source = header.read_text(encoding="utf-8") if header.is_file() else ""
    version = manifest["shared"]["raylib"]["version"].split(".")
    if not re.search(rf"^#define RAYLIB_VERSION_MAJOR\s+{re.escape(version[0])}$", source, re.M):
        raise SystemExit(f"installed raylib does not match {'.'.join(version)}")
    print(f"{'raylib':12} {'.'.join(version)}")
    print(f"toolchain verification passed for {selected}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
