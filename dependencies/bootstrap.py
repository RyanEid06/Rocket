#!/usr/bin/env python3
"""Install Rocket's checksum-pinned native developer dependencies."""

from __future__ import annotations

import argparse
import hashlib
import json
import platform
import shutil
import tarfile
import tempfile
import urllib.error
import urllib.request
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent
MANIFEST = ROOT / "manifest.json"
CACHE = ROOT / "cache"
INSTALLED = ROOT / "installed"


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
    raise SystemExit(f"unsupported Rocket developer host: {system}-{machine}")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def valid_archive(path: Path, package: dict[str, object]) -> bool:
    return (
        path.is_file()
        and path.stat().st_size == int(package["size"])
        and sha256(path) == package["sha256"]
    )


def download(package: dict[str, object]) -> Path:
    CACHE.mkdir(parents=True, exist_ok=True)
    destination = CACHE / str(package["archive"])
    if valid_archive(destination, package):
        print(f"verified cached {destination.name}")
        return destination
    if destination.exists() and destination.stat().st_size > int(package["size"]):
        destination.unlink()
    existing = destination.stat().st_size if destination.exists() else 0
    headers = {"User-Agent": "Rocket-Phase19-bootstrap/1"}
    if existing:
        headers["Range"] = f"bytes={existing}-"
    request = urllib.request.Request(str(package["url"]), headers=headers)
    try:
        response = urllib.request.urlopen(request, timeout=120)
    except urllib.error.HTTPError as error:
        if existing and error.code == 416:
            destination.unlink(missing_ok=True)
            return download(package)
        raise
    append = existing > 0 and getattr(response, "status", 200) == 206
    mode = "ab" if append else "wb"
    if existing and not append:
        existing = 0
    print(f"downloading {package['archive']} from byte {existing}")
    with response, destination.open(mode) as output:
        shutil.copyfileobj(response, output, length=1024 * 1024)
    if not valid_archive(destination, package):
        actual_size = destination.stat().st_size if destination.exists() else 0
        actual_hash = sha256(destination) if destination.exists() else "missing"
        raise SystemExit(
            f"checksum failure for {destination.name}: size {actual_size}, "
            f"sha256 {actual_hash}"
        )
    return destination


def extract_single_root(archive: Path, destination: Path) -> None:
    INSTALLED.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=".extract-", dir=INSTALLED))
    try:
        if archive.suffix == ".zip":
            with zipfile.ZipFile(archive) as source:
                source.extractall(temporary)
        else:
            with tarfile.open(archive, "r:*") as source:
                source.extractall(temporary, filter="data")
        entries = sorted(temporary.iterdir())
        source_root = entries[0] if len(entries) == 1 and entries[0].is_dir() else temporary
        if destination.exists():
            shutil.rmtree(destination)
        if source_root == temporary:
            destination.mkdir(parents=True)
            for entry in entries:
                shutil.move(str(entry), destination / entry.name)
        else:
            shutil.move(str(source_root), destination)
    finally:
        shutil.rmtree(temporary, ignore_errors=True)


def install(package: dict[str, object], marker: str) -> None:
    destination = INSTALLED / str(package["installDirectory"])
    marker_path = destination / marker
    if marker_path.is_file():
        ensure_executable_marker(marker_path, marker)
        print(f"already installed {destination.name}")
        return
    extract_single_root(download(package), destination)
    marker_path = destination / marker
    if not marker_path.is_file():
        raise SystemExit(f"archive did not install expected {marker}")
    ensure_executable_marker(marker_path, marker)


def ensure_executable_marker(marker_path: Path, marker: str) -> None:
    """Restore the POSIX execute bit lost by Python's ZIP extraction.

    Ninja's Windows archive contains an `.exe`, while the Linux and macOS
    archives contain a bare `ninja` executable. `zipfile.extractall` does not
    preserve Unix mode bits, so restored Actions caches repair the marker before
    any workflow attempts to execute it.
    """
    if marker != "ninja":
        return
    marker_path.chmod(marker_path.stat().st_mode | 0o111)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", choices=("windows-x64", "linux-x64", "linux-arm64", "macos-arm64"))
    parser.add_argument("--verify-only", action="store_true")
    arguments = parser.parse_args()
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if manifest.get("schemaVersion") != 2:
        raise SystemExit("dependencies/manifest.json must use schema version 2")
    selected = arguments.host or host_alias()
    packages = manifest["platforms"][selected]
    if arguments.verify_only:
        failures = []
        for package in (packages["llvm"], packages["ninja"], manifest["shared"]["raylib"]):
            archive = CACHE / package["archive"]
            if archive.exists() and not valid_archive(archive, package):
                failures.append(archive.name)
        if failures:
            raise SystemExit("invalid cached archives: " + ", ".join(failures))
        print(f"cached dependency verification passed for {selected}")
        return 0
    executable = ".exe" if selected == "windows-x64" else ""
    install(packages["ninja"], "ninja" + executable)
    install(packages["llvm"], "bin/llvm-config" + executable)
    install(manifest["shared"]["raylib"], "src/raylib.h")
    print(f"pinned dependencies installed for {selected}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
