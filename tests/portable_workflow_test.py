#!/usr/bin/env python3
"""Portable CTest workflows for Rocket hardening, compatibility, and tooling."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time


class WorkflowError(RuntimeError):
    pass


def run(arguments: list[str], *, cwd: Path | None = None,
        expected: int = 0) -> subprocess.CompletedProcess[str]:
    completed = subprocess.run(
        arguments, cwd=cwd, env=os.environ.copy(), text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    if completed.returncode != expected:
        command = " ".join(arguments)
        raise WorkflowError(
            f"command returned {completed.returncode}, expected {expected}: "
            f"{command}\n{completed.stdout}{completed.stderr}")
    return completed


def require_pattern(text: str, pattern: str, label: str) -> None:
    if not re.search(pattern, text, re.MULTILINE):
        raise WorkflowError(f"{label} did not match {pattern!r}:\n{text}")


def safe_clean(path: Path, source_dir: Path) -> None:
    resolved = path.resolve()
    allowed = (source_dir / "out").resolve()
    if not resolved.is_relative_to(allowed) or resolved == allowed:
        raise WorkflowError(f"refusing to replace workflow path outside {allowed}: {resolved}")
    if resolved.exists():
        shutil.rmtree(resolved)
    resolved.mkdir(parents=True)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8", newline="\n")


def compiler_case(compiler: Path, arguments: list[str], pattern: str,
                  label: str) -> None:
    completed = run([str(compiler), *arguments])
    require_pattern(completed.stdout + completed.stderr, pattern, label)


def minimizer(args: argparse.Namespace) -> None:
    compiler = args.compiler.resolve()
    source = args.input.resolve()
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    candidate_path = output.parent / "rocket-minimize-candidate.rocket"
    lines = source.read_text(encoding="utf-8").splitlines() or [""]

    def reproduces(candidate: list[str]) -> tuple[bool, int, str]:
        candidate_path.write_text("\n".join(candidate) + "\n", encoding="utf-8",
                                  newline="\n")
        completed = subprocess.run(
            [str(compiler), "check", str(candidate_path)],
            env=os.environ.copy(), text=True, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, check=False)
        text = completed.stdout + completed.stderr
        return (completed.returncode == args.expected_exit and
                (not args.match or re.search(args.match, text) is not None),
                completed.returncode, text)

    try:
        matched, status, text = reproduces(lines)
        if not matched:
            raise WorkflowError(
                f"input does not reproduce exit {args.expected_exit} and "
                f"pattern {args.match!r}; exit={status}\n{text}")
        granularity = 2
        while len(lines) > 1:
            chunk_size = (len(lines) + granularity - 1) // granularity
            reduced = False
            for start in range(0, len(lines), chunk_size):
                candidate = lines[:start] + lines[start + chunk_size:]
                if not candidate:
                    candidate = [""]
                if reproduces(candidate)[0]:
                    lines = candidate
                    granularity = max(2, granularity - 1)
                    reduced = True
                    break
            if not reduced:
                if granularity >= len(lines):
                    break
                granularity = min(len(lines), granularity * 2)
        output.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
        if not reproduces(lines)[0]:
            raise WorkflowError("minimized output no longer reproduces")
        print(f"Minimized compiler reproducer to {len(lines)} line(s): {output}")
    finally:
        candidate_path.unlink(missing_ok=True)


def compatibility(args: argparse.Namespace) -> None:
    compiler = args.compiler.resolve()
    source_dir = args.source_dir.resolve()
    work = args.work.resolve()
    safe_clean(work, source_dir)
    packages = work / "phase16-packages"
    shutil.copytree(source_dir / "tests" / "fixtures" / "phase16_packages",
                    packages, ignore=shutil.ignore_patterns(".rocketc"))
    fixtures = source_dir / "tests" / "fixtures"
    cases = [
        ("2.1", "compiler-version", ["--version"], r"^rocketc 2\.1\.0$"),
        ("1.0", "hello-source", ["run", str(source_dir / "examples" / "hello.rocket")], r"Hello from Rocket"),
        ("1.1", "collections-source", ["check", str(fixtures / "phase11_map_set_tuple.rocket")], r"check succeeded"),
        ("1.2", "traits-source", ["check", str(fixtures / "phase12_traits.rocket")], r"check succeeded"),
        ("1.3", "native-package", ["check", str(fixtures / "phase13_native_package")], r"check succeeded"),
        ("1.4", "raylib-package", ["check", str(source_dir / "examples" / "raylib_showcase")], r"check succeeded"),
        ("1.5", "standard-library-source", ["check", str(fixtures / "phase15_text_streams.rocket")], r"check succeeded"),
        ("1.6", "package-resolution", ["resolve", str(packages / "app")], r"resolved 3 package"),
        ("1.6", "locked-package-source", ["check", str(packages / "app")], r"check succeeded"),
        ("1.7", "machine-message-schema", ["check", str(source_dir / "examples" / "hello.rocket"), "--message-format=json"], r"rocket-message-1"),
        ("1.8", "ownership-concurrency-source", ["check", str(source_dir / "examples" / "ownership_concurrency.rocket")], r"check succeeded"),
    ]
    results: list[dict[str, str]] = []
    for release, name, command, pattern in cases:
        compiler_case(compiler, command, pattern, name)
        results.append({"release": release, "name": name, "status": "passed"})
    report = work / f"rocket-2.1-{args.configuration.lower()}.json"
    write_json(report, {
        "schema": "rocket-compatibility-1", "version": "2.1.0",
        "target": args.target, "configuration": args.configuration,
        "compiler_sha256": sha256_file(compiler), "cases": results,
    })
    print(f"Rocket 2.1 compatibility passed: {len(results)} release-line cases ({report})")


def application(args: argparse.Namespace) -> None:
    compiler = args.compiler.resolve()
    source_dir = args.source_dir.resolve()
    work = args.work.resolve()
    safe_clean(work, source_dir)
    for index in range(args.package_count):
        name = f"layer_{index:03d}"
        root = work / name
        (root / "src").mkdir(parents=True)
        manifest = (
            f'[package]\nname = "{name}"\nversion = "1.0.0"\n'
            'license = "MIT"\nentry = "src/main.rocket"\n')
        if index == 0:
            source = "pub fn value() -> Int:\n    return 1\n"
        else:
            previous = f"layer_{index - 1:03d}"
            manifest += f'\n[dependencies]\n{previous} = "path:../{previous}"\n'
            source = (f"import {previous}\n\npub fn value() -> Int:\n"
                      f"    return {previous}.value() + 1\n")
        (root / "rocket.toml").write_text(manifest, encoding="utf-8", newline="\n")
        (root / "src" / "main.rocket").write_text(source, encoding="utf-8", newline="\n")
    last = f"layer_{args.package_count - 1:03d}"
    app = work / "application"
    (app / "src").mkdir(parents=True)
    (app / "rocket.toml").write_text(
        '[package]\nname = "phase20_application"\nversion = "2.1.0"\n'
        'license = "MIT"\nentry = "src/main.rocket"\n\n'
        f'[dependencies]\n{last} = "path:../{last}"\n',
        encoding="utf-8", newline="\n")
    (app / "src" / "main.rocket").write_text(
        f"import {last}\n\nfn main() -> Int:\n    print({last}.value())\n    return 0\n",
        encoding="utf-8", newline="\n")
    compiler_case(compiler, ["resolve", str(app)], r"resolved", "resolve")
    compiler_case(compiler, ["build", str(app)], r"built", "build")
    for index in range(args.iterations):
        compiler_case(compiler, ["run", str(app)],
                      rf"(?m)^{args.package_count}$", f"run-{index}")
    processes: list[subprocess.Popen[str]] = []
    for suffix in ("a", "b"):
        root = work / f"parallel_{suffix}"
        (root / "src").mkdir(parents=True)
        (root / "rocket.toml").write_text(
            f'[package]\nname = "parallel_{suffix}"\nversion = "2.1.0"\n'
            'entry = "src/main.rocket"\n', encoding="utf-8", newline="\n")
        (root / "src" / "main.rocket").write_text(
            "fn main() -> Int:\n    return 0\n", encoding="utf-8", newline="\n")
        processes.append(subprocess.Popen(
            [str(compiler), "build", str(root)], env=os.environ.copy(),
            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE))
    for process in processes:
        stdout, stderr = process.communicate()
        if process.returncode != 0:
            raise WorkflowError(f"parallel package build failed:\n{stdout}{stderr}")
    compiler_case(compiler, ["test", str(source_dir / "examples" / "raylib_showcase")],
                  r"4 passed; 0 failed", "raylib headless tests")
    compiler_case(compiler, ["run", str(source_dir / "examples" / "ownership_concurrency.rocket")],
                  r"41[\r\n]+3[\r\n]+42", "ownership application")
    report = work / "application-validation.json"
    write_json(report, {
        "schema": "rocket-application-validation-1", "version": "2.1.0",
        "target": args.target, "configuration": args.configuration,
        "package_count": args.package_count + 1,
        "repeated_runs": args.iterations, "parallel_package_builds": 2,
        "raylib_headless_tests": 4,
        "ownership_concurrency_application": "passed",
        "compiler_sha256": sha256_file(compiler),
    })
    print(f"Rocket 2.1 application validation passed: {report}")


def json_lines(text: str, label: str) -> list[dict[str, object]]:
    values: list[dict[str, object]] = []
    for line in text.splitlines():
        if not line.strip():
            continue
        value = json.loads(line)
        if value.get("schema") != "rocket-message-1":
            raise WorkflowError(f"{label} message has an unexpected schema: {line}")
        values.append(value)
    return values


def tooling(args: argparse.Namespace) -> None:
    compiler = args.compiler.resolve()
    source_dir = args.source_dir.resolve()
    work = args.work.resolve()
    safe_clean(work, source_dir)
    hello = source_dir / "examples" / "hello.rocket"
    outputs = {
        "coverage": ("coverage", work / "coverage.json", "rocket-coverage-1"),
        "profile": ("profile", work / "profile.json", "rocket-profile-1"),
        "benchmark": ("benchmark", work / "benchmark.json", "rocket-benchmark-1"),
    }
    for label, (command, path, schema) in outputs.items():
        arguments = [str(compiler), command, str(hello)]
        if command == "benchmark":
            arguments.extend(["--iterations", "3"])
        arguments.extend(["--output", str(path)])
        run(arguments)
        if json.loads(path.read_text(encoding="utf-8")).get("schema") != schema:
            raise WorkflowError(f"{label} schema mismatch")
    check = json_lines(run([str(compiler), "check", str(hello),
                            "--message-format=json"]).stdout, "check")
    build = json_lines(run([str(compiler), "build", str(hello),
                            "--message-format=json"]).stdout, "build")
    tests = json_lines(run([str(compiler), "test",
                            str(source_dir / "tests" / "fixtures" / "phase15_test_package"),
                            "--message-format=json"]).stdout, "test")
    if "build-finished" not in {value.get("reason") for value in check}:
        raise WorkflowError("machine-readable check completion missing")
    if "build-finished" not in {value.get("reason") for value in build}:
        raise WorkflowError("machine-readable build completion missing")
    if "test-summary" not in {value.get("reason") for value in tests}:
        raise WorkflowError("machine-readable test summary missing")
    print(f"Rocket coverage, profile, benchmark, and machine-output validation passed: {work}")


def artifact_directory(source: Path, target: str) -> Path:
    configured = os.environ.get("ROCKET_ARTIFACT_ROOT", "")
    root = Path(configured).resolve() / source.stem if configured else source.parent
    return root / ".rocketc" / "targets" / target


def debugging(args: argparse.Namespace) -> None:
    compiler = args.compiler.resolve()
    source_dir = args.source_dir.resolve()
    work = args.work.resolve()
    safe_clean(work, source_dir)
    source = source_dir / "tests" / "fixtures" / "phase6_types.rocket"
    artifact = artifact_directory(source, args.target)
    executable_suffix = ".exe" if args.target == "windows-x64" else ""
    records: list[dict[str, object]] = []
    llvm_bin = args.llvm_bin.resolve()
    for name, extra, optimized in (("debug", ["--debug"], False),
                                    ("optimized", [], True)):
        run([str(compiler), "build", str(source), *extra])
        case = work / name
        case.mkdir()
        executable = artifact / f"phase6_types{executable_suffix}"
        source_map = artifact / "phase6_types.rocket.map.json"
        copied_executable = case / executable.name
        copied_map = case / source_map.name
        shutil.copy2(executable, copied_executable)
        shutil.copy2(source_map, copied_map)
        mapping = json.loads(copied_map.read_text(encoding="utf-8"))
        if (mapping.get("format") != "rocket-source-map-1" or
                bool(mapping.get("optimized")) != optimized or
                len(mapping.get("functions", [])) < 2):
            raise WorkflowError(f"{name} Rocket source map is incomplete")
        debug_hash = ""
        if args.target == "windows-x64":
            pdb = artifact / "phase6_types.pdb"
            copied_pdb = case / pdb.name
            shutil.copy2(pdb, copied_pdb)
            pdbutil = llvm_bin / "llvm-pdbutil.exe"
            text = run([str(pdbutil), "dump", "--modi=0", "--symbols",
                        "--files", "-l", str(copied_pdb)]).stdout
            for pattern in (r"rocket:\\source\\phase6_types\.rocket",
                            r"line/column/addr entries", r"S_GPROC32",
                            r"S_(LOCAL|CONSTANT)"):
                require_pattern(text, pattern, f"{name} PDB")
            debug_hash = sha256_file(copied_pdb)
        else:
            dwarfdump = llvm_bin / "llvm-dwarfdump"
            text = run([str(dwarfdump), "--debug-info", "--debug-line",
                        str(copied_executable)]).stdout
            for pattern in (r"phase6_types\.rocket", r"DW_TAG_subprogram",
                            r"DW_TAG_(variable|formal_parameter)", r"debug_line"):
                require_pattern(text, pattern, f"{name} DWARF")
            debug_hash = sha256_file(copied_executable)
        records.append({
            "configuration": name, "optimized": optimized,
            "executable_sha256": sha256_file(copied_executable),
            "debug_information_sha256": debug_hash,
            "source_map_sha256": sha256_file(copied_map),
            "functions": len(mapping["functions"]),
        })
    panic = source_dir / "tests" / "fixtures" / "int_overflow.rocket"
    run([str(compiler), "build", str(panic), "--debug"])
    panic_map = artifact_directory(panic, args.target) / "int_overflow.rocket.map.json"
    panic_text = panic_map.read_text(encoding="utf-8")
    if "int_overflow.rocket" not in panic_text or '"line"' not in panic_text:
        raise WorkflowError("panic source map lacks the failing Rocket location")
    report = work / "report.json"
    write_json(report, {
        "schema": "rocket-debug-validation-1",
        "debugger_contract": ("CodeView/PDB plus rocket-source-map-1"
                              if args.target == "windows-x64"
                              else "DWARF plus rocket-source-map-1"),
        "target": args.target, "configurations": records,
        "panic_location": "int_overflow.rocket",
    })
    print(f"Rocket debug validation passed: {report}")


def repl(args: argparse.Namespace) -> None:
    compiler = args.compiler.resolve()
    source_dir = args.source_dir.resolve()
    work = args.work.resolve()
    safe_clean(work, source_dir)
    source = work / "session.rocket"
    body = ["fn main() -> Int:"]
    body.extend(f"    print({expression})" for expression in args.expression)
    body.append("    return 0")
    source.write_text("\n".join(body) + "\n", encoding="utf-8", newline="\n")
    started = time.perf_counter()
    completed = run([str(compiler), "run", str(source)])
    elapsed = round((time.perf_counter() - started) * 1000, 3)
    sys.stdout.write(completed.stdout)
    print(json.dumps({
        "schema": "rocket-repl-evaluation-1", "status": 0,
        "elapsed_ms": elapsed,
        "model": "incremental source accumulation with cached AOT artifacts",
    }, separators=(",", ":")))


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser()
    commands = result.add_subparsers(dest="command", required=True)
    minimum = commands.add_parser("minimizer")
    minimum.add_argument("--compiler", type=Path, required=True)
    minimum.add_argument("--input", type=Path, required=True)
    minimum.add_argument("--output", type=Path, required=True)
    minimum.add_argument("--expected-exit", type=int, required=True)
    minimum.add_argument("--match", default="")
    for name in ("compatibility", "application", "tooling", "debugging", "repl"):
        command = commands.add_parser(name)
        command.add_argument("--compiler", type=Path, required=True)
        command.add_argument("--source-dir", type=Path, required=True)
        command.add_argument("--work", type=Path, required=True)
        if name in ("compatibility", "application"):
            command.add_argument("--configuration", required=True)
            command.add_argument("--target", required=True)
        if name == "application":
            command.add_argument("--package-count", type=int, default=16)
            command.add_argument("--iterations", type=int, default=3)
        if name == "debugging":
            command.add_argument("--target", required=True)
            command.add_argument("--llvm-bin", type=Path, required=True)
        if name == "repl":
            command.add_argument("--expression", action="append", required=True)
    return result


def main() -> int:
    args = parser().parse_args()
    actions = {
        "minimizer": minimizer, "compatibility": compatibility,
        "application": application, "tooling": tooling,
        "debugging": debugging, "repl": repl,
    }
    try:
        actions[args.command](args)
        return 0
    except (WorkflowError, OSError, ValueError, json.JSONDecodeError) as error:
        print(f"portable Rocket workflow failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
