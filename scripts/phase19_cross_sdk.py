#!/usr/bin/env python3
"""Assemble a host-executable Rocket cross SDK from explicit target inputs."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import shutil
import stat
import sys
from pathlib import Path, PurePosixPath


ROOT = Path(__file__).resolve().parents[1]
PHASE19_ROOT = (ROOT / "out" / "phase19").resolve()
TARGETS = {
    "windows-x64": ("x86_64-pc-windows-msvc", ".lib"),
    "linux-x64": ("x86_64-unknown-linux-gnu", ".a"),
    "linux-arm64": ("aarch64-unknown-linux-gnu", ".a"),
    "macos-arm64": ("arm64-apple-macosx", ".a"),
}
CROSS_PATHS = {
    ("windows-x64", "linux-x64"),
    ("windows-x64", "linux-arm64"),
    ("linux-x64", "linux-arm64"),
    ("linux-x64", "windows-x64"),
}


class CrossSdkFailure(RuntimeError):
    pass


def host_alias() -> str:
    system = platform.system().lower()
    machine = platform.machine().lower()
    if system == "windows" and machine in {"amd64", "x86_64"}:
        return "windows-x64"
    if system == "linux" and machine in {"amd64", "x86_64"}:
        return "linux-x64"
    if system == "linux" and machine in {"aarch64", "arm64"}:
        return "linux-arm64"
    if system == "darwin" and machine in {"aarch64", "arm64"}:
        return "macos-arm64"
    raise CrossSdkFailure(f"unsupported cross-SDK host: {system}-{machine}")


def within(child: Path, parent: Path) -> bool:
    try:
        child.resolve().relative_to(parent.resolve())
        return True
    except ValueError:
        return False


def replace_directory(path: Path) -> None:
    resolved = path.resolve()
    if resolved == PHASE19_ROOT or not within(resolved, PHASE19_ROOT):
        raise CrossSdkFailure(f"refusing to replace non-Phase-19 path: {resolved}")
    if resolved.exists():
        shutil.rmtree(resolved)
    resolved.mkdir(parents=True)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def require_file(path: Path, description: str) -> Path:
    resolved = path.resolve()
    if not resolved.is_file():
        raise CrossSdkFailure(f"missing {description}: {resolved}")
    return resolved


def copy_file(source: Path, destination: Path, executable: bool = False) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    if executable and os.name != "nt":
        destination.chmod(destination.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def copy_host_llvm(llvm_root: Path, target: str, destination: Path) -> list[str]:
    suffix = ".exe" if os.name == "nt" else ""
    librarian = "llvm-lib" if target == "windows-x64" else "llvm-ar"
    linker = "lld-link" if target == "windows-x64" else "ld.lld"
    tools = ["clang", librarian, linker]
    copied: list[str] = []
    for tool in tools:
        source = require_file(llvm_root / "bin" / f"{tool}{suffix}", f"host LLVM {tool}")
        destination_path = destination / "bin" / source.name
        copy_file(source, destination_path, executable=True)
        copied.append(destination_path.relative_to(destination).as_posix())

    for source in sorted((llvm_root / "bin").glob("*.dll")):
        destination_path = destination / "bin" / source.name
        copy_file(source, destination_path)
        copied.append(destination_path.relative_to(destination).as_posix())
    llvm_library = llvm_root / "lib"
    if llvm_library.is_dir():
        for source in sorted(llvm_library.iterdir()):
            if not source.is_file() and not source.is_symlink():
                continue
            if not (source.name.endswith(".dylib") or ".so" in source.name):
                continue
            destination_path = destination / "lib" / source.name
            copy_file(source.resolve(), destination_path)
            copied.append(destination_path.relative_to(destination).as_posix())
    clang_resources = llvm_library / "clang"
    if clang_resources.is_dir():
        shutil.copytree(clang_resources, destination / "lib" / "clang", symlinks=False)
        copied.append("lib/clang")
    return copied


def copy_linux_sysroot(source: Path, destination: Path) -> None:
    if not source.is_dir():
        raise CrossSdkFailure(f"missing Linux sysroot: {source}")
    shutil.copytree(source, destination, symlinks=False)
    required = [destination / "usr" / "include", destination / "usr" / "lib"]
    if not all(path.is_dir() for path in required):
        raise CrossSdkFailure(
            "Linux sysroot must contain usr/include and usr/lib"
        )


def all_regular_files(root: Path) -> list[Path]:
    return sorted(
        (path for path in root.rglob("*") if path.is_file()),
        key=lambda path: path.as_posix(),
    )


def write_checksums(sdk: Path) -> int:
    manifest = sdk / "SHA256SUMS.txt"
    lines = []
    for path in all_regular_files(sdk):
        if path == manifest:
            continue
        relative = path.relative_to(sdk).as_posix()
        lines.append(f"{sha256(path)}  {relative}")
    manifest.write_text("\n".join(lines) + "\n", encoding="ascii", newline="\n")
    return len(lines)


def verify_checksums(sdk: Path) -> int:
    manifest = require_file(sdk / "SHA256SUMS.txt", "cross-SDK checksum manifest")
    entries: set[str] = set()
    for line in manifest.read_text(encoding="ascii").splitlines():
        digest, separator, relative = line.partition("  ")
        pure = PurePosixPath(relative)
        if (
            len(digest) != 64 or any(value not in "0123456789abcdef" for value in digest)
            or separator != "  " or pure.is_absolute() or ".." in pure.parts
            or relative in entries
        ):
            raise CrossSdkFailure(f"invalid cross-SDK checksum line: {line!r}")
        path = sdk.joinpath(*pure.parts)
        if not path.is_file() or sha256(path) != digest:
            raise CrossSdkFailure(f"cross-SDK checksum mismatch: {relative}")
        entries.add(relative)
    expected = {
        path.relative_to(sdk).as_posix()
        for path in all_regular_files(sdk) if path != manifest
    }
    if entries != expected:
        raise CrossSdkFailure("cross-SDK checksum coverage differs from its tree")
    return len(entries)


def assemble(arguments: argparse.Namespace) -> dict[str, object]:
    host = arguments.host or host_alias()
    target = arguments.target
    if host != host_alias():
        raise CrossSdkFailure(
            f"declared cross-SDK host {host} does not match native host {host_alias()}"
        )
    if (host, target) not in CROSS_PATHS:
        raise CrossSdkFailure(f"unsupported Rocket cross path: {host} -> {target}")
    output = arguments.output.resolve()
    replace_directory(output)
    for directory in ("bin", "lib", "share/rocket"):
        (output / directory).mkdir(parents=True, exist_ok=True)

    llvm_root = arguments.host_llvm_root.resolve()
    runtime = require_file(arguments.target_runtime, "target Rocket runtime")
    triple, runtime_suffix = TARGETS[target]
    runtime_destination = output / "lib" / f"rocket_runtime{runtime_suffix}"
    copy_file(runtime, runtime_destination)
    llvm_files = copy_host_llvm(llvm_root, target, output)

    if target.startswith("linux-"):
        if arguments.sysroot is None:
            raise CrossSdkFailure("Linux cross SDK requires --sysroot")
        copy_linux_sysroot(arguments.sysroot.resolve(), output / "sysroot")
    elif target == "windows-x64":
        if len(arguments.windows_library_directory) != 3:
            raise CrossSdkFailure(
                "Windows cross SDK requires exactly three --windows-library-directory values"
            )
        for name, source_name in zip(
            ("msvc", "ucrt", "um"), arguments.windows_library_directory,
            strict=True,
        ):
            source = Path(source_name).resolve()
            libraries = sorted(source.glob("*.lib"))
            if not libraries:
                raise CrossSdkFailure(f"no Windows libraries found in {source}")
            destination = output / "lib" / name
            destination.mkdir()
            for library in libraries:
                copy_file(library, destination / library.name)

    (output / "share" / "rocket" / "target.txt").write_text(
        f"rocket-target-sdk-1\nalias={target}\ntriple={triple}\n",
        encoding="ascii", newline="\n",
    )
    report: dict[str, object] = {
        "schema": "rocket-cross-sdk-report-1",
        "version": "2.1.0",
        "host": host,
        "target": target,
        "triple": triple,
        "runtime_sha256": sha256(runtime_destination),
        "host_llvm_tools": llvm_files,
        "passed": True,
    }
    (output / "CROSS_SDK.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    report["checksum_files"] = write_checksums(output)
    verify_checksums(output)
    print(f"Rocket cross SDK assembled: {host} -> {target} ({report['checksum_files']} files)")
    return report


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", choices=tuple(TARGETS))
    parser.add_argument("--target", required=True, choices=tuple(TARGETS))
    parser.add_argument("--host-llvm-root", required=True, type=Path)
    parser.add_argument("--target-runtime", required=True, type=Path)
    parser.add_argument("--sysroot", type=Path)
    parser.add_argument("--windows-library-directory", action="append", default=[])
    parser.add_argument("--output", required=True, type=Path)
    return parser.parse_args()


def main() -> int:
    assemble(parse_arguments())
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except CrossSdkFailure as error:
        print(f"phase19-cross-sdk: error: {error}", file=sys.stderr)
        raise SystemExit(1)
