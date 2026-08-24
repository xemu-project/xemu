#!/usr/bin/env python3
# v2.87 current regression ownership.
"""Regression guard for v1.95 OPT Pass 10 lifecycle/resource hardening."""
from __future__ import annotations

import argparse
import hashlib
import pathlib

from v287_source_test_utils import (
    extract_function, extract_member_functions, read_cheat_engine_implementation,
    strip_preentry_cheat_header_additions,
)


PROTECTED_V194_FILES = {
    "cheat-engine-memory.c": "64f7027881f3f29a0c95a4c1fcf350bf39d06e42f4508955b9bdeefde50a359d",
    "cheat-engine-memory.h": "e47bb779c456cc58bc3bd6e7852c1f72332af4c35d8d82f6195eb1e62bc79d90",
    "external-code-memory.c": "149d9da2cbe8187b6af0448d61024ca5f9ec2d9cc7364ce56188fd6a4eaf580d",
    "memory-tools.hh": "42df7062c462dd16bccd64fcaa7fe9b39de5808b6898ea89a975be84bfbeb847",
    "memory-tools-inject.cc": "07ca236adbe9b879b69b0557cd11189e1deaa50e0509fb7525a82d418d9d4609",
    "backend/xemu-dbg.c": "0fff339f0c480f2654d8d8805820fae462cff11b85a1c7e11a23ca074429c1ef",
    "backend/whpx-debug.c": "7ba0d7e6d5efacf298ff07520c6bb40df7a947e925b8ef9c05f7e8cac8b66368",
}
CHEAT_HEADER_V194_SHA256 = "3580c3193a0bfbe0e3ffbf6fc72b8c46f92a21614759e6f73304b59a96fbb6c1"
XEMU_XBE_V194_SHA256 = "aeb0ecdf66d17f73719c0cd663c2d8f5a5dc3b5b0c73b55973252925946881d2"
CHEAT_REMAINING_V194_SHA256 = "62f074e005d0835e8774c5a1e70d4725b82334d2d4e935d3f3e90719a5d5b293"
MEMORY_REMAINING_V194_SHA256 = "51a1311acc22c477382080deb40014b07c623908aec496f7e60ed9656f8a677a"

PREENTRY_SCOPED_CHEAT_METHODS = {
    "MaybeAutoLoadCurrentGame",
    "NotifyGameResetRequested",
    "ObserveRequestedGameReset",
    "BlockIdentityKey",
    "PreEntrySelectionKey",
    "IsPreEntrySelected",
    "RememberPreEntrySelection",
    "ApplySelectedPreEntryPatches",
    "ConsumePreEntryPrefix",
    "ParseSource",
    "LoadMatchingCurrentGameFile",
    "SetGroupSelected",
    "SetPatchGroupSelected",
    "DisableAllCheats",
    "DeactivateLiveFHooks",
    "ForgetFHookOwnershipForNewGuest",
    "CountGroupSelection",
    "CountPatchGroupSelection",
    "DrawCheat",
    "DrawPatch",
    "DrawGroup",
    "DrawPatchGroup",
    "Tick",
    "DrawMenuBar",
    "Draw",
}


def sha256(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def member_digest(texts: list[str], class_name: str, excluded: set[str]) -> tuple[str, int]:
    digest = hashlib.sha256()
    count = 0
    for text in texts:
        for name, body in extract_member_functions(text, class_name):
            if name in excluded:
                continue
            digest.update(name.encode("utf-8"))
            digest.update(b"\0")
            digest.update(body.encode("utf-8"))
            digest.update(b"\0")
            count += 1
    return digest.hexdigest(), count



def member_digest_sorted(texts: list[str], class_name: str, excluded: set[str]) -> tuple[str, int]:
    records: list[tuple[str, str]] = []
    for text in texts:
        for name, body in extract_member_functions(text, class_name):
            if name in excluded:
                continue
            records.append((name, body))
    records.sort(key=lambda item: (item[0], item[1]))
    digest = hashlib.sha256()
    for name, body in records:
        digest.update(name.encode("utf-8"))
        digest.update(b"\0")
        digest.update(body.encode("utf-8"))
        digest.update(b"\0")
    return digest.hexdigest(), len(records)

def require(text: str, needle: str, what: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing {what}: {needle}")



def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    root = pathlib.Path(parser.parse_args().root).resolve()
    debug = root / "ui/xui/debug-tools"

    # The behavior-bearing bridges and every explicitly protected prior-pass
    # runtime file remain byte-identical to v1.94. The v2.74 Inject pair is
    # delegated to its exact dedicated fingerprint guard.
    v274_scoped_files = {
        "memory-tools.hh", "memory-tools-inject.cc",
        "cheat-engine-memory.h",  # v2.86 removes only a stale orphan comment.
    }
    for rel, expected in PROTECTED_V194_FILES.items():
        if rel in v274_scoped_files:
            continue
        actual = sha256(debug / rel)
        if actual != expected:
            raise AssertionError(f"protected v1.94 runtime file changed: {rel}")
    if sha256(root / "xemu-xbe.c") != XEMU_XBE_V194_SHA256:
        raise AssertionError("v1.90 XBE polling/scratch behavior changed")

    cheat_hh = (debug / "cheat-engine.hh").read_text(encoding="utf-8")
    stripped_hh = strip_preentry_cheat_header_additions(cheat_hh)
    if hashlib.sha256(stripped_hh.encode("utf-8")).hexdigest() != CHEAT_HEADER_V194_SHA256:
        raise AssertionError(
            "cheat-engine.hh changed outside the separately guarded PREENTRY/Patch scope"
        )

    cheat = read_cheat_engine_implementation(debug)
    guard = (debug / "guest-pause-guard.hh").read_text(encoding="utf-8")
    guard += "\n" + (debug / "guest-pause-guard.cc").read_text(encoding="utf-8")
    dump = (debug / "memory-tools-dump.cc").read_text(encoding="utf-8")
    labels = (debug / "memory-tools-labels.cc").read_text(encoding="utf-8")

    # Shared pause ownership: only the guard owns transactional start/stop
    # symmetry. It resumes exactly once and never resumes a guest that was
    # already stopped when the transaction began.
    for needle in (
        "m_was_running(runstate_is_running())",
        "const int stop_result = vm_stop(RUN_STATE_PAUSED);",
        "const bool stopped = !runstate_is_running();",
        "m_resume = stopped;",
        "m_valid = stop_result == 0 && stopped;",
        "~XemuDebugGuestPauseGuard()",
        "Resume();",
        "vm_start();",
        "m_resume = false;",
    ):
        require(guard, needle, "scoped guest pause contract")
    require(guard, "bool IsValid() const", "coherent-read pause validity API")
    if "class TypeFGuestPauseGuard" in cheat:
        raise AssertionError("duplicate Type-F-only pause guard implementation returned")
    require(cheat, "using TypeFGuestPauseGuard = XemuDebugGuestPauseGuard;",
            "Type-F compatibility alias to shared pause owner")

    install = extract_function(cheat, "bool CheatEngineWindow::InstallFHook(")
    require(install, "auto cleanup_failed_install = [&]()", "single failed-install cleanup helper")
    require(install, "state.retired_may_be_referenced = false;", "never-reachable failed install marker")
    require(install, "ReleaseFHookCaveIfSafe(state);", "failed install resource release")
    if install.count("cleanup_failed_install();") != 5:
        raise AssertionError("all five post-allocation install failure paths must share rollback")

    release = extract_function(cheat, "bool CheatEngineWindow::ReleaseFHookCaveIfSafe(FHookState &state)")
    deactivate = extract_function(cheat, "void CheatEngineWindow::DeactivateFHook(uint64_t key)")
    for name, fn in (("ReleaseFHookCaveIfSafe", release), ("DeactivateFHook", deactivate)):
        require(fn, "TypeFGuestPauseGuard guest_pause;", f"{name} scoped pause")
        if "const bool was_running = runstate_is_running();" in fn or "vm_start();" in fn:
            raise AssertionError(f"{name} still owns manual resume/error-path bookkeeping")

    # Every other CheatEngineWindow method remains exact v1.94 code. Use a sorted
    # digest so translation-unit ownership changes do not change the behavior freeze.
    # This protects
    # F0/F1 lifecycle, Tick ordering, debugger CodeCave ownership, UI, and code
    # execution semantics outside the explicitly hardened ownership methods.
    digest, count = member_digest_sorted(
        [cheat], "CheatEngineWindow",
        {"InstallFHook", "ReleaseFHookCaveIfSafe", "DeactivateFHook"}
        | PREENTRY_SCOPED_CHEAT_METHODS,
    )
    if (digest, count) != (CHEAT_REMAINING_V194_SHA256, 57):
        raise AssertionError("unscoped CheatEngineWindow method changed from v1.94")

    dump_page = extract_function(dump, "void MemoryToolsWindow::DumpCurrentPage(")
    require(dump_page, "XemuDebugGuestPauseGuard guest_pause;", "page-dump scoped pause")
    if "vm_start();" in dump_page or "const bool was_running" in dump_page:
        raise AssertionError("page dump still owns manual early-return resume paths")

    dump_labels = extract_function(labels, "void MemoryToolsWindow::DumpLabels()")
    require(dump_labels, "XemuDebugGuestPauseGuard guest_pause;", "label-dump scoped pause")
    require(dump_labels, "guest_pause.Resume();", "historical label-dump resume boundary")
    close_pos = dump_labels.index("std::fclose(fp)")
    resume_pos = dump_labels.index("guest_pause.Resume();")
    status_pos = dump_labels.index("char status[640]")
    if not (close_pos < resume_pos < status_pos):
        raise AssertionError("label dump resume boundary moved from v1.94")

    # Full-range RAM dump is intentionally different: it pauses and deliberately
    # leaves the VM paused. Never accidentally convert that user-visible behavior
    # to a scoped auto-resume transaction.
    dump_ram = extract_function(dump, "void MemoryToolsWindow::DumpRam(")
    require(dump_ram, "Leave\n    // the VM paused afterward", "full dump leave-paused contract")
    require(dump_ram, "vm_stop(RUN_STATE_PAUSED);", "full dump explicit pause")
    if "XemuDebugGuestPauseGuard" in dump_ram or "vm_start();" in dump_ram:
        raise AssertionError("full RAM dump must continue to leave the VM paused")

    memory_texts = [
        (debug / name).read_text(encoding="utf-8")
        for name in (
            "memory-tools.cc", "memory-tools-memory.cc",
            "memory-tools-memory-ui.cc", "memory-tools-search.cc",
            "memory-tools-search-ui.cc",
            "memory-tools-debugger.cc", "memory-tools-debugger-ui.cc",
            "memory-tools-inject.cc",
            "memory-tools-labels.cc", "memory-tools-labels-ui.cc", "memory-tools-dump.cc",
            "memory-tools-dump-ui.cc",
        )
    ]
    digest, count = member_digest_sorted(
        memory_texts, "MemoryToolsWindow", {
            "DumpCurrentPage", "DumpLabels", "DrawRegisters", "Draw",
            "InjectNop", "OpenInstructionChanger", "ApplyInstructionChange",
            "RestoreInstructionChange", "RestoreTrackedInstructionPatch",
            "DrawInstructionChanger", "DrawCodeCaveBuilder", "DrawAddressContextMenu",
            "DrawDebugger", "DrawBreakpoints", "DrawBreakpointContents", "DrawChanges",
            "RecordCodeCaveChange", "ClearCodeCaveChange",
        }
    )
    if (digest, count) != (MEMORY_REMAINING_V194_SHA256, 89):
        raise AssertionError("unscoped MemoryToolsWindow method changed from v1.94")

    print("PASS: v1.95 Pass-10 lifecycle/resource/error-path hardening invariants")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
