#!/usr/bin/env python3
"""Compile a PO file and emit the resulting MO catalog as C++ data."""

import pathlib
import subprocess
import sys
import tempfile


def main() -> int:
    if len(sys.argv) != 4:
        raise SystemExit(f"usage: {sys.argv[0]} MSGFMT INPUT.po OUTPUT.cc")

    msgfmt, po_file, output = sys.argv[1:]
    with tempfile.TemporaryDirectory() as temporary_directory:
        catalog = pathlib.Path(temporary_directory) / "catalog.mo"
        subprocess.run(
            [msgfmt, "--check", f"--output-file={catalog}", po_file], check=True
        )
        data = catalog.read_bytes()

    lines = [
        f"// Generated from {pathlib.Path(po_file).as_posix()}; do not edit.",
        "#include <cstddef>",
        "",
        "extern const unsigned char xemu_sv_catalog[] = {",
    ]
    for offset in range(0, len(data), 12):
        chunk = ", ".join(f"0x{byte:02x}" for byte in data[offset : offset + 12])
        lines.append(f"    {chunk},")
    lines.extend(
        [
            "};",
            "",
            "extern const size_t xemu_sv_catalog_size = sizeof(xemu_sv_catalog);",
            "",
        ]
    )
    pathlib.Path(output).write_text("\n".join(lines), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
