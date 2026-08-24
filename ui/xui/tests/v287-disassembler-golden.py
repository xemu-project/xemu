#!/usr/bin/env python3
# v2.87 current regression ownership.
"""Static/structural regression guard for bulk/reusable disassembly."""
from __future__ import annotations

import argparse
import pathlib

from v287_source_test_utils import extract_function


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    root = pathlib.Path(ap.parse_args().root).resolve()
    text = (root / "ui/xui/debug-tools/cheat-engine-memory.c").read_text()

    context = text[
        text.index("typedef struct XemuCheatDisasmContext {"):
        text.index("typedef struct XemuCheatDisasmPageCache {")
    ]
    cache_struct = text[
        text.index("typedef struct XemuCheatDisasmPageCache {"):
        text.index("} XemuCheatDisasmPageCache;")
        + len("} XemuCheatDisasmPageCache;")
    ]
    load_page = extract_function(text, "static int xemu_cheat_disasm_load_page(")
    paired = extract_function(text, "int xemu_cheat_disassemble_paired(")
    page = extract_function(text, "int xemu_cheat_disassemble_page(")

    # v1.80: runtime-sized page caches remain pointer backed and page reads
    # stay outside the per-instruction loop.
    assert "uint8_t *bytes;" in cache_struct
    assert "bytes[TARGET_PAGE_SIZE]" not in cache_struct
    assert "g_malloc" not in load_page and "g_free" not in load_page
    assert "memset(cache, 0, sizeof(*cache))" not in load_page
    assert "XemuCheatDisasmPageCache caches[2]" in paired
    assert "xemu_cheat_disasm_window" in paired
    assert "cpu_memory_rw_debug(cpu, (vaddr)(pc + j)" not in paired
    assert "for (j = 0; j < sizeof(code)" not in paired

    # v1.86: Capstone and its three runtime-sized scratch pages are owned by a
    # per-thread reusable context, not opened/allocated/freed on every decode.
    for needle in (
        "G_PRIVATE_INIT(xemu_cheat_disasm_context_free)",
        "cs_open(CS_ARCH_X86, CS_MODE_32, &context->handle)",
        "context->insn = cs_malloc(context->handle)",
        "g_realloc(context->paired_page_bytes[0], page_size)",
        "g_realloc(context->paired_page_bytes[1], page_size)",
        "g_realloc(context->page_bytes, page_size)",
        "cs_free(context->insn, 1)",
        "cs_close(&context->handle)",
    ):
        assert needle in context, needle
    assert "cs_open(" not in paired and "cs_close(" not in paired
    assert "cs_malloc(" not in paired and "cs_free(" not in paired
    assert "xemu_cheat_disasm_context_get(page_size, &context_error)" in paired
    assert "cs_disasm_iter(context->handle" in paired
    assert "context->paired_page_bytes[0]" in paired
    assert "context->paired_page_bytes[1]" in paired

    assert "cs_open(" not in page and "cs_close(" not in page
    assert "cs_malloc(" not in page and "cs_free(" not in page
    assert "xemu_cheat_disasm_context_get(page_size, &context_error)" in page
    assert "context->page_bytes" in page
    assert "cs_disasm_iter(context->handle" in page

    # Physical translation remains one page-base translation in page mode,
    # not one translation per decoded row. Exact focus resync is preserved.
    assert page.count("xemu_cheat_virtual_to_physical_cpu") == 1
    assert "pc < address && pc + context->insn->size > address" in page

    print("PASS: v1.80/v1.86 bulk + reusable Capstone disassembly invariants")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
