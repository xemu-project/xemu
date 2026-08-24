#!/usr/bin/env python3
# v2.87 current regression ownership.
"""v2.88.4 current final whole Debug Tools production audit guard."""
from __future__ import annotations

import argparse
import hashlib
import pathlib

from v287_source_test_utils import extract_function, extract_member_functions

EXPECTED_PRODUCTION_FILE_COUNT = 87
EXPECTED_PRODUCTION_SHA256 = "7fd56812d6e16d44f6265c8192d9ccfe47177c17f809da1d72d97dc7ad12a9e2"
EXPECTED_GUEST_METHOD_COUNT = 43
EXPECTED_UNCHANGED_GUEST_METHOD_COUNT = 38
EXPECTED_UNCHANGED_GUEST_METHOD_SHA256 = "f71d7028b41166a2209c216cecaf8cf8a8f257e5591ea61cea8e5cb34927d241"
EXPECTED_CLEANED_GUEST_METHOD_SHA256 = "930bc54af2cea04d2edb4e9bd0570ab6a7e90aaaa0370efc51565879fdaa5270"

NON_RUNTIME_FILES = {
    "README.md", "CHANGELOG.md", "build-capstone.sh", "build-capstone-windows.sh",
    "restore-executable-bits.py", "validate-project-layout.py",
}

REMOVED_LEGACY_GUEST_METHODS = {
    "BuildRecursiveDeletePlan",
    "DrawKernelDeleteConfirmation",
    "DrawKernelDeleteControls",
    "DrawKernelDeleteFolderConfirmation",
    "DrawKernelDeleteFolderControls",
    "DrawKernelImportConfirmation",
    "DrawKernelImportControls",
    "PrepareImportFolder",
    "RefreshDeleteCandidates",
    "RefreshDeleteFolderCandidates",
    "RefreshSelectedDeleteFolderPreview",
    "StartKernelDeleteSelectedFile",
    "StartKernelDeleteSelectedFolder",
    "StartKernelImportFolder",
}

CLEANED_GUEST_METHODS = {
    "DrawTestPanel", "HandleKernelDeleteCompletion", "StartHddDelete",
    "Tick", "TickWaitingSafePoint",
}

SCOPED_SHA256 = {
    # v2.88.1 startup-safe debugger F0 ownership lifecycle hotfix.
    "cheat-engine.hh": "5c6c588fd4b7211f1ba5646f1e329a58c8388abb271844469df6b1a2a72f7927",
    "cheat-engine-source.cc": "af9c38e979a3b45ae5a447a5274f26415de1e545b730b91e7d4b9e70862b8027",
    # v2.88 Breakpoints/Changes debugger panel ownership.
    "memory-tools-debugger-ui.cc": "4ff5279684836340cad7799862cdeb2c7781f6183180b3d35ebdb5607d47ccd2",
    "memory-tools-inject-ui.cc": "9b220407b9a72db8d9cb4e5c27a69b95502066986392ecab0ea82e0c5f92a145",
    "memory-tools-inject.cc": "8241a3fee525a665564b6ecbced97ef9c255f8137751c52ae505195e0feaf5e5",
    "memory-tools.hh": "269460c28ee6a9a29c6ad89d52f17fd7fd90c65148664184b4562661882cbe9a",
    "cheat-engine-memory.h": "926686ff6b935dc9f51075f9775133b354537cd3e767d25d8f661c83c7d31dd2",
    "current-game.cc": "9142fecec0402d0350ca5a57f5025b4422880a130000898b2ad9a298ca1d3c0f",
    "current-game.hh": "b104d8b6480eb94792b77aa651d16607b19b113e5cd86d0c00f9e3b8e30939af",
    "guest-kernel-rpc-completion.cc": "75d0ddcfee624c37e0e98b0791d3b685e1d952728b23784040cc7be66f969562",
    "guest-kernel-rpc-filesystem.cc": "fd1d9fd272df66e3468038a37198f1b7b6a68cb9d92b9bec9a151eff6fab5d27",
    "guest-kernel-rpc-status.hh": "bc91f4e72568e786bb6f5519805b6b9afed3e3a085fff2ff86c8eb4a07aa4e27",
    "guest-kernel-rpc-ui.cc": "3d4db96bb31840a7c29aa36ac47c1694129bcbf84c28712a2e4c77be91a9d3fd",
    "guest-kernel-rpc.cc": "bb90daadac29cee34d9ad17b44b715cba3b72a726d7f9e7405db9b8f7fcd6811",
    "guest-kernel-rpc.hh": "52500432403b134e56961260762d2fbd8584e802c0019cc6355cfe99633b2780",
    "guest-pause-guard.hh": "2973d651414a7ca4d906eaf6d892a637a11f82ec13ca3642a95f25128e89cf27",
    "hdd-directory.hh": "0773cd6773517ba0b29b324b9f52cbdc306bb977da5dfecd96739455801453e6",
    "hdd-export-service.cc": "57c8253a9b177d66bfee02c37eb23d0ac5e8849403270d1feaab0b8bcccc3b63",
    "host-export-utils.hh": "6d684ad1c6a79932d2fd32d2ba7887f5fb4b054b79ed53d3f97919c18f32273a",
    "kernel-rpc-filesystem.cc": "229023ce8d542b928e5519e45ec9897e47b5bb2852d383407adb63ad478aab55",
    "kernel-rpc-filesystem.hh": "035ea9035a6c94268ce580b42e5312e012016e0c9b9448dfb28bc3239b9958e4",
    "kernel-rpc-utils.hh": "fda48029a94f2642bda5a6b9792782bae8e9b9edf0af106f2832b2bcd6137068",
    "label-packs.cc": "3d34cd4b0df60559f18b2877ec8ffa31f4084b6816ba595fa364e91d625befaf",
    "label-symbol-utils.hh": "19f796f4b9b01dd1a3d19a42729e4245691b50278cad02be9a26a6b6ab93829e",
    "map-labels.cc": "b8002332a41c80ff2d6eb12264782349e1b5701dd2beac18a3fa2395584cf124",
    "meson.build": "1b8e409589b815c9048b1ff4cecc02117c4c6767c48f1be59b1d4615e6619681",
    "pdb-labels.cc": "502414b1c779080001690ad1d3f489041d944939504a1e887ef62fa3edc2ff50",
    "xdvdfs-export-service.cc": "cf8f06e731cc4dc46140b6fa816e4a62de4e1e7f3e3e8964c3de0819e7d6dcc8",
}


def require(text: str, needle: str, what: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing {what}: {needle}")


def forbid(text: str, needle: str, what: str) -> None:
    if needle in text:
        raise AssertionError(f"unexpected {what}: {needle}")


def production_digest(debug: pathlib.Path) -> tuple[int, str]:
    digest = hashlib.sha256(); count = 0
    for path in sorted(p for p in debug.rglob("*") if p.is_file()):
        relp = path.relative_to(debug); rel = relp.as_posix()
        if "tests" in relp.parts or rel in NON_RUNTIME_FILES:
            continue
        rb = rel.encode("utf-8"); data = path.read_bytes()
        digest.update(len(rb).to_bytes(4, "little")); digest.update(rb)
        digest.update(len(data).to_bytes(8, "little")); digest.update(data)
        count += 1
    return count, digest.hexdigest()


def body_digest(records: list[tuple[str, str]]) -> str:
    digest = hashlib.sha256()
    for name, body in sorted(records):
        digest.update(name.encode("utf-8")); digest.update(b"\0")
        digest.update(body.encode("utf-8")); digest.update(b"\0")
    return digest.hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser(); ap.add_argument("--root", default=".")
    root = pathlib.Path(ap.parse_args().root).resolve()
    debug = root / "ui/xui/debug-tools"

    actual = production_digest(debug)
    expected = (EXPECTED_PRODUCTION_FILE_COUNT, EXPECTED_PRODUCTION_SHA256)
    if actual != expected:
        raise AssertionError(f"v2.88.4 production surface changed (files={actual[0]}, sha256={actual[1]})")
    for rel, expected_sha in SCOPED_SHA256.items():
        got = hashlib.sha256((debug / rel).read_bytes()).hexdigest()
        if got != expected_sha:
            raise AssertionError(f"v2.88 audited production file changed: {rel} ({got})")

    guest_files = (
        "guest-kernel-rpc.cc", "guest-kernel-rpc-ui.cc",
        "guest-kernel-rpc-completion.cc", "guest-kernel-rpc-filesystem.cc",
    )
    guest = "\n".join((debug / rel).read_text(encoding="utf-8") for rel in guest_files)
    records = extract_member_functions(guest, "GuestKernelRpcManager")
    names = {name for name, _ in records}
    if len(records) != EXPECTED_GUEST_METHOD_COUNT:
        raise AssertionError(f"unexpected GuestKernelRpcManager method count: {len(records)}")
    if names & REMOVED_LEGACY_GUEST_METHODS:
        raise AssertionError(f"retired Kernel RPC diagnostic mutation methods returned: {sorted(names & REMOVED_LEGACY_GUEST_METHODS)}")
    unchanged = [(n, b) for n, b in records if n not in CLEANED_GUEST_METHODS]
    cleaned = [(n, b) for n, b in records if n in CLEANED_GUEST_METHODS]
    if len(unchanged) != EXPECTED_UNCHANGED_GUEST_METHOD_COUNT or body_digest(unchanged) != EXPECTED_UNCHANGED_GUEST_METHOD_SHA256:
        raise AssertionError("a Guest Kernel RPC method outside the five Phase-11 cleanup bodies changed from confirmed v2.85.1")
    if body_digest(cleaned) != EXPECTED_CLEANED_GUEST_METHOD_SHA256:
        raise AssertionError("a Phase-11-cleaned Guest Kernel RPC body changed without audit update")

    core = (debug / "guest-kernel-rpc.cc").read_text(encoding="utf-8")
    ui = (debug / "guest-kernel-rpc-ui.cc").read_text(encoding="utf-8")
    completion = (debug / "guest-kernel-rpc-completion.cc").read_text(encoding="utf-8")
    filesystem = (debug / "guest-kernel-rpc-filesystem.cc").read_text(encoding="utf-8")
    header = (debug / "guest-kernel-rpc.hh").read_text(encoding="utf-8")
    status = (debug / "guest-kernel-rpc-status.hh").read_text(encoding="utf-8")

    forbid(core, "ImGui::", "ImGui ownership in Kernel RPC scheduling core")
    forbid(core, '#include "../common.hh"', "UI common header in Kernel RPC core")
    forbid(core, '#include "../misc.hh"', "UI/file-dialog header in Kernel RPC core")
    ui_names = {n for n, _ in extract_member_functions(ui, "GuestKernelRpcManager")}
    if ui_names != {"DrawTestPanel"}:
        raise AssertionError(f"unexpected Kernel RPC UI ownership after dead-code cleanup: {sorted(ui_names)}")
    for button in ('RUN HARMLESS IRQL TEST', 'RUN READ-ONLY FILE TEST', 'RUN PATH DIAGNOSTICS'):
        require(ui, button, f"retained diagnostic button {button}")
    for legacy in REMOVED_LEGACY_GUEST_METHODS:
        forbid(header + guest, legacy, "retired diagnostic mutation method")
    forbid(header, "KernelDeleteSingleFile", "retired standalone delete mode")
    for field in ("m_delete_candidates", "m_delete_folder_candidates", "m_import_area_index",
                  "m_delete_confirm_pending", "m_import_confirm_pending"):
        forbid(header, field, "retired diagnostic mutation state")

    for token in ("IrqlName", "NtStatusName", "kDeleteIrqlOffset", "kImportIrqlOffset",
                  "kSafePointSampleIntervalMs"):
        require(status, token, f"shared Kernel RPC status helper {token}")
    for duplicate in ("const char *IrqlName(", "const char *NtStatusName(",
                      "constexpr uint64_t kSafePointSampleIntervalMs"):
        forbid(core + completion, duplicate, "duplicate Kernel RPC status implementation")

    # Production delete/import still uses the same kernel-owned mutation path.
    start_delete = extract_function(filesystem, "bool GuestKernelRpcManager::StartHddDelete(")
    require(start_delete, "BuildRawPartitionSetSnapshot", "fresh delete start snapshot")
    require(start_delete, "XemuKernelFs::SameDeletePlan", "fresh delete-plan identity gate")
    require(start_delete, "LoadRecursiveDeleteEntry", "kernel delete entry executor")
    delete_completion = extract_function(completion, "void GuestKernelRpcManager::HandleKernelDeleteCompletion(")
    require(delete_completion, "BuildRawPartitionSnapshot", "fresh post-delete FATX verification")
    forbid(delete_completion, "m_delete_candidates", "retired candidate fallback")
    start_import = extract_function(filesystem, "bool GuestKernelRpcManager::StartHddImport(")
    require(start_import, "SameImportPlan", "fresh import-plan identity gate")
    require(start_import, "LoadKernelImportOperation", "kernel import executor preparation")
    for token in ("m_import_write_status", "m_import_flush_status", "m_import_close_status",
                  "BuildRawPartitionSnapshot("):
        require(completion, token, f"write/flush/close/fresh-verify contract token {token}")

    host = (debug / "host-export-utils.hh").read_text(encoding="utf-8")
    hdd_export = (debug / "hdd-export-service.cc").read_text(encoding="utf-8")
    xdvdfs_export = (debug / "xdvdfs-export-service.cc").read_text(encoding="utf-8")
    for helper in ("IsReservedWindowsDeviceName", "HostSafeName", "NumberedCandidate"):
        require(host, helper, f"shared host-export helper {helper}")
        forbid(hdd_export, f"{helper}(const", f"duplicate HDD export helper {helper}")
        forbid(xdvdfs_export, f"{helper}(const", f"duplicate XDVDFS export helper {helper}")
    for service, text in (("HDD", hdd_export), ("XDVDFS", xdvdfs_export)):
        create = extract_function(text, "bool CreateUniqueHostDirectory(")
        if create.count("ec == std::errc::file_exists") != 1:
            raise AssertionError(f"{service} export duplicate file_exists retry blocks returned")
    require(xdvdfs_export, 'using XemuDebugBinaryUtils::range_inside;', "shared XDVDFS range helper")
    forbid(xdvdfs_export, "bool RangeInside(", "duplicate XDVDFS range helper")

    hdd_hh = (debug / "hdd-directory.hh").read_text(encoding="utf-8")
    pause_hh = (debug / "guest-pause-guard.hh").read_text(encoding="utf-8")
    memory_h = (debug / "cheat-engine-memory.h").read_text(encoding="utf-8")
    forbid(hdd_hh, "HasSnapshot()", "unused HDD snapshot accessor")
    forbid(pause_hh, "WasRunning()", "unused guest-pause accessor")
    forbid(memory_h, "Legacy newline-delimited virtual-only text helper", "stale orphan comment")

    # Final whole-tree cleanup: no dead public accessors or retired filesystem
    # compatibility wrappers may creep back into production.
    current_cc = (debug / "current-game.cc").read_text(encoding="utf-8")
    current_hh = (debug / "current-game.hh").read_text(encoding="utf-8")
    for accessor in ("LoadedLabelPacks()", "LoadedMapPath()", "LoadedPdbPath()", "XbePdbIdentity()"):
        forbid(current_hh, accessor, "unused Current Game accessor")
    require(current_cc, "template <typename Output>", "shared local-file reader template")
    require(current_cc, "static bool read_local_file(", "shared local-file reader")
    if current_cc.count("static bool read_local_file(") != 1:
        raise AssertionError("duplicate Current Game local-file reader returned")
    for wrapper in ("read_local_binary_file", "read_local_text_file"):
        body = extract_function(current_cc, f"static bool {wrapper}(")
        require(body, "return read_local_file(", f"{wrapper} delegates to shared local-file reader")

    label_utils = (debug / "label-symbol-utils.hh").read_text(encoding="utf-8")
    map_labels = (debug / "map-labels.cc").read_text(encoding="utf-8")
    pdb_labels = (debug / "pdb-labels.cc").read_text(encoding="utf-8")
    require(label_utils, "sort_and_dedupe_labels", "shared label sort/dedupe helper")
    require(map_labels, "XemuLabelSymbolUtils::sort_and_dedupe_labels(labels);", "MAP shared label cleanup")
    require(pdb_labels, "XemuLabelSymbolUtils::sort_and_dedupe_labels(labels);", "PDB shared label cleanup")

    kernel_fs_cc = (debug / "kernel-rpc-filesystem.cc").read_text(encoding="utf-8")
    kernel_fs_hh = (debug / "kernel-rpc-filesystem.hh").read_text(encoding="utf-8")
    retired_backend = (
        "FatxPathFromComponents", "NativeEPathFromComponents",
        "ValidateCurrentTitleDataPath", "BuildRecursiveDeletePlan",
        "BuildImportPlan", "ValidateImportHostPlan",
        "ValidateImportDestinationAvailable", "ValidateImportHostEntry(",
    )
    for symbol in retired_backend:
        forbid(kernel_fs_cc + kernel_fs_hh, symbol, "retired test-era filesystem wrapper")
    for field in ("std::string area;", "std::string title_directory;", "uint32_t title_id = 0;"):
        forbid(kernel_fs_hh, field, "retired TransferPlan metadata")
    for phase in ("Mutating", "Verifying"):
        forbid(header, phase, "unused filesystem operation phase")
    forbid(hdd_hh, "Snapshot()", "unused HDD snapshot accessor")


    # Final duplicate-helper audit: keep the stable KRPC public names while
    # delegating byte decoding to the shared binary helpers, and reuse the
    # shared ASCII case helper in portable label packs.
    krpc_utils = (debug / "kernel-rpc-utils.hh").read_text(encoding="utf-8")
    require(krpc_utils, '#include "binary-utils.hh"', "shared KRPC binary helper include")
    require(krpc_utils, "return XemuDebugBinaryUtils::read_le16(p);", "shared KRPC LE16 decode")
    require(krpc_utils, "return XemuDebugBinaryUtils::read_le32(p);", "shared KRPC LE32 decode")
    label_packs = (debug / "label-packs.cc").read_text(encoding="utf-8")
    require(label_packs, '#include "label-symbol-utils.hh"', "shared label-pack case helper include")
    forbid(label_packs, "static std::string upper(", "duplicate label-pack uppercase helper")
    require(label_packs, "XemuLabelSymbolUtils::upper_ascii(", "shared label-pack uppercase helper")

    meson = (debug / "meson.build").read_text(encoding="utf-8")
    require(meson, "'guest-kernel-rpc-ui.cc',", "Kernel RPC UI Meson registration")

    print("PASS: v2.88 final production audit removes dead Kernel-RPC mutation UI/state, consolidates shared export/status helpers, preserves live HDD/KRPC contracts, and fingerprints the complete Debug Tools production surface")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
