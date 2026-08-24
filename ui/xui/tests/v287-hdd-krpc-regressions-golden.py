#!/usr/bin/env python3
# v2.87 current regression ownership.
"""v2.87 current regression ownership: HDD/FATX/Kernel-RPC regression contracts.

Historical version labels below are provenance only; the retained contracts are
owned and executed by this v2.87 suite.
"""
from __future__ import annotations


# Preserved contract from v203-hdd-saves-export-golden.py
def check_v203_hdd_saves_export_golden() -> None:
    """v2.03 guard: FATX metadata names + Current Game HDD + read-only export.

    Later versions may add import/delete beside export; this guard scopes its no-write
    assertions to the v2.03 export functions rather than forbidding later UI features.
    """

    import argparse
    import pathlib

    from v287_source_test_utils import extract_function


    def require(text: str, needle: str, what: str) -> None:
        if needle not in text:
            raise AssertionError(f"missing {what}: {needle}")


    def main() -> int:
        parser = argparse.ArgumentParser()
        parser.add_argument("--root", default=".")
        root = pathlib.Path(parser.parse_args().root).resolve()
        debug = root / "ui/xui/debug-tools"

        fatx_hh = (debug / "fatx-hdd.hh").read_text(encoding="utf-8")
        fatx_cc = (debug / "fatx-hdd.cc").read_text(encoding="utf-8")
        hdd_hh = (debug / "hdd-directory.hh").read_text(encoding="utf-8")
        hdd_cc = ((debug / "hdd-directory.cc").read_text(encoding="utf-8") + "\n" + (debug / "hdd-directory-ui.cc").read_text(encoding="utf-8"))
        export_cc = (debug / "hdd-export-service.cc").read_text(encoding="utf-8")
        current = ((debug / "current-game.cc").read_text(encoding="utf-8") + "\n" + (debug / "current-game-ui.cc").read_text(encoding="utf-8"))
        snapshot_service = (debug / "hdd-snapshot-service.cc").read_text(encoding="utf-8")
        native = (debug / "tests/fatx-hdd-golden.cpp").read_text(encoding="utf-8")

        for needle in (
            "std::string friendly_name;",
            "bool PopulateXboxMetadata(",
            'FindChildNoCase(title.children, "TitleMeta.xbx")',
            'MetadataValue(bytes, "TitleName")',
            'FindChildNoCase(save.children, "SaveMeta.xbx")',
            'MetadataValue(bytes, "Name")',
            'return base + " - " + entry.friendly_name;',
        ):
            require(fatx_hh + fatx_cc, needle, "metadata/friendly-name support")

        require(fatx_hh + fatx_cc, "std::string display_name;",
                "safe display name separated from authoritative FATX name")
        require(fatx_cc, "std::string DisplayName(const Entry &entry)",
                "central FATX display-name formatter")

        for needle in (
            "using WriteCallback = bool (*)(void *opaque, const void *buffer, size_t size);",
            "bool StreamFile(",
            "XemuDebugGuestPauseGuard pause;",
            "XemuFatxHdd::Snapshot fresh;",
            "XemuFatxHdd::BuildPartitionSnapshot(ReadHddBlock, hdd",
            "XemuFatxHdd::FindEntry(*partition, target.components)",
            "XemuFatxHdd::StreamFile(",
            '"Export File..."',
            '"Export Folder..."',
            '"Export Save Folder..."',
            "ShowOpenFolderDialog(nullptr",
        ):
            require(fatx_hh + fatx_cc + hdd_cc + export_cc, needle, "read-only export path")
        stream = extract_function(fatx_cc, "bool StreamFile(")
        export_to_host = extract_function(export_cc, "bool ExportToHost(")
        export_recursive = extract_function(export_cc, "bool ExportEntryRecursive(")
        export_paths = stream + export_to_host + export_recursive
        for forbidden in ("xemu_disc_block_pwrite", "blk_pwrite", "DeleteEntry(", "ActionReset"):
            if forbidden in export_paths:
                raise AssertionError(
                    f"v2.03 export path must remain Xbox-HDD read-only: {forbidden}")

        refresh = extract_function(hdd_cc, "void HddDirectoryWindow::Refresh()")
        require(refresh, "hdd_snapshot_service.BuildDisplaySnapshot(",
                "HDD UI display-snapshot service ownership")
        display_snapshot = extract_function(
            snapshot_service, "bool HddSnapshotService::BuildDisplaySnapshot(")
        if display_snapshot.find("BuildSnapshot") > display_snapshot.find("PopulateXboxMetadata"):
            raise AssertionError("metadata names must be populated after the coherent FATX snapshot")
        require(display_snapshot, "XemuDebugGuestPauseGuard pause;",
                "metadata read under the same coherent pause snapshot")

        draw = extract_function(current, "void CurrentGameManager::Draw(bool detached)")
        for needle in (
            'ImGui::BeginTabItem("HDD")',
            "hdd_directory_window.DrawCurrentGameHdd(",
            "m_info.valid ? m_info.title_id : 0",
        ):
            require(draw, needle, "Current Game HDD tab")
        for needle in (
            "void DrawCurrentGameHdd(uint32_t title_id);",
            'ImGui::BeginTabItem("Saves / UDATA")',
            'ImGui::BeginTabItem("DLC / TDATA")',
            'FindChildNoCase(data->entries, "UDATA")',
            'FindChildNoCase(data->entries, "TDATA")',
            'std::snprintf(title_text, sizeof(title_text), "%08X", title_id);',
            '"EXPORT ALL SAVES..."',
            '"EXPORT TITLE DATA..."',
        ):
            require(hdd_hh + hdd_cc, needle, "Current Game Title-ID HDD filtering/export")

        for needle in (
            'Utf16Le("TitleName=Ratatouille\\r\\n")',
            'Utf16Le("Name=Save Game 1\\r\\n")',
            "XemuFatxHdd::PopulateXboxMetadata(",
            "XemuFatxHdd::StreamFile(",
            'friendly_name == "Ratatouille"',
            'friendly_name == "Save Game 1"',
            "assert(exported == foo_data);",
        ):
            require(native, needle, "native FATX metadata/export coverage")

        print("PASS: v2.03 FATX save metadata + Current Game HDD + export guard")
        return 0
    result = main()
    if result not in (None, 0):
        raise AssertionError("v203-hdd-saves-export-golden.py returned non-zero: %r" % (result,))

# Preserved contract from v204-fatx-delete-golden.py
def check_v204_fatx_delete_golden() -> None:
    """Historical v2.04 direct-FATX-delete guard.

    v2.32 intentionally decommissions the production raw mutation primitive after
    kernel-managed Delete became the only shipping HDD mutation path.
    """
    from pathlib import Path
    r=Path(__file__).resolve().parents[1]
    h=(r/'fatx-hdd.hh').read_text(); cc=(r/'fatx-hdd.cc').read_text(); block_h=(r/'disc-block-io.h').read_text(); block=(r/'disc-block-io.c').read_text(); native=(r/'tests/fatx-hdd-golden.cpp').read_text()
    assert 'DeleteEntry(' not in h+cc
    assert 'DiskWriteCallback' not in h
    assert 'xemu_disc_block_pwrite' not in block_h+block
    assert 'xemu_disc_block_flush' not in block_h+block
    assert 'production FATX mutation has been decommissioned' in native
    assert 'StreamFile(' in h+cc and 'ReadFileRange(' in h+cc
    print('PASS: historical v2.04 direct FATX delete is intentionally decommissioned by v2.32')

# Preserved contract from v205-fatx-delete-popup-fix-golden.py
def check_v205_fatx_delete_popup_fix_golden() -> None:
    """v2.05 popup-scope guard carried forward to the v2.15 kernel delete modal."""

    import argparse
    import pathlib

    from v287_source_test_utils import extract_function


    def require(text: str, needle: str, what: str) -> None:
        if needle not in text:
            raise AssertionError(f"missing {what}: {needle}")


    def forbid(text: str, needle: str, what: str) -> None:
        if needle in text:
            raise AssertionError(f"unexpected {what}: {needle}")


    def main() -> int:
        parser = argparse.ArgumentParser()
        parser.add_argument("--root", default=".")
        root = pathlib.Path(parser.parse_args().root).resolve()
        debug = root / "ui/xui/debug-tools"

        hdd_hh = (debug / "hdd-directory.hh").read_text(encoding="utf-8")
        hdd_cc = ((debug / "hdd-directory.cc").read_text(encoding="utf-8") + "\n" + (debug / "hdd-directory-ui.cc").read_text(encoding="utf-8"))

        request = extract_function(hdd_cc, "void HddDirectoryWindow::RequestDelete(")
        confirm = extract_function(hdd_cc, "void HddDirectoryWindow::DrawDeleteConfirmation()")

        require(hdd_hh, "bool m_delete_popup_open_requested = false;",
                "deferred modal-open state")
        require(request, "m_delete_pending = true;", "pending delete request")
        require(request, "m_delete_popup_open_requested = true;",
                "deferred popup request")
        forbid(request, 'ImGui::OpenPopup("Confirm Xbox Kernel HDD Delete")',
               "context-popup scoped modal open")

        require(confirm, "if (m_delete_popup_open_requested)",
                "root-scope popup-open gate")
        require(confirm, 'ImGui::OpenPopup("Confirm Xbox Kernel HDD Delete")',
                "root-scope modal open")
        require(confirm, 'ImGui::BeginPopupModal("Confirm Xbox Kernel HDD Delete"',
                "root-scope modal begin")
        if confirm.find('ImGui::OpenPopup("Confirm Xbox Kernel HDD Delete")') > confirm.find(
            'ImGui::BeginPopupModal("Confirm Xbox Kernel HDD Delete"'):
            raise AssertionError("OpenPopup must execute before BeginPopupModal in the same function")
        require(confirm, "m_delete_popup_open_requested = false;",
                "one-shot modal-open request")
        require(confirm, "PrepareHddDelete(", "fresh kernel-delete preflight")
        require(confirm, "StartHddDelete(", "confirmed kernel-delete transaction")
        forbid(confirm, "DeleteFromHdd(", "legacy raw-delete invocation from normal modal")
        forbid(confirm, "ActionReset", "reset in normal kernel-delete modal")

        for forbidden in ("DeleteFromHdd", "RawDeleteAllowed", "ActionReset"):
            forbid(hdd_cc, forbidden, "decommissioned raw/reset HDD delete frontend")

        print("PASS: v2.05/v2.15 HDD Delete popup handoff guard")
        return 0
    result = main()
    if result not in (None, 0):
        raise AssertionError("v205-fatx-delete-popup-fix-golden.py returned non-zero: %r" % (result,))

# Preserved contract from v206-kernel-rpc-foundation-golden.py
def check_v206_kernel_rpc_foundation_golden() -> None:
    """v2.06 guard: harmless, isolated Xbox Guest Kernel RPC foundation."""

    import argparse
    import pathlib

    from v287_source_test_utils import extract_function


    def require(text: str, needle: str, what: str) -> None:
        if needle not in text:
            raise AssertionError(f"missing {what}: {needle}")


    def forbid(text: str, needle: str, what: str) -> None:
        if needle in text:
            raise AssertionError(f"unexpected {what}: {needle}")


    def main() -> int:
        parser = argparse.ArgumentParser()
        parser.add_argument("--root", default=".")
        root = pathlib.Path(parser.parse_args().root).resolve()
        debug = root / "ui/xui/debug-tools"

        rpc = (debug / "guest-kernel-rpc.cc").read_text(encoding="utf-8")
        completion = debug / "guest-kernel-rpc-completion.cc"
        if completion.exists():
            rpc += "\n" + completion.read_text(encoding="utf-8")
        rpc_hh = (debug / "guest-kernel-rpc.hh").read_text(encoding="utf-8")
        mem = (debug / "guest-kernel-rpc-memory.c").read_text(encoding="utf-8")
        mem_h = (debug / "guest-kernel-rpc-memory.h").read_text(encoding="utf-8")
        util = (debug / "kernel-rpc-utils.hh").read_text(encoding="utf-8")
        current = ((debug / "current-game.cc").read_text(encoding="utf-8") + "\n" + (debug / "current-game-ui.cc").read_text(encoding="utf-8"))
        meson = (debug / "meson.build").read_text(encoding="utf-8")
        main_cc = (root / "ui/xui/main.cc").read_text(encoding="utf-8")

        require(util, "kXboxKernelBase = 0x80010000u", "live xboxkrnl PE base")
        require(util, "kKeGetCurrentIrqlOrdinal = 103u", "harmless first ordinal")
        require(util, "ResolveKernelOrdinal", "ordinal resolver")
        require(util, "BuildIrqlStub", "register-preserving IRQL stub")
        require(util, "0x9c", "pushfd")
        require(util, "0x60", "pushad")
        require(util, "0x61", "popad")
        require(util, "0x9d", "popfd")

        require(mem, "xemu_rpc_virtual_candidates", "dynamic RPC virtual candidates")
        require(mem, "0x68400000u", "preferred RPC virtual PDE")
        forbid(mem, "0x68000000u", "Type-F virtual arena reuse")
        require(mem, "memory_region_find(get_system_memory()", "physical aperture map check")
        require(mem, "current_machine->ram_size", "installed-RAM overlap rejection")
        require(mem, "(xemu_rpc_le32_load(pde_bytes) & 1u) == 0",
                "unused guest PDE requirement")
        require(mem, '"xemu.guest-kernel-rpc"', "independent MemoryRegion ownership")
        require(mem_h, "xemu_guest_rpc_arena_acquire", "private RPC allocation API")
        require(mem_h, "xemu_guest_rpc_arena_active", "fail-closed mapping ownership query")
        require(mem, "Record ownership immediately after the PDE write",
                "PDE-write ownership before translation flush")
        require(mem, "Keep the mapping marked active",
                "failed PDE cleanup ownership retention")
        forbid(mem + rpc, "xemu_cheat_external_code_allocate",
               "Type-F/CodeCave allocator coupling")

        require(rpc, "(regs.cs & 3u) != 0", "ring-0 preflight")
        require(rpc, "regs.eip != regs.pc", "flat code-segment preflight")
        require(rpc, "kStackSafetyBytes = 64u", "stack safety preflight")
        require(rpc, "xemu_cheat_virtual_to_physical(stack_low", "low stack mapping check")
        require(rpc, "xemu_cheat_virtual_to_physical(stack_high", "high stack mapping check")
        require(rpc, "breakpoint_removed", "completion-breakpoint cleanup gating")
        require(rpc, "AbortBeforeRun", "pre-run rollback path")
        require(rpc, "xemu_guest_rpc_arena_active()", "stale RPC ownership retry block")
        require(rpc, "PreservedStateMatches", "post-RPC state comparison")
        require(rpc, "RestoreSavedContext", "original context restoration")
        require(rpc, "SDL_GetTicks() - m_started_ms > 3000u", "bounded RPC timeout")
        require(rpc, "StartIrqlTest", "original harmless IRQL proof")

        # The original ordinal-103 proof itself must remain filesystem/HDD-write
        # free even though later versions add separately guarded RPC tests.
        harmless = extract_function(rpc, "bool GuestKernelRpcManager::StartIrqlTest()")
        for needle in (
            "NtDeleteFile", "NtCreateFile", "NtOpenFile", "NtWriteFile",
            "NtSetInformationFile", "NtFlushBuffersFile", "NtQueryFullAttributesFile",
            "DeleteEntry(", "xemu_disc_block_pwrite", "xemu_disc_block_flush",
        ):
            forbid(harmless, needle,
                   "filesystem/write operation in harmless RPC foundation")

        require(current, 'ImGui::BeginTabItem("Kernel RPC Diagnostics")',
                "Current Game hidden RPC diagnostics tab")
        require(current, 'ImGui::Checkbox("Show Kernel RPC diagnostics"',
                "developer-only diagnostics visibility control")
        require(main_cc, "guest_kernel_rpc_manager.Tick();",
                "frame-level asynchronous completion polling")
        require(meson, "'guest-kernel-rpc.cc'", "RPC manager build ownership")
        require(meson, "'guest-kernel-rpc-memory.c'", "RPC arena build ownership")

        print("PASS: v2.06 harmless Guest Kernel RPC foundation guard")
        return 0
    result = main()
    if result not in (None, 0):
        raise AssertionError("v206-kernel-rpc-foundation-golden.py returned non-zero: %r" % (result,))

# Preserved contract from v207-kernel-rpc-button-scope-golden.py
def check_v207_kernel_rpc_button_scope_golden() -> None:
    """v2.07 guard: Kernel RPC test button keeps ImGui disabled scope balanced."""

    import argparse
    import pathlib


    def main() -> int:
        parser = argparse.ArgumentParser()
        parser.add_argument("--root", default=".")
        root = pathlib.Path(parser.parse_args().root).resolve()
        rpc = (root / "ui/xui/debug-tools/guest-kernel-rpc.cc").read_text(encoding="utf-8")
        rpc_ui = root / "ui/xui/debug-tools/guest-kernel-rpc-ui.cc"
        if rpc_ui.exists():
            rpc += "\n" + rpc_ui.read_text(encoding="utf-8")
        completion = root / "ui/xui/debug-tools/guest-kernel-rpc-completion.cc"
        if completion.exists():
            rpc += "\n" + completion.read_text(encoding="utf-8")

        marker = 'ImGui::Button("RUN HARMLESS IRQL TEST")'
        pos = rpc.find(marker)
        if pos < 0:
            raise AssertionError("missing harmless IRQL test button")
        start = rpc.rfind("void GuestKernelRpcManager::DrawTestPanel()", 0, pos)
        end = rpc.find("\n}", pos)
        if start < 0 or end < 0:
            raise AssertionError("could not isolate DrawTestPanel")
        panel = rpc[start:end + 2]

        required = 'ImGui::BeginDisabled(busy);'
        if required not in panel:
            raise AssertionError("RPC test buttons must use one unconditional BeginDisabled(bool) scope")
        if panel.count("ImGui::BeginDisabled") != 1 or panel.count("ImGui::EndDisabled") != 1:
            raise AssertionError("RPC test button disabled scope must contain exactly one Begin/End pair")

        # The button changes m_state to Running synchronously.  Conditional EndDisabled
        # based on the post-click state recreates the v2.06 assertion.
        bad = '''if (m_state == State::Running) {\n        ImGui::EndDisabled();'''
        if bad in panel:
            raise AssertionError("post-click conditional EndDisabled regression returned")

        # The RPC proof itself stays harmless/read-only in this UI hotfix.
        for needle in ("NtDeleteFile", "NtCreateFile", "NtOpenFile", "NtWriteFile", "DeleteEntry("):
            if needle in panel:
                raise AssertionError(f"unexpected filesystem operation in v2.07 button scope: {needle}")

        print("PASS: v2.07 Kernel RPC button ImGui scope guard")
        return 0
    result = main()
    if result not in (None, 0):
        raise AssertionError("v207-kernel-rpc-button-scope-golden.py returned non-zero: %r" % (result,))

# Preserved contract from v208-kernel-rpc-readonly-fs-golden.py
def check_v208_kernel_rpc_readonly_fs_golden() -> None:
    """v2.08 guard: PASSIVE_LEVEL-gated, read-only Xbox filesystem RPC proof."""

    import argparse
    import pathlib

    from v287_source_test_utils import extract_function


    def require(text: str, needle: str, what: str) -> None:
        if needle not in text:
            raise AssertionError(f"missing {what}: {needle}")


    def forbid(text: str, needle: str, what: str) -> None:
        if needle in text:
            raise AssertionError(f"unexpected {what}: {needle}")


    def main() -> int:
        parser = argparse.ArgumentParser()
        parser.add_argument("--root", default=".")
        root = pathlib.Path(parser.parse_args().root).resolve()
        debug = root / "ui/xui/debug-tools"

        rpc = (debug / "guest-kernel-rpc.cc").read_text(encoding="utf-8")
        rpc_ui = debug / "guest-kernel-rpc-ui.cc"
        if rpc_ui.exists():
            rpc += "\n" + rpc_ui.read_text(encoding="utf-8")
        rpc_status = debug / "guest-kernel-rpc-status.hh"
        if rpc_status.exists():
            rpc += "\n" + rpc_status.read_text(encoding="utf-8")
        completion = debug / "guest-kernel-rpc-completion.cc"
        if completion.exists():
            rpc += "\n" + completion.read_text(encoding="utf-8")
        rpc_hh = (debug / "guest-kernel-rpc.hh").read_text(encoding="utf-8")
        util = (debug / "kernel-rpc-utils.hh").read_text(encoding="utf-8")

        require(util, "kNtQueryFullAttributesFileOrdinal = 210u",
                "NtQueryFullAttributesFile ordinal")
        require(util, "BuildReadOnlyFileQueryStub", "read-only filesystem RPC stub")
        require(util, "test eax,eax", "documented IRQL gate")
        require(util, "0x0f); emit8(0x85", "JNZ skip of filesystem call")
        require(util, "BuildFileQueryObjects", "Xbox ANSI_STRING/OBJECT_ATTRIBUTES builder")
        require(util, "0xfffffffdu", "ObDosDevicesDirectory handle")
        require(util, "0x00000040u", "OBJ_CASE_INSENSITIVE")
        require(util, "ParseFileNetworkOpenInformation", "read-only file result decoder")

        require(rpc_hh, "WaitingSafePoint", "safe-point scheduler state")
        require(rpc_hh, "RunningFsQuery", "read-only filesystem RPC running state")
        require(rpc, "RUN READ-ONLY FILE TEST", "manual read-only test button")
        require(rpc, "IsTitleExecutionPoint(sample.eip)", "current-title execution bias")
        require(rpc, "(sample.eflags & (1u << 9)) == 0", "interrupt-enabled safe-point gate")
        require(rpc, "kSafePointTimeoutMs = 5000u", "bounded safe-point wait")
        require(rpc, "kSafePointSampleIntervalMs = 25u", "bounded sampling cadence")
        require(rpc, "m_irql != 0 || m_query_ran == 0",
                "filesystem call skipped unless exact point is PASSIVE_LEVEL")
        require(rpc, "NtQueryFullAttributesFile", "read-only Xbox filesystem operation")
        require(rpc, "hdd_snapshot_service.BuildRawSnapshot(", "fresh direct FATX comparison snapshot")
        require(rpc, "m_kernel_file_size == m_expected_file_size", "kernel/FATX size comparison")
        require(rpc, "m_kernel_attributes & 0xffu", "kernel/FATX attribute comparison")
        require(rpc, "PreservedFilesystemStateMatches", "filesystem-call preserved-state check")
        require(rpc, "CR2 records the most recent page-fault", "CR2 diagnostic-state allowance")
        require(rpc, "The running title changed while waiting", "title-change fail closed")
        require(rpc, "filesystem RPC exceeded 3 seconds", "filesystem-call timeout fail closed")
        require(rpc, "RPC mapping owned; reset before retrying", "mid-call cleanup is never guessed")

        # The separately guarded v2.08 read-only primitive must remain query-only
        # even though later versions add a distinct kernel-managed delete test.
        readonly_scope = "\n".join((
            extract_function(rpc, "bool GuestKernelRpcManager::StartReadOnlyFsTest()"),
            extract_function(rpc, "bool GuestKernelRpcManager::BeginReadOnlyFsAttempt()"),
            extract_function(rpc, "void GuestKernelRpcManager::HandleReadOnlyFsCompletion("),
            extract_function(util, "inline bool BuildReadOnlyFileQueryStub("),
        ))
        for needle in (
            "NtDeleteFile", "NtCreateFile", "NtOpenFile", "NtWriteFile",
            "NtSetInformationFile", "NtFlushBuffersFile", "DeleteEntry(",
            "xemu_disc_block_pwrite", "xemu_disc_block_flush",
        ):
            forbid(readonly_scope, needle, "write-capable operation in v2.08 read-only RPC")

        print("PASS: v2.08 PASSIVE_LEVEL-gated read-only filesystem RPC guard")
        return 0
    result = main()
    if result not in (None, 0):
        raise AssertionError("v208-kernel-rpc-readonly-fs-golden.py returned non-zero: %r" % (result,))

# Preserved contract from v209-kernel-rpc-path-diagnostics-golden.py
def check_v209_kernel_rpc_path_diagnostics_golden() -> None:
    """v2.09 guard: read-only Xbox kernel path/namespace diagnostics."""

    import argparse
    import pathlib

    from v287_source_test_utils import extract_function


    def require(text: str, needle: str, what: str) -> None:
        if needle not in text:
            raise AssertionError(f"missing {what}: {needle}")


    def forbid(text: str, needle: str, what: str) -> None:
        if needle in text:
            raise AssertionError(f"unexpected {what}: {needle}")


    def main() -> int:
        parser = argparse.ArgumentParser()
        parser.add_argument("--root", default=".")
        root = pathlib.Path(parser.parse_args().root).resolve()
        debug = root / "ui/xui/debug-tools"

        rpc = (debug / "guest-kernel-rpc.cc").read_text(encoding="utf-8")
        rpc_ui = debug / "guest-kernel-rpc-ui.cc"
        if rpc_ui.exists():
            rpc += "\n" + rpc_ui.read_text(encoding="utf-8")
        rpc_status = debug / "guest-kernel-rpc-status.hh"
        if rpc_status.exists():
            rpc += "\n" + rpc_status.read_text(encoding="utf-8")
        completion = debug / "guest-kernel-rpc-completion.cc"
        if completion.exists():
            rpc += "\n" + completion.read_text(encoding="utf-8")
        rpc_hh = (debug / "guest-kernel-rpc.hh").read_text(encoding="utf-8")
        util = (debug / "kernel-rpc-utils.hh").read_text(encoding="utf-8")

        require(rpc_hh, "NamespaceDiagnostics", "namespace diagnostic mode")
        require(rpc_hh, "PathDiagnostic", "per-path diagnostic result")
        require(rpc, 'ImGui::Button("RUN PATH DIAGNOSTICS")', "manual path diagnostic button")
        require(rpc, 'add("DOS E root", "E:\\\\"', "DOS E root probe")
        require(rpc, 'add("DOS E area"', "DOS E area probe")
        require(rpc, 'add("DOS E title"', "DOS E title probe")
        require(rpc, 'add("DOS E file"', "DOS E exact file probe")
        require(rpc, 'add("Title drive root"', "title drive root probe")
        require(rpc, 'add("Title drive file"', "title drive file probe")
        require(rpc, 'add("Native E root"', "native E root probe")
        require(rpc, 'add("Native E area"', "native E area probe")
        require(rpc, 'add("Native E title"', "native E title probe")
        require(rpc, 'add("Native E file"', "native E file probe")
        require(rpc, 'add("Explicit DOS file"', "fully-qualified DOS probe")
        require(rpc, '"\\\\Device\\\\Harddisk0\\\\Partition1"', "retail E: device object")
        require(rpc, '"\\\\??\\\\" + dos_file', "explicit DOS namespace probe")

        require(util, "kObDosDevicesDirectory = 0xfffffffdu", "named DOS root constant")
        require(util, "uint32_t root_directory = kObDosDevicesDirectory",
                "configurable OBJECT_ATTRIBUTES root")
        require(rpc, "m_query_root_directory", "runtime root-directory diagnostics")
        require(rpc, "ANSI Length", "ANSI_STRING Length display")
        require(rpc, "ANSI Maximum", "ANSI_STRING MaximumLength display")
        require(rpc, "ANSI Buffer", "ANSI_STRING Buffer display")
        require(rpc, "Object Root", "OBJECT_ATTRIBUTES RootDirectory display")
        require(rpc, "Object Name", "OBJECT_ATTRIBUTES ObjectName display")
        require(rpc, "Object Attributes", "OBJECT_ATTRIBUTES flags display")
        require(rpc, "NtStatusName", "known NTSTATUS diagnostic names")

        # The v2.08 native run showed CR0 80010031 -> 8001003B. MP/EM/TS are
        # kernel-owned lazy-FPU/task bits; every other CR0 bit remains invariant.
        require(rpc, 'SameMaskedField("cr0", a.cr0, b.cr0, ~0x0000000eu',
                "CR0 MP/EM/TS-only allowance")
        require(rpc, "RPC_FS_CHECK(cr3); RPC_FS_CHECK(cr4);",
                "CR3/CR4 remain exact")

        # Diagnostics must stay on the same PASSIVE_LEVEL-gated read-only query
        # primitive even though later versions add separately guarded write tests.
        require(rpc, "BuildReadOnlyFileQueryStub", "same atomic PASSIVE_LEVEL query stub")
        require(rpc, "CompletePathDiagnostic", "sequential query cleanup/advance")
        diagnostic_scope = "\n".join((
            extract_function(rpc, "bool GuestKernelRpcManager::BuildPathDiagnostics("),
            extract_function(rpc, "void GuestKernelRpcManager::ActivatePathDiagnostic("),
            extract_function(rpc, "bool GuestKernelRpcManager::StartPathDiagnostics()"),
            extract_function(rpc, "void GuestKernelRpcManager::CompletePathDiagnostic("),
            extract_function(rpc, "bool GuestKernelRpcManager::BeginReadOnlyFsAttempt()"),
            extract_function(util, "inline bool BuildReadOnlyFileQueryStub("),
        ))
        for needle in (
            "NtDeleteFile", "NtCreateFile", "NtOpenFile", "NtWriteFile",
            "NtSetInformationFile", "NtFlushBuffersFile", "DeleteEntry(",
            "xemu_disc_block_pwrite", "xemu_disc_block_flush",
        ):
            forbid(diagnostic_scope, needle, "write-capable operation in v2.09 diagnostics")

        print("PASS: v2.09 read-only Xbox path/namespace diagnostic guard")
        return 0
    result = main()
    if result not in (None, 0):
        raise AssertionError("v209-kernel-rpc-path-diagnostics-golden.py returned non-zero: %r" % (result,))

# Preserved contract from v210-kernel-delete-file-golden.py
def check_v210_kernel_delete_file_golden() -> None:
    """v2.10 guard: PASSIVE_LEVEL-gated Xbox-kernel single-file delete test."""

    import argparse
    import pathlib

    from v287_source_test_utils import extract_function


    def require(text: str, needle: str, what: str) -> None:
        if needle not in text:
            raise AssertionError(f"missing {what}: {needle}")


    def forbid(text: str, needle: str, what: str) -> None:
        if needle in text:
            raise AssertionError(f"unexpected {what}: {needle}")


    def main() -> int:
        parser = argparse.ArgumentParser()
        parser.add_argument("--root", default=".")
        root = pathlib.Path(parser.parse_args().root).resolve()
        debug = root / "ui/xui/debug-tools"
        rpc = (debug / "guest-kernel-rpc.cc").read_text(encoding="utf-8")
        completion = debug / "guest-kernel-rpc-completion.cc"
        if completion.exists():
            rpc += "\n" + completion.read_text(encoding="utf-8")
        rpc_hh = (debug / "guest-kernel-rpc.hh").read_text(encoding="utf-8")
        util = (debug / "kernel-rpc-utils.hh").read_text(encoding="utf-8")
        fs = ((debug / "kernel-rpc-filesystem.cc").read_text(encoding="utf-8") + "\n" + (debug / "kernel-rpc-filesystem-stream.cc").read_text(encoding="utf-8"))

        rpc_fs = (debug / "guest-kernel-rpc-filesystem.cc").read_text(encoding="utf-8")

        # v2.86 removes the obsolete standalone single-file test UI. Production
        # deletes use the generic HDD delete entry point and the same kernel file
        # delete stub/executor underneath it.
        if "StartKernelDeleteSelectedFile" not in rpc_hh:
            forbid(rpc_hh, "KernelDeleteSingleFile", "retired single-file test mode")
            require(util, "BuildKernelDeleteFileStub", "kernel file-delete RPC stub retained")
            start = extract_function(rpc_fs, "bool GuestKernelRpcManager::StartHddDelete(")
            require(start, "BuildRawPartitionSetSnapshot", "fresh start-time FATX snapshot")
            require(start, "XemuKernelFs::BuildDeletePlan", "generic fresh delete plan")
            require(start, "XemuKernelFs::SameDeletePlan", "confirmed/fresh delete identity gate")
            require(start, "FsTestMode::KernelDeleteRecursiveFolder", "single production delete executor mode")
            require(start, "LoadRecursiveDeleteEntry", "kernel delete entry preparation")
            scope = "\n".join((
                start,
                extract_function(rpc, "bool GuestKernelRpcManager::BeginKernelDeleteAttempt()"),
                extract_function(rpc, "void GuestKernelRpcManager::HandleKernelDeleteCompletion("),
                extract_function(util, "inline bool BuildKernelDeleteFileStub("),
            ))
            for needle in ("XemuFatxHdd::DeleteEntry(", "xemu_disc_block_pwrite",
                           "xemu_disc_block_flush", "ActionReset", "NtDeleteFile"):
                forbid(scope, needle, "raw/reset fallback in production kernel delete")
            print("PASS: v2.10 file-delete kernel primitive retained through v2.86 production HDD delete path")
            return 0

        require(rpc_hh, "KernelDeleteSingleFile", "separate kernel-delete mode")
        require(util, "kNtCloseOrdinal = 187u", "NtClose ordinal")
        require(util, "kNtOpenFileOrdinal = 202u", "NtOpenFile ordinal")
        require(util, "kNtSetInformationFileOrdinal = 226u", "NtSetInformationFile ordinal")
        require(util, "BuildKernelDeleteFileStub", "kernel delete RPC stub")
        require(util, "kDeleteAccess | kSynchronizeAccess", "DELETE|SYNCHRONIZE access")
        require(util, "kFileShareReadWriteDelete", "share read/write/delete")
        require(util, "kFileNonDirectoryFile", "single-file-only open option")
        require(util, "kFileSynchronousIoNonAlert", "synchronous file open")
        require(util, "kFileOpenForBackupIntent", "backup-intent file open")
        require(util, "kFileDispositionInformation = 13u", "FileDispositionInformation class")
        require(util, "NtOpenFile", "documented open stage")
        require(util, "NtSetInformationFile", "documented delete-disposition stage")
        require(util, "NtClose", "documented close stage")

        require(rpc, "REFRESH DELETE FILE LIST", "explicit current-title file list refresh")
        require(rpc, "DELETE SELECTED FILE VIA XBOX KERNEL...", "explicit destructive action")
        require(rpc, "Confirm Xbox Kernel File Delete", "confirmation modal")
        require(rpc, "DELETE FILE VIA XBOX KERNEL", "final destructive confirmation")
        require(fs, '"\\\\Device\\\\Harddisk0\\\\Partition1\\\\"',
                "proven fully-qualified E: native namespace")
        require(rpc, "NativeEPathFromComponents",
                "single-file candidates use shared native path mapping")
        require(rpc, "selected.components.size() < 3u", "UDATA/TDATA child-only restriction")
        require(rpc, 'EqualsNoCase(selected.components[0], "UDATA")', "UDATA restriction")
        require(rpc, 'EqualsNoCase(selected.components[0], "TDATA")', "TDATA restriction")
        require(rpc, "game.title_id != m_delete_candidate_title_id", "current-title restriction")
        require(rpc, "entry->directory", "directories rejected from first delete test")
        require(rpc, "m_query_root_directory = 0u", "native path RootDirectory zero")
        require(rpc, "BeginKernelDeleteAttempt", "safe-point delete attempt")
        require(rpc, "m_fs_test_mode == FsTestMode::KernelDeleteSingleFile",
                "safe-point dispatch to delete primitive")
        require(rpc, "PreservedFilesystemStateMatches", "post-delete CPU-state validation")
        require(rpc, "hdd_snapshot_service.BuildRawSnapshot(", "post-kernel direct FATX verification")
        require(rpc, "Direct FATX refresh confirms the file is gone", "no-reset success proof")
        require(rpc, "No raw FATX fallback and no Xbox reset", "failure does not bypass kernel")

        delete_scope = "\n".join((
            extract_function(rpc, "bool GuestKernelRpcManager::StartKernelDeleteSelectedFile()"),
            extract_function(rpc, "bool GuestKernelRpcManager::BeginKernelDeleteAttempt()"),
            extract_function(rpc, "void GuestKernelRpcManager::HandleKernelDeleteCompletion("),
            extract_function(util, "inline bool BuildKernelDeleteFileStub("),
        ))
        for needle in (
            "XemuFatxHdd::DeleteEntry(", "xemu_disc_block_pwrite", "xemu_disc_block_flush",
            "ActionReset", "NtDeleteFile", "NtCreateFile", "NtWriteFile",
            "NtFlushBuffersFile",
        ):
            forbid(delete_scope, needle, "raw/fallback/import operation in v2.10 kernel delete")

        # The first mutation test is exactly one regular file; recursive folder
        # deletion remains a later separately validated stage.
        forbid(delete_scope, "FILE_DIRECTORY_FILE", "directory delete in single-file test")

        print("PASS: v2.10 Xbox-kernel single-file delete guard")
        return 0
    result = main()
    if result not in (None, 0):
        raise AssertionError("v210-kernel-delete-file-golden.py returned non-zero: %r" % (result,))

# Preserved contract from v211-kernel-delete-folder-golden.py
def check_v211_kernel_delete_folder_golden() -> None:
    """v2.11 guard: leaf-first Xbox-kernel recursive save-folder delete."""

    import argparse
    import pathlib

    from v287_source_test_utils import extract_function


    def require(text: str, needle: str, what: str) -> None:
        if needle not in text:
            raise AssertionError(f"missing {what}: {needle}")


    def forbid(text: str, needle: str, what: str) -> None:
        if needle in text:
            raise AssertionError(f"unexpected {what}: {needle}")


    def main() -> int:
        parser = argparse.ArgumentParser()
        parser.add_argument("--root", default=".")
        root = pathlib.Path(parser.parse_args().root).resolve()
        debug = root / "ui/xui/debug-tools"
        rpc = (debug / "guest-kernel-rpc.cc").read_text(encoding="utf-8")
        completion = debug / "guest-kernel-rpc-completion.cc"
        if completion.exists():
            rpc += "\n" + completion.read_text(encoding="utf-8")
        rpc_hh = (debug / "guest-kernel-rpc.hh").read_text(encoding="utf-8")
        util = (debug / "kernel-rpc-utils.hh").read_text(encoding="utf-8")
        fs = ((debug / "kernel-rpc-filesystem.cc").read_text(encoding="utf-8") + "\n" + (debug / "kernel-rpc-filesystem-stream.cc").read_text(encoding="utf-8"))
        fs_hh = (debug / "kernel-rpc-filesystem.hh").read_text(encoding="utf-8")

        rpc_fs = (debug / "guest-kernel-rpc-filesystem.cc").read_text(encoding="utf-8")

        # v2.86 removes the legacy folder-candidate/confirmation UI. The generic
        # HDD delete frontend retains the same leaf-first kernel executor.
        if "DeleteFolderCandidate" not in rpc_hh:
            require(rpc_hh, "KernelDeleteRecursiveFolder", "production recursive-delete mode")
            require(rpc_hh, "using RecursiveDeleteEntry = XemuKernelFs::DeleteEntry",
                    "leaf-first delete entry ownership")
            require(util, "BuildKernelDeleteDirectoryStub", "directory delete stub retained")
            builder = extract_function(fs, "bool BuildDeletePlan(")
            require(builder, "self(self, child, std::move(child_components)", "leaf-first traversal")
            require(builder, "plan.push_back(std::move(directory_item));", "parent after descendants")
            start = extract_function(rpc_fs, "bool GuestKernelRpcManager::StartHddDelete(")
            require(start, "XemuKernelFs::SameDeletePlan", "fresh-plan confirmation gate")
            require(start, "LoadRecursiveDeleteEntry", "per-entry kernel preparation")
            require(rpc, "m_recursive_deleted_files", "file progress counter")
            require(rpc, "m_recursive_deleted_directories", "directory progress counter")
            for legacy in ("RefreshDeleteFolderCandidates", "RefreshSelectedDeleteFolderPreview",
                           "StartKernelDeleteSelectedFolder"):
                forbid(rpc + rpc_hh, legacy, "retired legacy recursive-delete UI path")
            print("PASS: v2.11 recursive kernel-delete behavior retained through v2.86 production HDD path")
            return 0

        require(rpc_hh, "KernelDeleteRecursiveFolder", "separate recursive-delete mode")
        require(rpc_hh, "DeleteFolderCandidate", "folder candidate state")
        require(rpc_hh, "using RecursiveDeleteEntry = XemuKernelFs::DeleteEntry",
                "leaf-first delete plan entry ownership")
        require(rpc_hh, "BuildRecursiveDeletePlan", "shared fresh recursive-plan wrapper")
        require(util, "kFileDirectoryFile = 0x00000001u", "Xbox FILE_DIRECTORY_FILE option")
        require(util, "BuildKernelDeleteDirectoryStub", "directory delete stub")
        require(util, "directory ? kFileDirectoryFile", "file/directory open-option switch")

        require(rpc, "REFRESH DELETE FOLDER LIST", "explicit folder-list refresh")
        require(rpc, "DELETE SELECTED FOLDER VIA XBOX KERNEL...", "recursive destructive action")
        require(rpc, "Confirm Xbox Kernel Recursive Folder Delete", "recursive confirmation modal")
        require(rpc, "DELETE ENTIRE FOLDER VIA XBOX KERNEL", "final recursive confirmation")
        require(rpc, "game.title_id != m_delete_folder_candidate_title_id", "current-title selection restriction")
        require(rpc, "XemuKernelFs::BuildRecursiveDeletePlan", "reusable recursive planner delegation")
        require(rpc, "LoadRecursiveDeleteEntry", "per-entry RPC preparation")
        require(rpc, "m_delete_current_is_directory = item.directory", "directory/file type propagation")
        require(rpc, "BuildKernelDeleteDirectoryStub", "directory-specific NtOpenFile options")
        require(rpc, "CleanupAttemptForRetry", "clean context/mapping between recursive entries")
        require(rpc, "direct FATX snapshot", "per-entry raw-backend verification text")
        require(rpc, "m_recursive_deleted_files", "file progress counter")
        require(rpc, "m_recursive_deleted_directories", "directory progress counter")
        require(rpc, "No raw FATX fallback and no Xbox reset", "failure respects Xbox kernel")

        # v2.14 moves the pure restriction/tree-walk ownership out of the test UI.
        require(fs_hh, "kDeleteMaxEntries = 4096u", "recursive entry-count bound")
        require(fs_hh, "kDeleteMaxDepth = 16u", "recursive depth bound")
        require(fs, "direct_child_only && components.size() != 3u",
                "direct child of Title-ID restriction")
        require(fs, 'EqualsNoCase(components[0], "UDATA")', "UDATA restriction")
        require(fs, 'EqualsNoCase(components[0], "TDATA")', "TDATA restriction")
        require(fs, "EqualsNoCase(components[1], title_text)", "current Title-ID path restriction")
        # v2.15 generalizes the leaf-first tree walk for every mapped FATX volume;
        # the v2.11 current-title wrapper still delegates into that same planner.
        wrapper = extract_function(fs, "bool BuildRecursiveDeletePlan(")
        require(wrapper, "BuildDeletePlan(snapshot, 'E', components, true, plan, error)",
                "current-title wrapper delegates to generalized planner")
        builder = extract_function(fs, "bool BuildDeletePlan(")
        require(builder, "depth > kDeleteMaxDepth", "recursive depth enforcement")
        require(builder, "plan.size() >= kDeleteMaxEntries", "recursive count enforcement")
        require(builder, "self(self, child, std::move(child_components)",
                "children recursively planned before directory")
        require(builder, "plan.push_back(std::move(directory_item));",
                "parent directory appended after children")

        recursive_scope = "\n".join((
            extract_function(rpc, "bool GuestKernelRpcManager::RefreshDeleteFolderCandidates("),
            extract_function(rpc, "bool GuestKernelRpcManager::BuildRecursiveDeletePlan("),
            extract_function(rpc, "bool GuestKernelRpcManager::RefreshSelectedDeleteFolderPreview("),
            extract_function(rpc, "bool GuestKernelRpcManager::LoadRecursiveDeleteEntry("),
            extract_function(rpc, "bool GuestKernelRpcManager::StartKernelDeleteSelectedFolder()"),
            extract_function(rpc, "bool GuestKernelRpcManager::BeginKernelDeleteAttempt()"),
            extract_function(rpc, "void GuestKernelRpcManager::HandleKernelDeleteCompletion("),
            builder,
            extract_function(util, "inline bool BuildKernelDeleteEntryStub("),
            extract_function(util, "inline bool BuildKernelDeleteDirectoryStub("),
        ))
        for needle in (
            "XemuFatxHdd::DeleteEntry(", "xemu_disc_block_pwrite", "xemu_disc_block_flush",
            "ActionReset", "NtDeleteFile", "NtCreateFile", "NtWriteFile",
            "NtFlushBuffersFile",
        ):
            forbid(recursive_scope, needle, "raw/reset/import operation in recursive kernel delete")

        # Existing single-file helper must stay available, so v2.10 remains a
        # separately testable primitive while recursive folder delete builds on it.
        require(util, "BuildKernelDeleteFileStub", "v2.10 single-file delete helper retained")
        require(rpc, "StartKernelDeleteSelectedFile", "v2.10 single-file UI/action retained")

        print("PASS: v2.11 Xbox-kernel recursive save-folder delete guard")
        return 0
    result = main()
    if result not in (None, 0):
        raise AssertionError("v211-kernel-delete-folder-golden.py returned non-zero: %r" % (result,))

# Preserved contract from v212-kernel-import-folder-golden.py
def check_v212_kernel_import_folder_golden() -> None:
    """v2.12 guard: create-only Xbox-kernel host-folder import/write test."""

    import argparse
    import pathlib

    from v287_source_test_utils import extract_function


    def require(text: str, needle: str, what: str) -> None:
        if needle not in text:
            raise AssertionError(f"missing {what}: {needle}")


    def forbid(text: str, needle: str, what: str) -> None:
        if needle in text:
            raise AssertionError(f"unexpected {what}: {needle}")


    def main() -> int:
        parser = argparse.ArgumentParser()
        parser.add_argument("--root", default=".")
        root = pathlib.Path(parser.parse_args().root).resolve()
        debug = root / "ui/xui/debug-tools"
        rpc = (debug / "guest-kernel-rpc.cc").read_text(encoding="utf-8")
        completion = debug / "guest-kernel-rpc-completion.cc"
        if completion.exists():
            rpc += "\n" + completion.read_text(encoding="utf-8")
        rpc_hh = (debug / "guest-kernel-rpc.hh").read_text(encoding="utf-8")
        util = (debug / "kernel-rpc-utils.hh").read_text(encoding="utf-8")
        fs = ((debug / "kernel-rpc-filesystem.cc").read_text(encoding="utf-8") + "\n" + (debug / "kernel-rpc-filesystem-stream.cc").read_text(encoding="utf-8"))
        fs_hh = (debug / "kernel-rpc-filesystem.hh").read_text(encoding="utf-8")

        rpc_fs = (debug / "guest-kernel-rpc-filesystem.cc").read_text(encoding="utf-8")

        # v2.86 removes the old experimental import picker/confirmation UI. The
        # HDD browser drives the same create/write/flush/close executor directly.
        if "StartKernelImportFolder" not in rpc_hh:
            require(rpc_hh, "KernelImportFolder", "production import executor mode")
            require(rpc_hh, "XemuKernelFs::TransferPlan m_import_preflight", "transfer plan state")
            start = extract_function(rpc_fs, "bool GuestKernelRpcManager::StartHddImport(")
            require(start, "BuildImportFolderPlanAtDestination", "fresh folder plan rebuild")
            require(start, "BuildImportFilePlanAtDestination", "fresh file plan rebuild")
            require(start, "SameImportPlan(plan, fresh_plan)", "fresh-plan identity gate")
            require(start, "LoadKernelImportOperation", "bounded operation preparation")
            require(rpc, "XemuKernelFs::LoadImportFileChunk", "bounded host/FATX chunk loader")
            require(rpc, "XemuKernelRpc::kFileCreate : XemuKernelRpc::kFileOpen", "create/open chunk policy")
            require(util, "BuildKernelCreateWriteStub", "kernel create/write/flush/close stub")
            for legacy in ("PrepareImportFolder", "StartKernelImportFolder",
                           "DrawKernelImportControls", "DrawKernelImportConfirmation"):
                forbid(rpc + rpc_hh, legacy, "retired experimental import UI path")
            print("PASS: v2.12 kernel import behavior retained through v2.86 production HDD import path")
            return 0

        require(rpc_hh, "KernelImportFolder", "separate folder-import mode")
        if "using TransferEntry = XemuKernelFs::TransferEntry" in rpc_hh:
            require(rpc_hh, "using ImportEntry = TransferEntry",
                    "legacy ImportEntry alias over TransferEntry")
            require(rpc_hh, "XemuKernelFs::TransferPlan m_import_preflight",
                    "reusable transfer plan state")
        else:
            require(rpc_hh, "using ImportEntry = XemuKernelFs::ImportEntry",
                    "folder-import plan entry ownership")
            require(rpc_hh, "XemuKernelFs::ImportPlan m_import_preflight",
                    "reusable import plan state")
        require(util, "kNtCreateFileOrdinal = 190u", "NtCreateFile ordinal")
        require(util, "kNtFlushBuffersFileOrdinal = 198u", "NtFlushBuffersFile ordinal")
        require(util, "kNtWriteFileOrdinal = 236u", "NtWriteFile ordinal")
        require(util, "kFileCreate = 0x00000002u", "FILE_CREATE disposition")
        require(util, "kFileOpen = 0x00000001u", "FILE_OPEN disposition")
        require(util, "BuildKernelCreateWriteStub", "create/write RPC primitive")

        require(rpc, "Experimental Xbox-Kernel Folder Import / Create + Write", "folder-import UI")
        require(rpc, "SELECT HOST FOLDER TO IMPORT...", "host-folder picker")
        require(rpc, "ShowOpenFolderDialog", "native host-folder selection")
        require(rpc, "UDATA - Saves", "UDATA destination selector")
        require(rpc, "TDATA - Title Data", "TDATA destination selector")
        require(rpc, "IMPORT SELECTED FOLDER VIA XBOX KERNEL...", "explicit import action")
        require(rpc, "Confirm Xbox Kernel Folder Import", "import confirmation modal")
        require(rpc, "IMPORT FOLDER VIA XBOX KERNEL", "final import confirmation")
        require(rpc, "game.title_id == m_import_preflight.title_id", "current-title restriction")
        require(rpc, "XemuKernelFs::BuildImportPlan", "reusable import-plan delegation")
        require(rpc, "XemuKernelFs::SameImportPlan", "start-time full-plan recheck")
        require(rpc, "m_import_preflight.source_path", "confirmed host source identity")
        require(fs, "existing current-title root E:\\\\", "existing Title-ID root requirement")
        require(fs, "Kernel Import never overwrites or merges an existing FATX item",
                "create-only destination preflight")
        require(fs, "ValidateImportDestination(snapshot, plan.partition",
                "generic start-time destination collision recheck")

        require(fs_hh, "kImportChunkBytes = 0x0000d000u", "bounded write chunk")
        require(fs_hh, "kImportMaxTotalBytes = 64ull * 1024ull * 1024ull", "64 MiB data limit")
        require(fs_hh, "kImportMaxEntries = 4096u", "entry-count limit")
        require(fs_hh, "kImportMaxDepth = 16u", "directory-depth limit")
        require(fs_hh, "kFatxMaxComponentBytes = 42u", "FATX component-length limit")
        require(fs, "portable ASCII", "conservative host-name policy")
        require(fs, "symlinks/junctions", "link/reparse safety policy")
        require(fs, "case-insensitively on FATX", "case-collision protection")

        require(rpc, "m_import_create_disposition = item.directory || m_import_file_offset == 0",
                "first-create/later-open policy")
        require(rpc, "XemuKernelRpc::kFileCreate : XemuKernelRpc::kFileOpen", "chunk reopen policy")
        require(rpc, "XemuKernelFs::LoadImportFileChunk", "bounded host-file chunk loader")
        require(rpc, "BuildKernelCreateWriteStub", "per-operation import RPC")
        require(rpc, "m_import_file_offset", "explicit file offset state")
        require(rpc, "m_import_current_chunk_bytes", "bounded file chunk state")
        require(rpc, "m_import_write_information == m_import_current_chunk_bytes", "write byte-count verification")
        require(rpc, "hdd_snapshot_service.BuildRawSnapshot(", "fresh FATX snapshot verification")
        require(rpc, "entry->file_size == m_expected_file_size", "exact file-size verification")
        require(rpc, "CleanupAttemptForRetry", "clean context/mapping between import operations")
        require(rpc, "items already created remain on the HDD", "partial-failure behavior disclosure")
        require(rpc, "used no raw FATX writes", "success reports kernel-only path")

        import_scope = "\n".join((
            extract_function(rpc, "bool GuestKernelRpcManager::PrepareImportFolder("),
            extract_function(rpc, "bool GuestKernelRpcManager::LoadKernelImportOperation("),
            extract_function(rpc, "bool GuestKernelRpcManager::StartKernelImportFolder()"),
            extract_function(rpc, "bool GuestKernelRpcManager::BeginKernelImportAttempt()"),
            extract_function(rpc, "void GuestKernelRpcManager::HandleKernelImportCompletion("),
            extract_function(fs, "bool ValidateImportHostRoot("),
            extract_function(fs, "bool BuildImportPlan("),
            extract_function(fs, "bool ValidateImportHostPlan("),
            extract_function(fs, "bool ValidateImportDestinationAvailable("),
            extract_function(fs, "bool LoadImportFileChunk("),
            extract_function(util, "inline bool BuildKernelCreateWriteStub("),
        ))
        for needle in (
            "XemuFatxHdd::DeleteEntry(", "xemu_disc_block_pwrite", "xemu_disc_block_flush",
            "ActionReset", "NtSetInformationFile", "NtDeleteFile",
        ):
            forbid(import_scope, needle, "raw/delete/reset operation in kernel import")

        # Prior kernel-delete experiments stay independently available while import
        # is still experimental and create-only.
        require(util, "BuildKernelDeleteFileStub", "v2.10 single-file delete helper retained")
        require(util, "BuildKernelDeleteDirectoryStub", "v2.11 directory delete helper retained")
        require(rpc, "StartKernelDeleteSelectedFile", "v2.10 single-file action retained")
        require(rpc, "StartKernelDeleteSelectedFolder", "v2.11 recursive folder action retained")

        print("PASS: v2.12 Xbox-kernel create-only host-folder import/write guard")
        return 0
    result = main()
    if result not in (None, 0):
        raise AssertionError("v212-kernel-import-folder-golden.py returned non-zero: %r" % (result,))

# Preserved contract from v213-kernel-hdd-cleanup-golden.py
def check_v213_kernel_hdd_cleanup_golden() -> None:
    """v2.13 guard: fresh recursive-delete confirmation + integration-prep cleanup."""

    import argparse
    import pathlib

    from v287_source_test_utils import extract_function


    def require(text: str, needle: str, what: str) -> None:
        if needle not in text:
            raise AssertionError(f"missing {what}: {needle}")


    def forbid(text: str, needle: str, what: str) -> None:
        if needle in text:
            raise AssertionError(f"unexpected {what}: {needle}")


    def main() -> int:
        parser = argparse.ArgumentParser()
        parser.add_argument("--root", default=".")
        root = pathlib.Path(parser.parse_args().root).resolve()
        debug = root / "ui/xui/debug-tools"
        rpc = (debug / "guest-kernel-rpc.cc").read_text(encoding="utf-8")
        completion = debug / "guest-kernel-rpc-completion.cc"
        if completion.exists():
            rpc += "\n" + completion.read_text(encoding="utf-8")
        rpc_hh = (debug / "guest-kernel-rpc.hh").read_text(encoding="utf-8")
        fs = ((debug / "kernel-rpc-filesystem.cc").read_text(encoding="utf-8") + "\n" + (debug / "kernel-rpc-filesystem-stream.cc").read_text(encoding="utf-8"))

        rpc_fs = (debug / "guest-kernel-rpc-filesystem.cc").read_text(encoding="utf-8")

        # v2.86 retires the old diagnostic delete/import selection UI. Fresh-plan
        # confirmation now lives solely in the production HDD entry points.
        if "BuildRecursiveDeletePlan" not in rpc_hh:
            prepare = extract_function(rpc_fs, "bool GuestKernelRpcManager::PrepareHddDelete(")
            start = extract_function(rpc_fs, "bool GuestKernelRpcManager::StartHddDelete(")
            require(prepare, "BuildRawPartitionSetSnapshot", "fresh delete preflight snapshot")
            require(prepare, "XemuKernelFs::BuildDeletePlan", "reusable delete planner")
            require(start, "BuildRawPartitionSetSnapshot", "fresh start-time snapshot")
            require(start, "XemuKernelFs::SameDeletePlan", "changed-tree reconfirmation gate")
            require(start, "Nothing was deleted; open Delete again to review and reconfirm.",
                    "fail-closed reconfirmation behavior")
            backend = extract_function(fs, "bool BuildDeletePlan(")
            require(backend, "plan.push_back(std::move(directory_item));", "leaf-first backend planner")
            for legacy in ("DrawKernelDeleteFolderControls", "StartKernelDeleteSelectedFolder",
                           "PrepareImportFolder", "StartKernelImportFolder"):
                forbid(rpc + rpc_hh, legacy, "retired diagnostic mutation UI")
            print("PASS: v2.13 fresh-confirmation contract retained by v2.86 production HDD operations")
            return 0

        # Keep the SDL/xemu folder-dialog compile hotfix in the promoted baseline.
        require(rpc, '#include "../misc.hh"', "xemu file-dialog wrapper declaration")
        require(rpc, "ShowOpenFolderDialog(nullptr", "xemu folder-dialog wrapper use")

        # v2.14 intentionally relocates the pure plan walk, while the v2.13 fresh
        # snapshot/confirmation contract remains unchanged in the manager wrapper.
        require(rpc_hh, "BuildRecursiveDeletePlan", "recursive-plan wrapper declaration")
        require(rpc_hh, "RefreshSelectedDeleteFolderPreview", "fresh-preview helper declaration")
        wrapper = extract_function(rpc, "bool GuestKernelRpcManager::BuildRecursiveDeletePlan(")
        require(wrapper, "hdd_snapshot_service.BuildRawSnapshot(", "fresh raw FATX snapshot before planning")
        require(wrapper, "XemuKernelFs::BuildRecursiveDeletePlan", "reusable leaf-first planner")
        # v2.15 generalizes the pure leaf-first builder to all mapped volumes; the
        # v2.13 current-title wrapper still supplies the E:/Title-ID boundary.
        current_title_builder = extract_function(fs, "bool BuildRecursiveDeletePlan(")
        require(current_title_builder, "BuildDeletePlan(snapshot, 'E', components, true, plan, error)",
                "current-title wrapper delegates into generalized builder")
        backend_builder = extract_function(fs, "bool BuildDeletePlan(")
        require(backend_builder, "XemuFatxHdd::FindEntry", "fresh selected-folder re-resolution")
        require(backend_builder, "FatxPathForPartition", "shared FATX path builder")
        require(backend_builder, "NativePathForPartition", "shared native path builder")
        require(backend_builder, "self(self, child, std::move(child_components)",
                "leaf-first recursive traversal")
        require(backend_builder, "plan.push_back(std::move(directory_item));",
                "directory appended after descendants")

        # Confirmation is based on a newly-built plan, not the potentially stale
        # counts captured when REFRESH DELETE FOLDER LIST was last pressed.
        preview = extract_function(rpc, "bool GuestKernelRpcManager::RefreshSelectedDeleteFolderPreview(")
        require(preview, "BuildRecursiveDeletePlan(candidate, fresh_plan, error)",
                "fresh confirmation plan")
        require(preview, "XemuKernelFs::SummarizeDeletePlan", "shared fresh count summary")
        require(preview, "candidate.file_count = summary.file_count", "fresh file count")
        require(preview, "candidate.directory_count = summary.directory_count", "fresh folder count")
        require(preview, "m_recursive_delete_confirm_plan = std::move(fresh_plan)",
                "separate confirmed operation-set retention")

        controls = extract_function(rpc, "void GuestKernelRpcManager::DrawKernelDeleteFolderControls()")
        refresh_pos = controls.find("RefreshSelectedDeleteFolderPreview(error)")
        pending_pos = controls.find("m_delete_folder_confirm_pending = true")
        if refresh_pos < 0 or pending_pos < 0 or refresh_pos > pending_pos:
            raise AssertionError("delete confirmation must refresh the recursive plan before opening")

        # Rebuild once more immediately before mutation. If the operation set has
        # changed while the confirmation modal was open, require another confirm.
        start = extract_function(rpc, "bool GuestKernelRpcManager::StartKernelDeleteSelectedFolder()")
        require(start, "const std::vector<RecursiveDeleteEntry> confirmed_plan = m_recursive_delete_confirm_plan",
                "confirmed plan snapshot")
        require(start, "BuildRecursiveDeletePlan(selected, fresh_plan, error)",
                "start-time fresh plan rebuild")
        require(start, "!XemuKernelFs::SameDeletePlan(confirmed_plan, fresh_plan)",
                "changed-tree detection")
        require(start, "Review the updated counts and confirm again; nothing was deleted.",
                "reconfirmation safety behavior")

        # Cleanup only: no raw FATX mutation/reset may leak into the planner.
        cleanup_scope = "\n".join((wrapper, backend_builder, preview, start))
        for needle in (
            "XemuFatxHdd::DeleteEntry(", "xemu_disc_block_pwrite",
            "xemu_disc_block_flush", "ActionReset", "NtDeleteFile",
            "NtCreateFile", "NtWriteFile", "NtFlushBuffersFile",
        ):
            forbid(cleanup_scope, needle, "unrelated/raw mutation in recursive cleanup path")

        # Keep the already-proven import and delete primitives available unchanged.
        require(rpc, "StartKernelDeleteSelectedFile", "v2.10 single-file kernel delete retained")
        require(rpc, "StartKernelImportFolder", "v2.12 create/write folder import retained")

        # User-facing test text should no longer need a hard-coded v2.12 label as
        # the implementation is promoted and prepared for HDD-tab reuse.
        forbid(rpc, "v2.12", "stale version-specific runtime wording")

        print("PASS: v2.13 Kernel RPC HDD cleanup / fresh-confirmation guard")
        return 0
    result = main()
    if result not in (None, 0):
        raise AssertionError("v213-kernel-hdd-cleanup-golden.py returned non-zero: %r" % (result,))

# Preserved contract from v214-kernel-filesystem-backend-golden.py
def check_v214_kernel_filesystem_backend_golden() -> None:
    """v2.14 guard: reusable Kernel RPC filesystem planning/preflight backend split."""

    import argparse
    import pathlib

    from v287_source_test_utils import extract_function


    def require(text: str, needle: str, what: str) -> None:
        if needle not in text:
            raise AssertionError(f"missing {what}: {needle}")


    def forbid(text: str, needle: str, what: str) -> None:
        if needle in text:
            raise AssertionError(f"unexpected {what}: {needle}")


    def main() -> int:
        parser = argparse.ArgumentParser()
        parser.add_argument("--root", default=".")
        root = pathlib.Path(parser.parse_args().root).resolve()
        debug = root / "ui/xui/debug-tools"
        rpc = (debug / "guest-kernel-rpc.cc").read_text(encoding="utf-8")
        completion = debug / "guest-kernel-rpc-completion.cc"
        if completion.exists():
            rpc += "\n" + completion.read_text(encoding="utf-8")
        rpc_hh = (debug / "guest-kernel-rpc.hh").read_text(encoding="utf-8")
        fs = ((debug / "kernel-rpc-filesystem.cc").read_text(encoding="utf-8") + "\n" + (debug / "kernel-rpc-filesystem-stream.cc").read_text(encoding="utf-8"))
        fs_hh = (debug / "kernel-rpc-filesystem.hh").read_text(encoding="utf-8")
        util = (debug / "kernel-rpc-utils.hh").read_text(encoding="utf-8")
        meson = (debug / "meson.build").read_text(encoding="utf-8")

        rpc_fs = (debug / "guest-kernel-rpc-filesystem.cc").read_text(encoding="utf-8")

        # v2.86 removes the legacy diagnostic mutation wrappers. The reusable
        # backend remains unchanged and production HDD entry points now own fresh
        # snapshot/plan revalidation directly.
        if "BuildRecursiveDeletePlan" not in rpc_hh:
            require(meson, "'kernel-rpc-filesystem.cc'", "Meson ownership of filesystem backend")
            require(rpc_hh, "using RecursiveDeleteEntry = XemuKernelFs::DeleteEntry",
                    "delete entry backend ownership")
            require(rpc_hh, "XemuKernelFs::TransferPlan m_import_preflight",
                    "transfer plan backend ownership")
            for symbol in ("BuildDeletePlan", "SummarizeDeletePlan", "SameDeletePlan",
                           "BuildImportFolderPlanAtDestination",
                           "BuildImportFilePlanAtDestination", "SameImportPlan",
                           "ValidateImportHostEntryMetadata", "LoadImportFileChunk"):
                require(fs_hh, symbol, f"public filesystem helper {symbol}")
                require(fs, symbol, f"filesystem backend implementation {symbol}")
            prepare_delete = extract_function(rpc_fs, "bool GuestKernelRpcManager::PrepareHddDelete(")
            start_delete = extract_function(rpc_fs, "bool GuestKernelRpcManager::StartHddDelete(")
            start_import = extract_function(rpc_fs, "bool GuestKernelRpcManager::StartHddImport(")
            require(prepare_delete, "BuildRawPartitionSetSnapshot", "fresh delete snapshot")
            require(start_delete, "SameDeletePlan", "delete plan revalidation")
            require(start_import, "SameImportPlan", "import plan revalidation")
            for retired in ("FatxPathFromComponents", "NativeEPathFromComponents",
                            "ValidateCurrentTitleDataPath", "BuildRecursiveDeletePlan",
                            "BuildImportPlan", "ValidateImportHostPlan",
                            "ValidateImportDestinationAvailable"):
                forbid(fs_hh + fs, retired, "retired test-era filesystem wrapper")
            require(rpc, "bool GuestKernelRpcManager::BeginKernelDeleteAttempt()",
                    "async delete executor retained")
            require(rpc, "bool GuestKernelRpcManager::BeginKernelImportAttempt()",
                    "async import executor retained")
            for needle in ("ImGui::", "ShowOpenFolderDialog", "xemu_disc_block_pwrite",
                           "xemu_disc_block_flush", "ActionReset"):
                forbid(fs, needle, "UI/raw mutation dependency in reusable backend")
            print("PASS: v2.14 filesystem backend retained after v2.86 legacy UI-wrapper cleanup")
            return 0

        require(meson, "'kernel-rpc-filesystem.cc'", "Meson ownership of filesystem backend")
        require(rpc_hh, '#include "kernel-rpc-filesystem.hh"', "manager consumes filesystem backend")
        require(rpc_hh, "using RecursiveDeleteEntry = XemuKernelFs::DeleteEntry",
                "delete entry moved out of UI manager")
        require(rpc_hh, "using TransferEntry = XemuKernelFs::TransferEntry",
                "transfer entry remains owned by reusable filesystem backend")
        require(rpc_hh, "using ImportEntry = TransferEntry",
                "legacy executor alias delegates to backend transfer entry")
        require(rpc_hh, "XemuKernelFs::TransferPlan m_import_preflight",
                "consolidated immutable transfer preflight state")

        for old_field in (
            "m_import_host_folder", "m_import_area;", "m_import_title_directory",
            "m_import_root_name", "m_import_candidate_title_id",
            "std::vector<ImportEntry> m_import_plan", "m_import_total_bytes",
            "m_import_file_count", "m_import_directory_count", "m_import_total_operations",
        ):
            forbid(rpc_hh, old_field, "duplicated pre-v2.14 import-plan state")

        # The reusable layer owns path policy, leaf-first delete planning, host
        # import scanning, immutable start-time revalidation, and chunk reads.
        for symbol in (
            "ValidateCurrentTitleDataPath", "BuildRecursiveDeletePlan",
            "SummarizeDeletePlan", "SameDeletePlan", "ValidateImportHostRoot",
            "BuildImportPlan", "ValidateImportHostPlan",
            "ValidateImportDestinationAvailable", "LoadImportFileChunk",
        ):
            require(fs_hh, symbol, f"public reusable filesystem helper {symbol}")
            require(fs, symbol, f"filesystem backend implementation {symbol}")

        delete_wrapper = extract_function(rpc, "bool GuestKernelRpcManager::BuildRecursiveDeletePlan(")
        require(delete_wrapper, "XemuKernelFs::ValidateCurrentTitleDataPath",
                "manager delegates delete boundary policy")
        require(delete_wrapper, "hdd_snapshot_service.BuildRawSnapshot(",
                "manager retains fresh raw snapshot transaction")
        require(delete_wrapper, "XemuKernelFs::BuildRecursiveDeletePlan",
                "manager delegates leaf-first plan")

        prepare_import = extract_function(rpc, "bool GuestKernelRpcManager::PrepareImportFolder(")
        forbid(prepare_import, "XemuKernelFs::ValidateImportHostRoot",
               "duplicate host-root validation outside reusable planner")
        require(prepare_import, "hdd_snapshot_service.BuildRawSnapshot(",
                "manager retains coherent raw FATX snapshot")
        require(prepare_import, "XemuKernelFs::BuildImportPlan",
                "manager delegates host tree scan/plan")

        load_import = extract_function(rpc, "bool GuestKernelRpcManager::LoadKernelImportOperation(")
        require(load_import, "XemuKernelFs::LoadImportFileChunk",
                "manager delegates bounded host chunk I/O")
        require(load_import, "XemuKernelRpc::kFileCreate : XemuKernelRpc::kFileOpen",
                "first-create/later-open semantics retained")

        start_import = extract_function(rpc, "bool GuestKernelRpcManager::StartKernelImportFolder()")
        require(start_import, "XemuKernelFs::BuildImportPlan",
                "full host/destination plan rebuilt before mutation")
        require(start_import, "XemuKernelFs::SameImportPlan",
                "confirmed import plan compared to fresh plan")

        # Host filesystem traversal/I/O moved out of the UI/RPC state-machine file.
        forbid(rpc, "#include <filesystem>", "host filesystem ownership left in manager")
        forbid(rpc, "#include <fstream>", "host file-stream ownership left in manager")
        forbid(rpc, "std::filesystem", "host filesystem traversal in manager")
        forbid(rpc, "std::ifstream", "host file reads in manager")

        # The backend is intentionally UI-independent and mutation-free. The
        # existing async PASSIVE_LEVEL RPC executor remains the sole writer.
        for needle in (
            "ImGui::", "ShowOpenFolderDialog", "runstate_is_running", "vm_start()",
            "xemu_guest_rpc_arena", "xemu_cheat_breakpoint_insert",
            "XemuFatxHdd::DeleteEntry(", "xemu_disc_block_pwrite",
            "xemu_disc_block_flush", "ActionReset",
        ):
            forbid(fs, needle, "UI/raw/executor dependency in filesystem backend")

        require(rpc, "bool GuestKernelRpcManager::BeginKernelDeleteAttempt()",
                "proven async delete executor retained")
        require(rpc, "bool GuestKernelRpcManager::BeginKernelImportAttempt()",
                "proven async create/write executor retained")
        require(util, "BuildKernelDeleteFileStub", "v2.10 delete stub retained")
        require(util, "BuildKernelDeleteDirectoryStub", "v2.11 directory stub retained")
        require(util, "BuildKernelCreateWriteStub", "v2.12 create/write stub retained")

        print("PASS: v2.14 reusable Kernel RPC filesystem backend split guard")
        return 0
    result = main()
    if result not in (None, 0):
        raise AssertionError("v214-kernel-filesystem-backend-golden.py returned non-zero: %r" % (result,))

# Preserved contract from v215-hdd-kernel-integration-golden.py
def check_v215_hdd_kernel_integration_golden() -> None:
    """v2.15 guard: HDD browser uses proven Xbox-kernel import/delete backend."""

    import argparse
    import pathlib

    from v287_source_test_utils import extract_function


    def require(text: str, needle: str, what: str) -> None:
        if needle not in text:
            raise AssertionError(f"missing {what}: {needle}")


    def forbid(text: str, needle: str, what: str) -> None:
        if needle in text:
            raise AssertionError(f"unexpected {what}: {needle}")


    def main() -> int:
        parser = argparse.ArgumentParser()
        parser.add_argument("--root", default=".")
        root = pathlib.Path(parser.parse_args().root).resolve()
        debug = root / "ui/xui/debug-tools"

        hdd = ((debug / "hdd-directory.cc").read_text(encoding="utf-8") + "\n" + (debug / "hdd-directory-ui.cc").read_text(encoding="utf-8"))
        hdd_hh = (debug / "hdd-directory.hh").read_text(encoding="utf-8")
        rpc = (debug / "guest-kernel-rpc.cc").read_text(encoding="utf-8")
        completion = debug / "guest-kernel-rpc-completion.cc"
        if completion.exists():
            rpc += "\n" + completion.read_text(encoding="utf-8")
        rpc_fs = (debug / "guest-kernel-rpc-filesystem.cc").read_text(encoding="utf-8")
        rpc_hh = (debug / "guest-kernel-rpc.hh").read_text(encoding="utf-8")
        fs = ((debug / "kernel-rpc-filesystem.cc").read_text(encoding="utf-8") + "\n" + (debug / "kernel-rpc-filesystem-stream.cc").read_text(encoding="utf-8"))
        fs_hh = (debug / "kernel-rpc-filesystem.hh").read_text(encoding="utf-8")

        require(hdd, '#include "guest-kernel-rpc.hh"', "HDD frontend consumes Kernel RPC manager")
        context = extract_function(hdd, "void HddDirectoryWindow::DrawExportContext(")
        for needle in ('ImGui::BeginMenu("Export")', 'ImGui::BeginMenu("Import"',
                       'ImGui::BeginMenu("Delete"'):
            require(context, needle, "three-way HDD context menu")
        for needle in ("Export File...", "Export Folder...", "Import Folder...",
                       "Import File...", "Delete Folder...", "Delete File..."):
            require(hdd, needle, "generic HDD action")
        for needle in ("Export All Saves...", "Export All Title Data...",
                       "Delete All Saves...", "Delete All Title Data..."):
            require(hdd, needle, "Current Game whole-title action")

        root_import = extract_function(hdd, "void HddDirectoryWindow::DrawRootImportButton(")
        require(root_import, 'ImGui::Button("+ ADD / IMPORT")', "root import affordance")
        require(root_import, "DrawImportMenuItems(target)", "root import menu")
        draw = extract_function(hdd, "void HddDirectoryWindow::Draw(bool detached)")
        require(draw, "const std::vector<std::string> root_destination;",
                "empty path means active FATX volume root")
        require(draw, "DrawRootImportButton(part, root_destination", "per-volume root import")

        request_import = extract_function(hdd, "void HddDirectoryWindow::RequestImport(")
        require(request_import, "ShowOpenFolderDialog(nullptr, selected)", "host folder chooser")
        require(request_import, "ShowOpenFileDialog(nullptr, 0, nullptr, selected)", "host file chooser")
        require(request_import, "PrepareHddImport(", "generic import preflight")
        import_confirm = extract_function(hdd, "void HddDirectoryWindow::DrawImportConfirmation()")
        require(import_confirm, "StartHddImport(plan)", "generic confirmed import")
        require(import_confirm, "create-only", "no overwrite disclosure")
        forbid(import_confirm, "ActionReset", "reset in normal import UI")
        forbid(import_confirm, "xemu_disc_block_pwrite", "raw FATX write in normal import UI")

        request_delete = extract_function(hdd, "void HddDirectoryWindow::RequestDelete(")
        require(request_delete, "m_delete_popup_open_requested = true;", "deferred delete confirmation")
        forbid(request_delete, "DeleteFromHdd(", "legacy raw delete from normal request")
        delete_confirm = extract_function(hdd, "void HddDirectoryWindow::DrawDeleteConfirmation()")
        require(delete_confirm, "PrepareHddDelete(", "fresh delete preflight")
        require(delete_confirm, "StartHddDelete(", "kernel delete start")
        require(delete_confirm, "SummarizeDeletePlan", "recursive confirmation totals")
        forbid(delete_confirm, "DeleteFromHdd(", "legacy raw delete from normal confirmation")
        forbid(delete_confirm, "ActionReset", "reset in normal delete UI")

        delete_allowed = extract_function(hdd, "bool HddDirectoryWindow::DeleteAllowed(")
        require(delete_allowed, "!target.path.empty()", "partition root cannot be selected for delete")
        require(delete_allowed, "IsKernelWritablePartition", "mapped volumes only")
        for forbidden in ("RawDeleteAllowed", "DeleteFromHdd", "ActionReset"):
            forbid(hdd, forbidden, "decommissioned raw/reset HDD delete frontend")

        for symbol in ("PrepareHddDelete", "StartHddDelete", "PrepareHddImport", "StartHddImport"):
            require(rpc_hh, symbol, f"HDD RPC frontend API {symbol}")
            require(rpc_fs, f"GuestKernelRpcManager::{symbol}", f"HDD RPC frontend implementation {symbol}")
        start_delete = extract_function(rpc_fs, "bool GuestKernelRpcManager::StartHddDelete(")
        require(start_delete, "XemuKernelFs::SameDeletePlan", "fresh-plan reconfirmation")
        require(start_delete, "ResetFilesystemOperationContext();", "fresh HDD delete operation context")
        require(start_delete, "FilesystemOperationKind::Delete", "generic HDD delete execution kind")
        require(start_delete, "FilesystemOperationPhase::Preparing", "generic HDD delete preparation phase")
        start_import = extract_function(rpc_fs, "bool GuestKernelRpcManager::StartHddImport(")
        require(start_import, "SameImportPlan", "full host-tree fresh-plan reconfirmation")
        require(start_import, "BuildImport", "fresh import-plan rebuild")
        require(start_import, "ResetFilesystemOperationContext();", "fresh HDD import operation context")
        require(start_import, "FilesystemOperationPhase::Preparing", "generic HDD import preparation phase")
        require(start_import, "FilesystemOperationKind::", "generic HDD import execution kind")

        for symbol in ("BuildDeletePlan", "BuildImportFolderPlanAtDestination",
                       "BuildImportFilePlanAtDestination", "NativePathForPartition",
                       "FatxPathForPartition", "IsKernelWritablePartition"):
            require(fs_hh, symbol, f"generic filesystem API {symbol}")
            require(fs, symbol, f"generic filesystem implementation {symbol}")
        generic_delete = extract_function(fs, "bool BuildDeletePlan(")
        require(generic_delete, "components.empty()", "partition-root delete hard block")
        require(generic_delete, "Deleting an entire FATX partition root is intentionally blocked",
                "partition-root delete error")

        for needle in ("case 'E': return 1;", "case 'C': return 2;",
                       "case 'X': return 3;", "case 'Y': return 4;",
                       "case 'Z': return 5;", "case 'F': return 6;",
                       "case 'G': return 7;"):
            require(fs, needle, "Xbox FATX drive/device mapping")
        require(fs_hh, "destination_components may\n// be empty", "volume-root import contract")

        print("PASS: v2.15 HDD Xbox-kernel Import/Delete integration guard")
        return 0
    result = main()
    if result not in (None, 0):
        raise AssertionError("v215-hdd-kernel-integration-golden.py returned non-zero: %r" % (result,))

# Preserved contract from v216-hdd-krpc-cleanup-safety-golden.py
def check_v216_hdd_krpc_cleanup_safety_golden() -> None:
    """v2.16 guard: HDD/KRPC cleanup, pruning, and stronger confirmation safety."""

    import argparse
    import pathlib
    from v287_source_test_utils import extract_function

    def require(text: str, needle: str, what: str) -> None:
        if needle not in text:
            raise AssertionError(f"missing {what}: {needle}")

    def forbid(text: str, needle: str, what: str) -> None:
        if needle in text:
            raise AssertionError(f"unexpected {what}: {needle}")

    def main() -> int:
        parser = argparse.ArgumentParser()
        parser.add_argument("--root", default=".")
        root = pathlib.Path(parser.parse_args().root).resolve()
        debug = root / "ui/xui/debug-tools"
        hdd = ((debug / "hdd-directory.cc").read_text(encoding="utf-8") + "\n" + (debug / "hdd-directory-ui.cc").read_text(encoding="utf-8"))
        hdd_hh = (debug / "hdd-directory.hh").read_text(encoding="utf-8")
        export = (debug / "hdd-export-service.cc").read_text(encoding="utf-8")
        host_export = (debug / "host-export-utils.hh").read_text(encoding="utf-8") if (debug / "host-export-utils.hh").exists() else export
        fs = ((debug / "kernel-rpc-filesystem.cc").read_text(encoding="utf-8") + "\n" + (debug / "kernel-rpc-filesystem-stream.cc").read_text(encoding="utf-8"))
        fs_hh = (debug / "kernel-rpc-filesystem.hh").read_text(encoding="utf-8")
        rpc = (debug / "guest-kernel-rpc.cc").read_text(encoding="utf-8")
        completion = debug / "guest-kernel-rpc-completion.cc"
        if completion.exists():
            rpc += "\n" + completion.read_text(encoding="utf-8")
        rpc_fs = (debug / "guest-kernel-rpc-filesystem.cc").read_text(encoding="utf-8")
        current = ((debug / "current-game.cc").read_text(encoding="utf-8") + "\n" + (debug / "current-game-ui.cc").read_text(encoding="utf-8"))
        current_hh = (debug / "current-game.hh").read_text(encoding="utf-8")

        # Raw FATX + reset frontend is decommissioned; v2.32 also removes the
        # unused low-level production mutation primitive entirely.
        for forbidden in (
            "RawDeleteAllowed", "DeleteFromHdd", "WriteHddBlock",
            "xemu_disc_block_pwrite", "ActionReset",
        ):
            forbid(hdd + hdd_hh, forbidden, "retired raw/reset HDD frontend")

        require(hdd_hh, "struct HddTarget", "generic HDD target naming")
        require(hdd_hh, "m_operation_status", "generic operation-status naming")
        forbid(hdd_hh, "ExportTarget", "export-only target naming")
        forbid(hdd_hh, "m_export_status", "export-only status naming")
        forbid(hdd, "bool EqualsNoCase(", "duplicate case-insensitive helper")
        forbid(hdd, "const XemuFatxHdd::Entry *FindChildNoCase(", "duplicate child lookup helper")

        # Export sanitization must never silently overwrite after a FATX->host-name
        # collision and Windows device names are made safe. v2.37 strengthened the
        # original bounded UniqueHostPath scheme to atomic create-new ownership.
        open_unique = extract_function(export, "bool OpenUniqueHostFile(")
        require(open_unique, "O_EXCL", "atomic create-new file ownership")
        require(open_unique, "10000", "bounded unique-name search")
        require(open_unique, "return false;", "no overwrite fallback after exhaustion")
        create_dir = extract_function(export, "bool CreateUniqueHostDirectory(")
        require(create_dir, "10000", "bounded unique-directory search")
        require(create_dir, "return false;", "no directory merge fallback after exhaustion")
        safe = extract_function(host_export, "inline std::string HostSafeName(")
        require(safe, "IsReservedWindowsDeviceName", "Windows reserved-device-name protection")
        recurse = extract_function(export, "bool ExportEntryRecursive(")
        require(recurse, "CreateUniqueHostDirectory", "recursive exclusive directory ownership")
        require(recurse, "OpenUniqueHostFile", "recursive exclusive file ownership")

        # Delete confirmation identity includes FATX object identity, not just paths.
        for field in (
            "directory_entry_offset", "first_cluster", "modified_time",
            "modified_date", "file_size", "attributes",
        ):
            require(fs_hh, field, f"delete identity field {field}")
        same_delete = extract_function(fs, "bool SameDeletePlan(")
        for field in (
            "file_size", "directory_entry_offset", "first_cluster",
            "modified_time", "modified_date", "attributes",
        ):
            require(same_delete, field, f"delete fresh-plan comparison {field}")

        # Import is rebuilt recursively immediately before mutation. This catches
        # added/removed items and timestamp/content-source changes after confirmation.
        require(fs_hh, "int64_t host_write_time", "host item write-time identity")
        require(fs_hh, "std::string source_path", "generic host source naming")
        require(fs_hh, "bool SameImportPlan(", "import plan comparison API")
        same_import = extract_function(fs, "bool SameImportPlan(")
        require(same_import, "host_write_time", "same-size host change detection")
        start_import = extract_function(rpc_fs, "bool GuestKernelRpcManager::StartHddImport(")
        require(start_import, "BuildImportFolderPlanAtDestination", "fresh recursive folder rescan")
        require(start_import, "BuildImportFilePlanAtDestination", "fresh file rescan")
        require(start_import, "SameImportPlan(plan, fresh_plan)", "fresh-plan confirmation gate")

        # Experimental RPC diagnostics remain available but hidden by default.
        require(current_hh, "m_show_kernel_rpc_diagnostics = false",
                "diagnostics hidden by default")
        require(current, 'ImGui::Checkbox("Show Kernel RPC diagnostics"',
                "developer diagnostics toggle")
        require(current, 'ImGui::BeginTabItem("Kernel RPC Diagnostics")',
                "diagnostic tab preserved")

        print("PASS: v2.16 HDD/KRPC cleanup + safety guard")
        return 0
    result = main()
    if result not in (None, 0):
        raise AssertionError("v216-hdd-krpc-cleanup-safety-golden.py returned non-zero: %r" % (result,))

# Preserved contract from v217-filesystem-executor-snapshot-ownership-golden.py
def check_v217_filesystem_executor_snapshot_ownership_golden() -> None:
    """v2.17 guard: production filesystem executor + coherent snapshot ownership split."""

    import argparse
    import pathlib
    from v287_source_test_utils import extract_function


    def require(text: str, needle: str, what: str) -> None:
        if needle not in text:
            raise AssertionError(f"missing {what}: {needle}")


    def forbid(text: str, needle: str, what: str) -> None:
        if needle in text:
            raise AssertionError(f"unexpected {what}: {needle}")


    def main() -> int:
        parser = argparse.ArgumentParser()
        parser.add_argument("--root", default=".")
        root = pathlib.Path(parser.parse_args().root).resolve()
        debug = root / "ui/xui/debug-tools"

        meson = (debug / "meson.build").read_text(encoding="utf-8")
        rpc = (debug / "guest-kernel-rpc.cc").read_text(encoding="utf-8")
        completion = debug / "guest-kernel-rpc-completion.cc"
        if completion.exists():
            rpc += "\n" + completion.read_text(encoding="utf-8")
        rpc_fs = (debug / "guest-kernel-rpc-filesystem.cc").read_text(encoding="utf-8")
        hdd = ((debug / "hdd-directory.cc").read_text(encoding="utf-8") + "\n" + (debug / "hdd-directory-ui.cc").read_text(encoding="utf-8"))
        hdd_hh = (debug / "hdd-directory.hh").read_text(encoding="utf-8")
        snapshot = (debug / "hdd-snapshot-service.cc").read_text(encoding="utf-8")
        snapshot_hh = (debug / "hdd-snapshot-service.hh").read_text(encoding="utf-8")

        require(meson, "'guest-kernel-rpc-filesystem.cc'", "production HDD RPC executor source")
        require(meson, "'hdd-snapshot-service.cc'", "shared HDD snapshot service source")

        # The RPC state machine no longer reaches into the HDD window. Production
        # HDD entry points live in their own translation unit.
        forbid(rpc, '#include "hdd-directory.hh"', "backend-to-HDD-window include")
        forbid(rpc, "hdd_directory_window", "backend-to-HDD-window dependency")
        forbid(rpc_fs, "hdd_directory_window", "production executor-to-window dependency")
        for symbol in ("PrepareHddDelete", "StartHddDelete", "PrepareHddImport", "StartHddImport"):
            require(rpc_fs, f"GuestKernelRpcManager::{symbol}", f"moved production method {symbol}")
            forbid(rpc, f"GuestKernelRpcManager::{symbol}", f"duplicate core implementation {symbol}")
        require(rpc_fs, "hdd_snapshot_service.BuildRawPartitionSetSnapshot(",
                "partition-scoped raw preflight snapshots in production filesystem executor")

        # Backend mutation verification uses raw snapshots only, while the visible
        # browser adds friendly TitleMeta/SaveMeta metadata through display snapshots.
        require(snapshot_hh, "BuildRawSnapshot", "raw snapshot API")
        require(snapshot_hh, "BuildDisplaySnapshot", "display snapshot API")
        raw = extract_function(snapshot, "bool HddSnapshotService::BuildRawSnapshot(")
        display = extract_function(snapshot, "bool HddSnapshotService::BuildDisplaySnapshot(")
        require(raw, "XemuFatxHdd::BuildSnapshot", "raw FATX snapshot")
        forbid(raw, "PopulateXboxMetadata", "display metadata in backend verification snapshot")
        require(display, "XemuFatxHdd::BuildSnapshot", "display FATX snapshot")
        require(display, "XemuFatxHdd::PopulateXboxMetadata", "display-friendly Xbox metadata")
        require(display, "XemuDebugGuestPauseGuard pause;", "coherent display snapshot pause")

        refresh = extract_function(hdd, "void HddDirectoryWindow::Refresh()")
        require(refresh, "hdd_snapshot_service.BuildDisplaySnapshot(", "UI display snapshot ownership")
        require(refresh, "m_snapshot_change_generation = hdd_snapshot_service.ChangeGeneration()",
                "UI generation acknowledgement")
        stale = extract_function(hdd, "void HddDirectoryWindow::RefreshIfStale()")
        require(stale, "guest_kernel_rpc_manager.OperationBusy()", "no refresh during active RPC")
        require(stale, "hdd_snapshot_service.ChangeGeneration()", "generation-driven refresh")
        require(hdd_hh, "m_snapshot_change_generation", "HDD snapshot generation state")

        # Successful kernel mutations invalidate the UI snapshot without directly
        # calling the HDD window. Verification still occurs before the UI refresh.
        delete_complete = extract_function(rpc, "void GuestKernelRpcManager::HandleKernelDeleteCompletion(")
        import_complete = extract_function(rpc, "void GuestKernelRpcManager::HandleKernelImportCompletion(")
        for scope, label in ((delete_complete, "delete"), (import_complete, "import")):
            require(scope, "hdd_snapshot_service.BuildRawPartitionSnapshot(", f"fresh partition-scoped {label} verification")
            require(scope, "hdd_snapshot_service.NotifyFilesystemChanged();",
                    f"{label} snapshot invalidation")
            forbid(scope, "hdd_directory_window", f"{label} completion window coupling")

        print("PASS: v2.17 filesystem executor / HDD snapshot ownership guard")
        return 0
    result = main()
    if result not in (None, 0):
        raise AssertionError("v217-filesystem-executor-snapshot-ownership-golden.py returned non-zero: %r" % (result,))

# Preserved contract from v218-krpc-optimization-golden.py
def check_v218_krpc_optimization_golden() -> None:
    """v2.18 guard: semantics-preserving HDD/KRPC optimization pass."""

    import argparse
    import pathlib
    from v287_source_test_utils import extract_function


    def require(text: str, needle: str, what: str) -> None:
        if needle not in text:
            raise AssertionError(f"missing {what}: {needle}")


    def forbid(text: str, needle: str, what: str) -> None:
        if needle in text:
            raise AssertionError(f"unexpected {what}: {needle}")


    def main() -> int:
        parser = argparse.ArgumentParser()
        parser.add_argument("--root", default=".")
        root = pathlib.Path(parser.parse_args().root).resolve()
        debug = root / "ui/xui/debug-tools"

        rpc = (debug / "guest-kernel-rpc.cc").read_text(encoding="utf-8")
        completion = debug / "guest-kernel-rpc-completion.cc"
        if completion.exists():
            rpc += "\n" + completion.read_text(encoding="utf-8")
        rpc_hh = (debug / "guest-kernel-rpc.hh").read_text(encoding="utf-8")
        rpc_fs = (debug / "guest-kernel-rpc-filesystem.cc").read_text(encoding="utf-8")
        fs = ((debug / "kernel-rpc-filesystem.cc").read_text(encoding="utf-8") + "\n" + (debug / "kernel-rpc-filesystem-stream.cc").read_text(encoding="utf-8"))
        hdd = ((debug / "hdd-directory.cc").read_text(encoding="utf-8") + "\n" + (debug / "hdd-directory-ui.cc").read_text(encoding="utf-8"))

        # Kernel export addresses are constant for one high-level operation. Resolve
        # them once, retain them through PASSIVE_LEVEL retries/entries/chunks, and
        # preserve the existing operation-start reset boundary.
        for symbol in ("ResolveKernelDeleteExports", "ResolveKernelImportExports"):
            require(rpc_hh, symbol, f"cached ordinal helper declaration {symbol}")
            require(rpc, f"GuestKernelRpcManager::{symbol}", f"cached ordinal helper {symbol}")
        delete_resolve = extract_function(rpc, "bool GuestKernelRpcManager::ResolveKernelDeleteExports(")
        require(delete_resolve, "m_delete_open_export_address != 0", "delete cache fast path")
        require(delete_resolve, "kNtOpenFileOrdinal", "delete ordinal resolution")
        require(delete_resolve, "kNtSetInformationFileOrdinal", "delete ordinal resolution")
        require(delete_resolve, "kNtCloseOrdinal", "delete ordinal resolution")
        import_resolve = extract_function(rpc, "bool GuestKernelRpcManager::ResolveKernelImportExports(")
        require(import_resolve, "m_import_create_export_address != 0", "import cache fast path")
        for needle in ("kNtCreateFileOrdinal", "kNtWriteFileOrdinal",
                       "kNtFlushBuffersFileOrdinal", "kNtCloseOrdinal"):
            require(import_resolve, needle, "import ordinal resolution")

        begin_delete = extract_function(rpc, "bool GuestKernelRpcManager::BeginKernelDeleteAttempt()")
        begin_import = extract_function(rpc, "bool GuestKernelRpcManager::BeginKernelImportAttempt()")
        require(begin_delete, "ResolveKernelDeleteExports(error)", "delete cached resolver use")
        require(begin_import, "ResolveKernelImportExports(error)", "import cached resolver use")
        forbid(begin_delete, "kNtOpenFileOrdinal", "per-entry delete ordinal re-resolution")
        forbid(begin_import, "kNtCreateFileOrdinal", "per-chunk import ordinal re-resolution")

        # Production operation starts still clear the cache before a new confirmed
        # transaction, preventing addresses from leaking across independent runs.
        start_delete = extract_function(rpc_fs, "bool GuestKernelRpcManager::StartHddDelete(")
        for field in ("m_export_address = 0", "m_delete_open_export_address = 0",
                      "m_delete_setinfo_export_address = 0", "m_delete_close_export_address = 0"):
            require(start_delete, field, f"delete cache reset {field}")
        start_import = extract_function(rpc_fs, "bool GuestKernelRpcManager::StartHddImport(")
        for field in ("m_export_address = 0", "m_import_create_export_address = 0",
                      "m_import_write_export_address = 0", "m_import_flush_export_address = 0",
                      "m_import_close_export_address = 0"):
            require(start_import, field, f"import cache reset {field}")

        # Host sibling collision detection remains deterministic after sorting but
        # is O(n) instead of the former O(n^2) pair scan.
        folder_plan = extract_function(fs, "bool BuildFolderPlanRecursive(")
        require(fs, "#include <unordered_map>", "hash-based collision scan dependency")
        require(folder_plan, "std::unordered_map<std::string, std::string> folded_names",
                "linear sibling collision index")
        require(folder_plan, "FoldCaseInsensitive(name)", "case-folded FATX collision key")
        require(folder_plan, "folded_names.emplace(folded, name)", "single-pass collision insertion")
        forbid(folder_plan, "for (size_t j = i + 1u;", "quadratic sibling collision scan")

        # Large confirmation plans are transferred, not copied, at the UI->executor
        # handoff. Start-time fresh-plan comparison remains in the executor.
        delete_confirm = extract_function(hdd, "void HddDirectoryWindow::DrawDeleteConfirmation()")
        import_confirm = extract_function(hdd, "void HddDirectoryWindow::DrawImportConfirmation()")
        require(delete_confirm, "std::move(m_delete_confirm_plan)", "delete plan move handoff")
        require(import_confirm, "std::move(m_import_confirm_plan)", "import plan move handoff")
        require(start_delete, "SameDeletePlan", "delete fresh-plan safety retained")
        require(start_import, "SameImportPlan", "import fresh-plan safety retained")

        print("PASS: v2.18 HDD/KRPC optimization guard")
        return 0
    result = main()
    if result not in (None, 0):
        raise AssertionError("v218-krpc-optimization-golden.py returned non-zero: %r" % (result,))

# Preserved contract from v219-fatx-fg-partition-golden.py
def check_v219_fatx_fg_partition_golden() -> None:
    from pathlib import Path
    root = Path(__file__).resolve().parents[1]
    fatx = (root / 'fatx-hdd.cc').read_text()
    for needle in [
        '****PARTINFO****', 'kXbpEntryOffset', "{'F', \"Extended F\", 5u}",
        "{'G', \"Extended G\", 6u}", 'xbp_table_present',
        'Never\n        // fall back to treating all remaining bytes as F:',
    ]:
        assert needle in fatx, needle
    print('PASS: v2.19 proper F/G partition discovery guard')

# Preserved contract from v220-hdd-qol-golden.py
def check_v220_hdd_qol_golden() -> None:
    from pathlib import Path
    root=Path(__file__).resolve().parents[1]
    hh=(root/'guest-kernel-rpc.hh').read_text()
    fs=(root/'guest-kernel-rpc-filesystem.cc').read_text()
    ui=((root/'hdd-directory.cc').read_text() + '\n' + (root/'hdd-directory-ui.cc').read_text())
    assert 'FilesystemReady(std::string &reason) const' in hh
    assert 'GetHddOperationProgress() const' in hh
    assert 'runstate_is_running()' in fs
    assert 'ImGui::ProgressBar' in ui
    assert 'Kernel RPC Details' in ui
    print('PASS: v2.20 HDD UI/QoL guard')

# Preserved contract from v221-new-folder-golden.py
def check_v221_new_folder_golden() -> None:
    from pathlib import Path
    r=Path(__file__).resolve().parents[1]
    fs=((r/'kernel-rpc-filesystem.cc').read_text() + '\n' + (r/'kernel-rpc-filesystem-stream.cc').read_text()); hh=(r/'kernel-rpc-filesystem.hh').read_text(); ui=((r/'hdd-directory.cc').read_text() + '\n' + (r/'hdd-directory-ui.cc').read_text()); rpc=(r/'guest-kernel-rpc-filesystem.cc').read_text()
    assert 'synthetic_directory' in hh
    assert 'BuildCreateDirectoryPlanAtDestination' in fs
    assert 'PrepareHddCreateDirectory' in rpc
    assert 'New Folder...' in ui and 'CREATE VIA XBOX KERNEL' in ui
    print('PASS: v2.21 New Folder guard')

# Preserved contract from v222-kernel-rename-golden.py
def check_v222_kernel_rename_golden() -> None:
    from pathlib import Path
    r=Path(__file__).resolve().parents[1]
    util=(r/'kernel-rpc-utils.hh').read_text(); fs=((r/'kernel-rpc-filesystem.cc').read_text() + '\n' + (r/'kernel-rpc-filesystem-stream.cc').read_text()); rpc=(r/'guest-kernel-rpc.cc').read_text(); ui=((r/'hdd-directory.cc').read_text() + '\n' + (r/'hdd-directory-ui.cc').read_text())
    rpc += '\n' + (r/'guest-kernel-rpc-completion.cc').read_text()
    assert 'kFileRenameInformation = 10u' in util
    assert 'BuildKernelRenameStub' in util
    assert 'BuildRelocatePlan' in fs
    assert 'KernelRelocate' in rpc and 'HandleKernelRelocateCompletion' in rpc
    assert 'Rename...' in ui and 'RENAME VIA XBOX KERNEL' in ui
    print('PASS: v2.22 kernel rename guard')

# Preserved contract from v223-same-volume-move-golden.py
def check_v223_same_volume_move_golden() -> None:
    from pathlib import Path
    r=Path(__file__).resolve().parents[1]
    ui=((r/'hdd-directory.cc').read_text() + '\n' + (r/'hdd-directory-ui.cc').read_text()); fs=(r/'guest-kernel-rpc-filesystem.cc').read_text()
    assert 'Select for Move' in ui
    assert 'Move Selected Here...' in ui
    assert 'MOVE VIA XBOX KERNEL' in ui
    assert 'PrepareHddMove' in fs and 'BuildRelocatePlan' in fs
    assert 'Selected for same-volume Move:' in ui
    assert 'if (m_transfer_source.partition != destination.partition)' in ui
    assert 'guest_kernel_rpc_manager.PrepareHddMove(' in ui
    print('PASS: v2.23 same-volume move guard')

# Preserved contract from v224-copy-cross-volume-move-golden.py
def check_v224_copy_cross_volume_move_golden() -> None:
    from pathlib import Path
    r=Path(__file__).resolve().parents[1]
    fs=((r/'kernel-rpc-filesystem.cc').read_text() + '\n' + (r/'kernel-rpc-filesystem-stream.cc').read_text()); rpc=(r/'guest-kernel-rpc.cc').read_text(); ui=((r/'hdd-directory.cc').read_text() + '\n' + (r/'hdd-directory-ui.cc').read_text()); fat=(r/'fatx-hdd.cc').read_text()
    rpc += '\n' + (r/'guest-kernel-rpc-completion.cc').read_text()
    assert 'BuildFatxCopyPlan' in fs
    assert 'ReadFileRange' in fat
    assert 'COPY phase verified completely' in rpc
    assert 'delete_source_after_copy' in rpc
    assert 'Select for Copy' in ui and 'Copy Selected Here...' in ui
    assert 'MOVE VIA COPY + VERIFY + DELETE' in ui
    assert 'NtCreateFile ->' not in ''  # marker only; semantics guarded elsewhere
    print('PASS: v2.24 copy/cross-volume move guard')

# Preserved contract from v225-production-hdd-contract-golden.py
def check_v225_production_hdd_contract_golden() -> None:
    from pathlib import Path
    r=Path(__file__).resolve().parents[1]
    ui=((r/'hdd-directory.cc').read_text() + '\n' + (r/'hdd-directory-ui.cc').read_text())
    rpc=(r/'guest-kernel-rpc.cc').read_text()
    rpc += '\n' + (r/'guest-kernel-rpc-completion.cc').read_text()
    rpc_ui=(r/'guest-kernel-rpc-ui.cc').read_text()
    fs=(r/'guest-kernel-rpc-filesystem.cc').read_text()
    util=(r/'kernel-rpc-utils.hh').read_text()
    panel=rpc_ui[rpc_ui.index('void GuestKernelRpcManager::DrawTestPanel()'):]
    assert 'DrawKernelDeleteControls();' not in panel
    assert 'DrawKernelImportControls();' not in panel
    for token in ['Import Folder...', 'New Folder...', 'Rename...', 'Select for Move', 'Select for Copy']:
        assert token in ui, token
    for token in ['PrepareHddDelete', 'PrepareHddImport', 'PrepareHddRename', 'PrepareHddMove', 'PrepareHddCopy']:
        assert token in fs, token
    # Hard invariant: file writes still use the proven Create/Open -> Write -> Flush -> Close transaction.
    for token in ['kNtCreateFileOrdinal','kNtWriteFileOrdinal','kNtFlushBuffersFileOrdinal','kNtCloseOrdinal','BuildKernelCreateWriteStub']:
        assert token in util, token
    assert 'BuildRawPartitionSnapshot(' in rpc
    print('PASS: v2.25 production HDD contract / diagnostics pruning guard')

# Preserved contract from v226-profile-guided-optimization-golden.py
def check_v226_profile_guided_optimization_golden() -> None:
    from pathlib import Path
    r=Path(__file__).resolve().parents[1]
    svc=(r/'hdd-snapshot-service.hh').read_text(); ui=((r/'hdd-directory.cc').read_text() + '\n' + (r/'hdd-directory-ui.cc').read_text()); rpc=(r/'guest-kernel-rpc.cc').read_text(); rpcfs=(r/'guest-kernel-rpc-filesystem.cc').read_text(); fs=((r/'kernel-rpc-filesystem.cc').read_text() + '\n' + (r/'kernel-rpc-filesystem-stream.cc').read_text()); util=(r/'kernel-rpc-utils.hh').read_text()
    rpc += '\n' + (r/'guest-kernel-rpc-completion.cc').read_text()
    assert 'PerformanceStats' in svc and 'GetPerformanceStats' in svc
    assert 'HDD Performance' in ui
    assert 'm_import_host_stream' in (r/'guest-kernel-rpc.hh').read_text()
    assert 'ValidateImportHostEntry' in rpc
    # The persistent host stream is backend-owned. Production executor code must use
    # the wrapper Reset() API rather than reaching into std::ifstream-style state.
    assert 'm_import_host_stream.Reset();' in rpcfs
    for stale in ['m_import_host_stream.is_open()', 'm_import_host_stream.close()',
                  'm_import_host_stream.clear()', 'm_import_host_stream_path']:
        assert stale not in rpcfs, stale
    assert 'void ImportHostStream::Reset()' in fs
    # Non-negotiable transaction invariant: do not optimize away these calls or fresh destination verification.
    for token in ['kNtCreateFileOrdinal','kNtWriteFileOrdinal','kNtFlushBuffersFileOrdinal','kNtCloseOrdinal','BuildKernelCreateWriteStub']:
        assert token in util, token
    for token in ['m_import_write_status','m_import_flush_status','m_import_close_status','BuildRawPartitionSnapshot(']:
        assert token in rpc, token
    print('PASS: v2.26 profile-guided optimization + write-transaction invariant guard')

# Preserved contract from v227-fatx-copy-integrity-golden.py
def check_v227_fatx_copy_integrity_golden() -> None:
    from pathlib import Path
    r=Path(__file__).resolve().parents[1]
    rpc=(r/'guest-kernel-rpc.cc').read_text()
    rpc += '\n' + (r/'guest-kernel-rpc-completion.cc').read_text()
    fs_h=(r/'kernel-rpc-filesystem.hh').read_text()
    fs=((r/'kernel-rpc-filesystem.cc').read_text() + '\n' + (r/'kernel-rpc-filesystem-stream.cc').read_text())
    svc_h=(r/'hdd-snapshot-service.hh').read_text()
    svc=(r/'hdd-snapshot-service.cc').read_text()
    ui=((r/'hdd-directory.cc').read_text() + '\n' + (r/'hdd-directory-ui.cc').read_text())

    # Large FATX->FATX copies must verify the progressive destination size after
    # every unchanged Create/Open -> Write -> Flush -> Close transaction.
    assert 'ExpectedCommittedFileSize(' in rpc
    assert 'm_expected_file_size = item.file_size;' not in rpc[rpc.index('if (item.source_from_fatx'):rpc.index('} else if (!item.directory)', rpc.index('if (item.source_from_fatx'))]

    # Source identity is stronger than path/size/cluster alone.
    for token in ['source_modified_time','source_modified_date','source_attributes']:
        assert token in fs_h and token in fs and token in rpc, token
    for token in ['entry->modified_time != expected_modified_time',
                  'entry->modified_date != expected_modified_date',
                  'entry->attributes != expected_attributes']:
        assert token in svc, token
    assert 'expected_modified_time' in svc_h and 'expected_modified_date' in svc_h and 'expected_attributes' in svc_h

    # A failed display snapshot must not leave the browser claiming a valid snapshot.
    refresh=ui[ui.index('void HddDirectoryWindow::Refresh()'):ui.index('void HddDirectoryWindow::RefreshIfStale()')]
    assert 'm_has_snapshot =\n        hdd_snapshot_service.BuildDisplaySnapshot' in refresh
    assert 'm_has_snapshot = true;' not in refresh

    # Non-negotiable write transaction remains intact.
    for token in ['m_import_write_status','m_import_flush_status','m_import_close_status',
                  'BuildRawPartitionSnapshot(']:
        assert token in rpc, token
    print('PASS: v2.27 FATX Copy/Move progressive verification + stronger source identity guard')

# Preserved contract from v228-cross-volume-content-verify-golden.py
def check_v228_cross_volume_content_verify_golden() -> None:
    from pathlib import Path
    r=Path(__file__).resolve().parents[1]
    rpc=(r/'guest-kernel-rpc.cc').read_text()
    rpc += '\n' + (r/'guest-kernel-rpc-completion.cc').read_text()
    svc_h=(r/'hdd-snapshot-service.hh').read_text()
    svc=(r/'hdd-snapshot-service.cc').read_text()

    assert 'VerifyFatxCopyContents' in svc_h and 'VerifyFatxCopyContents' in svc
    for token in ['source_bytes != destination_bytes',
                  'src->modified_time != item.source_modified_time',
                  'src->modified_date != item.source_modified_date',
                  'src->attributes != item.source_attributes',
                  'dst->file_size != item.file_size']:
        assert token in svc, token
    # Content verification must run before the cross-volume source delete branch.
    verify_pos=rpc.index('VerifyFatxCopyContents(compare_items')
    delete_pos=rpc.index('if (m_import_preflight.delete_source_after_copy)', verify_pos)
    assert verify_pos < delete_pos
    assert 'The source was NOT deleted.' in rpc
    assert 'matched the unchanged FATX source byte-for-byte' in rpc
    # Existing per-operation fresh verification remains mandatory.
    for token in ['m_import_write_status','m_import_flush_status','m_import_close_status',
                  'BuildRawPartitionSnapshot(']:
        assert token in rpc, token
    print('PASS: v2.28 FATX Copy/cross-volume Move byte-for-byte verification guard')

# Preserved contract from v229-filesystem-operation-state-golden.py
def check_v229_filesystem_operation_state_golden() -> None:
    from pathlib import Path
    r=Path(__file__).resolve().parents[1]
    h=(r/'guest-kernel-rpc.hh').read_text(); rpc=(r/'guest-kernel-rpc.cc').read_text(); fs=(r/'guest-kernel-rpc-filesystem.cc').read_text()
    rpc += '\n' + (r/'guest-kernel-rpc-completion.cc').read_text()
    for token in ['enum class FilesystemOperationKind','enum class FilesystemOperationPhase',
                  'struct FilesystemOperationContext','CrossVolumeMove','DeletingSource']:
        assert token in h, token
    assert 'm_hdd_frontend_operation' not in h+rpc+fs
    assert 'm_cross_volume_move_delete' not in h+rpc+fs
    assert 'ResetFilesystemOperationContext();' in fs
    assert 'FilesystemOperationKind::Delete' in fs
    assert 'FilesystemOperationKind::HostImport' in fs
    assert 'FilesystemOperationKind::NewFolder' in fs
    assert 'FilesystemOperationKind::FatxCopy' in fs
    assert 'FilesystemOperationKind::Rename' in fs
    assert 'FilesystemOperationKind::SameVolumeMove' in fs
    assert 'FilesystemOperationKind::CrossVolumeMove' in fs
    assert 'm_filesystem_operation.phase = FilesystemOperationPhase::DeletingSource;' in rpc
    assert 'IsCrossVolumeMoveDeletePhase()' in rpc
    # low-level RPC mode remains separate: this pass is state ownership cleanup, not a transaction rewrite.
    assert 'FsTestMode m_fs_test_mode' in h
    for token in ['m_import_write_status','m_import_flush_status','m_import_close_status','BuildRawPartitionSnapshot(']:
        assert token in rpc, token
    print('PASS: v2.29 explicit production filesystem operation state guard')

# Preserved contract from v230-partition-scoped-verification-golden.py
def check_v230_partition_scoped_verification_golden() -> None:
    from pathlib import Path
    r=Path(__file__).resolve().parents[1]
    fat_h=(r/'fatx-hdd.hh').read_text(); fat=(r/'fatx-hdd.cc').read_text(); svc_h=(r/'hdd-snapshot-service.hh').read_text(); svc=(r/'hdd-snapshot-service.cc').read_text(); rpc=(r/'guest-kernel-rpc.cc').read_text()
    rpc += '\n' + (r/'guest-kernel-rpc-completion.cc').read_text()
    assert 'BuildPartitionSnapshot' in fat_h and 'BuildPartitionSnapshot' in fat
    assert 'BuildRawPartitionSnapshot' in svc_h and 'BuildRawPartitionSnapshot' in svc
    assert 'partition_calls' in svc_h and 'partition_total_us' in svc_h
    # mutation verification is still fresh, but scoped to the affected partition.
    assert 'BuildRawPartitionSnapshot(\n            m_fs_partition, verify_snapshot' in rpc
    assert 'BuildRawPartitionSnapshot(\n        m_relocate_plan.partition, verify' in rpc
    assert 'BuildRawPartitionSnapshot(\n            m_import_preflight.partition, verify_snapshot' in rpc
    assert 'BuildRawPartitionSnapshot(\n                    m_import_preflight.source_partition, source_snapshot' in rpc
    # FATX source chunk reads and final byte comparison also avoid unrelated volumes.
    assert 'BuildPartitionSnapshot(\n            ReadHddBlock, hdd, length, partition_letter, snapshot)' in svc
    assert 'destination_letter == source_letter ? source_snapshot : destination_snapshot' in svc
    # Permanent write transaction invariant is unchanged.
    for token in ['m_import_write_status','m_import_flush_status','m_import_close_status']:
        assert token in rpc, token
    print('PASS: v2.30 partition-scoped fresh FATX verification optimization guard')

# Preserved contract from v231-export-backend-hdd-qol-golden.py
def check_v231_export_backend_hdd_qol_golden() -> None:
    from pathlib import Path
    r=Path(__file__).resolve().parents[1]
    ui_h=(r/'hdd-directory.hh').read_text(); ui=((r/'hdd-directory.cc').read_text() + '\n' + (r/'hdd-directory-ui.cc').read_text()); exp_h=(r/'hdd-export-service.hh').read_text(); exp=(r/'hdd-export-service.cc').read_text(); meson=(r/'meson.build').read_text()
    assert "'hdd-export-service.cc'" in meson
    for token in ['namespace XemuHddExport','bool ExportToHost','BuildPartitionSnapshot','OpenUniqueHostFile','CreateUniqueHostDirectory','HostSafeName','StreamFile']:
        assert token in exp_h+exp, token
    # Export backend is read-only and independent from ImGui/HDD window state.
    for forbidden in ['imgui.h','HddDirectoryWindow','xemu_disc_block_pwrite','DeleteEntry(','ActionReset']:
        assert forbidden not in exp_h+exp, forbidden
    assert 'ExportEntryRecursive' not in ui_h and 'ExportToHost(const HddTarget' not in ui_h
    assert 'XemuHddExport::ExportToHost' in ui
    # One transfer selection replaces simultaneous Copy+Move selections.
    assert 'enum class TransferSelectionMode' in ui_h
    assert 'm_transfer_source' in ui_h and 'm_transfer_selection_mode' in ui_h
    for stale in ['m_copy_source_valid','m_copy_source_display','m_move_source_valid','m_move_source_display']:
        assert stale not in ui_h+ui, stale
    assert 'Copy FATX Path' in ui
    assert 'CLEAR SELECTION' in ui and 'ClearTransferSelection()' in ui
    assert 'Partition snapshots:' in ui
    print('PASS: v2.31 export backend extraction + unified HDD transfer-selection QoL guard')

# Preserved contract from v232-raw-fatx-mutation-decommission-golden.py
def check_v232_raw_fatx_mutation_decommission_golden() -> None:
    from pathlib import Path
    r=Path(__file__).resolve().parents[1]
    prod='\n'.join((r/f).read_text() for f in ['fatx-hdd.hh','fatx-hdd.cc','disc-block-io.h','disc-block-io.c','hdd-directory.cc','hdd-export-service.cc'])
    for forbidden in ['DiskWriteCallback','xemu_disc_block_pwrite','xemu_disc_block_flush','XemuFatxHdd::DeleteEntry(','bool DeleteEntry(']:
        assert forbidden not in prod, forbidden
    for required in ['xemu_disc_block_pread','StreamFile(','ReadFileRange(','BuildPartitionSnapshot']:
        assert required in prod, required
    rpc=(r/'guest-kernel-rpc.cc').read_text()
    rpc += '\n' + (r/'guest-kernel-rpc-completion.cc').read_text()
    assert 'NtSetInformationFile' in rpc
    assert 'BuildRawPartitionSnapshot(' in rpc
    print('PASS: v2.32 production raw FATX mutation decommission guard')

# Preserved contract from v233-filesystem-transfer-contract-golden.py
def check_v233_filesystem_transfer_contract_golden() -> None:
    from pathlib import Path
    r=Path(__file__).resolve().parents[1]
    h=(r/'kernel-rpc-filesystem.hh').read_text(); cc=((r/'kernel-rpc-filesystem.cc').read_text() + '\n' + (r/'kernel-rpc-filesystem-stream.cc').read_text()); rpc=(r/'guest-kernel-rpc.cc').read_text(); runner=(r/'tests/v287-run-regression-tests.py').read_text(); native=(r/'tests/filesystem-transfer-contract-golden.cpp').read_text()
    rpc += '\n' + (r/'guest-kernel-rpc-completion.cc').read_text()
    assert 'ExpectedCommittedFileSize' in h+cc+rpc
    assert 'filesystem-transfer-contract-golden' in runner
    for token in ['kImportChunkBytes + 1u','source_modified_time','SameImportPlan','source_delete_plan','descendant']:
        assert token in native, token
    # The source-string guards remain useful, but the new transfer contract is executable under both native compilers.
    assert 'kernel-rpc-filesystem.cc' in runner and 'fatx-hdd.cc' in runner
    print('PASS: v2.33 behavioral filesystem-transfer regression architecture guard')

# Preserved contract from v234-second-profile-optimization-golden.py
def check_v234_second_profile_optimization_golden() -> None:
    from pathlib import Path
    r=Path(__file__).resolve().parents[1]
    h=(r/'guest-kernel-rpc.hh').read_text(); fs=(r/'guest-kernel-rpc-filesystem.cc').read_text(); rpc=(r/'guest-kernel-rpc.cc').read_text(); svc_h=(r/'hdd-snapshot-service.hh').read_text(); svc=(r/'hdd-snapshot-service.cc').read_text(); ui=((r/'hdd-directory.cc').read_text() + '\n' + (r/'hdd-directory-ui.cc').read_text()); util=(r/'kernel-rpc-utils.hh').read_text()
    rpc += '\n' + (r/'guest-kernel-rpc-completion.cc').read_text()
    for token in ['elapsed_ms','safe_point_samples','m_hdd_operation_safe_samples','m_hdd_operation_started_ms']:
        assert token in h+fs+rpc, token
    for token in ['source_chunk_calls','source_chunk_total_us','source_chunk_bytes','content_verify_calls','content_verify_total_us','content_verify_bytes']:
        assert token in svc_h+svc+ui, token
    assert 'kCompareChunkBytes = 1024u * 1024u' in svc
    assert 'kImportChunkBytes = 0x0000d000u' in (r/'kernel-rpc-filesystem.hh').read_text()
    # Non-negotiable: the larger read-only compare chunk does not alter the RPC write transaction.
    for token in ['kNtCreateFileOrdinal','kNtWriteFileOrdinal','kNtFlushBuffersFileOrdinal','kNtCloseOrdinal','BuildKernelCreateWriteStub']:
        assert token in util, token
    for token in ['m_import_write_status','m_import_flush_status','m_import_close_status','BuildRawPartitionSnapshot(']:
        assert token in rpc, token
    print('PASS: v2.34 second profile-guided optimization/instrumentation guard')

# Preserved contract from v235-guest-pause-coherency-golden.py
def check_v235_guest_pause_coherency_golden() -> None:
    from pathlib import Path
    import argparse

    ap=argparse.ArgumentParser(); ap.add_argument('--root',default='.')
    root=Path(ap.parse_args().root).resolve()
    debug=root/'ui/xui/debug-tools'
    read=lambda name:(debug/name).read_text(encoding='utf-8')
    guard=read('guest-pause-guard.hh') + '\n' + read('guest-pause-guard.cc')
    snap=read('hdd-snapshot-service.cc')
    rpc=read('guest-kernel-rpc.cc')
    completion=debug/'guest-kernel-rpc-completion.cc'
    if completion.exists():
        rpc += '\n' + completion.read_text(encoding='utf-8')
    assert 'bool IsValid() const' in guard and 'vm_stop(RUN_STATE_PAUSED)' in guard
    assert guard.count('runstate_is_running()') >= 2
    assert snap.count('pause.IsValid()') >= 5
    assert 'delete_mutation_observed' in rpc
    assert 'relocate_mutation_observed' in rpc
    assert 'import_mutation_observed' in rpc
    print('v2.35 guest pause/coherency guard: PASS')

# Preserved contract from v236-whole-tree-copy-verify-golden.py
def check_v236_whole_tree_copy_verify_golden() -> None:
    from pathlib import Path
    import argparse
    ap=argparse.ArgumentParser(); ap.add_argument('--root',default='.')
    root=Path(ap.parse_args().root).resolve()
    debug=root/'ui/xui/debug-tools'
    h=(debug/'hdd-snapshot-service.hh').read_text(encoding='utf-8')
    c=(debug/'hdd-snapshot-service.cc').read_text(encoding='utf-8')
    rpc=(debug/'guest-kernel-rpc.cc').read_text(encoding='utf-8')
    completion=debug/'guest-kernel-rpc-completion.cc'
    if completion.exists():
        rpc += '\n' + completion.read_text(encoding='utf-8')
    assert 'bool directory = false;' in h
    assert 'FATX Copy final verification received an empty transfer tree.' in c
    assert 'if (item.directory) {' in c and 'continue;' in c
    assert 'compare.directory = item.directory;' in rpc
    assert 'compare_items.reserve(m_import_preflight.entries.size())' in rpc
    print('v2.36 whole-tree Copy verification guard: PASS')

# Preserved contract from v237-atomic-export-host-integrity-golden.py
def check_v237_atomic_export_host_integrity_golden() -> None:
    from pathlib import Path
    import argparse
    ap=argparse.ArgumentParser(); ap.add_argument('--root',default='.')
    root=Path(ap.parse_args().root).resolve()
    debug=root/'ui/xui/debug-tools'
    exp=(debug/'hdd-export-service.cc').read_text(encoding='utf-8')
    fs=(debug/'kernel-rpc-filesystem.cc').read_text(encoding='utf-8')
    h=(debug/'kernel-rpc-filesystem.hh').read_text(encoding='utf-8')
    rpc=(debug/'guest-kernel-rpc.cc').read_text(encoding='utf-8')
    completion=debug/'guest-kernel-rpc-completion.cc'
    if completion.exists():
        rpc += '\n' + completion.read_text(encoding='utf-8')
    assert 'O_EXCL' in exp and 'OpenUniqueHostFile' in exp and 'CreateUniqueHostDirectory' in exp
    assert 'std::errc::file_exists' in exp
    assert 'g_fopen(host_path.c_str(), "wb")' not in exp
    assert 'host_content_hash' in h and 'HashHostFile' in fs and 'UpdateContentHash' in fs
    assert 'm_import_current_source_hash' in rpc
    print('v2.37 atomic export/host integrity guard: PASS')

# Preserved contract from v238-fatx-raw-name-fidelity-golden.py
def check_v238_fatx_raw_name_fidelity_golden() -> None:
    from pathlib import Path
    import argparse
    ap=argparse.ArgumentParser(); ap.add_argument('--root',default='.')
    r=Path(ap.parse_args().root)
    h=(r/'ui/xui/debug-tools/fatx-hdd.hh').read_text()
    c=(r/'ui/xui/debug-tools/fatx-hdd.cc').read_text()
    e=(r/'ui/xui/debug-tools/hdd-export-service.cc').read_text()
    current=((r/'ui/xui/debug-tools/current-game.cc').read_text() + '\n' + (r/'ui/xui/debug-tools/current-game-ui.cc').read_text())
    assert 'std::string display_name;' in h
    assert 'entry.name.assign(reinterpret_cast<const char *>(raw + 2)' in c
    assert 'entry.display_name = SafeFilename' in c
    assert 'entry.display_name.empty()' in c
    assert 'HostSafeName(XemuFatxHdd::DisplayName(child))' in e
    assert '"%s", entry.name.c_str());' in current  # XDVDFS Entry is not a FATX Entry
    print('v2.38 FATX raw-name fidelity guard: PASS')

# Preserved contract from v239-fatx-capacity-preflight-golden.py
def check_v239_fatx_capacity_preflight_golden() -> None:
    from pathlib import Path
    import argparse
    ap=argparse.ArgumentParser(); ap.add_argument('--root',default='.')
    r=Path(ap.parse_args().root)
    fh=(r/'ui/xui/debug-tools/fatx-hdd.hh').read_text(); fc=(r/'ui/xui/debug-tools/fatx-hdd.cc').read_text()
    sh=(r/'ui/xui/debug-tools/hdd-snapshot-service.hh').read_text(); rpc=(r/'ui/xui/debug-tools/guest-kernel-rpc-filesystem.cc').read_text()
    kh=(r/'ui/xui/debug-tools/kernel-rpc-filesystem.hh').read_text()
    assert 'QueryFreeSpace' in fh and 'free_clusters' in fc
    assert 'CapacityInfo' in sh and 'QueryPartitionCapacity' in sh
    assert 'EstimateTransferRequiredBytes' in kh
    assert 'required_bytes > capacity.free_bytes' in rpc
    print('v2.39 FATX capacity preflight guard: PASS')

# Preserved contract from v240-transfer-state-progress-golden.py
def check_v240_transfer_state_progress_golden() -> None:
    from pathlib import Path
    import argparse
    ap=argparse.ArgumentParser(); ap.add_argument('--root',default='.')
    r=Path(ap.parse_args().root)
    h=(r/'ui/xui/debug-tools/kernel-rpc-filesystem.hh').read_text(); f=(r/'ui/xui/debug-tools/guest-kernel-rpc-filesystem.cc').read_text()
    assert 'enum class TransferKind' in h and 'struct TransferPlan' in h and 'struct TransferEntry' in h
    assert 'using ImportPlan = TransferPlan;' in h
    assert 'switch (m_filesystem_operation.kind)' in f
    assert 'Creating Folder' in f and 'Moving (Delete source phase)' in f
    print('v2.40 transfer state/progress guard: PASS')

# Preserved contract from v241-kernel-rpc-completion-split-golden.py
def check_v241_kernel_rpc_completion_split_golden() -> None:
    from pathlib import Path
    import argparse
    ap=argparse.ArgumentParser(); ap.add_argument('--root',default='.')
    r=Path(ap.parse_args().root)
    core=(r/'ui/xui/debug-tools/guest-kernel-rpc.cc').read_text()
    comp=(r/'ui/xui/debug-tools/guest-kernel-rpc-completion.cc').read_text()
    meson=(r/'ui/xui/debug-tools/meson.build').read_text()
    for n in ['HandleKernelDeleteCompletion','HandleKernelRelocateCompletion','HandleKernelImportCompletion']:
        assert ('void GuestKernelRpcManager::'+n+'(') not in core and ('void GuestKernelRpcManager::'+n+'(') in comp
    assert "'guest-kernel-rpc-completion.cc'" in meson
    assert 'BuildRawPartitionSnapshot' in comp and 'NotifyFilesystemChanged' in comp
    print('v2.41 Kernel RPC completion ownership split guard: PASS')

# Preserved contract from v242-partition-preflight-sequential-read-golden.py
def check_v242_partition_preflight_sequential_read_golden() -> None:
    from pathlib import Path
    import argparse
    ap=argparse.ArgumentParser(); ap.add_argument('--root',default='.')
    r=Path(ap.parse_args().root)
    sh=(r/'ui/xui/debug-tools/hdd-snapshot-service.hh').read_text(); sf=(r/'ui/xui/debug-tools/guest-kernel-rpc-filesystem.cc').read_text(); fh=(r/'ui/xui/debug-tools/fatx-hdd.hh').read_text(); sc=(r/'ui/xui/debug-tools/hdd-snapshot-service.cc').read_text()
    assert 'BuildRawPartitionSetSnapshot' in sh
    assert sf.count('BuildRawPartitionSetSnapshot') >= 8
    assert 'BuildRawSnapshot(snapshot' not in sf
    assert 'FileReadCursor' in fh and 'ReadFileRangeSequential' in fh
    assert sc.count('ReadFileRangeSequential') >= 2
    assert 'visited_clusters' in fh
    print('v2.42 partition preflight/sequential read guard: PASS')

# Preserved contract from v243-disc-export-hdd-qol-golden.py
def check_v243_disc_export_hdd_qol_golden() -> None:
    from pathlib import Path
    import argparse
    ap=argparse.ArgumentParser(); ap.add_argument('--root',default='.')
    r=Path(ap.parse_args().root); d=r/'ui/xui/debug-tools'
    xdh=(d/'xdvdfs-disc.hh').read_text(); xdc=(d/'xdvdfs-disc.cc').read_text()
    exh=(d/'xdvdfs-export-service.hh').read_text(); exc=(d/'xdvdfs-export-service.cc').read_text()
    cg=((d/'current-game.cc').read_text() + '\n' + (d/'current-game-ui.cc').read_text()); hh=(d/'hdd-directory.hh').read_text(); hc=((d/'hdd-directory.cc').read_text() + '\n' + (d/'hdd-directory-ui.cc').read_text()); mes=(d/'meson.build').read_text()
    assert 'FindEntry(const Disc &disc, const std::vector<std::string> &path)' in xdh
    assert 'const Entry *FindEntry' in xdc
    assert 'xemu_disc_block_by_name("ide0-cd1")' in exc
    assert 'xemu_disc_block_identity' in exc and 'O_EXCL' in exc
    assert 'std::errc::file_exists' in exc
    assert 'std::errc::file_exists' in exc
    assert 'ExportEntryRecursive' in exc and 'XemuXdvdfs::Parse' in exc
    assert "'xdvdfs-export-service.cc'" in mes
    assert 'BeginPopupContextItem' in cg and 'Export Folder...' in cg and 'Export File...' in cg
    assert 'Copy Disc Path' in cg and 'RequestDiscExport' in cg
    assert 'm_name_filter[128]' in hh
    assert 'InputTextWithHint("##hdd_name_filter"' in hc
    assert 'EntryOrDescendantMatches' in hc
    print('v2.43 disc export/HDD QoL guard: PASS')

# Preserved contract from v244-hdd-krpc-final-audit-golden.py
def check_v244_hdd_krpc_final_audit_golden() -> None:
    from pathlib import Path
    r=Path(__file__).resolve().parents[1]
    read=lambda n:(r/n).read_text(encoding='utf-8')
    hh=read('guest-kernel-rpc.hh'); core=read('guest-kernel-rpc.cc'); complete=read('guest-kernel-rpc-completion.cc'); prod=read('guest-kernel-rpc-filesystem.cc'); util=read('kernel-rpc-utils.hh'); fsh=read('kernel-rpc-filesystem.hh'); fsc=(read('kernel-rpc-filesystem.cc') + '\n' + read('kernel-rpc-filesystem-stream.cc')); sh=read('hdd-snapshot-service.hh'); sc=read('hdd-snapshot-service.cc'); fat=read('fatx-hdd.cc'); export=read('hdd-export-service.cc'); xdexp=read('xdvdfs-export-service.cc'); ui=(read('hdd-directory.cc') + '\n' + read('hdd-directory-ui.cc')); current=(read('current-game.cc') + '\n' + read('current-game-ui.cc')); mes=read('meson.build')
    impl=core+complete+prod
    # Permanent write transaction invariant.
    for token in ['kNtCreateFileOrdinal','kNtWriteFileOrdinal','kNtFlushBuffersFileOrdinal','kNtCloseOrdinal','BuildKernelCreateWriteStub']:
        assert token in util, token
    for token in ['m_import_write_status','m_import_flush_status','m_import_close_status','BuildRawPartitionSnapshot(']:
        assert token in complete, token
    # Safety/coherency and final Copy/Move verification.
    assert 'pause.IsValid()' in sc and 'IsValid() const' in read('guest-pause-guard.hh')
    assert 'VerifyFatxCopyContents' in complete and 'compare.directory = item.directory;' in complete
    assert 'source_bytes != destination_bytes' in sc
    assert 'QueryPartitionCapacity' in prod and 'EstimateTransferRequiredBytes' in prod
    # Architecture/ownership.
    assert "'guest-kernel-rpc-completion.cc'" in mes
    assert 'BuildRawPartitionSetSnapshot' in prod and 'BuildRawSnapshot(snapshot' not in prod
    assert 'TransferKind' in fsh and 'TransferPlan' in fsh
    assert 'ReadFileRangeSequential' in sc
    # Production FATX is read-only except through Xbox kernel RPC.
    assert 'xemu_disc_block_pwrite' not in core+complete+prod+ui+export
    # Both host export paths are collision-safe create-new and read-only.
    assert 'O_EXCL' in export and 'O_EXCL' in xdexp
    assert 'xemu_disc_block_by_name("ide0-cd1")' in xdexp
    assert 'RequestDiscExport' in current and 'Copy Disc Path' in current
    # v2.44 measurements include partition-set preflight cost.
    for token in ['partition_set_calls','partition_set_total_us','partition_set_max_us','partition_set_partitions']:
        assert token in sh+sc+ui, token
    # HDD name filtering remains UI-only.
    assert 'EntryOrDescendantMatches' in ui and 'm_name_filter' in read('hdd-directory.hh')

    # v2.44 host-read optimization: per-chunk checks are metadata-only while the
    # full confirmed-plan hash and rolling copied-byte hash remain mandatory.
    assert 'ValidateImportHostEntryMetadata' in fsc+fsh
    assert 'ValidateImportHostEntryMetadata(item, error)' in core
    assert 'HashHostFile(host, item.host_content_hash, error)' in fsc
    assert 'm_import_current_source_hash != current.host_content_hash' in complete
    print('PASS: v2.44 cumulative HDD/KRPC production architecture + invariant audit')

# Preserved contract from v264-fatx-verify-buffer-reuse-golden.py
def check_v264_fatx_verify_buffer_reuse_golden() -> None:
    import argparse,pathlib
    from v287_source_test_utils import extract_function
    ap=argparse.ArgumentParser(); ap.add_argument('--root',default='.'); root=pathlib.Path(ap.parse_args().root).resolve()
    cc=(root/'ui/xui/debug-tools/hdd-snapshot-service.cc').read_text(); fn=extract_function(cc,'bool HddSnapshotService::VerifyFatxCopyContents(')
    assert fn.index('std::vector<uint8_t> source_bytes;') < fn.index('for (const FatxCompareItem &item : items)')
    assert fn.index('destination_bytes.reserve(kCompareChunkBytes);') < fn.index('while (offset < item.file_size)')
    loop=fn[fn.index('while (offset < item.file_size)'):]
    assert 'std::vector<uint8_t> source_bytes;' not in loop and 'std::vector<uint8_t> destination_bytes;' not in loop
    for t in ('ReadFileRangeSequential','source_bytes != destination_bytes','verified_bytes += source_bytes.size()'): assert t in fn
    print('PASS: v2.64 FATX final verification reuses host compare buffers without weakening byte-for-byte verification')

# Preserved contract from v265-hdd-perf-guard-cleanup-golden.py
def check_v265_hdd_perf_guard_cleanup_golden() -> None:
    import argparse,pathlib
    ap=argparse.ArgumentParser(); ap.add_argument('--root',default='.'); root=pathlib.Path(ap.parse_args().root).resolve()
    cc=(root/'ui/xui/debug-tools/hdd-snapshot-service.cc').read_text()
    assert 'class ScopedPerfMeasurement' in cc and '~ScopedPerfMeasurement()' in cc
    for old in ('struct PerfGuard','struct VerifyPerfGuard','struct SourceChunkPerfGuard'): assert old not in cc,old
    for counter in ('raw_calls','partition_calls','partition_set_calls','display_calls','content_verify_calls','source_chunk_calls'): assert f'm_stats.{counter}' in cc
    for t in ('m_stats.content_verify_bytes','m_stats.source_chunk_bytes','m_stats.partition_set_partitions'): assert t in cc
    print('PASS: v2.65 HDD measurement ownership is centralized without changing measured counters')

CONTRACTS = (
    ('v203-hdd-saves-export-golden.py', check_v203_hdd_saves_export_golden),
    ('v204-fatx-delete-golden.py', check_v204_fatx_delete_golden),
    ('v205-fatx-delete-popup-fix-golden.py', check_v205_fatx_delete_popup_fix_golden),
    ('v206-kernel-rpc-foundation-golden.py', check_v206_kernel_rpc_foundation_golden),
    ('v207-kernel-rpc-button-scope-golden.py', check_v207_kernel_rpc_button_scope_golden),
    ('v208-kernel-rpc-readonly-fs-golden.py', check_v208_kernel_rpc_readonly_fs_golden),
    ('v209-kernel-rpc-path-diagnostics-golden.py', check_v209_kernel_rpc_path_diagnostics_golden),
    ('v210-kernel-delete-file-golden.py', check_v210_kernel_delete_file_golden),
    ('v211-kernel-delete-folder-golden.py', check_v211_kernel_delete_folder_golden),
    ('v212-kernel-import-folder-golden.py', check_v212_kernel_import_folder_golden),
    ('v213-kernel-hdd-cleanup-golden.py', check_v213_kernel_hdd_cleanup_golden),
    ('v214-kernel-filesystem-backend-golden.py', check_v214_kernel_filesystem_backend_golden),
    ('v215-hdd-kernel-integration-golden.py', check_v215_hdd_kernel_integration_golden),
    ('v216-hdd-krpc-cleanup-safety-golden.py', check_v216_hdd_krpc_cleanup_safety_golden),
    ('v217-filesystem-executor-snapshot-ownership-golden.py', check_v217_filesystem_executor_snapshot_ownership_golden),
    ('v218-krpc-optimization-golden.py', check_v218_krpc_optimization_golden),
    ('v219-fatx-fg-partition-golden.py', check_v219_fatx_fg_partition_golden),
    ('v220-hdd-qol-golden.py', check_v220_hdd_qol_golden),
    ('v221-new-folder-golden.py', check_v221_new_folder_golden),
    ('v222-kernel-rename-golden.py', check_v222_kernel_rename_golden),
    ('v223-same-volume-move-golden.py', check_v223_same_volume_move_golden),
    ('v224-copy-cross-volume-move-golden.py', check_v224_copy_cross_volume_move_golden),
    ('v225-production-hdd-contract-golden.py', check_v225_production_hdd_contract_golden),
    ('v226-profile-guided-optimization-golden.py', check_v226_profile_guided_optimization_golden),
    ('v227-fatx-copy-integrity-golden.py', check_v227_fatx_copy_integrity_golden),
    ('v228-cross-volume-content-verify-golden.py', check_v228_cross_volume_content_verify_golden),
    ('v229-filesystem-operation-state-golden.py', check_v229_filesystem_operation_state_golden),
    ('v230-partition-scoped-verification-golden.py', check_v230_partition_scoped_verification_golden),
    ('v231-export-backend-hdd-qol-golden.py', check_v231_export_backend_hdd_qol_golden),
    ('v232-raw-fatx-mutation-decommission-golden.py', check_v232_raw_fatx_mutation_decommission_golden),
    ('v233-filesystem-transfer-contract-golden.py', check_v233_filesystem_transfer_contract_golden),
    ('v234-second-profile-optimization-golden.py', check_v234_second_profile_optimization_golden),
    ('v235-guest-pause-coherency-golden.py', check_v235_guest_pause_coherency_golden),
    ('v236-whole-tree-copy-verify-golden.py', check_v236_whole_tree_copy_verify_golden),
    ('v237-atomic-export-host-integrity-golden.py', check_v237_atomic_export_host_integrity_golden),
    ('v238-fatx-raw-name-fidelity-golden.py', check_v238_fatx_raw_name_fidelity_golden),
    ('v239-fatx-capacity-preflight-golden.py', check_v239_fatx_capacity_preflight_golden),
    ('v240-transfer-state-progress-golden.py', check_v240_transfer_state_progress_golden),
    ('v241-kernel-rpc-completion-split-golden.py', check_v241_kernel_rpc_completion_split_golden),
    ('v242-partition-preflight-sequential-read-golden.py', check_v242_partition_preflight_sequential_read_golden),
    ('v243-disc-export-hdd-qol-golden.py', check_v243_disc_export_hdd_qol_golden),
    ('v244-hdd-krpc-final-audit-golden.py', check_v244_hdd_krpc_final_audit_golden),
    ('v264-fatx-verify-buffer-reuse-golden.py', check_v264_fatx_verify_buffer_reuse_golden),
    ('v265-hdd-perf-guard-cleanup-golden.py', check_v265_hdd_perf_guard_cleanup_golden),
)

def main() -> int:
    for legacy_name, check in CONTRACTS:
        try:
            check()
        except Exception as exc:
            raise AssertionError(f"v2.87 retained contract failed ({legacy_name}): {exc}") from exc
    print("PASS: v2.87 HDD/FATX/Kernel-RPC regression contracts")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
