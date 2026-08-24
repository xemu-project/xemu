#!/usr/bin/env python3
# v2.87 current regression ownership.
"""Regression guard for same-address F0 A->B handoff.

The full Cheat Engine translation unit depends on xemu/QEMU, so this test
freezes the source/lifecycle invariants and models the resource ownership
transition that must hold when a user checks F0 A, unchecks A, then checks a
different F0 B at the same guest hook address.
"""
from __future__ import annotations

import argparse
import pathlib
import sys
from v287_source_test_utils import extract_function, read_cheat_engine_implementation


def assert_order(body: str, tokens: tuple[str, ...], name: str) -> None:
    last = -1
    for token in tokens:
        pos = body.find(token, last + 1)
        if pos < 0:
            raise AssertionError(f"{name} missing/order regression at `{token}`")
        last = pos


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    args = parser.parse_args()
    root = pathlib.Path(args.root).resolve()
    debug = root / "ui/xui/debug-tools"
    cc = read_cheat_engine_implementation(debug)
    hh = (debug / "cheat-engine.hh").read_text(encoding="utf-8")

    if "std::vector<FHookState> m_retired_f_hooks;" not in hh:
        raise AssertionError("retired F0 cave queue missing")
    for field in ("bool resume_points_valid = false;",
                  "std::vector<uint32_t> resume_points;",
                  "uint8_t retire_backoff_ticks = 0;",
                  "uint8_t retire_skip_ticks = 0;"):
        if field not in hh:
            raise AssertionError(f"lightweight retirement state missing: {field}")
    for decl in ("void RetireFHookResources(FHookState &state);",
                 "void ReleaseRetiredFHooks();"):
        if decl not in hh:
            raise AssertionError(f"missing lifecycle declaration: {decl}")

    draw = extract_function(cc, "void CheatEngineWindow::DrawCheat(")
    # v2.76 strengthens the UI OFF contract so the restore no longer depends
    # on the global Live Cheats flag. This historical handoff guard continues
    # to require the direct checkbox -> block deactivation path; the exact
    # unconditional ordering is owned by v287-ui-runtime-regressions-golden.py.
    assert_order(draw, (
        'ImGui::Checkbox("##selected", &block.selected)',
        "DeactivateFHooksForBlock(block_index);",
    ), "checkbox deactivation")

    deactivate = extract_function(cc, "void CheatEngineWindow::DeactivateFHook(")
    assert_order(deactivate, (
        "xemu_cheat_patch_virtual(state.hook_address,",
        "state.installed = false;",
        "state.retired_may_be_referenced = true;",
        "if (state.retired_may_be_referenced)",
        "RetireFHookResources(state);",
        "else if (!ReleaseFHookCaveIfSafe(state))",
    ), "DeactivateFHook")

    retire = extract_function(cc, "void CheatEngineWindow::RetireFHookResources(")
    for token in (
        "retired.external_entry = state.external_entry;",
        "retired.preserve_entry = state.preserve_entry;",
        "retired.temp_entry = state.temp_entry;",
        "retired.resume_points = std::move(state.resume_points);",
        "retired.retire_skip_ticks = 2;",
        "m_retired_f_hooks.push_back(std::move(retired));",
    ):
        if token not in retire:
            raise AssertionError(f"retirement resource transfer missing `{token}`")
    for token in (
        "state.external_entry = 0;",
        "state.allocation_size = 0;",
        "state.preserve_entry = 0;",
        "state.temp_entry = 0;",
    ):
        if token not in retire:
            raise AssertionError(f"old hook ownership not detached `{token}`")
    for forbidden in (
        "state.hook_address = 0",
        "state.overwrite_length = 0",
        "state.original_bytes.clear()",
    ):
        if forbidden in retire:
            raise AssertionError(f"retirement destroyed reusable hook identity: `{forbidden}`")

    install = extract_function(cc, "bool CheatEngineWindow::InstallFHook(")
    if "xemu_cheat_patch_virtual(hook_address, hook," not in install:
        raise AssertionError("F0 install does not use synchronized executable patch path")
    if "previous cave is still executing or could not be safely reclaimed" in install:
        raise AssertionError("old same-address retry/block path returned")
    assert_order(install, (
        "if (!state.installed && FHookHasTrackedEntries(state)",
        "!ReleaseFHookCaveIfSafe(state)",
        "RetireFHookResources(state);",
        "DetermineFHookLength(hook_address, overwrite_length)",
        "xemu_cheat_external_code_allocate(required_size, &external_entry)",
    ), "InstallFHook handoff")

    cleanup = extract_function(cc, "void CheatEngineWindow::ReleaseRetiredFHooks(")
    assert_order(cleanup, (
        "TypeFGuestPauseGuard guest_pause;",
        "ReleaseFHookCaveIfSafe(*it)",
        "m_retired_f_hooks.erase(it)",
    ), "retired cleanup")
    for token in ("retire_skip_ticks", "retire_backoff_ticks",
                  "std::min<int>(", "10, it->retire_backoff_ticks * 2"):
        if token not in cleanup:
            raise AssertionError(f"retired reaper backoff missing `{token}`")
    cache = extract_function(cc, "bool CheatEngineWindow::BuildFHookResumePointCache(")
    for token in (
        "state.code_size + 5u",
        "state.resume_points.push_back(row.virtual_address);",
        "state.resume_points_valid = true;",
    ):
        if token not in cache:
            raise AssertionError(f"resume-point cache missing `{token}`")
    if 'g_ascii_strcasecmp(row.mnemonic, "call")' in cache:
        raise AssertionError("resume-point cache regressed to CALL-only tracking")
    may_ref = extract_function(cc, "bool CheatEngineWindow::FHookCaveMayStillBeReferenced(")
    if "xemu_cheat_disassemble_paired" in may_ref:
        raise AssertionError("retirement hot path still redisassembles cave every tick")
    if "state.resume_points.begin()" not in may_ref:
        raise AssertionError("stack scan does not check saved resume points")

    tick = extract_function(cc, "void CheatEngineWindow::Tick(")
    assert_order(tick, (
        "const bool cpu_available = xemu_cheat_cpu_available() != 0;",
        "if (!cpu_available)",
        "ReleaseRetiredFHooks();",
        "const bool run_live_blocks = m_engine_enabled && m_live_cheats_enabled;",
        "for (const auto &entry : m_f_hooks)",
        "TypeFGuestPauseGuard guest_pause;",
        "if (!run_live_blocks)",
    ), "Tick cleanup")

    owns = extract_function(cc, "bool CheatEngineWindow::ActiveFHookOwnsAddress(")
    if "for (const FHookState &state : m_retired_f_hooks)" not in owns:
        raise AssertionError("retired executable caves are not protected from Inject edits")

    auto_load = extract_function(cc, "void CheatEngineWindow::MaybeAutoLoadCurrentGame(")
    for needle in (
        "const bool guest_ownership_changed =",
        "m_force_forget_f_hooks_on_next_game_observation ||",
        "was_game_valid &&",
        "(!game_valid || game_identity != m_seen_game_identity)",
        "if (guest_ownership_changed)",
        "ForgetFHookOwnershipForNewGuest();",
        "m_force_forget_f_hooks_on_next_game_observation = false;",
    ):
        if needle not in auto_load:
            raise AssertionError(f"startup-safe F0 ownership transition missing: {needle}")
    if auto_load.index("if (guest_ownership_changed)") > auto_load.index("for (auto &block : m_blocks)"):
        raise AssertionError("F0 ownership boundary must be handled before live-cheat state cleanup")

    observe_reset = extract_function(cc, "void CheatEngineWindow::ObserveRequestedGameReset(")
    if "m_force_forget_f_hooks_on_next_game_observation = true;" not in observe_reset:
        raise AssertionError("explicit same-title reset no longer forces stale F0 ownership cleanup")
    if observe_reset.index("m_force_forget_f_hooks_on_next_game_observation = true;") > observe_reset.index("current_game_manager.RefreshRunningXbe(true);"):
        raise AssertionError("explicit reset ownership flag must be armed before the forced XBE observation")

    # Model the ownership-boundary decision separately from PREENTRY's startup
    # identity edge. Initial invalid->valid may be delayed XBE discovery for
    # the same live address space and must not strand a debugger JMP by wiping
    # its cave. Confirmed old-guest loss/change and explicit Reset still clear.
    def ownership_changed(was_valid: bool, game_valid: bool, same_identity: bool, force: bool) -> bool:
        return force or (was_valid and (not game_valid or not same_identity))

    if ownership_changed(False, True, False, False):
        raise AssertionError("initial invalid->valid observation incorrectly forgets an active debugger F0")
    if not ownership_changed(True, False, False, False):
        raise AssertionError("valid->invalid guest end must forget stale F0 ownership")
    if not ownership_changed(True, True, False, False):
        raise AssertionError("Game A->Game B must forget stale F0 ownership")
    if ownership_changed(True, True, True, False):
        raise AssertionError("same observed guest must retain F0 ownership")
    if not ownership_changed(True, True, True, True):
        raise AssertionError("explicit same-title Reset must force stale F0 ownership cleanup")
    forget = extract_function(cc, "void CheatEngineWindow::ForgetFHookOwnershipForNewGuest(")
    assert_order(forget, (
        "m_f_hooks.clear();",
        "m_retired_f_hooks.clear();",
        "xemu_cheat_external_code_reset_allocations();",
    ), "title-change reset")

    # Minimal ownership model of the user workflow. The key invariant is that
    # A's still-live cave belongs to the retirement queue, not to the reusable
    # hook state, so B can allocate a different cave without waiting.
    a = {
        "hook": 0x0008C552,
        "installed": True,
        "external": 0x70000000,
        "preserve": 0,
        "temp": 0x71000000,
        "original": bytes.fromhex("0F 85 A9 00 00 00"),
    }
    retired: list[dict[str, object]] = []

    # Uncheck A: once the original hook is restored, a previously reachable
    # cave is always detached first. This remains true even when the currently
    # visible EIP is outside the cave because an interrupt/exception frame may
    # still hold a resume EIP into it.
    a["installed"] = False
    retired.append({"external": a["external"], "temp": a["temp"], "grace": 2})
    a["external"] = 0
    a["temp"] = 0
    if a["external"] or a["temp"]:
        raise AssertionError("A still owns retired allocations")
    if not retired[0]["external"]:
        raise AssertionError("retired Cave A was lost")
    if retired[0]["grace"] != 2:
        raise AssertionError("retired Cave A lost mandatory grace ticks")

    # A saved interrupt/exception EIP can point at any valid instruction
    # boundary, not only at the instruction after CALL. Model the stack check
    # that keeps the old cave alive in that case.
    resume_points = [0x70000000, 0x70000007, 0x70000011, 0x70000017]
    saved_stack_words = [0xD003DB34, 0x70000011, 0x0008C558]
    if not any(word in resume_points for word in saved_stack_words):
        raise AssertionError("saved non-CALL resume EIP was not recognized")

    # Check B at exactly the same hook. It adopts/reuses only hook metadata and
    # receives a fresh executable allocation; Cave A remains independently live.
    b = dict(a)
    b["installed"] = True
    b["external"] = 0x70001000
    b["temp"] = 0x71001000
    if b["hook"] != a["hook"]:
        raise AssertionError("same-address handoff model changed hook")
    if b["external"] == retired[0]["external"]:
        raise AssertionError("B reused Cave A while A may still be executing")
    if b["original"] != a["original"]:
        raise AssertionError("B lost the true original hook bytes")

    print("PASS: F0 same-address check/uncheck/check handoff + retired cave lifecycle")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
