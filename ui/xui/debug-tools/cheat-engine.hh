//
// xemu RAW Cheat Engine
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
#include "x86-cheat-assembler.hh"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

/* Defined by cheat-engine-memory.h.  Only a reference appears in this
 * interface, so keep the QEMU/debug bridge header out of this C++ header and
 * forward-declare the existing register snapshot type here. */
struct XemuCheatX86Registers;

class CheatEngineWindow
{
public:
    bool is_open = false;

    struct FTempBankInfo {
        std::string cheat_name;
        std::string display_name;
        uint32_t hook_address = 0;
        uint32_t cave_address = 0;
        uint32_t cave_size = 0;
        uint32_t temp_address = 0;
    };

    struct DebuggerF0HookInfo {
        bool installed = false;
        uint32_t hook_address = 0;
        uint32_t overwrite_length = 0;
        uint32_t cave_address = 0;
        uint32_t code_size = 0;
        uint32_t return_address = 0;
    };

    CheatEngineWindow();

    void Draw(bool detached = false);
    void Tick();
    void NotifyGameResetRequested();
    const std::vector<FTempBankInfo> &GetActiveF0TempBanks() const;

    /* x86 Debugger Inject > CodeCave uses the exact Type-F0 assembler,
     * allocator, hook sizing, rollback, and cave retirement rules as the
     * normal Cheat Engine. The debugger owns one temporary hook at a time. */
    bool InstallDebuggerF0(uint32_t hook_address, const std::string &source,
                           DebuggerF0HookInfo &info, std::string &status);
    bool RemoveDebuggerF0(std::string &status);
    bool GetDebuggerF0HookInfo(DebuggerF0HookInfo &info) const;
    bool ActiveFHookOwnsAddress(uint32_t hook_address) const;

private:
    enum class GuestAddressSpace {
        Physical,
        Virtual,
    };

    enum class PreEntryLifecycle {
        Idle,
        ResetRequested,
        ApplyPending,
    };

    struct RawCode {
        uint32_t command = 0;
        uint32_t value = 0;
        int source_line = 0;
        std::string source;
        /* F0/F1 own their body text through DEADCODE. Keeping the body inside
         * one logical RawCode makes D/E and Type-9 scopes treat a complete
         * external cave as one command instead of counting source lines. */
        std::vector<XemuCheatAsmLine> f_body;
        /* Parse-time Type-F probe cache. The final F0 still assembles once at
         * its allocator-selected cave address, but steady ticks never rebuild
         * the source signature or repeat the address-independent probe. */
        bool f_precompiled = false;
        bool f_precompile_ok = false;
        std::vector<uint8_t> f_probe_code;
        std::vector<uint8_t> f_probe_data;
        bool f_uses_preserve = false;
        uint32_t f_preserve_bytes = 0;
        bool f_uses_temp = false;
        uint32_t f_temp_bytes = 0;
        std::string f_definition_signature;
        std::string f_precompile_error;
        int f_precompile_error_line = 0;
        bool f_terminated = false;
        /* F1 uses DEADCODE 000000NN, where NN=01..08 is the number of
         * meaningful bytes in the final padded 8-byte raw line. F0 leaves
         * this at zero because its DEADCODE directive has no operand. */
        uint8_t f_final_valid_bytes = 0;
    };

    struct CheatBlock {
        std::string name;
        std::string description;
        std::string credits;
        bool selected = false;
        bool enabled = false;
        bool preentry = false;
        bool preentry_applied = false;
        std::string preentry_error;
        int group_index = 0;
        std::string group_path;
        uint32_t identity_ordinal = 0;
        std::vector<RawCode> codes;
    };

    struct CheatGroup {
        std::string name;
        std::vector<int> child_groups;
        std::vector<size_t> cheats;
    };

    struct FileHeader {
        std::string hash;
        std::string game_id;
        std::string name;
        uint32_t title_id = 0;
        bool title_id_valid = false;
    };

    struct AddressContext {
        GuestAddressSpace space = GuestAddressSpace::Virtual;
        uint32_t base = 0;
        uint32_t remaining = 0;
        bool until_end = false;
    };

    struct SwitchState {
        bool on = false;
        bool previous_condition = false;
    };

    struct FHookState {
        size_t owner_block = 0;
        uint32_t hook_address = 0;
        uint32_t overwrite_length = 0;
        uint32_t external_entry = 0;
        uint32_t allocation_size = 0;
        uint32_t code_size = 0;
        uint32_t preserve_entry = 0;
        uint32_t preserve_size = 0;
        uint32_t temp_entry = 0;
        uint32_t temp_size = 0;
        bool installed = false;
        bool retired_may_be_referenced = false;
        bool resume_points_valid = false;
        std::vector<uint32_t> resume_points;
        uint8_t retire_backoff_ticks = 0;
        uint8_t retire_skip_ticks = 0;
        std::vector<uint8_t> original_bytes;
        std::string definition_signature;
    };

    bool m_engine_enabled = true;
    bool m_code_aware_skip = false;
    bool m_auto_load_current_game = true;
    bool m_live_cheats_enabled = false;
    bool m_seen_game_valid = false;
    bool m_force_forget_f_hooks_on_next_game_observation = false;
    PreEntryLifecycle m_preentry_lifecycle = PreEntryLifecycle::Idle;
    bool m_show_help = false;
    std::string m_source;
    std::vector<CheatBlock> m_blocks;
    std::vector<CheatGroup> m_groups;
    std::vector<std::string> m_parse_messages;
    std::string m_last_runtime_message;
    std::string m_last_preentry_message;
    std::string m_file_status;
    std::string m_loaded_path;
    uint64_t m_seen_game_generation = UINT64_MAX;
    std::string m_seen_game_identity;
    std::unordered_map<uint64_t, SwitchState> m_switches;
    /* Only selected PREENTRY identities need session persistence: absence means
     * unselected. The key includes code-file path, group path, and block name so
     * duplicate Patch names in different groups/games do not share selection. */
    std::unordered_set<std::string> m_selected_preentry_keys;
    std::unordered_map<uint64_t, FHookState> m_f_hooks;
    /* A disabled/replaced F0 may still be executing in its old cave (or have
     * a saved resume EIP pending into it). Detach those allocations from the hook
     * owner so a different F0 can immediately reuse the same guest hook while
     * the old cave is reclaimed asynchronously once it is no longer live. */
    std::vector<FHookState> m_retired_f_hooks;
    /* The debugger draws F0 T-register banks every frame, while hook state
     * normally changes only on enable/disable/reload. Cache the sorted display
     * metadata and invalidate it only when active hook ownership changes. */
    mutable std::vector<FTempBankInfo> m_f_temp_bank_cache;
    mutable bool m_f_temp_bank_cache_dirty = true;
    std::vector<uint64_t> m_active_f_hooks_scratch;
    std::vector<std::pair<size_t, uint64_t>> m_f_deactivate_scratch;
    std::vector<AddressContext> m_address_context_scratch;

    static constexpr uint64_t kDebuggerFHookKey = UINT64_MAX;
    static constexpr size_t kDebuggerFHookOwner = (size_t)-1;

    bool ParseDebuggerF0Source(uint32_t hook_address, const std::string &source,
                               RawCode &code, std::string &error) const;
    void FillDebuggerF0HookInfo(const FHookState &state,
                                DebuggerF0HookInfo &info) const;

    void InvalidateFTempBankCache();
    void ParseSource(bool preserve_states = true);
    void ExecuteBlock(size_t block_index, CheatBlock &block);
    bool ExecuteBasicWrite(const RawCode &code, GuestAddressSpace active_space,
                           uint32_t active_base);
    bool ExecuteArithmetic(const CheatBlock &block, size_t index,
                           GuestAddressSpace active_space, uint32_t active_base,
                           size_t &next_index);
    bool ExecuteSerial(const CheatBlock &block, size_t index,
                       GuestAddressSpace active_space, uint32_t active_base,
                       size_t &next_index);
    bool ExecuteCopy(const CheatBlock &block, size_t index,
                     GuestAddressSpace active_space, uint32_t active_base,
                     size_t &next_index);
    bool ExecutePointer(const CheatBlock &block, size_t index,
                        GuestAddressSpace active_space, uint32_t active_base,
                        size_t &next_index);
    bool ExecuteBitwise(const RawCode &code, GuestAddressSpace active_space,
                        uint32_t active_base);
    bool PrepareAddressContext(const RawCode &code,
                               const std::vector<AddressContext> &contexts,
                               uint32_t active_base, AddressContext &next_context,
                               bool &push_context);
    bool ExecuteRawBytes(const CheatBlock &block, size_t index,
                         GuestAddressSpace active_space, uint32_t active_base,
                         size_t &next_index);
    void PrecompileTypeF(RawCode &code);
    bool ExecuteTypeF(size_t block_index, size_t code_index, const RawCode &code,
                      GuestAddressSpace active_space, uint32_t active_base,
                      std::vector<uint64_t> &active_hooks);
    bool ExecuteConditional(size_t block_index, const CheatBlock &block,
                            size_t index, const RawCode &code,
                            const std::vector<AddressContext> &contexts,
                            GuestAddressSpace active_space, uint32_t active_base,
                            size_t &next_index);
    void MaybeAutoLoadCurrentGame();
    void ObserveRequestedGameReset();
    void ApplySelectedPreEntryPatches();
    std::string BlockIdentityKey(const CheatBlock &block) const;
    std::string PreEntrySelectionKey(const CheatBlock &block) const;
    bool IsPreEntrySelected(const CheatBlock &block) const;
    void RememberPreEntrySelection(CheatBlock &block);
    bool LoadMatchingCurrentGameFile(bool force_reload = false);
    bool LoadSourceFile(const std::string &path);
    std::string CheatDirectory() const;
    void DrawGroup(int group_index);
    void DrawPatchGroup(int group_index);
    void DrawCheat(size_t block_index);
    void DrawPatch(size_t block_index);
    void SetGroupSelected(int group_index, bool selected);
    void SetPatchGroupSelected(int group_index, bool selected);
    void SetLiveCheatsEnabled(bool enabled);
    void DisableAllCheats(bool clear_selection);
    void DrawMenuBar();
    void DrawHelpPopup();
    bool ReloadCurrentCheatFile();
    bool EditOrCreateCurrentCheatFile();
    bool OpenCheatDirectory();
    bool OpenPathExternally(const std::string &path);
    std::string SuggestedCurrentCheatPath() const;
    void CountGroupSelection(int group_index, size_t &selected,
                             size_t &total) const;
    void CountPatchGroupSelection(int group_index, size_t &selected,
                                  size_t &total) const;

    bool ReadGuest(GuestAddressSpace space, uint32_t address,
                   void *buffer, size_t size);
    bool WriteGuest(GuestAddressSpace space, uint32_t address,
                    const void *buffer, size_t size);
    bool ReadValue(GuestAddressSpace space, uint32_t address,
                   size_t size, uint32_t &value);
    bool WriteValue(GuestAddressSpace space, uint32_t address,
                    size_t size, uint32_t value);
    bool InstallFHook(size_t owner_block, uint64_t key,
                      uint32_t hook_address,
                      const std::vector<uint8_t> &probe_code,
                      const std::vector<uint8_t> &probe_data,
                      const std::vector<XemuCheatAsmLine> *f0_source,
                      bool f0_uses_preserve, uint32_t preserve_bytes,
                      bool f0_uses_temp, uint32_t temp_bytes,
                      const std::string &definition_signature);
    bool DetermineFHookLength(uint32_t hook_address,
                              uint32_t &overwrite_length);
    static bool ParseFRawHex(const RawCode &code, std::vector<uint8_t> &bytes,
                             std::string &error, int &error_line);
    static bool FHookHasTrackedEntries(const FHookState &state);
    static bool FHookHasResources(const FHookState &state);
    static void ClearReleasedFHookState(FHookState &state);
    bool BuildFHookResumePointCache(FHookState &state);
    bool FHookCaveMayStillBeReferenced(const FHookState &state,
                                        const XemuCheatX86Registers &regs);
    bool ReleaseFHookCaveIfSafe(FHookState &state);
    void RetireFHookResources(FHookState &state);
    void ReleaseRetiredFHooks();
    void DeactivateFHook(uint64_t key);
    void DeactivateFHooksForBlock(size_t owner_block);
    void DeactivateLiveFHooks();
    void DeactivateAllFHooks();
    void ForgetFHookOwnershipForNewGuest();

    static bool ParseCodeLine(const std::string &line, RawCode &out,
                              int source_line);
    static bool ParseHeader(const std::string &source, FileHeader &out);
    static bool ParseGameId(const std::string &text, uint32_t &title_id);
    static bool HashMatches(const std::string &file_hash,
                            const std::string &current_hash);
    static std::string Trim(const std::string &s);
    static std::string Upper(const std::string &s);
    static bool ConsumePreEntryPrefix(std::string &spec);
    static std::string NormalizeTypeFLine(const std::string &line);
    static std::string TypeFDirective(const std::string &line);
    static bool ParseDeadcodeCount(const std::string &text, uint8_t &out);

    static uint32_t Type9Count(const RawCode &code);
    size_t LogicalSpan(const std::vector<RawCode> &codes, size_t index) const;
    size_t SkipLogicalCommands(const std::vector<RawCode> &codes,
                               size_t start, uint32_t count) const;
};

extern CheatEngineWindow cheat_engine_window;
