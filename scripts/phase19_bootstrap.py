#!/usr/bin/env python3
"""Portable, isolated Rocket 2.1 stage0 -> stage3 bootstrap proof."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PHASE19_ROOT = (ROOT / "out" / "phase19").resolve()
TARGETS = {
    "windows-x64": ("x86_64-pc-windows-msvc", ".exe", ".obj", ".lib", ".dll"),
    "linux-x64": ("x86_64-unknown-linux-gnu", "", ".o", ".a", ".so"),
    "linux-arm64": ("aarch64-unknown-linux-gnu", "", ".o", ".a", ".so"),
    "macos-arm64": ("arm64-apple-macosx", "", ".o", ".a", ".dylib"),
}


class BootstrapFailure(RuntimeError):
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
    raise BootstrapFailure(f"unsupported native bootstrap host: {system}-{machine}")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def within(child: Path, parent: Path) -> bool:
    try:
        child.resolve().relative_to(parent.resolve())
        return True
    except ValueError:
        return False


def clean_output(path: Path) -> None:
    resolved = path.resolve()
    if resolved == PHASE19_ROOT or not within(resolved, PHASE19_ROOT):
        raise BootstrapFailure(f"refusing to clean non-Phase-19 output: {resolved}")
    if resolved.exists():
        shutil.rmtree(resolved)
    resolved.mkdir(parents=True)


def quote(command: list[str]) -> str:
    return subprocess.list2cmdline(command) if os.name == "nt" else " ".join(
        "'" + value.replace("'", "'\\''") + "'" for value in command
    )


class Runner:
    def __init__(self, report: dict[str, object]):
        self.report = report
        self.count = 0

    def run(
        self,
        command: list[str | Path],
        *,
        env: dict[str, str],
        expected: int = 0,
        pattern: str | None = None,
        cwd: Path = ROOT,
        label: str = "",
    ) -> str:
        native = [str(value) for value in command]
        print("+ " + quote(native), flush=True)
        started = time.monotonic()
        result = subprocess.run(
            native, cwd=cwd, env=env, text=True, errors="replace",
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False
        )
        duration = time.monotonic() - started
        if result.stdout:
            print(result.stdout, end="" if result.stdout.endswith("\n") else "\n")
        if result.returncode != expected:
            raise BootstrapFailure(
                f"{label or native[0]} returned {result.returncode}, expected {expected}"
            )
        if pattern and re.search(pattern, result.stdout, re.MULTILINE) is None:
            raise BootstrapFailure(
                f"{label or native[0]} output did not match {pattern!r}"
            )
        self.count += 1
        self.report.setdefault("commands", []).append(
            {
                "label": label,
                "command": native,
                "status": result.returncode,
                "seconds": round(duration, 3),
            }
        )
        return result.stdout


def compiler_environment(
    base: dict[str, str], arguments: argparse.Namespace, artifact_root: Path
) -> dict[str, str]:
    result = base.copy()
    result.update(
        {
            "ROCKET_STAGE0": str(arguments.stage0),
            "ROCKET_CLANG": str(arguments.clang),
            "ROCKET_LIBRARIAN": str(arguments.librarian),
            "ROCKET_RUNTIME": str(arguments.runtime),
            "ROCKET_NATIVE_LIBRARY_ROOT": str(arguments.native_library_root),
            "ROCKET_ARTIFACT_ROOT": str(artifact_root),
            "ROCKET_NATIVE_TARGET": arguments.target,
        }
    )
    if arguments.macos_sdk_root is not None:
        result["ROCKET_MACOS_SDK_ROOT"] = str(arguments.macos_sdk_root)
    return result


def build_self_hosted_stage(
    runner: Runner,
    compiler: Path,
    stage: str,
    arguments: argparse.Namespace,
    environment: dict[str, str],
    output: Path,
) -> Path:
    triple, executable_suffix, object_suffix, _, _ = TARGETS[arguments.target]
    source = ROOT / "compiler" / "src" / "main.rocket"
    ir = output / f"{stage}.ll"
    obj = output / f"{stage}{object_suffix}"
    executable = output / f"{stage}{executable_suffix}"
    runner.run(
        [compiler, "--emit-ir", source, ir], env=environment,
        label=f"{stage}-emit-ir"
    )
    compile_command = [
        arguments.clang, "-c", "-O2", "-Wno-override-module",
        f"--target={triple}",
    ]
    if arguments.macos_sdk_root is not None:
        compile_command.append(f"--sysroot={arguments.macos_sdk_root}")
    compile_command.extend([ir, "-o", obj])
    runner.run(
        compile_command, env=environment, label=f"{stage}-compile"
    )
    link = [
        arguments.clang, obj, arguments.runtime, f"--target={triple}",
        "-fuse-ld=lld",
    ]
    if arguments.macos_sdk_root is not None:
        link.append(f"--sysroot={arguments.macos_sdk_root}")
    for directory in arguments.library_directory:
        link.extend(["-L", directory])
        if arguments.target != "windows-x64":
            link.append(f"-Wl,-rpath,{directory}")
    if arguments.target == "windows-x64":
        link.extend(
            ["-Wl,/Brepro", "-Wl,/DEBUG:FULL", f"-Wl,/PDB:{output / (stage + '.pdb')}"]
        )
    elif arguments.target.startswith("linux-"):
        link.extend(
            ["-Wl,--build-id=sha1", "-lcurl", "-lcrypto", "-licuuc",
             "-licudata", "-lpthread", "-ldl", "-lstdc++", "-lm"]
        )
    else:
        link.extend(
            ["-Wl,-no_uuid", "-lcurl", "-lcrypto", "-licuuc",
             "-licudata", "-lpthread", "-lc++", "-lm", "-framework",
             "Security", "-framework", "CoreFoundation"]
        )
    link.extend(["-o", executable])
    runner.run(link, env=environment, label=f"{stage}-link")
    if not executable.is_file():
        raise BootstrapFailure(f"{stage} executable was not produced: {executable}")
    return executable


def validate_stages(
    runner: Runner,
    stages: list[Path],
    arguments: argparse.Namespace,
    base_environment: dict[str, str],
    output: Path,
) -> tuple[list[str], int]:
    validations = 0
    versions: list[str] = []
    target_outputs: list[str] = []
    source = ROOT / "compiler" / "src" / "main.rocket"
    for index, compiler in enumerate(stages):
        env = compiler_environment(
            base_environment, arguments, output / "validation-artifacts" / f"stage{index}"
        )
        versions.append(
            runner.run([compiler, "--version"], env=env, label=f"stage{index}-version").strip()
        )
        target_outputs.append(
            runner.run(
                [compiler, "target", "--verbose"], env=env,
                label=f"stage{index}-target"
            ).strip()
        )
        if index > 0:
            runner.run(
                [compiler, "--self-test-lexer"], env=env,
                pattern="lexer tests passed", label=f"stage{index}-lexer"
            )
            runner.run(
                [compiler, "--self-test-parser"], env=env,
                pattern="parser tests passed", label=f"stage{index}-parser"
            )
            validations += 2
    if len(set(versions)) != 1 or versions[0] != "rocketc 2.1.0":
        raise BootstrapFailure(f"bootstrap version mismatch: {versions}")
    if len(set(target_outputs)) != 1:
        raise BootstrapFailure("stage0-stage3 target --verbose output differs")
    stage3_env = compiler_environment(
        base_environment, arguments, output / "validation-artifacts" / "stage3"
    )
    runner.run([stages[3], "--check-hir", source], env=stage3_env, label="stage3-hir")
    runner.run([stages[3], "--check-mir", source], env=stage3_env, label="stage3-mir")
    validations += 2

    positive = [
        "phase18_async_join.rocket", "phase18_nested_await.rocket",
        "phase18_weak.rocket", "phase18_unique_buffer.rocket",
        "phase18_concurrency.rocket", "phase18_async_file.rocket",
        "phase18_async_socket.rocket", "phase18_async_cancel.rocket",
        "phase18_process.rocket", "phase18_task_group.rocket",
        "phase18_task_cancel.rocket", "phase18_thread.rocket",
        "phase18_structured_cleanup.rocket", "phase18_unsafe_local.rocket",
    ]
    negative = [
        "phase18_await_context_failure.rocket", "phase18_send_failure.rocket",
        "phase18_async_result_send_failure.rocket", "phase18_suspension_failure.rocket",
        "phase18_unique_buffer_move_failure.rocket", "phase18_scoped_escape_failure.rocket",
        "phase18_weak_share_failure.rocket", "phase18_task_weak_failure.rocket",
        "phase18_pointer_send_failure.rocket", "phase18_guard_release_failure.rocket",
        "phase18_task_reuse_failure.rocket", "phase18_mutex_share_failure.rocket",
        "phase18_capture_move_failure.rocket", "phase18_transitive_move_failure.rocket",
        "phase18_buffer_element_share_failure.rocket",
    ]
    fixtures = ROOT / "tests" / "fixtures"
    for stage_index, compiler in enumerate(stages):
        env = compiler_environment(
            base_environment, arguments,
            output / "validation-artifacts" / f"stage{stage_index}"
        )
        for name in positive:
            runner.run(
                [compiler, "check", fixtures / name], env=env,
                label=f"stage{stage_index}-{name}-positive"
            )
            validations += 1
        for name in negative:
            runner.run(
                [compiler, "check", fixtures / name], env=env, expected=1,
                pattern=r"R410[0-9]+", label=f"stage{stage_index}-{name}-negative"
            )
            validations += 1

    check_sources = [
        ROOT / "examples" / "hello.rocket",
        fixtures / "llvm_operators.rocket",
        fixtures / "runtime_collections.rocket",
        fixtures / "phase6_types.rocket",
        fixtures / "phase7_stdlib.rocket",
        fixtures / "phase15_platform.rocket",
        fixtures / "phase15_network.rocket",
        fixtures / "phase13_native_package",
        ROOT / "examples" / "raylib_showcase",
    ]
    for stage_index, compiler in enumerate(stages):
        env = compiler_environment(
            base_environment, arguments,
            output / "validation-artifacts" / f"stage{stage_index}"
        )
        for source_path in check_sources:
            runner.run(
                [compiler, "check", source_path], env=env,
                label=f"stage{stage_index}-check-{source_path.name}"
            )
            validations += 1

    native_runs = [
        (ROOT / "examples" / "hello.rocket", r"Hello from Rocket"),
        (fixtures / "phase7_stdlib.rocket", r"Rocket Standard Library"),
        (fixtures / "phase15_binary_io.rocket", r"phase15-binary-ok"),
        (fixtures / "phase15_text_streams.rocket", r"phase15-text-streams-ok"),
        (fixtures / "phase15_crypto.rocket", r"phase15-crypto-ok"),
        (fixtures / "phase15_network.rocket", r"phase15-network-ok"),
        (fixtures / "phase15_platform.rocket", r"phase15-platform-ok"),
        (fixtures / "phase15_archive.rocket", r"phase15-archive-ok"),
        (fixtures / "phase15_sqlite.rocket", r"phase15-sqlite-ok"),
        (fixtures / "phase18_concurrency.rocket", r"published"),
        (fixtures / "phase18_async_file.rocket", r"R[\r\n]+T"),
        (fixtures / "phase18_thread.rocket", r"thread-result"),
        (fixtures / "phase13_native_package", r"native-interop-ok"),
    ]
    for source_path, pattern in native_runs:
        runner.run(
            [stages[3], "run", source_path], env=stage3_env,
            pattern=pattern, label=f"stage3-run-{source_path.name}"
        )
        validations += 1
    runner.run(
        [stages[3], "test", fixtures / "phase8_package"], env=stage3_env,
        pattern=r"2 passed; 0 failed", label="stage3-package-test"
    )
    runner.run(
        [stages[3], "test", ROOT / "examples" / "raylib_showcase"],
        env=stage3_env, pattern=r"4 passed; 0 failed", label="stage3-raylib-test"
    )
    validations += 2

    _, _, _, static_suffix, dynamic_suffix = TARGETS[arguments.target]
    for package, suffix in (
        ("phase13_static_library", static_suffix),
        ("phase13_dynamic_library", dynamic_suffix),
    ):
        runner.run(
            [stages[3], "build", fixtures / package], env=stage3_env,
            pattern=re.escape(suffix), label=f"stage3-build-{package}"
        )
        validations += 1

    generated = output / "generated-parity"
    generated.mkdir()
    generation_commands = [
        ("header", [stages[3], "emit-header", fixtures / "phase13_static_library", "--output"]),
        ("bindings", [stages[3], "bind", ROOT / "tests" / "native" / "phase13_native.h", "--output"]),
        ("raylib", [stages[3], "bind", ROOT / "examples" / "raylib_showcase" / "native" / "rocket_raylib_adapter.h", "--output"]),
    ]
    for name, prefix in generation_commands:
        first = generated / f"{name}-1.{'h' if name == 'header' else 'rocket'}"
        second = generated / f"{name}-2.{'h' if name == 'header' else 'rocket'}"
        runner.run(prefix + [first], env=stage3_env, label=f"{name}-generate-1")
        runner.run(prefix + [second], env=stage3_env, label=f"{name}-generate-2")
        if sha256(first) != sha256(second):
            raise BootstrapFailure(f"non-deterministic generated {name}")
        validations += 2

    runner.run(
        [stages[3], "run", fixtures / "int_overflow.rocket"], env=stage3_env,
        expected=101, pattern=r"Int arithmetic overflow", label="stage3-overflow"
    )
    validations += 1
    return versions, validations


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", required=True, choices=tuple(TARGETS))
    parser.add_argument("--configuration", required=True, choices=("Debug", "Release"))
    parser.add_argument("--stage0", required=True, type=Path)
    parser.add_argument("--runtime", required=True, type=Path)
    parser.add_argument("--clang", required=True, type=Path)
    parser.add_argument("--librarian", required=True, type=Path)
    parser.add_argument("--native-library-root", required=True, type=Path)
    parser.add_argument("--library-directory", action="append", default=[])
    parser.add_argument("--macos-sdk-root", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    started_wall = time.time()
    started = time.monotonic()
    arguments.stage0 = arguments.stage0.resolve()
    arguments.runtime = arguments.runtime.resolve()
    arguments.clang = arguments.clang.resolve()
    arguments.librarian = arguments.librarian.resolve()
    arguments.native_library_root = arguments.native_library_root.resolve()
    arguments.output = arguments.output.resolve()
    arguments.library_directory = [str(Path(value).resolve()) for value in arguments.library_directory]
    if arguments.macos_sdk_root is not None:
        arguments.macos_sdk_root = arguments.macos_sdk_root.resolve()
    detected = host_alias()
    if detected != arguments.target:
        raise BootstrapFailure(
            f"native bootstrap target {arguments.target} does not match host {detected}"
        )
    for name in ("stage0", "runtime", "clang", "librarian"):
        value = getattr(arguments, name)
        if not value.is_file():
            raise BootstrapFailure(f"missing {name}: {value}")
    if not arguments.native_library_root.is_dir():
        raise BootstrapFailure(
            f"missing native library root: {arguments.native_library_root}"
        )
    if arguments.target == "macos-arm64":
        if arguments.macos_sdk_root is None:
            raise BootstrapFailure("macOS bootstrap requires --macos-sdk-root")
        if not arguments.macos_sdk_root.is_dir():
            raise BootstrapFailure(
                f"missing macOS SDK root: {arguments.macos_sdk_root}"
            )
    clean_output(arguments.output)

    report: dict[str, object] = {
        "schema": "rocket-bootstrap-report-2",
        "version": "2.1.0",
        "configuration": arguments.configuration,
        "host": detected,
        "target": arguments.target,
        "triple": TARGETS[arguments.target][0],
        "started_epoch": int(started_wall),
        "inputs": {
            "stage0": str(arguments.stage0),
            "stage0_sha256": sha256(arguments.stage0),
            "runtime": str(arguments.runtime),
            "runtime_sha256": sha256(arguments.runtime),
            "clang": str(arguments.clang),
            "librarian": str(arguments.librarian),
            "macos_sdk_root": (
                str(arguments.macos_sdk_root)
                if arguments.macos_sdk_root is not None else None
            ),
        },
        "commands": [],
    }
    runner = Runner(report)
    base_environment = os.environ.copy()
    stage1_artifacts = arguments.output / "stage1-artifacts"
    stage1_environment = compiler_environment(
        base_environment, arguments, stage1_artifacts
    )
    runner.run(
        [arguments.stage0, "build", ROOT / "compiler", "--target", arguments.target],
        env=stage1_environment, label="stage0-build-stage1"
    )
    executable_suffix = TARGETS[arguments.target][1]
    built_stage1 = (
        stage1_artifacts / "rocket_compiler" / ".rocketc" / "targets"
        / arguments.target / f"main{executable_suffix}"
    )
    if not built_stage1.is_file():
        raise BootstrapFailure(f"stage0 did not produce stage1: {built_stage1}")
    stage1 = arguments.output / f"stage1{executable_suffix}"
    shutil.copy2(built_stage1, stage1)
    stage_environment = compiler_environment(
        base_environment, arguments, arguments.output / "stage-artifacts"
    )
    stage2 = build_self_hosted_stage(
        runner, stage1, "stage2", arguments, stage_environment, arguments.output
    )
    stage3 = build_self_hosted_stage(
        runner, stage2, "stage3", arguments, stage_environment, arguments.output
    )
    stage2_ir = arguments.output / "stage2.ll"
    stage3_ir = arguments.output / "stage3.ll"
    stage2_ir_hash = sha256(stage2_ir)
    stage3_ir_hash = sha256(stage3_ir)
    if stage2_ir_hash != stage3_ir_hash:
        raise BootstrapFailure(
            f"stage2/stage3 IR differs: {stage2_ir_hash} != {stage3_ir_hash}"
        )
    versions, validations = validate_stages(
        runner, [arguments.stage0, stage1, stage2, stage3], arguments,
        base_environment, arguments.output
    )
    stage_hashes = {
        "stage1": sha256(stage1),
        "stage2": sha256(stage2),
        "stage3": sha256(stage3),
        "stage2_ir": stage2_ir_hash,
        "stage3_ir": stage3_ir_hash,
    }
    report.update(
        {
            "versions": versions,
            "hashes": stage_hashes,
            "ir_deterministic": True,
            "validation_cases": validations,
            "command_count": runner.count,
            "seconds": round(time.monotonic() - started, 3),
            "passed": True,
        }
    )
    (arguments.output / "bootstrap-report.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    checksum_lines = [
        "schema  rocket-bootstrap-report-2",
        "version  rocketc 2.1.0",
        f"host  {detected}",
        f"target  {arguments.target}",
        f"stage1{executable_suffix}  {stage_hashes['stage1']}",
        f"stage2{executable_suffix}  {stage_hashes['stage2']}",
        f"stage3{executable_suffix}  {stage_hashes['stage3']}",
        f"stage2.ll  {stage2_ir_hash}",
        f"stage3.ll  {stage3_ir_hash}",
        "deterministic  true",
        f"validation-cases  {validations}",
    ]
    (arguments.output / "SHA256SUMS.txt").write_text(
        "\n".join(checksum_lines) + "\n", encoding="ascii"
    )
    print(
        f"Rocket bootstrap passed for {arguments.target}: "
        f"stage2/stage3 IR {stage2_ir_hash}; {validations} validation cases"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except BootstrapFailure as error:
        print(f"phase19-bootstrap: error: {error}", file=sys.stderr)
        raise SystemExit(1)
