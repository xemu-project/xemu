#!/usr/bin/env python3
# v2.87 current regression ownership.
"""Source invariants for the Current Game mounted-disc browser."""
from __future__ import annotations

import argparse
import pathlib


def need(text: str, token: str, label: str) -> None:
    if token not in text:
        raise SystemExit(f"FAIL: {label}: missing {token!r}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    args = ap.parse_args()
    root = pathlib.Path(args.root).resolve()
    dt = root / "ui/xui/debug-tools"
    cc = ((dt / "current-game.cc").read_text(encoding="utf-8") + "\n" + (dt / "current-game-ui.cc").read_text(encoding="utf-8"))
    hh = (dt / "current-game.hh").read_text(encoding="utf-8")
    meson = (dt / "meson.build").read_text(encoding="utf-8")
    bridge = (dt / "disc-block-io.c").read_text(encoding="utf-8")

    need(cc, 'xemu_disc_block_by_name("ide0-cd1")', "mounted DVD backend bridge")
    need(bridge, "blk_by_name(name)", "mounted DVD BlockBackend lookup")
    need(bridge, "(BdrvRequestFlags)0", "typed blk_pread request flags")
    need(cc, "XemuXdvdfs::Parse", "XDVDFS parser use")
    need(cc, 'FindRootFile(m_disc, "default.xbe")', "default.xbe lookup")
    need(cc, "sha256_bytes", "complete disc XBE hashing from the one loaded buffer")
    if "sha256_disc_file" in cc:
        raise SystemExit("FAIL: default.xbe must not be reread from DVD only for hashing")
    need(cc, 'BeginTabItem("Disc Contents")', "Disc Contents tab")
    need(cc, 'BeginTabItem("Game Info")', "Game Info tab")
    need(cc, 'ImGui::TextWrapped("XBE SHA-256', "disc XBE SHA display")
    need(hh, "disc_xbe_sha256", "disc XBE metadata")
    need(meson, "'xdvdfs-disc.cc'", "parser build ownership")
    need(meson, "'disc-block-io.c'", "C-only block bridge build ownership")
    if "system/block-backend-" in cc or "blk_pread(" in cc:
        raise SystemExit("FAIL: QEMU BlockBackend C headers/calls must stay out of current-game.cc")
    if "std::ifstream" in cc:
        raise SystemExit("FAIL: Current Game disc reader must use the mounted backend, not reopen the host ISO path")
    need(cc, "read_disc_file(blk", "single mounted-disc XBE read")

    print("PASS: Current Game mounted-disc/XBE SHA source invariants")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
