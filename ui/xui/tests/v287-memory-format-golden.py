#!/usr/bin/env python3
# v2.87 current regression ownership.
"""Golden tests for hot-path Memory Viewer hexadecimal formatting helpers.

The generated harness compiles the exact helper bodies from the split MemoryTools implementation and
compares their output to snprintf's historical %02X / %08X formatting. This
protects the allocation/formatting cleanup without requiring a full xemu build.
"""
from __future__ import annotations

import argparse
import pathlib
import subprocess
import tempfile
from v287_source_test_utils import extract_function, read_memory_tools_implementation


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--compiler", required=True)
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    source = read_memory_tools_implementation(root / "ui/xui/debug-tools")
    byte_helper = extract_function(source, "static void format_hex_byte(")
    u32_helper = extract_function(source, "static void format_hex_u32(")

    harness = f'''#include <cstdint>\n#include <cstdio>\n#include <cstring>\n#include <random>\n#include <iostream>\n\n{byte_helper}\n\n{u32_helper}\n\nint main() {{\n    for (unsigned value = 0; value < 256; ++value) {{\n        char old_text[3];\n        char new_text[3];\n        std::snprintf(old_text, sizeof(old_text), "%02X", value);\n        format_hex_byte(static_cast<uint8_t>(value), new_text);\n        if (std::strcmp(old_text, new_text) != 0) return 1;\n    }}\n\n    const uint32_t edges[] = {{\n        0u, 1u, 0xFu, 0x10u, 0xFFu, 0x100u, 0xFFFFu,\n        0x10000u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu,\n    }};\n    for (uint32_t value : edges) {{\n        char old_text[9];\n        char new_text[9];\n        std::snprintf(old_text, sizeof(old_text), "%08X", value);\n        format_hex_u32(value, new_text);\n        if (std::strcmp(old_text, new_text) != 0) return 2;\n    }}\n\n    std::mt19937 rng(0x16612u);\n    for (unsigned i = 0; i < 1000000; ++i) {{\n        const uint32_t value = rng();\n        char old_text[9];\n        char new_text[9];\n        std::snprintf(old_text, sizeof(old_text), "%08X", value);\n        format_hex_u32(value, new_text);\n        if (std::strcmp(old_text, new_text) != 0) return 3;\n    }}\n\n    std::cout << "PASS: Memory Viewer hex formatting golden (256 bytes + 1,000,011 u32 values)\\n";\n    return 0;\n}}\n'''

    with tempfile.TemporaryDirectory(prefix="xemu-memory-format-golden-") as tmp:
        cpp = pathlib.Path(tmp) / "memory-format-golden.cpp"
        exe = pathlib.Path(tmp) / "memory-format-golden"
        cpp.write_text(harness, encoding="utf-8")
        subprocess.run([
            args.compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
            str(cpp), "-o", str(exe),
        ], check=True, cwd=root)
        subprocess.run([str(exe)], check=True, cwd=root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
