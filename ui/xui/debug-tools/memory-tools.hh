//
// xemu Memory Viewer / Search / x86 Debugger
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
#pragma once

#include "../common.hh"
#include "cheat-engine-memory.h"
#include "breakpoint-conditions.hh"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

class MemoryToolsWindow
{
public:
    bool is_open = false;

    MemoryToolsWindow();
    void Draw(bool detached = false);

private:
    enum class AddressSpace {
        Physical = 0,
        Virtual = 1,
    };

    enum class ValueKind {
        U8 = 0,
        U16,
        U32,
        S8,
        S16,
        S32,
        Float32,
    };

    enum class FirstScanMode {
        Exact = 0,
        UnknownInitial,
    };

    enum class NextScanMode {
        Exact = 0,
        NotEqual,
        Changed,
        Unchanged,
        Increased,
        Decreased,
        GreaterThan,
        LessThan,
    };

    enum class ContextOrigin {
        Memory = 0,
        Search,
        Debugger,
    };

    struct SearchResult {
        uint32_t address = 0;
        uint32_t previous_raw = 0;
        uint32_t current_raw = 0;
    };

    /* Small direct-mapped presentation cache for clipped Search-result rows.
     * Every entry carries its complete render key, so reuse is allowed only
     * when index/address/raw values/value kind are all still identical. */
    struct SearchDisplayCacheEntry {
        size_t result_index = SIZE_MAX;
        uint32_t address = 0;
        uint32_t previous_raw = 0;
        uint32_t current_raw = 0;
        ValueKind value_kind = ValueKind::U32;
        char address_text[16] = {};
        char previous_text[64] = {};
        char current_text[64] = {};
    };

    struct VirtualDumpRegion {
        uint64_t start = 0;
        uint64_t end_exclusive = 0;
    };

    struct VirtualPageMapping {
        uint32_t virtual_page = 0;
        uint64_t physical_page = 0;
    };

    // Compact secondary index for Physical -> Virtual alias lookup. The main
    // page map stays in virtual-address order; this index is sorted by
    // physical page then virtual page so alias queries do not rescan the
    // entire mapped virtual address space.
    struct PhysicalAliasPage {
        uint32_t physical_page = 0;
        uint32_t virtual_page = 0;
    };

    struct MemoryMapRegion {
        uint64_t virtual_start = 0;
        uint64_t virtual_end_exclusive = 0;
        uint64_t physical_start = 0;
        uint64_t physical_end_exclusive = 0;
    };

    struct ViewerState {
        uint32_t address = 0;
        char address_text[16] = {};
        std::string status;
        bool request_scroll = true;

        // Address range currently visible in the hex pane. Cross-highlighting
        // uses this to avoid needlessly moving the opposite pane when the
        // paired byte is already on screen.
        bool visible_range_valid = false;
        uint64_t visible_start = 0;
        uint64_t visible_end_exclusive = 0;
    };

    struct ExecuteBreakpoint {
        uint32_t address = 0;
        bool enabled = true;
        std::string condition_text;
        std::vector<XemuBreakpointCondition> conditions;
    };

    struct DataWatchpoint {
        uint32_t address = 0;
        uint32_t length = 4;
        int access_flags = XEMU_CHEAT_WATCH_WRITE;
        bool enabled = true;
        std::string condition_text;
        std::vector<XemuBreakpointCondition> conditions;
    };

    enum class BreakpointConditionTarget {
        None = 0,
        Execute,
        Watchpoint,
    };

    enum class DebugStepMode {
        None = 0,
        UserStep,
        ContinuePastBreakpoint,
    };

    enum class DebugFlowKind {
        None = 0,
        Jump,
        ConditionalJump,
        Call,
        Return,
    };

    struct DebugFlowInfo {
        DebugFlowKind kind = DebugFlowKind::None;
        bool target_valid = false;
        uint32_t target = 0;
        bool fallthrough_valid = false;
        uint32_t fallthrough = 0;
    };

    // Draw-only control-flow metadata. Disassembly rows are immutable between
    // RefreshDisassembly() calls, so branch classification and an exact
    // in-page target-row lookup are computed once per refresh instead of in
    // both disassembly panes on every frame.
    struct DisassemblyFlowCache {
        DebugFlowInfo flow;
        size_t target_index = (size_t)-1;
    };

    ViewerState m_physical_viewer;
    ViewerState m_virtual_viewer;
    bool m_memory_writing_enabled = false;
    bool m_request_memory_tab = false;
    bool m_request_debugger_tab = false;

    // Unified memory workspace selection. A click in either pane keeps the
    // exact physical byte and its selected virtual alias paired/highlighted.
    bool m_have_memory_selection = false;
    bool m_have_selected_physical = false;
    bool m_have_selected_virtual = false;
    uint32_t m_selected_physical_address = 0;
    uint32_t m_selected_virtual_address = 0;
    AddressSpace m_memory_edit_space = AddressSpace::Physical;
    char m_memory_edit_text[3] = {};
    bool m_memory_edit_focus_requested = false;

    AddressSpace m_search_space = AddressSpace::Physical;
    ValueKind m_value_kind = ValueKind::U32;
    FirstScanMode m_first_mode = FirstScanMode::Exact;
    NextScanMode m_next_mode = NextScanMode::Changed;
    bool m_aligned = true;
    bool m_value_hex = false;
    bool m_have_first_scan = false;
    bool m_snapshot_mode = false;
    uint32_t m_scan_start = 0x00000000u;
    uint32_t m_scan_end = 0x03FFFFFFu;
    char m_scan_start_text[16] = {};
    char m_scan_end_text[16] = {};
    char m_search_value_text[32] = {};
    std::string m_search_status;
    std::vector<SearchResult> m_results;
    std::array<SearchDisplayCacheEntry, 256> m_search_display_cache = {};

    uint32_t m_snapshot_start = 0;
    uint32_t m_snapshot_end = 0;
    ValueKind m_snapshot_kind = ValueKind::U32;
    bool m_snapshot_aligned = true;
    std::vector<uint8_t> m_snapshot;
    std::vector<uint8_t> m_snapshot_valid_pages;

    std::string m_dump_status;
    bool m_detached_rendering = false;

    // Snapshot of all guest virtual pages currently backed by Xbox RAM.
    std::vector<VirtualPageMapping> m_virtual_page_map;
    std::vector<PhysicalAliasPage> m_physical_alias_page_index;
    std::vector<MemoryMapRegion> m_memory_map_regions;
    std::vector<uint32_t> m_physical_aliases;
    char m_map_virtual_text[16] = {};
    char m_map_physical_text[16] = {};
    std::string m_memory_map_status;
    uint64_t m_memory_map_ram_size = 0;
    uint64_t m_memory_map_unique_physical_pages = 0;
    uint64_t m_memory_map_generation = 0;
    uint64_t m_memory_map_attempt_generation = (uint64_t)-1;
    bool m_memory_map_valid = false;
    size_t m_active_map_region = (size_t)-1;

    // x86 debugger/disassembler state.
    char m_disasm_address_text[16] = {};
    uint32_t m_disasm_address = 0x00010000u;
    int m_disasm_instruction_count = 32;
    bool m_disasm_full_page = true;
    bool m_follow_eip = true;
    std::vector<XemuCheatDisasmRow> m_disassembly_rows;
    std::vector<DisassemblyFlowCache> m_disassembly_flow_cache;
    // Per-row display strings are immutable between disassembly/label changes.
    // Cache both panes so the render loop does not reformat bytes/instructions
    // or binary-search the label database every frame.
    std::vector<std::string> m_disassembly_virtual_text;
    std::vector<std::string> m_disassembly_physical_text;
    uint64_t m_disassembly_label_generation = UINT64_MAX;
    bool m_disassembly_cached_labels_enabled = false;
    // Reused 4 KiB decode workspace. A page can contain thousands of short
    // x86 instructions, so keeping this buffer avoids a ~672 KiB allocation
    // on every debugger refresh/navigation.
    std::vector<XemuCheatDisasmRow> m_disassembly_page_scratch;
    bool m_have_disasm_selection = false;
    uint32_t m_selected_disasm_virtual = 0;
    uint64_t m_selected_disasm_physical = 0;
    bool m_selected_disasm_physical_valid = false;
    float m_disasm_scroll_y = 0.0f;
    float m_disasm_pane_height = 320.0f;
    bool m_disasm_scroll_to_focus = false;
    uint32_t m_disasm_focus_virtual = 0;
    uint64_t m_code_patch_generation = 0;
    std::string m_debug_status;

    // XBE label browser. The XBE virtual address is always the master key;
    // physical addresses are resolved from the running page tables on demand.
    bool m_labels_enabled = true;
    bool m_label_browser_open = false;
    bool m_label_browser_focus_requested = false;
    char m_label_search[128] = {};
    int m_label_filter = 0; // 0=All, then XemuXbeLabels::Type + 1
    int m_label_source_filter = 0; // 0=All, then XemuXbeLabels::Source + 1
    std::vector<size_t> m_visible_label_cache;
    uint64_t m_visible_label_generation = UINT64_MAX;
    char m_visible_label_search[128] = {};
    int m_visible_label_filter = -1;
    int m_visible_label_source_filter = -1;
    int m_selected_label_index = -1;
    std::string m_label_status;

    struct InstructionChangeRecord {
        uint32_t address = 0;
        uint32_t span = 0;
        uint8_t original_bytes[15] = {};
        std::string original_text;
        std::vector<uint8_t> last_applied_bytes;
        std::string last_applied_text;
        bool active = false;
        bool display_hex = false;
    };

    struct CodeCaveChangeRecord {
        uint32_t address = 0;
        std::vector<uint8_t> original_bytes;
        std::vector<uint8_t> changed_bytes;
        std::string original_text;
        std::string changed_text;
        bool active = false;
        bool display_hex = false;
    };

    // x86 Debugger right-click Inject helpers. Change edits one complete
    // instruction in place (shorter replacements are NOP padded). The first
    // bytes/ASM seen at an address are retained for the session so closing and
    // reopening Change can still safely revert the patch to its true original.
    // CodeCave owns one temporary Type-F0 hook through CheatEngineWindow so
    // RUN/RESTORE use the production cave allocator/rollback behavior.
    std::vector<InstructionChangeRecord> m_instruction_change_history;
    size_t m_change_instruction_record_index = (size_t)-1;
    bool m_change_instruction_open = false;
    bool m_change_instruction_focus_requested = false;
    uint32_t m_change_instruction_address = 0;
    uint32_t m_change_instruction_span = 0;
    uint8_t m_change_instruction_original_bytes[15] = {};
    std::string m_change_instruction_original_text;
    std::string m_change_instruction_current_text;
    std::vector<uint8_t> m_change_instruction_current_bytes;
    std::string m_change_instruction_source;
    std::vector<uint8_t> m_change_instruction_preview_bytes;
    std::vector<uint8_t> m_change_instruction_applied_bytes;
    std::string m_change_instruction_status;
    bool m_change_instruction_preview_valid = false;
    bool m_change_instruction_applied = false;

    bool m_code_cave_builder_open = false;
    bool m_code_cave_builder_focus_requested = false;
    uint32_t m_code_cave_hook_address = 0;
    uint32_t m_code_cave_overwrite_length = 0;
    std::vector<XemuCheatDisasmRow> m_code_cave_original_rows;
    std::string m_code_cave_source;
    std::string m_code_cave_status;
    CodeCaveChangeRecord m_code_cave_change;
    bool m_inject_disasm_refresh_pending = false;

    char m_breakpoint_address_text[16] = {};
    int m_breakpoint_kind = 0; // 0=Execute, 1=Read, 2=Write, 3=Read/Write
    std::vector<ExecuteBreakpoint> m_breakpoints;
    int m_watchpoint_length = 4;
    std::vector<DataWatchpoint> m_watchpoints;
    std::string m_breakpoint_status;

    // Per-breakpoint condition editor. YES/NO in the breakpoint table always
    // opens this same small multi-line editor; each non-empty line is ANDed.
    bool m_condition_editor_open = false;
    bool m_condition_editor_focus_requested = false;
    BreakpointConditionTarget m_condition_target = BreakpointConditionTarget::None;
    uint32_t m_condition_target_address = 0;
    uint32_t m_condition_target_length = 0;
    int m_condition_target_access_flags = 0;
    std::string m_condition_editor_text;
    std::string m_condition_editor_error;
    std::vector<XemuBreakpointCondition> m_condition_editor_preview;

    XemuCheatX86Registers m_registers = {};
    XemuCheatX86Registers m_break_registers = {};
    XemuCheatX86ExtraRegisters m_extra_registers = {};
    XemuCheatX86ExtraRegisters m_break_extra_registers = {};
    bool m_have_registers = false;
    bool m_have_extra_registers = false;
    bool m_have_break_registers = false;
    bool m_have_break_extra_registers = false;
    int m_register_view = 0; // 0=General, 1=x87/FPU, 2=MMX, 3=SSE
    bool m_debug_preferences_initialized = false;
    bool m_register_view_selection_pending = true;
    ImGuiContext *m_register_view_context = nullptr;
    bool m_was_debug_paused = false;
    uint32_t m_last_break_pc = 0;
    bool m_have_break_highlight = false;
    uint32_t m_last_break_highlight_pc = 0;
    bool m_last_break_highlight_is_access = false;
    uint64_t m_last_break_physical = 0;
    bool m_last_break_physical_valid = false;
    DebugStepMode m_debug_step_mode = DebugStepMode::None;
    bool m_resume_breakpoint_restore_pending = false;
    uint32_t m_resume_breakpoint_restore_address = 0;
    double m_last_live_register_refresh = -1.0;

    // Browser-style debugger navigation. User-directed Follow pushes a new
    // virtual address; Back/Forward never changes guest execution state.
    std::vector<uint32_t> m_debug_nav_history;
    size_t m_debug_nav_index = 0;
    bool m_have_debug_nav_history = false;
    bool m_debug_nav_key_consumed = false;

    // Programmatic Follow/Back/Forward changes the selected instruction after
    // the panes have already drawn for the current frame. Keep ImGui keyboard
    // navigation attached to that new selected row on the next frame instead
    // of leaving Up/Down anchored to the pre-navigation source instruction.
    bool m_disasm_keyboard_focus_requested = false;
    bool m_disasm_keyboard_focus_physical = false;
    bool m_disasm_last_keyboard_focus_physical = false;

    // Context-menu follows are deferred until both disassembly panes finish
    // drawing. Refreshing the row vector while one pane is iterating it can
    // invalidate the active row and made Follow behavior depend on which pane
    // had focus.
    int m_debug_nav_pending_action = 0; // 0=None, 1=Address, 2=Back, 3=Forward
    uint32_t m_debug_nav_pending_address = 0;
    std::string m_debug_nav_pending_status;

    // Live Current Registers editor. The Last Breakpoint register pane is a
    // snapshot and intentionally never enters this edit state.
    bool m_register_edit_active = false;
    bool m_register_edit_focus_requested = false;
    bool m_register_edit_is_temp = false;
    uint32_t m_register_edit_temp_address = 0;
    uint32_t m_selected_f0_temp_hook = 0;
    char m_register_edit_name[16] = {};
    char m_register_edit_text[16] = {};

    void DrawMemoryWorkspace();
    bool DrawScrollableMemoryPane(AddressSpace space, ViewerState &state,
                                  uint64_t range_start, uint64_t range_end_exclusive,
                                  const char *pane_id, float height);
    void DrawMemoryMapPane(float height);
    void DrawSearch();
    void DrawDebugger();
    void LoadDebuggerPreferences();
    void StoreDebuggerPreferences();
    void ResetDebuggerPreferences();
    void DrawRegisters(const XemuCheatX86Registers &regs, bool breakpoint_snapshot);
    void DrawGeneralRegisterTable(const XemuCheatX86Registers &regs,
                                  bool breakpoint_snapshot);
    void DrawExtraRegisterTable(const XemuCheatX86ExtraRegisters &regs,
                                bool have_regs, int view,
                                bool breakpoint_snapshot);
    void CaptureBreakpointExtraRegisters();
    void DrawF0TempRegisters();
    void DrawBreakpoints();
    void DrawBreakpointContents();
    void DrawChanges();
    void DrawBreakpointConditionEditor();
    void DrawLabelBrowser();
    void DumpLabels();
    void DrawDumpRam();
    void DumpPhysicalRam();
    void DumpMappedVirtualRam();
    void DumpFullRam();
    void DumpRam(bool dump_physical, bool dump_virtual);
    void DumpCurrentPage(AddressSpace space, uint32_t address);
    bool DumpRange(AddressSpace space, uint32_t base, uint64_t size,
                   const std::string &path, size_t &failed_pages) const;
    bool CollectRamVirtualPages(uint64_t ram_size,
                                std::vector<VirtualPageMapping> &pages,
                                bool &used_page_table_snapshot) const;
    bool ScanMappedVirtualRam(uint64_t ram_size,
                              std::vector<VirtualDumpRegion> &regions,
                              uint64_t &mapped_pages) const;
    bool RefreshMemoryMap();
    void LookupPhysicalAliases();
    size_t FindRegionForVirtual(uint32_t address) const;
    size_t FindRegionForPhysical(uint32_t address) const;
    bool SyncPhysicalFromVirtual(uint32_t virtual_address);
    bool SyncVirtualFromPhysical(uint32_t physical_address);
    void SelectMemoryByte(AddressSpace space, uint32_t address);
    void PrepareMemoryByteEdit(AddressSpace space, uint32_t address);
    bool SelectedAddressForSpace(AddressSpace space, uint32_t &address) const;
    void SelectMemoryMapRegion(size_t index, bool jump_to_start);
    void JumpViewerTo(AddressSpace space, uint32_t address);
    bool WriteVirtualMapIndex(const std::string &path, uint64_t ram_size,
                              const std::vector<VirtualDumpRegion> &regions,
                              uint64_t mapped_pages,
                              const std::vector<std::string> &region_files) const;
    std::string DumpDirectory() const;
    std::string DumpStem() const;

    bool Read(AddressSpace space, uint32_t address, void *buffer, size_t size) const;
    bool Write(AddressSpace space, uint32_t address, const void *buffer, size_t size) const;
    bool ReadRaw(AddressSpace space, uint32_t address, ValueKind kind,
                 uint32_t &raw) const;

    void ResetSearch();
    void FirstScan();
    void NextScan();
    bool CaptureSnapshot();
    bool ParseTarget(uint32_t &raw) const;
    bool MatchTarget(uint32_t raw, uint32_t target, NextScanMode mode) const;
    bool MatchPrevious(uint32_t current, uint32_t previous, NextScanMode mode) const;
    size_t ValueSize(ValueKind kind) const;
    void FormatValue(char *dst, size_t dst_size, uint32_t raw,
                     ValueKind kind) const;

    bool RefreshRegisters(XemuCheatX86Registers &regs);
    bool ResolveWatchpointAccessInstruction(uint32_t stop_pc,
                                            XemuCheatDisasmRow &access_row);
    void RefreshDisassembly();
    void RebuildDisassemblyFlowCache();
    void RebuildDisassemblyRenderCache();
    void UpdateBreakpointHitState();
    void FollowDebuggerAddress(uint32_t address, bool refresh_disassembly);
    void NavigateDebuggerAddress(uint32_t address);
    bool NavigateDebuggerBack();
    bool NavigateDebuggerForward();
    const XemuCheatDisasmRow *SelectedDisassemblyRow() const;
    bool AnalyzeControlFlow(const XemuCheatDisasmRow &row,
                            DebugFlowInfo &flow) const;
    bool ResolveControlFlowTarget(const XemuCheatDisasmRow &row,
                                  uint32_t &target) const;
    bool ResolveIndirectControlFlowTarget(const char *operand,
                                          uint32_t &target) const;
    bool RegisterValueByName(const char *name, uint32_t &value) const;
    void HandleDebuggerNavigationKeys();
    void BeginRegisterEdit(const char *name, uint32_t value);
    void BeginTempRegisterEdit(const char *name, uint32_t value,
                               uint32_t storage_address);
    bool CommitRegisterEdit();
    bool DrawDisassemblyPane(bool physical);
    bool StartDebugStep(DebugStepMode mode);
    bool IsEnabledBreakpointAt(uint32_t address) const;
    bool ContinueFilteredExecuteBreakpoint(uint32_t address);
    void OpenBreakpointConditionEditor(const ExecuteBreakpoint &bp);
    void OpenBreakpointConditionEditor(const DataWatchpoint &wp);
    bool ApplyBreakpointConditionEditor();
    void ClearBreakpointConditionEditor();
    bool AddExecuteBreakpoint(uint32_t address);
    void RemoveExecuteBreakpoint(size_t index);
    bool AddDataWatchpoint(uint32_t address, uint32_t length, int access_flags);
    void RemoveDataWatchpoint(size_t index);
    bool ResolveBreakpointVirtualAddress(AddressSpace space, uint32_t address,
                                         uint32_t &virtual_address);
    bool AddBreakpointByKind(uint32_t virtual_address, int kind);
    void DrawAddressContextMenu(AddressSpace space, uint32_t address,
                                ContextOrigin origin,
                                const char *copy_value = nullptr,
                                const XemuCheatDisasmRow *disasm_row = nullptr,
                                bool address_valid = true,
                                bool have_breakpoint_virtual_override = false,
                                uint32_t breakpoint_virtual_override = 0);
    bool ResolveContextVirtualAddress(AddressSpace space, uint32_t address,
                                      bool have_override, uint32_t override_address,
                                      uint32_t &virtual_address);
    bool CopyContextInstruction(AddressSpace space, uint32_t address,
                                const XemuCheatDisasmRow *disasm_row,
                                bool have_breakpoint_virtual_override,
                                uint32_t breakpoint_virtual_override);
    bool InjectNop(const XemuCheatDisasmRow &row);
    void OpenInstructionChanger(const XemuCheatDisasmRow &row);
    bool BuildInstructionChangePreview();
    bool ApplyInstructionChange();
    bool RestoreInstructionChange();
    bool RestoreTrackedInstructionPatch(uint32_t address);
    void RecordCodeCaveChange(uint32_t hook_address, uint32_t overwrite_length,
                              uint32_t cave_address);
    void ClearCodeCaveChange(uint32_t hook_address);
    void DrawInstructionChanger();
    void OpenCodeCaveBuilder(const XemuCheatDisasmRow &row);
    bool BuildCodeCaveTemplate(uint32_t hook_address);
    void DrawCodeCaveBuilder();
    void SetContextStatus(ContextOrigin origin, AddressSpace space,
                          const std::string &status);

    static bool ParseHexAddress(const char *text, uint32_t &value);
    static void SetHexText(char *dst, size_t dst_size, uint32_t value);
};

extern MemoryToolsWindow memory_tools_window;
