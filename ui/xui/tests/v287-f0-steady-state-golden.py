#!/usr/bin/env python3
# v2.87 current regression ownership.
"""Regression guard for the steady-state F0 fast path and final dead-code audit.

This test is intentionally source-structural: the full Cheat Engine translation
unit depends on the xemu/QEMU UI build, so the host-native golden suites cannot
link it directly.  We freeze the ordering that makes the optimization behavior-
preserving: all user-visible validation happens first, the exact existing F0
signature is built next, an unchanged installed hook returns before probe
assembly, and every changed/new hook still reaches the original assembler path.
"""
from __future__ import annotations

import argparse
import pathlib
import random
import string
import sys
from v287_source_test_utils import extract_function, read_memory_tools_implementation, read_cheat_engine_implementation


def build_signature(lines: list[str]) -> str:
    # Exact pre-Pass-12 definition signature used by ExecuteTypeF/InstallFHook.
    return "F0\n" + "".join(line + "\n" for line in lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--heavy", action="store_true")
    args = parser.parse_args()
    randomized_iterations = 100_000 if args.heavy else 1_000
    root = pathlib.Path(args.root).resolve()
    debug = root / "ui/xui/debug-tools"

    cheat_cc = read_cheat_engine_implementation(debug)
    body = extract_function(cheat_cc, "bool CheatEngineWindow::ExecuteTypeF(")
    precompile = extract_function(cheat_cc, "void CheatEngineWindow::PrecompileTypeF(")

    ordered = [
        "if (flags != 0)",
        "if (subtype != 0x0 && subtype != 0x1)",
        "if (!code.f_terminated)",
        "if (active_space != GuestAddressSpace::Virtual)",
        "const uint32_t hook_address = active_base + code.value;",
        "if (!code.f_precompiled || !code.f_precompile_ok)",
        "const std::vector<XemuCheatAsmLine> *f0_source",
        "InstallFHook(block_index, hook_key, hook_address",
        "active_hooks.push_back(hook_key);",
        "return true;",
    ]
    last = -1
    for token in ordered:
        pos = body.find(token, last + 1)
        if pos < 0:
            raise AssertionError(f"ExecuteTypeF missing/order regression at `{token}`")
        last = pos


    # Active F hooks are now kept in one reusable vector instead of a fresh
    # unordered_set per block/tick. Freeze the behavior-preserving conditions:
    # ExecuteBlock walks code indices monotonically, ExecuteTypeF appends the
    # current hook key, and retirement uses binary_search on that sorted vector.
    execute_block = extract_function(cheat_cc, "void CheatEngineWindow::ExecuteBlock(")
    active_tokens = (
        "m_address_context_scratch.clear();",
        "std::vector<AddressContext> &contexts = m_address_context_scratch;",
        "m_active_f_hooks_scratch.clear();",
        "std::vector<uint64_t> &f_active_hooks = m_active_f_hooks_scratch;",
        "ExecuteTypeF(block_index, i, code, active_space,",
        "std::binary_search(f_active_hooks.begin(), f_active_hooks.end(),",
    )
    last = -1
    for token in active_tokens:
        pos = execute_block.find(token, last + 1)
        if pos < 0:
            raise AssertionError(f"active F-hook scratch regression at `{token}`")
        last = pos

    # Model set-membership equivalence for monotonically appended source indices.
    # Duplicate values are intentionally included in some cases: set membership
    # and binary_search membership must still agree exactly.
    hook_rng = random.Random(0xAC71F0)
    for _ in range(randomized_iterations):
        block_index = hook_rng.randrange(0, 1 << 16)
        code_indices = sorted(hook_rng.randrange(0, 1 << 20) for _ in range(hook_rng.randrange(0, 64)))
        vec = [((block_index << 32) | idx) for idx in code_indices]
        old_set = set(vec)
        probes = [((block_index << 32) | hook_rng.randrange(0, 1 << 20)) for _ in range(12)]
        probes.extend(vec[:4])
        for probe in probes:
            # Python bisect models std::binary_search over the sorted vector.
            import bisect
            pos = bisect.bisect_left(vec, probe)
            new_has = pos < len(vec) and vec[pos] == probe
            if new_has != (probe in old_set):
                raise AssertionError("active F-hook vector membership mismatch")

    # Freeze the exact canonical F0 signature formula, now built once at parse time.
    required_signature_tokens = (
        "signature_size = 3u",
        "signature_size += src.text.size() + 1u",
        "code.f_definition_signature.reserve(signature_size)",
        r'code.f_definition_signature = "F0\n"',
        "code.f_definition_signature += src.text",
        r"code.f_definition_signature.push_back('\n')",
    )
    for token in required_signature_tokens:
        if token not in precompile:
            raise AssertionError(f"F0 definition signature changed: missing `{token}`")
    if "xemu_cheat_assemble_x86_32(code.f_body, assembled)" not in precompile:
        raise AssertionError("F0 probe no longer precompiled at parse time")
    if "ParseFRawHex(code, code.f_probe_code" not in precompile:
        raise AssertionError("F1 raw bytes no longer precompiled at parse time")
    if "xemu_cheat_assemble_x86_32(code.f_body, assembled)" in body:
        raise AssertionError("steady ExecuteTypeF path reintroduced per-tick F0 assembly")

    # Model signature stability across arbitrary source text. This protects the
    # equality key used by the early-return path from accidental normalization.
    rng = random.Random(0xF0166)
    alphabet = string.ascii_letters + string.digits + " []+*-_,.:$"
    for _ in range(randomized_iterations):
        lines = [
            "".join(rng.choice(alphabet) for _ in range(rng.randrange(0, 64)))
            for _ in range(rng.randrange(0, 24))
        ]
        old = "F0\n"
        for line in lines:
            old += line
            old += "\n"
        if build_signature(lines) != old:
            raise AssertionError("F0 definition signature model mismatch")

    # Final triple-audit dead paths must not silently return.
    combined = "\n".join((
        read_cheat_engine_implementation(debug),
        (debug / "cheat-engine.hh").read_text(encoding="utf-8"),
        read_memory_tools_implementation(debug),
        (debug / "memory-tools.hh").read_text(encoding="utf-8"),
        (debug / "cheat-engine-memory.c").read_text(encoding="utf-8"),
        (debug / "cheat-engine-memory.h").read_text(encoding="utf-8"),
        (debug / "external-code-memory.c").read_text(encoding="utf-8"),
    ))
    removed_symbols = (
        "MemoryToolsWindow::DrawViewer(",
        "MemoryToolsWindow::DrawMemoryMap(",
        "MemoryToolsWindow::LookupVirtualMapping(",
        "m_last_dump_directory",
        "m_last_virtual_translation",
        "m_have_virtual_translation",
        "xemu_cheat_disassemble_virtual",
        "xemu_cheat_external_code_info",
        "group.parent",
        "m_header",
    )
    for symbol in removed_symbols:
        if symbol in combined:
            raise AssertionError(f"removed dead path returned: {symbol}")

    print("PASS: F0 parse-time precompile + steady-state fast path + final dead-code audit")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
