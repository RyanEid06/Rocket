#!/usr/bin/env python3
"""Focused offline tests for Phase 19 dependency/release orchestration."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import inspect
import json
import shutil
import sys
from pathlib import Path


def load(name: str, path: Path):
    specification = importlib.util.spec_from_file_location(name, path)
    if specification is None or specification.loader is None:
        raise RuntimeError(f"could not load {path}")
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module


def check(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True, type=Path)
    parser.add_argument("--work", required=True, type=Path)
    arguments = parser.parse_args()
    root = arguments.source_dir.resolve()
    work = arguments.work.resolve()
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)

    dependency = json.loads((root / "dependencies" / "manifest.json").read_text())
    check(dependency["schemaVersion"] == 2, "dependency schema is not version 2")
    expected_targets = {"windows-x64", "linux-x64", "linux-arm64", "macos-arm64"}
    check(set(dependency["platforms"]) == expected_targets, "dependency target rows differ")
    for target, row in dependency["platforms"].items():
        for name in ("llvm", "ninja"):
            package = row[name]
            check(package["size"] > 0, f"{target} {name} has no byte size")
            check(
                len(package["sha256"]) == 64
                and all(value in "0123456789abcdef" for value in package["sha256"]),
                f"{target} {name} has an invalid SHA-256",
            )

    presets = json.loads((root / "CMakePresets.json").read_text())
    configure_names = {entry["name"] for entry in presets["configurePresets"]}
    build_names = {entry["name"] for entry in presets["buildPresets"]}
    test_names = {entry["name"] for entry in presets["testPresets"]}
    for target in expected_targets:
        for suffix in ("debug", "release", "stage0-debug", "stage0-release"):
            name = f"phase19-{target}-{suffix}"
            check(name in configure_names, f"missing configure preset {name}")
            check(name in build_names, f"missing build preset {name}")
            check(name in test_names, f"missing test preset {name}")

    workflow = (root / ".github" / "workflows" / "phase19-native.yml").read_text()
    for value in (
        "windows-2025-vs2026", "ubuntu-24.04", "ubuntu-24.04-arm",
        "macos-15", "phase19_bootstrap.py", "phase19_cross_sdk.py",
        "phase19_package.py",
    ):
        check(value in workflow, f"native workflow omits {value}")
    check(
        "brew --prefix curl" not in workflow,
        "macOS workflow must use Apple system libcurl and trust integration",
    )
    check(
        "librocket_runtime.a" in workflow,
        "POSIX native workflow does not use CMake's librocket_runtime archive name",
    )
    check(
        "ROCKET_HOST_CXX_COMPILER=$llvm/bin/clang++" in workflow,
        "macOS workflow does not use the LLVM 22 linker-compatible C++ toolchain",
    )
    check(
        'ROCKET_CXX_STANDARD_INCLUDE=$cxx_include' in workflow,
        "macOS workflow does not select Xcode's matching libc++ headers",
    )
    check(
        'ROCKET_MACOS_SDK_ROOT=$sdk' in workflow
        and '-isysroot "$ROCKET_MACOS_SDK_ROOT"' in workflow,
        "standalone macOS ABI probe does not use the SDK owning its libc++ headers",
    )
    check(
        workflow.count('--macos-sdk-root "$ROCKET_MACOS_SDK_ROOT"') == 2,
        "macOS bootstrap and package relocation do not share the accepted SDK root",
    )
    check(
        'ROCKET_CXX_STANDARD_LIBRARY=$cxx_runtime' in workflow,
        "macOS workflow does not select Xcode's matching libc++ runtime",
    )
    check(
        'cxx_runtime="$sdk/usr/lib/libc++.tbd"' in workflow
        and 'cxx_runtime="$llvm/lib/libc++.dylib"' not in workflow,
        "macOS workflow can still select LLVM's incompatible bundled libc++",
    )
    check(
        "Prove macOS libc++ header and runtime compatibility" in workflow
        and "std::unordered_map" in workflow
        and "std::runtime_error" in workflow,
        "macOS workflow lacks an early libc++ symbol and exception ABI probe",
    )

    package_tool = load("phase19_package", root / "scripts" / "phase19_package.py")
    bootstrap_tool = load("phase19_bootstrap", root / "scripts" / "phase19_bootstrap.py")
    cross_tool = load("phase19_cross_sdk", root / "scripts" / "phase19_cross_sdk.py")
    package_tree_source = inspect.getsource(package_tool.package_tree)
    check(
        "else f\"librocket_runtime{runtime_suffix}\"" in package_tree_source,
        "POSIX package lookup does not use CMake's librocket_runtime archive name",
    )
    check(
        'package / "lib" / f"rocket_runtime{runtime_suffix}"' in package_tree_source,
        "installed package does not normalize the POSIX runtime SDK name",
    )
    cmake = (root / "CMakeLists.txt").read_text()
    check(
        'set(ROCKET_CXX_STANDARD_INCLUDE "$ENV{ROCKET_CXX_STANDARD_INCLUDE}")' in cmake
        and "-nostdinc++" in cmake,
        "macOS CMake configuration can compile against LLVM's newer libc++ headers",
    )
    check(
        'set(ROCKET_CXX_STANDARD_LIBRARY "$ENV{ROCKET_CXX_STANDARD_LIBRARY}")' in cmake,
        "macOS CMake configuration does not honor the selected Xcode libc++ runtime",
    )
    check(
        'ROCKETC_MACOS_SDK_ROOT="${ROCKET_MACOS_SDK_ROOT}"' in cmake
        and '"ROCKET_MACOS_SDK_ROOT=${ROCKET_MACOS_SDK_ROOT}"' in cmake,
        "macOS CMake configuration does not propagate its SDK to native compiler links",
    )
    check(
        "ROCKET_CXX_RUNTIME_LIBRARY_DIRECTORY" not in cmake,
        "macOS builds still inject LLVM's incompatible libc++ runtime path",
    )
    wrapper_source = package_tool.wrapper("rocketc", "rocketc.bin")
    for value in ("ROCKET_CLANG", "ROCKET_LIBRARIAN", "ROCKET_RUNTIME"):
        check(value in wrapper_source, f"relocated POSIX SDK wrapper omits {value}")
    check(
        "$rocket_sdk_root/bin/clang" in wrapper_source,
        "stage0 and production wrappers do not share the SDK-local clang path",
    )
    check(
        "/usr/bin/xcrun --sdk macosx --show-sdk-path" in wrapper_source
        and "ROCKET_MACOS_SDK_ROOT" in wrapper_source,
        "relocated macOS SDK launchers do not discover the active Apple SDK",
    )
    copy_llvm_source = inspect.getsource(package_tool.copy_llvm)
    check(
        'destination = package / "bin" / installed_name' in copy_llvm_source,
        "POSIX LLVM driver aliases are replaced by resolved symlink names",
    )
    check(
        package_tool.macos_cxx_runtime_library("libc++.1.dylib")
        and package_tool.macos_cxx_runtime_library("libc++abi.1.dylib")
        and not package_tool.macos_cxx_runtime_library("libLLVM.dylib"),
        "macOS package filtering does not isolate LLVM's private C++ ABI runtimes",
    )
    mach_o_probe = work / "mach-o-probe"
    shell_probe = work / "shell-probe"
    mach_o_probe.write_bytes(b"\xcf\xfa\xed\xfeprobe")
    shell_probe.write_text("#!/bin/sh\n", encoding="ascii")
    check(
        package_tool.macos_mach_o(mach_o_probe)
        and not package_tool.macos_mach_o(shell_probe),
        "macOS packaging cannot distinguish Mach-O files from launcher scripts",
    )
    driver_probe_source = inspect.getsource(package_tool.verify_packaged_driver)
    check(
        '"-fuse-ld=lld"' in driver_probe_source
        and 'f"--sysroot={sysroot}"' in driver_probe_source,
        "relocation verification does not prove packaged Clang-to-LLD SDK linking",
    )
    macos_bundle_source = inspect.getsource(package_tool.bundle_macos_dependencies)
    check(
        '"-delete_rpath"' in macos_bundle_source,
        "macOS packages retain absolute build-machine runtime paths",
    )
    check(
        '"/usr/bin/codesign"' in macos_bundle_source
        and '"--timestamp=none"' in macos_bundle_source,
        "rewritten macOS package binaries lack deterministic ad-hoc signatures",
    )
    check(
        'extra+=(--library-directory "$llvm/lib")' not in workflow,
        "macOS bootstrap can still select LLVM's incompatible bundled libc++",
    )
    bootstrap_stage_source = inspect.getsource(bootstrap_tool.build_self_hosted_stage)
    check(
        'f"-Wl,-rpath,{directory}"' in bootstrap_stage_source,
        "POSIX bootstrap stages cannot load explicitly selected runtime libraries",
    )
    check(
        bootstrap_stage_source.count('f"--sysroot={arguments.macos_sdk_root}"') == 2,
        "macOS stage2/stage3 compile and link commands do not share the Apple SDK",
    )

    fake_llvm = work / "fake-llvm"
    fake_package = work / "fake-package"
    (fake_llvm / "bin").mkdir(parents=True)
    (fake_llvm / "lib" / "clang" / "22" / "include").mkdir(parents=True)
    (fake_package / "bin").mkdir(parents=True)
    (fake_package / "lib").mkdir(parents=True)
    (fake_package / "licenses").mkdir(parents=True)
    for name in (
        "clang", "llvm-ar", "ld.lld", "lld", "llvm-dwarfdump", "llvm-objdump"
    ):
        (fake_llvm / "bin" / name).write_bytes((name + "\n").encode())
    (fake_llvm / "lib" / "clang" / "22" / "include" / "stddef.h").write_text(
        "/* resource probe */\n", encoding="ascii"
    )
    real_require_file = package_tool.require_file

    def resolve_ld_alias(path: Path, description: str) -> Path:
        if path.name == "ld.lld":
            return (path.parent / "lld").resolve()
        return real_require_file(path, description)

    package_tool.require_file = resolve_ld_alias
    try:
        package_tool.copy_llvm(
            argparse.Namespace(target="linux-x64", llvm_root=fake_llvm),
            fake_package,
        )
    finally:
        package_tool.require_file = real_require_file
    check(
        (fake_package / "bin" / "ld.lld").is_file(),
        "copying a symlinked LLVM linker drops the ld.lld driver alias",
    )
    fake_macos_package = work / "fake-macos-package"
    for directory in ("bin", "lib", "licenses"):
        (fake_macos_package / directory).mkdir(parents=True)
    (fake_llvm / "bin" / "ld64.lld").write_bytes(b"ld64.lld\n")
    (fake_llvm / "lib" / "libc++.1.dylib").write_bytes(b"private-libc++\n")
    (fake_llvm / "lib" / "libc++abi.1.dylib").write_bytes(b"private-libc++abi\n")
    (fake_llvm / "lib" / "libLLVM.dylib").write_bytes(b"llvm-runtime\n")
    package_tool.copy_llvm(
        argparse.Namespace(target="macos-arm64", llvm_root=fake_llvm),
        fake_macos_package,
    )
    check(
        (fake_macos_package / "lib" / "libLLVM.dylib").is_file()
        and not (fake_macos_package / "lib" / "libc++.1.dylib").exists()
        and not (fake_macos_package / "lib" / "libc++abi.1.dylib").exists(),
        "macOS SDK package copied LLVM's private libc++ ABI into its loader path",
    )
    copy_host_llvm_source = inspect.getsource(cross_tool.copy_host_llvm)
    check(
        'destination / "bin" / installed_name' in copy_host_llvm_source,
        "cross SDKs replace LLVM driver aliases with resolved symlink names",
    )
    runtime_library_initialization = package_tree_source.find(
        "bundled_runtime_libraries: list[str] = []"
    )
    windows_branch = package_tree_source.find("if windows:")
    check(
        runtime_library_initialization >= 0
        and runtime_library_initialization < windows_branch,
        "package provenance runtime-library list is not initialized for Windows",
    )
    dependency_bootstrap_source = (root / "dependencies" / "bootstrap.py").read_text()
    check(
        "ensure_executable_marker(marker_path, marker)" in dependency_bootstrap_source,
        "POSIX Ninja installs do not restore executable mode after ZIP extraction",
    )
    check(
        "marker_path.chmod(marker_path.stat().st_mode | 0o111)" in dependency_bootstrap_source,
        "POSIX Ninja executable-mode restoration is not deterministic",
    )
    sample = work / "sample-sdk"
    (sample / "bin").mkdir(parents=True)
    (sample / "bin" / "rocketc").write_text("compiler\n", encoding="utf-8")
    (sample / "PACKAGE.md").write_text("package\n", encoding="utf-8")
    package_tool.write_checksums(sample)

    shared_source = work / "runtime-source" / "librocket-test.so.1"
    shared_source.parent.mkdir()
    shared_source.write_bytes(b"portable-runtime-library\n")
    shared_destination = work / "runtime-package"
    shared_destination.mkdir()
    copied = package_tool.copy_library_chain(shared_source, shared_destination)
    check(copied == [shared_source.name], "runtime library copy names differ")
    check(
        (shared_destination / shared_source.name).read_bytes()
        == shared_source.read_bytes(),
        "runtime library copy contents differ",
    )

    stage0_source = (root / "src" / "main.cpp").read_text()
    selfhost_source = (root / "compiler" / "src" / "main.rocket").read_text()
    bootstrap_source = (root / "scripts" / "phase19_bootstrap.py").read_text()
    for library in ("-lstdc++", "-lc++", "-lm"):
        for name, source in (
            ("stage0", stage0_source),
            ("selfhost", selfhost_source),
            ("bootstrap", bootstrap_source),
        ):
            check(library in source, f"{name} omits POSIX runtime link input {library}")
    check(
        "report_external_command_failure" in selfhost_source,
        "self-hosted native tool failures do not report the failed command",
    )
    for name, source in (("stage0", stage0_source), ("selfhost", selfhost_source)):
        check(
            "ROCKET_MACOS_SDK_ROOT" in source and "sysroot" in source,
            f"{name} does not propagate the active Apple SDK to native links",
        )
    check(package_tool.verify_checksums(sample) == 2, "checksum coverage count differs")
    (sample / "PACKAGE.md").write_text("tampered\n", encoding="utf-8")
    try:
        package_tool.verify_checksums(sample)
    except package_tool.PackageFailure:
        pass
    else:
        raise RuntimeError("checksum verification accepted a modified file")
    (sample / "PACKAGE.md").write_text("package\n", encoding="utf-8")
    package_tool.write_checksums(sample)

    first_zip = work / "sample-1.zip"
    second_zip = work / "sample-2.zip"
    package_tool.deterministic_zip(sample, first_zip, 1700000000)
    package_tool.deterministic_zip(sample, second_zip, 1700000000)
    check(package_tool.sha256(first_zip) == package_tool.sha256(second_zip), "ZIP differs")
    first_tar = work / "sample-1.tar.xz"
    second_tar = work / "sample-2.tar.xz"
    package_tool.deterministic_tar_xz(sample, first_tar, 1700000000)
    package_tool.deterministic_tar_xz(sample, second_tar, 1700000000)
    check(package_tool.sha256(first_tar) == package_tool.sha256(second_tar), "tar.xz differs")

    try:
        bootstrap_tool.clean_output(root / "out" / "build" / "windows-release")
    except bootstrap_tool.BootstrapFailure:
        pass
    else:
        raise RuntimeError("bootstrap safety guard accepted the frozen Release directory")
    try:
        package_tool.replace_directory(root / "out" / "package" / "rocket-2.0.0-windows-x64")
    except package_tool.PackageFailure:
        pass
    else:
        raise RuntimeError("package safety guard accepted the frozen Rocket 2.0 package")

    native_host = cross_tool.host_alias()
    cross_target = {
        "windows-x64": "linux-x64",
        "linux-x64": "linux-arm64",
    }.get(native_host)
    if cross_target:
        fake_llvm = work / "fake-host-llvm"
        executable_suffix = ".exe" if native_host == "windows-x64" else ""
        for name in ("clang", "llvm-ar", "ld.lld", "lld"):
            tool = fake_llvm / "bin" / f"{name}{executable_suffix}"
            tool.parent.mkdir(parents=True, exist_ok=True)
            tool.write_bytes(b"host-tool\n")
        runtime = work / "rocket_runtime.a"
        runtime.write_bytes(b"target-runtime\n")
        sysroot = work / "fake-sysroot"
        (sysroot / "usr" / "include").mkdir(parents=True)
        (sysroot / "usr" / "lib").mkdir(parents=True)
        (sysroot / "usr" / "include" / "stddef.h").write_text("/* test */\n")
        (sysroot / "usr" / "lib" / "crt1.o").write_bytes(b"crt\n")
        cross_output = work / "cross-sdk"
        real_cross_require_file = cross_tool.require_file

        def resolve_cross_linker_alias(path: Path, description: str) -> Path:
            if path.name == f"ld.lld{executable_suffix}":
                return (path.parent / f"lld{executable_suffix}").resolve()
            return real_cross_require_file(path, description)

        cross_tool.require_file = resolve_cross_linker_alias
        try:
            report = cross_tool.assemble(argparse.Namespace(
                host=native_host,
                target=cross_target,
                host_llvm_root=fake_llvm,
                target_runtime=runtime,
                sysroot=sysroot,
                windows_library_directory=[],
                output=cross_output,
            ))
        finally:
            cross_tool.require_file = real_cross_require_file
        check(report["passed"], "cross-SDK assembly report did not pass")
        check(
            cross_tool.verify_checksums(cross_output) > 0,
            "cross-SDK checksum verification is empty",
        )
        check(
            (cross_output / "share" / "rocket" / "target.txt").read_text()
            .splitlines()[1] == f"alias={cross_target}",
            "cross-SDK target metadata differs",
        )
        check(
            (cross_output / "bin" / f"ld.lld{executable_suffix}").is_file(),
            "cross-SDK assembly drops a symlinked ld.lld driver alias",
        )

    print("phase19-release-tooling-ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
