#!/usr/bin/env python3
"""Compile PO files and emit their MO catalogs as one C++ translation unit."""

import pathlib
import subprocess
import sys
import tempfile


def main() -> int:
    if len(sys.argv) < 4:
        raise SystemExit(
            f"usage: {sys.argv[0]} MSGFMT INPUT.po [INPUT.po ...] OUTPUT.cc"
        )

    msgfmt, *arguments = sys.argv[1:]
    po_files, output = arguments[:-1], arguments[-1]
    lines = [
        "// Generated translation data; do not edit.",
        "#include <cstddef>",
        "void xemu_i18n_register_catalog(const char *, const unsigned char *, std::size_t);",
        "",
    ]
    registrations = []

    with tempfile.TemporaryDirectory(
        prefix="xemu-i18n-", dir=pathlib.Path(output).resolve().parent
    ) as temporary_directory:
        for index, po_file in enumerate(po_files):
            language = pathlib.Path(po_file).stem
            valid_language = language.isascii() and all(
                character.isalnum() or character in "_-.@"
                for character in language
            )
            if not language or not valid_language:
                raise SystemExit(f"unsupported language code in PO filename: {po_file}")

            catalog = pathlib.Path(temporary_directory) / f"catalog-{index}.mo"
            subprocess.run(
                [msgfmt, "--check", f"--output-file={catalog}", po_file], check=True
            )
            data = catalog.read_bytes()
            symbol = f"xemu_catalog_{index}"
            lines.append(f"static const unsigned char {symbol}[] = {{")
            for offset in range(0, len(data), 12):
                chunk = ", ".join(f"0x{byte:02x}" for byte in data[offset : offset + 12])
                lines.append(f"    {chunk},")
            lines.extend(["};", ""])
            registrations.append((language, symbol))

    lines.extend(["void xemu_i18n_register_embedded_catalogs(void)", "{"])
    for language, symbol in registrations:
        lines.append(
            f'    xemu_i18n_register_catalog("{language}", {symbol}, sizeof({symbol}));'
        )
    lines.extend(["}", ""])
    pathlib.Path(output).write_text("\n".join(lines), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
