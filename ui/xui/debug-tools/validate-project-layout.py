#!/usr/bin/env python3
"""Validate xemu RAW Cheat Engine / Debugger ownership invariants.

This is intentionally a source-layout/build regression guard. It does not inspect
or change guest/runtime behavior.  Keep substantive custom debugger/cheat build
logic under ui/xui/debug-tools/ and leave upstream files as small integration
hooks.
"""

from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path


EXPECTED_BUILD_SH_SHA256 = "cedee87154310482803feea1099a803e6510ab0f0d4abdddc3e53d25e3719ad3"
CAPSTONE_HELPER = "ui/xui/debug-tools/build-capstone.sh"
CAPSTONE_WINDOWS_WRAPPER = "ui/xui/debug-tools/build-capstone-windows.sh"
EXECUTABLE_HELPER = "ui/xui/debug-tools/restore-executable-bits.py"
VALIDATOR = "ui/xui/debug-tools/validate-project-layout.py"
REGRESSION_RUNNER = "ui/xui/debug-tools/tests/v287-run-regression-tests.py"
ASSEMBLER_GOLDEN = "ui/xui/debug-tools/tests/assembler-golden.cpp"
ALLOCATOR_GOLDEN = "ui/xui/debug-tools/tests/v287-allocator-golden.py"
SEARCH_COMPARE_GOLDEN = "ui/xui/debug-tools/tests/v287-search-compare-golden.py"
F0_STEADY_GOLDEN = "ui/xui/debug-tools/tests/v287-f0-steady-state-golden.py"
MEMORY_FORMAT_GOLDEN = "ui/xui/debug-tools/tests/v287-memory-format-golden.py"
PASS3_STRUCTURAL_GOLDEN = "ui/xui/debug-tools/tests/v287-memory-tools-structural-refactor-golden.py"
PASS9_AUDIT_GOLDEN = "ui/xui/debug-tools/tests/v287-audit-pruning-cleanup-golden.py"

UPSTREAM_WORKFLOW_SHA256 = {
    ".github/workflows/build.yml": "44ada01c457ecf5f0195e71b71db17a0df4700ceb5ef3f424ce5d3dc5e1d616e",
    ".github/workflows/build-windows.yml": "fa6e7bdb576d4f9491ee1a12da2c8394849a9ca1a6e7e12bb4b003a5c0f54604",
    ".github/workflows/build-linux.yml": "86bf7d1b97b42258e193e5a319ad58fbde01a94a29bace186d25014530969ea7",
    ".github/workflows/build-macos.yml": "21bed54422c484dd9dffb604c8e68985b0c3370113d963d92d4d50857d240aa9",
    ".github/workflows/release.yml": "1ebd3de33da42c27cceb5a40c57b1727eff2c3193dd31a2046a3bb83c42b025f",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_text(path: Path, errors: list[str]) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except OSError as exc:
        errors.append(f"cannot read {path}: {exc}")
        return ""


def require_file(root: Path, rel: str, errors: list[str]) -> Path:
    path = root / rel
    if not path.is_file():
        errors.append(f"required file is missing: {rel}")
    return path


def require_once(text: str, needle: str, where: str, errors: list[str]) -> None:
    count = text.count(needle)
    if count != 1:
        errors.append(f"{where}: expected exactly one `{needle}` reference, found {count}")


def validate(root: Path) -> list[str]:
    errors: list[str] = []

    capstone = require_file(root, CAPSTONE_HELPER, errors)
    capstone_windows = require_file(root, CAPSTONE_WINDOWS_WRAPPER, errors)
    executable = require_file(root, EXECUTABLE_HELPER, errors)
    require_file(root, VALIDATOR, errors)
    require_file(root, REGRESSION_RUNNER, errors)
    require_file(root, ASSEMBLER_GOLDEN, errors)
    require_file(root, ALLOCATOR_GOLDEN, errors)
    require_file(root, SEARCH_COMPARE_GOLDEN, errors)
    require_file(root, F0_STEADY_GOLDEN, errors)
    require_file(root, MEMORY_FORMAT_GOLDEN, errors)
    require_file(root, PASS3_STRUCTURAL_GOLDEN, errors)
    require_file(root, PASS9_AUDIT_GOLDEN, errors)
    build_sh = require_file(root, "build.sh", errors)
    workflow_paths = {rel: require_file(root, rel, errors)
                      for rel in UPSTREAM_WORKFLOW_SHA256}

    obsolete_helper = root / "scripts/restore-executable-bits.py"
    if obsolete_helper.exists():
        errors.append(
            "obsolete scripts/restore-executable-bits.py returned; keep this helper under ui/xui/debug-tools/"
        )

    if build_sh.is_file():
        actual = sha256(build_sh)
        if actual != EXPECTED_BUILD_SH_SHA256:
            errors.append(
                "build.sh no longer matches the accepted Debug Tools integration "
                f"(expected {EXPECTED_BUILD_SH_SHA256}, got {actual})"
            )

    # Fork policy: preserve xemu's upstream multi-platform workflows exactly.
    # Debug Tools helpers are consumed by the separate local Docker builder.
    for rel, expected in UPSTREAM_WORKFLOW_SHA256.items():
        path = workflow_paths[rel]
        if path.is_file():
            actual = sha256(path)
            if actual != expected:
                errors.append(
                    f"{rel}: upstream workflow changed "
                    f"(expected {expected}, got {actual})"
                )

    regression_runner = root / REGRESSION_RUNNER
    regression_text = read_text(regression_runner, errors) if regression_runner.is_file() else ""
    for required_test in ("v287-allocator-golden.py", "v287-search-compare-golden.py",
                          "v287-f0-steady-state-golden.py", "v287-memory-format-golden.py",
                          "v287-memory-tools-structural-refactor-golden.py",
                          "v287-audit-pruning-cleanup-golden.py"):
        if required_test not in regression_text:
            errors.append(f"{REGRESSION_RUNNER}: required test `{required_test}` is not invoked")

    # Make sure the source-owned target-aware Capstone bootstrap stays generic
    # and the legacy Windows helper remains only a compatibility wrapper.
    if capstone.is_file():
        capstone_text = read_text(capstone, errors)
        if not capstone_text.startswith("#!/usr/bin/env bash"):
            errors.append(f"{CAPSTONE_HELPER}: expected bash shebang")
        for token in (
            "CAPSTONE_X86_SUPPORT=ON",
            "win64-cross",
            "Darwin",
            "Linux",
            "DEB_HOST_GNU_TYPE",
            "CMAKE_OSX_ARCHITECTURES",
            "CAPSTONE_PKG_CONFIG",
            "probe_capstone",
        ):
            if token not in capstone_text:
                errors.append(f"{CAPSTONE_HELPER}: target-aware bootstrap token missing: {token}")
    if capstone_windows.is_file():
        wrapper_text = read_text(capstone_windows, errors)
        if "build-capstone.sh" not in wrapper_text:
            errors.append(f"{CAPSTONE_WINDOWS_WRAPPER}: must delegate to build-capstone.sh")
        if "cmake -S" in wrapper_text or "capstone-engine/capstone" in wrapper_text:
            errors.append(f"{CAPSTONE_WINDOWS_WRAPPER}: implementation duplicated instead of delegating")

    if build_sh.is_file():
        build_text = read_text(build_sh, errors)
        for token in (
            'debug_tools_capstone_helper="${project_source_dir}/ui/xui/debug-tools/build-capstone.sh"',
            'XEMU_DEBUG_TOOLS_SKIP_CAPSTONE_BOOTSTRAP',
            '--platform "${platform}"',
            '--arch "${target_arch}"',
            '--enable-capstone',
        ):
            if token not in build_text:
                errors.append(f"build.sh: Debug Tools Capstone bootstrap token missing: {token}")

    if executable.is_file():
        executable_text = read_text(executable, errors)
        if not executable_text.startswith("#!/usr/bin/env python3"):
            errors.append(f"{EXECUTABLE_HELPER}: expected python3 shebang")
        if "--update-git-index" not in executable_text:
            errors.append(f"{EXECUTABLE_HELPER}: Git index update option is missing")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=None, help="xemu source tree root")
    args = parser.parse_args()

    root = Path(args.root).resolve() if args.root else Path(__file__).resolve().parents[3]
    errors = validate(root)
    if errors:
        print("Debug-tools ownership/layout validation FAILED:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    print("Debug-tools ownership/layout validation passed.")
    print(f"  Capstone helper: {CAPSTONE_HELPER}")
    print(f"  Windows compatibility wrapper: {CAPSTONE_WINDOWS_WRAPPER}")
    print(f"  Executable-bit helper: {EXECUTABLE_HELPER}")
    print(f"  build.sh Debug Tools integration SHA-256: {EXPECTED_BUILD_SH_SHA256}")
    print("  Upstream GitHub workflows: preserved")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
