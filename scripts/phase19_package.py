#!/usr/bin/env python3
"""Build and verify a relocatable, reproducible native Rocket 2.1 SDK."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import lzma
import os
import platform
import re
import shutil
import stat
import subprocess
import sys
import tarfile
import tempfile
import time
import zipfile
from pathlib import Path, PurePosixPath


ROOT = Path(__file__).resolve().parents[1]
PHASE19_ROOT = (ROOT / "out" / "phase19").resolve()
VERSION = "2.1.0"
TARGETS = {
    "windows-x64": ("x86_64-pc-windows-msvc", ".exe", ".lib"),
    "linux-x64": ("x86_64-unknown-linux-gnu", "", ".a"),
    "linux-arm64": ("aarch64-unknown-linux-gnu", "", ".a"),
    "macos-arm64": ("arm64-apple-macosx", "", ".a"),
}


class PackageFailure(RuntimeError):
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
    raise PackageFailure(f"unsupported native package host: {system}-{machine}")


def within(child: Path, parent: Path) -> bool:
    try:
        child.resolve().relative_to(parent.resolve())
        return True
    except ValueError:
        return False


def replace_directory(path: Path) -> None:
    resolved = path.resolve()
    if resolved == PHASE19_ROOT or not within(resolved, PHASE19_ROOT):
        raise PackageFailure(f"refusing to replace non-Phase-19 path: {resolved}")
    if resolved.exists():
        shutil.rmtree(resolved)
    resolved.mkdir(parents=True)


def remove_file(path: Path) -> None:
    resolved = path.resolve()
    if not within(resolved, PHASE19_ROOT):
        raise PackageFailure(f"refusing to replace non-Phase-19 file: {resolved}")
    resolved.unlink(missing_ok=True)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def require_file(path: Path, description: str) -> Path:
    resolved = path.resolve()
    if not resolved.is_file():
        raise PackageFailure(f"missing {description}: {resolved}")
    return resolved


def run(
    command: list[str | Path], *, env: dict[str, str] | None = None,
    cwd: Path = ROOT, expected: int = 0, pattern: str | None = None
) -> str:
    native = [str(value) for value in command]
    print("+ " + subprocess.list2cmdline(native), flush=True)
    result = subprocess.run(
        native, cwd=cwd, env=env, text=True, errors="replace",
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False
    )
    if result.stdout:
        print(result.stdout, end="" if result.stdout.endswith("\n") else "\n")
    if result.returncode != expected:
        raise PackageFailure(
            f"command returned {result.returncode}, expected {expected}: {native[0]}"
        )
    if pattern and re.search(pattern, result.stdout, re.MULTILINE) is None:
        raise PackageFailure(f"command output did not match {pattern!r}: {native[0]}")
    return result.stdout


def copy_tree(source: Path, destination: Path) -> None:
    if destination.exists():
        shutil.rmtree(destination)
    shutil.copytree(source, destination, symlinks=False)


def copy_library_chain(source_name: str | Path, destination: Path) -> list[str]:
    """Copy every linker/SONAME spelling in a shared-library symlink chain."""
    source = Path(source_name).absolute()
    if not source.is_file():
        raise PackageFailure(f"missing runtime shared library: {source}")
    names: list[str] = []
    current = source
    visited: set[Path] = set()
    while current.is_symlink():
        normalized = current.absolute()
        if normalized in visited:
            raise PackageFailure(f"runtime shared-library symlink cycle: {source}")
        visited.add(normalized)
        names.append(current.name)
        link = Path(os.readlink(current))
        current = link if link.is_absolute() else current.parent / link
    current = current.resolve()
    if not current.is_file():
        raise PackageFailure(f"broken runtime shared-library chain: {source}")
    names.append(current.name)
    copied: list[str] = []
    for name in dict.fromkeys(names):
        target = destination / name
        if target.exists():
            if sha256(target) != sha256(current):
                raise PackageFailure(
                    f"conflicting runtime shared-library basename: {name}"
                )
        else:
            shutil.copy2(current, target)
        copied.append(name)
    return copied


def capture(command: list[str | Path], *, expected: int = 0) -> str:
    native = [str(value) for value in command]
    result = subprocess.run(
        native, cwd=ROOT, text=True, errors="replace",
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
    )
    if result.returncode != expected:
        detail = result.stderr.strip() or result.stdout.strip()
        raise PackageFailure(
            f"command returned {result.returncode}, expected {expected}: "
            f"{native[0]}: {detail}"
        )
    return result.stdout


def macos_dependencies(path: Path) -> list[str]:
    output = capture(["/usr/bin/otool", "-L", path])
    dependencies: list[str] = []
    for line in output.splitlines()[1:]:
        value = line.strip().split(" (compatibility version", 1)[0]
        if value:
            dependencies.append(value)
    return dependencies


def macos_rpaths(path: Path) -> list[str]:
    output = capture(["/usr/bin/otool", "-l", path])
    lines = output.splitlines()
    result: list[str] = []
    for index, line in enumerate(lines):
        if line.strip() != "cmd LC_RPATH":
            continue
        for detail in lines[index + 1:index + 5]:
            value = detail.strip()
            if value.startswith("path ") and " (offset " in value:
                result.append(value[5:].split(" (offset ", 1)[0])
                break
    return result


def macos_system_library(value: str) -> bool:
    return value.startswith("/usr/lib/") or value.startswith("/System/Library/")


def macos_cxx_runtime_library(name: str) -> bool:
    """Return whether a dylib belongs to Apple's system C++ ABI stack."""
    return (
        name == "libc++.dylib"
        or name.startswith("libc++.")
        or name == "libc++abi.dylib"
        or name.startswith("libc++abi.")
    )


def macos_mach_o(path: Path) -> bool:
    """Recognize thin and universal Mach-O files without invoking otool."""
    try:
        with path.open("rb") as handle:
            magic = handle.read(4)
    except OSError:
        return False
    return magic in {
        b"\xfe\xed\xfa\xce", b"\xce\xfa\xed\xfe",
        b"\xfe\xed\xfa\xcf", b"\xcf\xfa\xed\xfe",
        b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca",
        b"\xca\xfe\xba\xbf", b"\xbf\xba\xfe\xca",
    }


def bundle_macos_dependencies(package: Path, seeds: list[Path]) -> list[str]:
    """Bundle and rewrite non-Apple dylibs needed by the shipped executables."""
    library_directory = package / "lib"
    queue = list(seeds)
    visited: set[Path] = set()
    copied: set[str] = set()
    while queue:
        source = queue.pop(0).resolve()
        if source in visited:
            continue
        visited.add(source)
        # POSIX packages also contain launcher shell scripts.  They are valid
        # queue seeds, but only Mach-O objects should ever be passed to otool.
        if not macos_mach_o(source):
            continue
        dependencies = macos_dependencies(source)
        for dependency in dependencies:
            if macos_system_library(dependency):
                continue
            if dependency.startswith("@loader_path/"):
                candidate = source.parent / dependency.removeprefix("@loader_path/")
            elif dependency.startswith("@executable_path/"):
                continue
            elif dependency.startswith("@rpath/"):
                candidate = source.parent / Path(dependency).name
                if not candidate.is_file():
                    continue
            else:
                candidate = Path(dependency)
            if not candidate.is_file():
                raise PackageFailure(
                    f"could not resolve macOS runtime dependency {dependency!r} "
                    f"required by {source}"
                )
            for name in copy_library_chain(candidate, library_directory):
                copied.add(name)
            queue.append(candidate)

    candidates = [
        path for directory in (package / "bin", package / "stage0", package / "lib")
        for path in directory.iterdir() if path.is_file()
    ]
    for path in sorted(candidates):
        if not macos_mach_o(path):
            continue
        dependencies = macos_dependencies(path)
        if path.parent == library_directory and path.suffix == ".dylib":
            capture(["/usr/bin/install_name_tool", "-id", f"@rpath/{path.name}", path])
        for dependency in dependencies:
            basename = Path(dependency).name
            if basename in copied or (library_directory / basename).is_file():
                capture([
                    "/usr/bin/install_name_tool", "-change", dependency,
                    f"@rpath/{basename}", path,
                ])
        rpath = "@loader_path" if path.parent == library_directory else "@loader_path/../lib"
        existing_rpaths = macos_rpaths(path)
        # Build and bootstrap products may contain absolute paths into the
        # dependency cache or Homebrew.  They are valid while assembling the
        # SDK but must never survive in a relocatable package.
        for existing in existing_rpaths:
            if Path(existing).is_absolute():
                capture(["/usr/bin/install_name_tool", "-delete_rpath", existing, path])
        if rpath not in existing_rpaths:
            capture(["/usr/bin/install_name_tool", "-add_rpath", rpath, path])
        # install_name_tool invalidates any existing signature.  Ad-hoc sign
        # each rewritten Mach-O so macOS will execute the relocated binaries.
        capture([
            "/usr/bin/codesign", "--force", "--sign", "-",
            "--timestamp=none", path,
        ])
    return sorted(copied)


def wrapper(name: str, real_name: str) -> str:
    return f"""#!/bin/sh
rocket_tool_directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
rocket_sdk_root=$(CDPATH= cd -- "$rocket_tool_directory/.." && pwd)
if [ "$(uname -s)" = "Darwin" ]; then
    DYLD_LIBRARY_PATH="$rocket_sdk_root/lib${{DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}}"
    export DYLD_LIBRARY_PATH
else
    LD_LIBRARY_PATH="$rocket_sdk_root/lib${{LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}}"
    export LD_LIBRARY_PATH
fi
# Supply only SDK-relative defaults.  A caller may still explicitly override
# any of these tools, while an ordinary relocated SDK never depends on a
# developer-shell toolchain discovery path.
: "${{ROCKET_CLANG:=$rocket_sdk_root/bin/clang}}"
: "${{ROCKET_LIBRARIAN:=$rocket_sdk_root/bin/llvm-ar}}"
: "${{ROCKET_RUNTIME:=$rocket_sdk_root/lib/rocket_runtime.a}}"
export ROCKET_CLANG ROCKET_LIBRARIAN ROCKET_RUNTIME
exec "$rocket_tool_directory/{real_name}" "$@"
"""


def install_host_program(source: Path, destination: Path, windows: bool) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if windows:
        shutil.copy2(source, destination)
        return
    real = destination.with_name(destination.name + ".bin")
    shutil.copy2(source, real)
    real.chmod(0o755)
    destination.write_text(wrapper(destination.name, real.name), encoding="utf-8", newline="\n")
    destination.chmod(0o755)


def copy_llvm(arguments: argparse.Namespace, package: Path) -> list[str]:
    llvm = arguments.llvm_root.resolve()
    suffix = ".exe" if arguments.target == "windows-x64" else ""
    required = ["clang", "llvm-lib" if suffix else "llvm-ar"]
    if arguments.target == "windows-x64":
        required.extend(["lld-link", "llvm-dwarfdump", "llvm-objdump"])
    elif arguments.target.startswith("linux-"):
        required.extend(["ld.lld", "lld", "llvm-dwarfdump", "llvm-objdump"])
    else:
        required.extend(["ld64.lld", "lld", "llvm-dwarfdump", "llvm-objdump"])
    copied: list[str] = []
    for name in required:
        installed_name = f"{name}{suffix}"
        source = require_file(llvm / "bin" / installed_name, f"LLVM tool {name}")
        # require_file resolves symlinks so that the copied bytes are regular,
        # but the installed filename must remain the requested driver alias.
        # In official POSIX archives ld.lld/ld64.lld may point at lld; losing
        # that alias makes Clang's -fuse-ld=lld discovery fail after relocation.
        destination = package / "bin" / installed_name
        shutil.copy2(source, destination)
        if not suffix:
            destination.chmod(0o755)
        copied.append(destination.name)
    for source in sorted((llvm / "bin").glob("*.cfg")):
        destination = package / "bin" / source.name
        shutil.copy2(source, destination)
        copied.append(destination.name)
    clang_header = next((llvm / "lib" / "clang").glob("*/include/stddef.h"), None)
    if clang_header is None:
        raise PackageFailure(f"missing Clang resource headers under {llvm}")
    clang_resources = require_file(clang_header, "Clang resource headers").parents[1]
    target_resource = package / "lib" / "clang" / clang_resources.name
    copy_tree(clang_resources, target_resource)
    if not suffix:
        for source in sorted((llvm / "lib").iterdir()):
            name = source.name
            if not source.is_file() and not source.is_symlink():
                continue
            if not (name.endswith(".dylib") or ".so" in name):
                continue
            if (arguments.target == "macos-arm64"
                    and macos_cxx_runtime_library(name)):
                # Xcode's libc++ headers and runtime are one matched ABI unit.
                # Shipping LLVM's private libc++ beside an Xcode-ABI binary
                # lets DYLD_LIBRARY_PATH substitute an incompatible runtime.
                continue
            destination = package / "lib" / name
            shutil.copy2(source.resolve(), destination)
    license_source = next(
        (candidate for candidate in (llvm / "LICENSE.TXT", llvm / "LICENSE.txt") if candidate.is_file()),
        None,
    )
    if license_source:
        shutil.copy2(license_source, package / "licenses" / "LLVM-LICENSE.txt")
    return copied


def git_value(*arguments: str) -> str:
    result = subprocess.run(
        ["git", "-C", str(ROOT), *arguments],
        cwd=ROOT,
        text=True,
        errors="replace",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip()
        raise PackageFailure(f"git {' '.join(arguments)} failed: {detail}")
    return result.stdout.strip()


def package_tree(arguments: argparse.Namespace) -> tuple[Path, dict[str, object]]:
    detected = host_alias()
    if detected != arguments.target:
        raise PackageFailure(
            f"native package target {arguments.target} does not match host {detected}"
        )
    triple, executable_suffix, runtime_suffix = TARGETS[arguments.target]
    build = arguments.build_dir.resolve()
    bootstrap = arguments.bootstrap_dir.resolve()
    package_parent = arguments.output_parent.resolve()
    if not within(package_parent, PHASE19_ROOT):
        raise PackageFailure(f"output parent must be under {PHASE19_ROOT}")
    package_parent.mkdir(parents=True, exist_ok=True)
    package = package_parent / f"rocket-{VERSION}-{arguments.target}"
    replace_directory(package)
    for directory in ("bin", "lib", "stage0", "share/rocket", "tools", "licenses"):
        (package / directory).mkdir(parents=True, exist_ok=True)

    stage0 = require_file(build / f"rocketc{executable_suffix}", "stage0 compiler")
    language_server = require_file(
        build / f"rocket-lsp{executable_suffix}", "language server"
    )
    runtime_name = (
        f"rocket_runtime{runtime_suffix}"
        if arguments.target == "windows-x64"
        else f"librocket_runtime{runtime_suffix}"
    )
    runtime = require_file(build / runtime_name, "runtime library")
    stage3 = require_file(
        bootstrap / f"stage3{executable_suffix}", "stage3 compiler"
    )
    bootstrap_report = require_file(
        bootstrap / "bootstrap-report.json", "bootstrap report"
    )
    parsed_bootstrap = json.loads(bootstrap_report.read_text(encoding="utf-8"))
    if not parsed_bootstrap.get("passed") or parsed_bootstrap.get("target") != arguments.target:
        raise PackageFailure("bootstrap report is not a passing report for this target")

    windows = arguments.target == "windows-x64"
    bundled_runtime_libraries: list[str] = []
    install_host_program(stage3, package / "bin" / f"rocketc{executable_suffix}", windows)
    install_host_program(
        language_server, package / "bin" / f"rocket-lsp{executable_suffix}", windows
    )
    install_host_program(
        stage0, package / "stage0" / f"rocketc-stage0{executable_suffix}", windows
    )
    # CMake prefixes POSIX static-library build products with "lib", whereas
    # an installed Rocket target SDK has one canonical runtime spelling across
    # all platforms.  Toolchain discovery and cross-SDK assembly both use this
    # installed name.
    packaged_runtime = package / "lib" / f"rocket_runtime{runtime_suffix}"
    shutil.copy2(runtime, packaged_runtime)
    llvm_tools = copy_llvm(arguments, package)

    if windows:
        if len(arguments.windows_library_directory) != 3:
            raise PackageFailure(
                "Windows packages require exactly three --windows-library-directory values"
            )
        for name, source_name in zip(
            ("msvc", "ucrt", "um"), arguments.windows_library_directory, strict=True
        ):
            source = Path(source_name).resolve()
            libraries = sorted(source.glob("*.lib"))
            if not libraries:
                raise PackageFailure(f"no native libraries found in {source}")
            destination = package / "lib" / name
            destination.mkdir()
            for library in libraries:
                shutil.copy2(library, destination / library.name)
    else:
        runtime_seeds: list[Path] = []
        for source_name in arguments.runtime_shared_library:
            source = Path(source_name).absolute()
            bundled_runtime_libraries.extend(
                copy_library_chain(source, package / "lib")
            )
            runtime_seeds.append(source)
        if arguments.target == "macos-arm64":
            if not runtime_seeds:
                raise PackageFailure(
                    "macOS packages require explicit --runtime-shared-library "
                    "inputs for non-Apple runtime dependencies"
                )
            runtime_seeds.extend(
                path for directory in (package / "bin", package / "stage0")
                for path in directory.iterdir() if path.is_file()
            )
            bundled_runtime_libraries.extend(
                bundle_macos_dependencies(package, runtime_seeds)
            )
        bundled_runtime_libraries = sorted(set(bundled_runtime_libraries))

    for name in ("README.md", "SECURITY.md", "CONTRIBUTING.md"):
        shutil.copy2(ROOT / name, package / name)
    for name in ("docs", "editors", "stdlib"):
        copy_tree(ROOT / name, package / name)
    shutil.copy2(bootstrap / "SHA256SUMS.txt", package / "BOOTSTRAP_SHA256SUMS.txt")
    for script in (
        "portable_workflow_test.py", "phase19_bootstrap.py",
        "phase19_cross_sdk.py", "phase19_export_linux_sysroot.py",
        "phase19_package.py"
    ):
        source = (ROOT / "tests" / script) if script == "portable_workflow_test.py" else (ROOT / "scripts" / script)
        shutil.copy2(source, package / "tools" / script)

    (package / "share" / "rocket" / "target.txt").write_text(
        f"rocket-target-sdk-1\nalias={arguments.target}\ntriple={triple}\n",
        encoding="ascii", newline="\n"
    )
    package_note = f"""# Rocket {VERSION} for {arguments.target}

`bin/rocketc{executable_suffix}` is the production Rocket-written compiler.
`stage0/rocketc-stage0{executable_suffix}` is the permanent C++20 bootstrap
compiler. The SDK contains its matching ABI-v1 runtime, LLVM 22.1.6 compiler,
LLD linker, librarian, Clang resource headers, standard library, language
server, target metadata, bootstrap proof, provenance, and checksums.

An ordinary native Rocket compile uses only tools inside this SDK plus the
target operating system's native SDK and system libraries. It does not require
an unrelated system compiler. Linux uses the host glibc development ABI;
macOS uses the installed Apple SDK, which cannot be redistributed by Rocket.
Release channel: {arguments.channel}.
"""
    (package / "PACKAGE.md").write_text(package_note, encoding="utf-8", newline="\n")

    commit = git_value("rev-parse", "HEAD")
    commit_epoch = int(git_value("show", "-s", "--format=%ct", "HEAD"))
    working_tree = "clean" if not git_value("status", "--porcelain") else "dirty"
    provenance: dict[str, object] = {
        "schema": "rocket-release-provenance-2",
        "version": VERSION,
        "target": arguments.target,
        "triple": triple,
        "configuration": arguments.configuration,
        "channel": arguments.channel,
        "source_commit": commit,
        "source_commit_epoch": commit_epoch,
        "working_tree": working_tree,
        "runtime_abi": 1,
        "llvm_version": "22.1.6",
        "archive_format": "deterministic-zip-2" if windows else "deterministic-tar-xz-2",
        "compiler_sha256": sha256(package / "bin" / ("rocketc.exe" if windows else "rocketc.bin")),
        "stage0_sha256": sha256(package / "stage0" / ("rocketc-stage0.exe" if windows else "rocketc-stage0.bin")),
        "runtime_sha256": sha256(packaged_runtime),
        "bootstrap_proof_sha256": sha256(package / "BOOTSTRAP_SHA256SUMS.txt"),
        "llvm_tools": llvm_tools,
        "bundled_runtime_libraries": bundled_runtime_libraries,
        "signed": False,
        "signing_note": "repository completion does not require an official signing certificate",
    }
    (package / "RELEASE-PROVENANCE.json").write_text(
        json.dumps(provenance, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    write_checksums(package)
    return package, provenance


def all_regular_files(root: Path) -> list[Path]:
    return sorted((path for path in root.rglob("*") if path.is_file()), key=lambda path: path.as_posix())


def write_checksums(package: Path) -> None:
    checksum_path = package / "SHA256SUMS.txt"
    lines = []
    for path in all_regular_files(package):
        if path == checksum_path or path.name == "SHA256SUMS.txt.p7s":
            continue
        relative = path.relative_to(package).as_posix()
        lines.append(f"{sha256(path)}  {relative}")
    checksum_path.write_text("\n".join(lines) + "\n", encoding="ascii", newline="\n")


def verify_checksums(package: Path) -> int:
    checksum_path = require_file(package / "SHA256SUMS.txt", "checksum manifest")
    entries: dict[str, str] = {}
    for line in checksum_path.read_text(encoding="ascii").splitlines():
        if not re.fullmatch(r"[0-9a-f]{64}  [^\r\n]+", line):
            raise PackageFailure(f"invalid checksum line: {line!r}")
        digest, relative = line.split("  ", 1)
        pure = PurePosixPath(relative)
        if pure.is_absolute() or ".." in pure.parts or relative in entries:
            raise PackageFailure(f"unsafe or duplicate checksum path: {relative}")
        path = package.joinpath(*pure.parts)
        if not path.is_file() or sha256(path) != digest:
            raise PackageFailure(f"checksum mismatch: {relative}")
        entries[relative] = digest
    expected = {
        path.relative_to(package).as_posix()
        for path in all_regular_files(package)
        if path != checksum_path and path.name != "SHA256SUMS.txt.p7s"
    }
    if set(entries) != expected:
        missing = sorted(expected - set(entries))
        extra = sorted(set(entries) - expected)
        raise PackageFailure(f"checksum coverage differs; missing={missing}, extra={extra}")
    return len(entries)


def sanitized_environment(package: Path, work: Path, target: str) -> dict[str, str]:
    keep = {}
    for name in ("SYSTEMROOT", "WINDIR", "COMSPEC", "PATHEXT", "NUMBER_OF_PROCESSORS"):
        if os.environ.get(name):
            keep[name] = os.environ[name]
    keep["PATH"] = os.pathsep.join(
        [str(package / "bin"), str(Path(os.environ.get("SystemRoot", "C:/Windows")) / "System32")]
        if os.name == "nt" else [str(package / "bin"), "/usr/bin", "/bin"]
    )
    keep["TEMP"] = str(work / "temp")
    keep["TMP"] = str(work / "temp")
    keep["HOME"] = str(work / "home")
    keep["USERPROFILE"] = str(work / "home")
    keep["LANG"] = "C.UTF-8" if target.startswith("linux-") else "C"
    keep["LC_ALL"] = keep["LANG"]
    keep["ROCKET_ARTIFACT_ROOT"] = str(work / "artifacts")
    keep["ROCKET_NATIVE_TARGET"] = target
    if target.startswith("linux-"):
        keep["LD_LIBRARY_PATH"] = str(package / "lib")
    elif target == "macos-arm64":
        keep["DYLD_LIBRARY_PATH"] = str(package / "lib")
    (work / "temp").mkdir(parents=True)
    (work / "home").mkdir(parents=True)
    return keep


def verify_packaged_driver(
    package: Path, work: Path, target: str, env: dict[str, str]
) -> None:
    """Prove the relocated Clang driver can find its matching LLD alias."""
    triple = TARGETS[target][0]
    clang = package / "bin" / "clang"
    source = work / "relocated-toolchain-probe.c"
    executable = work / "relocated-toolchain-probe"
    source.write_text(
        "int main(void) { return 0; }\n", encoding="ascii", newline="\n"
    )
    run([clang, "--version"], env=env, cwd=work, pattern=r"clang version 22\.1\.6")
    command: list[str | Path] = [
        clang, f"--target={triple}", source, "-fuse-ld=lld",
    ]
    command.append(
        "-Wl,-no_uuid" if target == "macos-arm64" else "-Wl,--build-id=sha1"
    )
    command.extend(["-o", executable])
    run(command, env=env, cwd=work)
    run([executable], env=env, cwd=work)


def verify_relocation(package: Path, arguments: argparse.Namespace) -> dict[str, object]:
    relocation_parent = arguments.relocation_parent.resolve()
    if not within(relocation_parent, PHASE19_ROOT):
        raise PackageFailure("relocation output must be under out/phase19")
    relocation = relocation_parent / arguments.target / "sanitized path with spaces"
    replace_directory(relocation.parent)
    shutil.copytree(package, relocation)
    work = relocation.parent / "work"
    work.mkdir()
    env = sanitized_environment(relocation, work, arguments.target)
    suffix = TARGETS[arguments.target][1]
    compiler = relocation / "bin" / f"rocketc{suffix}"
    stage0 = relocation / "stage0" / f"rocketc-stage0{suffix}"
    run([compiler, "--version"], env=env, cwd=work, pattern=r"^rocketc 2\.1\.0$")
    target_output = run(
        [compiler, "target", "--verbose"], env=env, cwd=work,
        pattern=rf"(?m)^target: {re.escape(arguments.target)}$"
    )
    run([stage0, "--version"], env=env, cwd=work, pattern=r"^rocketc 2\.1\.0$")
    if arguments.target != "windows-x64":
        verify_packaged_driver(relocation, work, arguments.target, env)
    fixture = work / "package-fixture"
    shutil.copytree(ROOT / "tests" / "fixtures" / "phase8_package", fixture)
    run([compiler, "check", fixture], env=env, cwd=work, pattern=r"check succeeded")
    run([compiler, "build", fixture], env=env, cwd=work, pattern=r"built ")
    run([compiler, "run", fixture], env=env, cwd=work, pattern=r"(?m)^42$")
    run([compiler, "test", fixture], env=env, cwd=work, pattern=r"2 passed; 0 failed")
    return {
        "schema": "rocket-relocation-report-2",
        "version": VERSION,
        "target": arguments.target,
        "package": package.name,
        "relocated_directory_name": relocation.name,
        "sanitized_environment_keys": sorted(env),
        "target_output": target_output.splitlines(),
        "checksum_files": verify_checksums(relocation),
        "passed": True,
    }


def archive_timestamp(epoch: int) -> tuple[int, int, int, int, int, int]:
    timestamp = dt.datetime.fromtimestamp(max(epoch, 315532800), tz=dt.timezone.utc)
    return (timestamp.year, timestamp.month, timestamp.day, timestamp.hour, timestamp.minute, timestamp.second)


def deterministic_zip(package: Path, destination: Path, epoch: int) -> None:
    remove_file(destination)
    timestamp = archive_timestamp(epoch)
    with zipfile.ZipFile(
        destination, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9,
        strict_timestamps=True
    ) as archive:
        for path in all_regular_files(package):
            relative = f"{package.name}/{path.relative_to(package).as_posix()}"
            info = zipfile.ZipInfo(relative, timestamp)
            mode = 0o755 if os.access(path, os.X_OK) else 0o644
            info.external_attr = (stat.S_IFREG | mode) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            info.create_system = 3
            archive.writestr(info, path.read_bytes(), compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)


def deterministic_tar_xz(package: Path, destination: Path, epoch: int) -> None:
    remove_file(destination)
    with destination.open("wb") as raw:
        with lzma.LZMAFile(raw, "w", preset=9) as compressed:
            with tarfile.open(fileobj=compressed, mode="w", format=tarfile.GNU_FORMAT) as archive:
                for path in all_regular_files(package):
                    relative = f"{package.name}/{path.relative_to(package).as_posix()}"
                    info = tarfile.TarInfo(relative)
                    info.size = path.stat().st_size
                    info.mtime = epoch
                    info.uid = 0
                    info.gid = 0
                    info.uname = ""
                    info.gname = ""
                    info.mode = 0o755 if os.access(path, os.X_OK) else 0o644
                    with path.open("rb") as source:
                        archive.addfile(info, source)


def create_archives(package: Path, provenance: dict[str, object], target: str) -> tuple[Path, str]:
    suffix = ".zip" if target == "windows-x64" else ".tar.xz"
    archive = package.with_name(package.name + suffix)
    repeat = package.with_name(package.name + ".repeat" + suffix)
    creator = deterministic_zip if suffix == ".zip" else deterministic_tar_xz
    epoch = int(provenance["source_commit_epoch"])
    creator(package, archive, epoch)
    creator(package, repeat, epoch)
    first = sha256(archive)
    second = sha256(repeat)
    remove_file(repeat)
    if first != second:
        raise PackageFailure(f"archive reproduction differs: {first} != {second}")
    return archive, first


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", required=True, choices=tuple(TARGETS))
    parser.add_argument("--configuration", required=True, choices=("Debug", "Release"))
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--bootstrap-dir", required=True, type=Path)
    parser.add_argument("--llvm-root", required=True, type=Path)
    parser.add_argument("--output-parent", required=True, type=Path)
    parser.add_argument("--relocation-parent", required=True, type=Path)
    parser.add_argument("--channel", choices=("local", "nightly", "preview", "stable"), default="local")
    parser.add_argument("--windows-library-directory", action="append", default=[])
    parser.add_argument("--runtime-shared-library", action="append", default=[])
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    started = time.monotonic()
    package, provenance = package_tree(arguments)
    covered = verify_checksums(package)
    relocation_report = verify_relocation(package, arguments)
    report_path = package / "RELOCATION_REPORT.json"
    report_path.write_text(
        json.dumps(relocation_report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    write_checksums(package)
    covered = verify_checksums(package)
    archive, archive_hash = create_archives(package, provenance, arguments.target)
    result = {
        "schema": "rocket-package-report-2",
        "version": VERSION,
        "target": arguments.target,
        "configuration": arguments.configuration,
        "package": str(package),
        "package_files": covered,
        "archive": str(archive),
        "archive_sha256": archive_hash,
        "relocation_passed": True,
        "archive_reproducible": True,
        "seconds": round(time.monotonic() - started, 3),
        "passed": True,
    }
    report = arguments.output_parent.resolve() / f"package-report-{arguments.target}.json"
    report.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(
        f"Rocket {VERSION} package passed for {arguments.target}: "
        f"{covered} files, archive sha256 {archive_hash}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except PackageFailure as error:
        print(f"phase19-package: error: {error}", file=sys.stderr)
        raise SystemExit(1)
