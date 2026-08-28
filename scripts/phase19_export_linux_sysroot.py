#!/usr/bin/env python3
"""Export the native Linux development ABI as an explicit Phase 19 sysroot."""

from __future__ import annotations

import argparse
import json
import platform
import shutil
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PHASE19_ROOT = (ROOT / "out" / "phase19").resolve()
LINUX_TARGETS = {
    "linux-x64": "x86_64-linux-gnu",
    "linux-arm64": "aarch64-linux-gnu",
}
LINUX_ARM64_LOADER = Path("lib/ld-linux-aarch64.so.1")


class SysrootFailure(RuntimeError):
    pass


def host_alias() -> str:
    if platform.system().lower() != "linux":
        raise SysrootFailure("Linux sysroots must be exported on a native Linux host")
    machine = platform.machine().lower()
    if machine in {"amd64", "x86_64"}:
        return "linux-x64"
    if machine in {"aarch64", "arm64"}:
        return "linux-arm64"
    raise SysrootFailure(f"unsupported Linux architecture: {machine}")


def within(child: Path, parent: Path) -> bool:
    try:
        child.resolve().relative_to(parent.resolve())
        return True
    except ValueError:
        return False


def replace_directory(path: Path) -> None:
    resolved = path.resolve()
    if resolved == PHASE19_ROOT or not within(resolved, PHASE19_ROOT):
        raise SysrootFailure(f"refusing to replace non-Phase-19 path: {resolved}")
    if resolved.exists():
        shutil.rmtree(resolved)
    resolved.mkdir(parents=True)


def copy_path(source_root: Path, relative: Path, output: Path) -> bool:
    source = source_root / relative
    if not source.exists():
        return False
    destination = output / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    if source.is_dir():
        shutil.copytree(source, destination, symlinks=False)
    else:
        shutil.copy2(source.resolve(), destination)
    return True


def export_sysroot(target: str, output: Path, source_root: Path = Path("/")) -> dict[str, object]:
    detected = host_alias()
    if detected != target:
        raise SysrootFailure(
            f"sysroot target {target} does not match native host {detected}"
        )
    multiarch = LINUX_TARGETS[target]
    output = output.resolve()
    replace_directory(output)
    roots = [
        Path("usr/include"),
        Path("usr/lib") / multiarch,
        Path("lib") / multiarch,
        Path("usr/lib/gcc") / multiarch,
    ]
    if target == "linux-x64":
        roots.append(Path("lib64"))
    else:
        roots.append(LINUX_ARM64_LOADER)
    copied = [relative.as_posix() for relative in roots if copy_path(source_root, relative, output)]
    required = [
        output / "usr" / "include" / "stdio.h",
        output / "usr" / "lib" / multiarch / "crt1.o",
        output / "usr" / "lib" / multiarch / "libc.so",
    ]
    if target == "linux-arm64":
        required.append(output / LINUX_ARM64_LOADER)
    if not all(path.exists() for path in required):
        missing = [str(path.relative_to(output)) for path in required if not path.exists()]
        raise SysrootFailure(
            "native Linux development ABI is incomplete; missing " + ", ".join(missing)
        )
    report: dict[str, object] = {
        "schema": "rocket-linux-sysroot-report-1",
        "target": target,
        "multiarch": multiarch,
        "copied_roots": copied,
        "passed": True,
    }
    (output / "ROCKET_SYSROOT.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(f"Rocket Linux sysroot exported for {target}: {', '.join(copied)}")
    return report


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", required=True, choices=tuple(LINUX_TARGETS))
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--source-root", type=Path, default=Path("/"))
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    export_sysroot(arguments.target, arguments.output, arguments.source_root.resolve())
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SysrootFailure as error:
        print(f"phase19-linux-sysroot: error: {error}", file=sys.stderr)
        raise SystemExit(1)
