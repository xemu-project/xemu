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

#include "cheat-engine.hh"
#include "guest-pause-guard.hh"
#include "current-game.hh"
#include "../font-manager.hh"
#include "../misc.hh"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include <glib.h>
#include <glib/gstdio.h>

#include "cheat-engine-memory.h"
#include "system/runstate.h"

static std::string TypeFHex32(uint32_t value)
{
    char buf[9];
    std::snprintf(buf, sizeof(buf), "%08X", value);
    return std::string(buf);
}

/* Keep the historical Type-F local name so the behavior-bearing hook methods
 * remain source-identical while sharing the generic Debug Tools pause owner. */
using TypeFGuestPauseGuard = XemuDebugGuestPauseGuard;

void CheatEngineWindow::InvalidateFTempBankCache()
{
    m_f_temp_bank_cache_dirty = true;
}

const std::vector<CheatEngineWindow::FTempBankInfo> &
CheatEngineWindow::GetActiveF0TempBanks() const
{
    if (!m_f_temp_bank_cache_dirty) {
        return m_f_temp_bank_cache;
    }

    m_f_temp_bank_cache.clear();
    if (m_f_temp_bank_cache.capacity() < m_f_hooks.size()) {
        m_f_temp_bank_cache.reserve(m_f_hooks.size());
    }

    for (const auto &entry : m_f_hooks) {
        const FHookState &state = entry.second;
        if (!state.installed || state.temp_entry == 0 || state.temp_size < 40u) {
            continue;
        }

        FTempBankInfo info;
        if (state.owner_block < m_blocks.size()) {
            info.cheat_name = m_blocks[state.owner_block].name;
        }
        if (info.cheat_name.empty()) {
            info.cheat_name = "F0 @ " + TypeFHex32(state.hook_address);
        }
        info.display_name = info.cheat_name + "  [hook " +
                            TypeFHex32(state.hook_address) + "]";
        info.hook_address = state.hook_address;
        info.cave_address = state.external_entry;
        info.cave_size = state.allocation_size;
        info.temp_address = state.temp_entry;
        m_f_temp_bank_cache.push_back(std::move(info));
    }
    std::sort(m_f_temp_bank_cache.begin(), m_f_temp_bank_cache.end(),
              [](const FTempBankInfo &a, const FTempBankInfo &b) {
                  if (a.hook_address != b.hook_address) {
                      return a.hook_address < b.hook_address;
                  }
                  return a.cheat_name < b.cheat_name;
              });
    m_f_temp_bank_cache_dirty = false;
    return m_f_temp_bank_cache;
}

bool CheatEngineWindow::ParseDebuggerF0Source(
    uint32_t hook_address, const std::string &source, RawCode &code,
    std::string &error) const
{
    code = {};
    error.clear();

    std::istringstream stream(source);
    std::string line;
    int line_number = 0;
    bool have_header = false;
    bool terminated = false;

    while (std::getline(stream, line)) {
        ++line_number;
        const std::string normalized = NormalizeTypeFLine(line);
        const std::string directive = TypeFDirective(normalized);
        if (directive.empty()) {
            continue;
        }

        if (!have_header) {
            RawCode header;
            if (!ParseCodeLine(line, header, line_number) ||
                header.command != 0xF0000000u) {
                error = "The first non-comment line must be $F0000000 AAAAAAAA.";
                return false;
            }
            if (header.value != hook_address) {
                error = "The F0 hook address must remain " + TypeFHex32(hook_address) +
                        " for this debugger selection.";
                return false;
            }
            code = std::move(header);
            have_header = true;
            continue;
        }

        if (!terminated) {
            std::istringstream tokens(directive);
            std::string name;
            std::string arg;
            std::string extra;
            tokens >> name >> arg >> extra;
            if (Upper(name) == "DEADCODE") {
                if (!arg.empty() || !extra.empty()) {
                    error = "Type-F0 DEADCODE must not have an operand (line " +
                            std::to_string(line_number) + ").";
                    return false;
                }
                terminated = true;
                continue;
            }

            code.f_body.push_back(XemuCheatAsmLine{line_number, normalized});
            continue;
        }

        /* Match the normal source parser: after DEADCODE only label-only and
         * DD declarations belong to this F0. They are physically attached
         * after the generated return JMP. */
        std::string after_label = directive;
        const size_t colon = after_label.find(':');
        bool label_only = false;
        if (colon != std::string::npos) {
            const std::string label_name = Trim(after_label.substr(0, colon));
            label_only = !label_name.empty() &&
                         Trim(after_label.substr(colon + 1)).empty();
            after_label = Trim(after_label.substr(colon + 1));
        }
        std::istringstream data_tokens(after_label);
        std::string data_name;
        data_tokens >> data_name;
        const bool is_dd = Upper(data_name) == "DD";
        if (!label_only && !is_dd) {
            error = "Only label/DD static data may follow DEADCODE (line " +
                    std::to_string(line_number) + ").";
            return false;
        }
        code.f_body.push_back(XemuCheatAsmLine{line_number, normalized});
    }

    if (!have_header) {
        error = "The Code Cave source is empty.";
        return false;
    }
    if (!terminated) {
        error = "Type-F0 source must end its executable section with $DEADCODE.";
        return false;
    }
    if (code.f_body.empty()) {
        error = "Type-F0 code cave contains no executable instructions.";
        return false;
    }

    code.f_terminated = true;
    return true;
}

void CheatEngineWindow::FillDebuggerF0HookInfo(
    const FHookState &state, DebuggerF0HookInfo &info) const
{
    info = {};
    info.installed = state.installed;
    info.hook_address = state.hook_address;
    info.overwrite_length = state.overwrite_length;
    info.cave_address = state.external_entry;
    info.code_size = state.code_size;
    info.return_address = state.hook_address + state.overwrite_length;
}

bool CheatEngineWindow::GetDebuggerF0HookInfo(DebuggerF0HookInfo &info) const
{
    info = {};
    const auto it = m_f_hooks.find(kDebuggerFHookKey);
    if (it == m_f_hooks.end()) {
        return false;
    }
    FillDebuggerF0HookInfo(it->second, info);
    return true;
}

bool CheatEngineWindow::ActiveFHookOwnsAddress(uint32_t address) const
{
    for (const auto &entry : m_f_hooks) {
        const FHookState &state = entry.second;
        if (!state.installed) {
            continue;
        }
        const uint64_t probe = address;
        const uint64_t hook_start = state.hook_address;
        const uint64_t hook_end = hook_start + state.overwrite_length;
        const uint64_t cave_start = state.external_entry;
        const uint64_t cave_end = cave_start + state.allocation_size;
        if ((probe >= hook_start && probe < hook_end) ||
            (state.external_entry != 0 && probe >= cave_start && probe < cave_end)) {
            return true;
        }
    }

    /* Retired caves are no longer reachable from their original hook, but an
     * already-running EIP/saved resume point may still use their executable memory.
     * Keep Inject/Change/NOP from editing that memory until reclamation. */
    for (const FHookState &state : m_retired_f_hooks) {
        if (state.external_entry == 0 || state.allocation_size == 0) {
            continue;
        }
        const uint64_t probe = address;
        const uint64_t cave_start = state.external_entry;
        const uint64_t cave_end = cave_start + state.allocation_size;
        if (probe >= cave_start && probe < cave_end) {
            return true;
        }
    }
    return false;
}

bool CheatEngineWindow::InstallDebuggerF0(
    uint32_t hook_address, const std::string &source,
    DebuggerF0HookInfo &info, std::string &status)
{
    info = {};
    status.clear();

    RawCode code;
    std::string parse_error;
    if (!ParseDebuggerF0Source(hook_address, source, code, parse_error)) {
        status = "Code Cave: " + parse_error;
        return false;
    }

    XemuCheatAsmResult assembled;
    if (!xemu_cheat_assemble_x86_32(code.f_body, assembled)) {
        status = "Code Cave assembler error";
        if (assembled.error_line > 0) {
            status += " on line " + std::to_string(assembled.error_line);
        }
        if (!assembled.error.empty()) {
            status += ": " + assembled.error;
        }
        return false;
    }

    std::string signature = "F0\n";
    for (const XemuCheatAsmLine &src : code.f_body) {
        signature += src.text;
        signature.push_back('\n');
    }

    m_last_runtime_message.clear();
    if (!InstallFHook(kDebuggerFHookOwner, kDebuggerFHookKey, hook_address,
                      assembled.bytes, assembled.data, &code.f_body,
                      assembled.uses_preserve, assembled.preserve_bytes,
                      assembled.uses_temp, assembled.temp_bytes, signature)) {
        status = m_last_runtime_message.empty()
                     ? "Code Cave hook installation failed."
                     : m_last_runtime_message;
        return false;
    }

    if (!GetDebuggerF0HookInfo(info) || !info.installed) {
        status = "Code Cave hook installed, but its debugger state could not be read.";
        return false;
    }

    status = "Code Cave RUNNING: " + TypeFHex32(info.hook_address) +
             " -> " + TypeFHex32(info.cave_address) +
             ", return " + TypeFHex32(info.return_address) + ".";
    return true;
}

bool CheatEngineWindow::RemoveDebuggerF0(std::string &status)
{
    status.clear();
    auto it = m_f_hooks.find(kDebuggerFHookKey);
    if (it == m_f_hooks.end() ||
        (!it->second.installed && !FHookHasTrackedEntries(it->second))) {
        status = "Code Cave is not running.";
        return true;
    }

    const uint32_t hook_address = it->second.hook_address;
    DeactivateFHook(kDebuggerFHookKey);
    it = m_f_hooks.find(kDebuggerFHookKey);
    if (it != m_f_hooks.end() && it->second.installed) {
        status = "Code Cave RESTORE failed; the hook is still active.";
        return false;
    }

    status = "Code Cave restored original bytes at " + TypeFHex32(hook_address) + ".";
    return true;
}

bool CheatEngineWindow::DetermineFHookLength(uint32_t hook_address,
                                                 uint32_t &overwrite_length)
{
    XemuCheatDisasmRow rows[16] = {};
    size_t row_count = 0;
    overwrite_length = 0;

    if (!xemu_cheat_disassembler_available()) {
        m_last_runtime_message =
            "Type-F automatic hook sizing requires the Capstone x86 decoder.";
        return false;
    }

    const int rc = xemu_cheat_disassemble_paired(hook_address,
                                                  (int)(sizeof(rows) / sizeof(rows[0])),
                                                  rows,
                                                  sizeof(rows) / sizeof(rows[0]),
                                                  &row_count);
    if (rc != XEMU_CHEAT_DISAS_OK || row_count == 0) {
        m_last_runtime_message =
            "Type-F could not disassemble the virtual hook address 0x";
        char address[16];
        std::snprintf(address, sizeof(address), "%08X", hook_address);
        m_last_runtime_message += address;
        return false;
    }

    for (size_t n = 0; n < row_count && overwrite_length < 5u; ++n) {
        if (rows[n].size == 0 || rows[n].size > 15u) {
            break;
        }
        overwrite_length += rows[n].size;
        if (overwrite_length > 32u) {
            break;
        }
    }

    if (overwrite_length < 5u || overwrite_length > 32u) {
        m_last_runtime_message =
            "Type-F could not find a complete 5-32 byte instruction span for the hook.";
        return false;
    }
    return true;
}

bool CheatEngineWindow::ParseFRawHex(const RawCode &code,
                                     std::vector<uint8_t> &bytes,
                                     std::string &error,
                                     int &error_line)
{
    bytes.clear();
    error.clear();
    error_line = code.source_line;

    if (code.f_final_valid_bytes < 1u || code.f_final_valid_bytes > 8u) {
        error = "Type-F1 requires DEADCODE 000000NN with NN from 01 through 08";
        return false;
    }
    if (code.f_body.empty()) {
        error = "Type-F1 raw cave contains no 32-bit data pairs";
        return false;
    }
    bytes.reserve(std::min<size_t>(code.f_body.size() * 8u, 0x10000u));

    auto decode_word = [](const std::string &token, uint8_t out[4]) -> bool {
        if (token.size() != 8) {
            return false;
        }
        auto nibble = [](char c) -> int {
            unsigned char u = (unsigned char)c;
            if (u >= '0' && u <= '9') return u - '0';
            u = (unsigned char)std::toupper(u);
            if (u >= 'A' && u <= 'F') return u - 'A' + 10;
            return -1;
        };
        for (size_t i = 0; i < 4; ++i) {
            const int hi = nibble(token[i * 2]);
            const int lo = nibble(token[i * 2 + 1]);
            if (hi < 0 || lo < 0) {
                return false;
            }
            out[i] = (uint8_t)((hi << 4) | lo);
        }
        return true;
    };

    for (size_t line_index = 0; line_index < code.f_body.size(); ++line_index) {
        const XemuCheatAsmLine &src = code.f_body[line_index];
        std::string line = Trim(src.text);
        if (!line.empty() && line[0] == '$') {
            line = Trim(line.substr(1));
        }

        size_t cut = line.size();
        size_t comment = line.find(';');
        if (comment != std::string::npos) cut = std::min(cut, comment);
        comment = line.find("//");
        if (comment != std::string::npos) cut = std::min(cut, comment);
        comment = line.find('#');
        if (comment != std::string::npos) cut = std::min(cut, comment);
        line.erase(cut);
        line = Trim(line);

        std::istringstream tokens(line);
        std::string left;
        std::string right;
        std::string extra;
        tokens >> left >> right >> extra;
        if (left.empty() || right.empty() || !extra.empty()) {
            error = "Type-F1 data lines must be exactly XXXXXXXX YYYYYYYY";
            error_line = src.source_line;
            return false;
        }

        uint8_t pair[8];
        if (!decode_word(left, &pair[0]) || !decode_word(right, &pair[4])) {
            error = "Type-F1 data lines require two 8-digit hexadecimal words";
            error_line = src.source_line;
            return false;
        }

        const bool final_line = line_index + 1u == code.f_body.size();
        const size_t valid = final_line ? code.f_final_valid_bytes : 8u;
        if (final_line && valid < 8u) {
            for (size_t i = valid; i < 8u; ++i) {
                if (pair[i] != 0) {
                    error = "Type-F1 bytes after the DEADCODE valid-byte count must be zero padding";
                    error_line = src.source_line;
                    return false;
                }
            }
        }

        bytes.insert(bytes.end(), pair, pair + valid);
        if (bytes.size() > 0x10000u) {
            error = "Type-F1 raw cave exceeds 64 KiB";
            error_line = src.source_line;
            return false;
        }
    }

    if (bytes.empty()) {
        error = "Type-F1 raw cave contains no executable bytes";
        return false;
    }
    return true;
}

void CheatEngineWindow::PrecompileTypeF(RawCode &code)
{
    code.f_precompiled = true;
    code.f_precompile_ok = false;
    code.f_probe_code.clear();
    code.f_probe_data.clear();
    code.f_uses_preserve = false;
    code.f_preserve_bytes = 0;
    code.f_uses_temp = false;
    code.f_temp_bytes = 0;
    code.f_definition_signature.clear();
    code.f_precompile_error.clear();
    code.f_precompile_error_line = code.source_line;

    const uint32_t subtype = (code.command >> 24) & 0xFu;
    if (!code.f_terminated) {
        code.f_precompile_error = "Type-F block is missing DEADCODE";
        return;
    }

    if (subtype == 0x0) {
        size_t signature_size = 3u;
        for (const XemuCheatAsmLine &src : code.f_body) {
            signature_size += src.text.size() + 1u;
        }
        code.f_definition_signature.reserve(signature_size);
        code.f_definition_signature = "F0\n";
        for (const XemuCheatAsmLine &src : code.f_body) {
            code.f_definition_signature += src.text;
            code.f_definition_signature.push_back('\n');
        }

        XemuCheatAsmResult assembled;
        if (!xemu_cheat_assemble_x86_32(code.f_body, assembled)) {
            code.f_precompile_error = assembled.error;
            code.f_precompile_error_line = assembled.error_line > 0
                                              ? assembled.error_line
                                              : code.source_line;
            return;
        }
        code.f_probe_code = std::move(assembled.bytes);
        code.f_probe_data = std::move(assembled.data);
        code.f_uses_preserve = assembled.uses_preserve;
        code.f_preserve_bytes = assembled.preserve_bytes;
        code.f_uses_temp = assembled.uses_temp;
        code.f_temp_bytes = assembled.temp_bytes;
        code.f_precompile_ok = true;
        return;
    }

    if (subtype == 0x1) {
        std::string error;
        int error_line = code.source_line;
        if (!ParseFRawHex(code, code.f_probe_code, error, error_line)) {
            code.f_precompile_error = error;
            code.f_precompile_error_line = error_line;
            return;
        }
        code.f_definition_signature.assign("F1\n", 3);
        code.f_definition_signature.append(
            reinterpret_cast<const char *>(code.f_probe_code.data()),
            code.f_probe_code.size());
        code.f_precompile_ok = true;
        return;
    }

    code.f_precompile_error = "Unsupported Type-F subtype";
}

bool CheatEngineWindow::InstallFHook(
    size_t owner_block, uint64_t key, uint32_t hook_address,
    const std::vector<uint8_t> &probe_code,
    const std::vector<uint8_t> &probe_data,
    const std::vector<XemuCheatAsmLine> *f0_source,
    bool f0_uses_preserve, uint32_t preserve_bytes,
    bool f0_uses_temp, uint32_t temp_bytes,
    const std::string &definition_signature)
{
    if (probe_code.empty() ||
        probe_code.size() + 5u + probe_data.size() > 0x10000u) {
        m_last_runtime_message =
            "Type-F hook install: assembled/raw cave is empty or exceeds 64 KiB.";
        return false;
    }
    if (f0_uses_preserve && (f0_source == nullptr || preserve_bytes == 0)) {
        m_last_runtime_message =
            "Type-F0 hook install: PRESERVE metadata was incomplete.";
        return false;
    }
    if (f0_uses_temp && (f0_source == nullptr || temp_bytes != 40u)) {
        m_last_runtime_message =
            "Type-F0 hook install: T0-T7/TFLAGS metadata was incomplete.";
        return false;
    }

    auto it = m_f_hooks.find(key);

    /* A source-position key can change when a user edits/reorders a cheat.
     * If another inactive state already owns the same guest hook address,
     * adopt its known-good hook/original-byte metadata. */
    if (it == m_f_hooks.end()) {
        for (auto other = m_f_hooks.begin(); other != m_f_hooks.end(); ++other) {
            if (other->first == key || other->second.hook_address != hook_address) {
                continue;
            }
            if (other->second.installed) {
                m_last_runtime_message =
                    "Type-F hook install: another active Type-F cheat already owns hook 0x" +
                    TypeFHex32(hook_address) + ". Disable it first.";
                return false;
            }

            FHookState adopted = std::move(other->second);
            m_f_hooks.erase(other);
            adopted.owner_block = owner_block;
            auto inserted = m_f_hooks.emplace(key, std::move(adopted));
            it = inserted.first;
            break;
        }
    }

    /* Normal 10 Hz ticks take this path. The source/RAW signature is stable
     * even when F0 contains absolute DD/preservation addresses, so we never
     * rewrite an already-active cave merely because its final bytes depend on
     * the allocator-selected address. */
    if (it != m_f_hooks.end() && it->second.installed &&
        it->second.hook_address == hook_address &&
        it->second.definition_signature == definition_signature) {
        if (it->second.owner_block != owner_block &&
            it->second.temp_entry != 0 && it->second.temp_size >= 40u) {
            InvalidateFTempBankCache();
        }
        it->second.owner_block = owner_block;
        return true;
    }

    TypeFGuestPauseGuard guest_pause;

    if (it != m_f_hooks.end() && it->second.hook_address != hook_address) {
        DeactivateFHook(key);
        if (it->second.installed) {
            m_last_runtime_message =
                "Type-F hook install: failed to restore the previous hook before moving it.";
            return false;
        }
        m_f_hooks.erase(it);
        it = m_f_hooks.end();
    }

    if (it == m_f_hooks.end()) {
        FHookState state;
        state.owner_block = owner_block;
        state.hook_address = hook_address;
        auto inserted = m_f_hooks.emplace(key, std::move(state));
        it = inserted.first;
    }

    FHookState &state = it->second;
    state.owner_block = owner_block;

    /* Every post-allocation install failure owns the same rollback contract:
     * these allocations were never reachable from a guest hook, so they may
     * be reclaimed immediately. Keep that contract in one place so a future
     * failure branch cannot forget either the reachability flag or release. */
    auto cleanup_failed_install = [&]() {
        state.retired_may_be_referenced = false;
        ReleaseFHookCaveIfSafe(state);
    };

    if (state.installed && state.definition_signature != definition_signature) {
        DeactivateFHook(key);
        if (state.installed) {
            m_last_runtime_message =
                "Type-F hook install: failed to restore the active hook before updating cave code.";
            return false;
        }
    }

    /* A previous version of this hook may still have allocations attached if
     * an earlier install/deactivation could not reclaim them immediately.
     * Never make the new F0 wait on that old cave: detach it into the retired
     * queue and let it finish safely while this hook receives a fresh cave. */
    if (!state.installed && FHookHasTrackedEntries(state) &&
        !ReleaseFHookCaveIfSafe(state)) {
        RetireFHookResources(state);
    }

    uint32_t overwrite_length = 0;
    if (!DetermineFHookLength(hook_address, overwrite_length)) {
        if (m_last_runtime_message.empty()) {
            m_last_runtime_message =
                "Type-F hook install: failed while determining the hook instruction span.";
        }
        return false;
    }
    state.overwrite_length = overwrite_length;
    state.original_bytes.resize(overwrite_length);
    if (!ReadGuest(GuestAddressSpace::Virtual, hook_address,
                   state.original_bytes.data(), state.original_bytes.size())) {
        m_last_runtime_message =
            "Type-F hook install: failed to read/save original hook bytes at 0x" +
            TypeFHex32(hook_address) + ".";
        return false;
    }

    const uint32_t required_size =
        (uint32_t)probe_code.size() + 5u + (uint32_t)probe_data.size();
    uint32_t external_entry = 0;
    if (!xemu_cheat_external_code_allocate(required_size, &external_entry)) {
        const char *detail = xemu_cheat_external_code_last_error();
        m_last_runtime_message =
            "Type-F hook install: could not allocate external executable/DD memory";
        if (detail != nullptr && detail[0] != '\0') {
            m_last_runtime_message += ": ";
            m_last_runtime_message += detail;
        }
        m_last_runtime_message += ".";
        return false;
    }
    state.external_entry = external_entry;
    state.allocation_size = (required_size + 0x0Fu) & ~0x0Fu;
    state.retired_may_be_referenced = false;

    if (f0_uses_preserve) {
        uint32_t preserve_entry = 0;
        if (!xemu_cheat_external_preserve_allocate(preserve_bytes,
                                                   &preserve_entry)) {
            m_last_runtime_message =
                "Type-F0 hook install: could not allocate private preservation frames.";
            cleanup_failed_install();
            return false;
        }
        state.preserve_entry = preserve_entry;
        state.preserve_size = (preserve_bytes + 0x0Fu) & ~0x0Fu;
    }

    if (f0_uses_temp) {
        uint32_t temp_entry = 0;
        if (!xemu_cheat_external_preserve_allocate(temp_bytes, &temp_entry)) {
            m_last_runtime_message =
                "Type-F0 hook install: could not allocate private T0-T7/TFLAGS bank.";
            cleanup_failed_install();
            return false;
        }
        state.temp_entry = temp_entry;
        state.temp_size = (temp_bytes + 0x0Fu) & ~0x0Fu;
    }

    const std::vector<uint8_t> *final_code = &probe_code;
    const std::vector<uint8_t> *final_data = &probe_data;
    XemuCheatAsmResult final_asm;
    if (f0_source != nullptr) {
        if (!xemu_cheat_assemble_x86_32_at(*f0_source,
                                           state.external_entry,
                                           state.preserve_entry,
                                           state.temp_entry,
                                           final_asm)) {
            m_last_runtime_message = "Type-F0 final assembler error";
            if (final_asm.error_line > 0) {
                m_last_runtime_message += " on source line " +
                    std::to_string(final_asm.error_line);
            }
            m_last_runtime_message += ": " + final_asm.error;
            cleanup_failed_install();
            return false;
        }
        if (final_asm.bytes.size() != probe_code.size() ||
            final_asm.data.size() != probe_data.size() ||
            final_asm.uses_preserve != f0_uses_preserve ||
            final_asm.uses_temp != f0_uses_temp) {
            m_last_runtime_message =
                "Type-F0 internal error: final assembly changed the probed cave layout.";
            cleanup_failed_install();
            return false;
        }
        final_code = &final_asm.bytes;
        final_data = &final_asm.data;
    }

    state.code_size = (uint32_t)final_code->size();
    state.definition_signature = definition_signature;

    /* Build one contiguous cave payload while the guest is paused:
     *   executable code | generated DEADCODE JMP | attached DD data
     * This guarantees that a label such as CarList points to storage owned by
     * the same allocation and that no neighboring cave can overlap it. */
    uint8_t return_jump[5];
    return_jump[0] = 0xE9;
    const uint32_t return_from = state.external_entry + state.code_size;
    const uint32_t return_to = hook_address + state.overwrite_length;
    const uint32_t return_rel = return_to - (return_from + 5u);
    return_jump[1] = (uint8_t)(return_rel & 0xFFu);
    return_jump[2] = (uint8_t)((return_rel >> 8) & 0xFFu);
    return_jump[3] = (uint8_t)((return_rel >> 16) & 0xFFu);
    return_jump[4] = (uint8_t)((return_rel >> 24) & 0xFFu);

    std::vector<uint8_t> payload;
    payload.reserve(final_code->size() + sizeof(return_jump) + final_data->size());
    payload.insert(payload.end(), final_code->begin(), final_code->end());
    payload.insert(payload.end(), return_jump, return_jump + sizeof(return_jump));
    payload.insert(payload.end(), final_data->begin(), final_data->end());
    if (!xemu_cheat_external_code_write(state.external_entry, 0,
                                         payload.data(), payload.size())) {
        m_last_runtime_message =
            "Type-F hook install: failed to write code/DEADCODE/DD payload to external memory.";
        cleanup_failed_install();
        return false;
    }

    /* Cache every executable instruction boundary once while the freshly-written
     * cave is available. These resume points cover normal CALL returns as well
     * as EIP values saved by guest interrupts/exceptions. A failure is
     * conservative: retirement can retry lazily. */
    BuildFHookResumePointCache(state);

    uint8_t hook[32];
    memset(hook, 0x90, state.overwrite_length);
    hook[0] = 0xE9;
    const uint32_t hook_rel = state.external_entry - (hook_address + 5u);
    hook[1] = (uint8_t)(hook_rel & 0xFFu);
    hook[2] = (uint8_t)((hook_rel >> 8) & 0xFFu);
    hook[3] = (uint8_t)((hook_rel >> 16) & 0xFFu);
    hook[4] = (uint8_t)((hook_rel >> 24) & 0xFFu);

    state.installed = true;
    if (!xemu_cheat_patch_virtual(hook_address, hook,
                                  state.overwrite_length)) {
        const std::string write_error =
            "Type-F hook install: failed to write the guest hook JMP at 0x" +
            TypeFHex32(hook_address) + ".";
        DeactivateFHook(key);
        if (state.installed) {
            m_last_runtime_message = write_error +
                " Automatic rollback also failed; the cave was retained.";
        } else {
            m_last_runtime_message = write_error +
                " Original hook bytes were restored.";
        }
        return false;
    }

    state.retired_may_be_referenced = false;
    InvalidateFTempBankCache();
    return true;
}

bool CheatEngineWindow::BuildFHookResumePointCache(FHookState &state)
{
    state.resume_points.clear();
    state.resume_points_valid = false;
    if (state.external_entry == 0 || state.code_size == 0) {
        state.resume_points_valid = true;
        return true;
    }

    /* The generated DEADCODE jump is executable too. An interrupt can land
     * between the final user instruction and that jump, so include its start
     * address in the immutable resume-point cache as well. DD data that follows
     * the jump is intentionally excluded. */
    const uint64_t executable_end =
        (uint64_t)state.external_entry + state.code_size + 5u;
    uint32_t decode_pc = state.external_entry;
    while ((uint64_t)decode_pc < executable_end) {
        XemuCheatDisasmRow rows[128] = {};
        size_t row_count = 0;
        const int rc = xemu_cheat_disassemble_paired(
            decode_pc, (int)(sizeof(rows) / sizeof(rows[0])), rows,
            sizeof(rows) / sizeof(rows[0]), &row_count);
        if (rc != XEMU_CHEAT_DISAS_OK || row_count == 0) {
            return false;
        }

        uint32_t next_pc = decode_pc;
        for (size_t i = 0; i < row_count; ++i) {
            const XemuCheatDisasmRow &row = rows[i];
            if ((uint64_t)row.virtual_address >= executable_end) {
                break;
            }
            if (row.size == 0) {
                return false;
            }
            const uint64_t after =
                (uint64_t)row.virtual_address + (uint64_t)row.size;
            if (after > executable_end) {
                return false;
            }

            /* x86 interrupt/exception frames and CALL returns resume at an
             * instruction boundary. Recording every valid start is therefore
             * stronger than tracking CALL return sites alone and also handles
             * caves that contain no CALL instructions at all. */
            state.resume_points.push_back(row.virtual_address);
            next_pc = (uint32_t)after;
        }
        if ((uint64_t)next_pc >= executable_end) {
            break;
        }
        if (next_pc <= decode_pc) {
            return false;
        }
        decode_pc = next_pc;
    }

    std::sort(state.resume_points.begin(), state.resume_points.end());
    state.resume_points.erase(
        std::unique(state.resume_points.begin(), state.resume_points.end()),
        state.resume_points.end());
    state.resume_points_valid = true;
    return true;
}

bool CheatEngineWindow::FHookCaveMayStillBeReferenced(
    const FHookState &state, const XemuCheatX86Registers &regs)
{
    if (state.external_entry == 0 || state.allocation_size == 0) {
        return false;
    }

    const uint64_t cave_start = state.external_entry;
    const uint64_t cave_end = cave_start + state.allocation_size;
    if ((uint64_t)regs.pc >= cave_start && (uint64_t)regs.pc < cave_end) {
        return true;
    }

    /* Resume points are decoded once when the cave is installed (or lazily
     * during retirement if that initial cache could not be built). This covers
     * CALL returns and EIP saved by guest interrupts/exceptions without
     * redisassembling immutable cave code on every retirement tick. */
    if (!state.resume_points_valid) {
        return true;
    }
    if (state.resume_points.empty()) {
        return false;
    }

    constexpr size_t kStackScanBytes = 0x10000;
    constexpr size_t kStackChunk = 0x1000;
    uint8_t buffer[kStackChunk];
    size_t scanned = 0;
    uint32_t address = regs.esp;

    while (scanned < kStackScanBytes) {
        const size_t page_remaining =
            0x1000u - (size_t)(address & 0x0FFFu);
        const size_t amount = std::min(
            std::min(kStackChunk, kStackScanBytes - scanned), page_remaining);
        if ((uint64_t)address + amount > 0x100000000ull) {
            break;
        }
        if (!ReadGuest(GuestAddressSpace::Virtual, address, buffer, amount)) {
            if (scanned == 0) {
                return true;
            }
            break;
        }

        for (size_t off = 0; off + 4 <= amount; ++off) {
            const uint32_t candidate =
                (uint32_t)buffer[off] |
                ((uint32_t)buffer[off + 1] << 8) |
                ((uint32_t)buffer[off + 2] << 16) |
                ((uint32_t)buffer[off + 3] << 24);
            if (std::binary_search(state.resume_points.begin(),
                                   state.resume_points.end(), candidate)) {
                return true;
            }
        }

        const uint64_t next_address = (uint64_t)address + amount;
        scanned += amount;
        if (next_address >= 0x100000000ull) {
            break;
        }
        address = (uint32_t)next_address;
    }
    return false;
}

bool CheatEngineWindow::FHookHasTrackedEntries(const FHookState &state)
{
    return state.external_entry != 0 || state.preserve_entry != 0 ||
           state.temp_entry != 0;
}

bool CheatEngineWindow::FHookHasResources(const FHookState &state)
{
    return (state.external_entry != 0 && state.allocation_size != 0) ||
           (state.preserve_entry != 0 && state.preserve_size != 0) ||
           (state.temp_entry != 0 && state.temp_size != 0);
}

void CheatEngineWindow::ClearReleasedFHookState(FHookState &state)
{
    state.external_entry = 0;
    state.allocation_size = 0;
    state.code_size = 0;
    state.preserve_entry = 0;
    state.preserve_size = 0;
    state.temp_entry = 0;
    state.temp_size = 0;
    state.definition_signature.clear();
    state.retired_may_be_referenced = false;
    state.resume_points_valid = false;
    state.resume_points.clear();
    state.retire_backoff_ticks = 0;
    state.retire_skip_ticks = 0;
}

bool CheatEngineWindow::ReleaseFHookCaveIfSafe(FHookState &state)
{
    const bool have_cave = state.external_entry != 0 && state.allocation_size != 0;
    const bool have_preserve = state.preserve_entry != 0 && state.preserve_size != 0;
    const bool have_temp = state.temp_entry != 0 && state.temp_size != 0;
    if (!have_cave && !have_preserve && !have_temp) {
        ClearReleasedFHookState(state);
        return true;
    }

    TypeFGuestPauseGuard guest_pause;

    bool safe_to_release = true;
    if (have_cave && state.retired_may_be_referenced) {
        if (!state.resume_points_valid && !BuildFHookResumePointCache(state)) {
            safe_to_release = false;
        } else {
            XemuCheatX86Registers regs = {};
            const bool have_regs = xemu_cheat_get_x86_registers(&regs) != 0;
            safe_to_release = have_regs &&
                              !FHookCaveMayStillBeReferenced(state, regs);
        }
    }

    bool released = false;
    if (safe_to_release) {
        bool code_ok = true;
        bool preserve_ok = true;
        bool temp_ok = true;
        if (have_cave) {
            code_ok = xemu_cheat_external_code_free(
                          state.external_entry, state.allocation_size) != 0;
            if (code_ok) {
                state.external_entry = 0;
                state.allocation_size = 0;
            }
        }
        /* The preservation block is reachable only from its owning cave. Once
         * that cave is unreachable/freed, zero and return the private frames. */
        if (code_ok && have_preserve) {
            preserve_ok = xemu_cheat_external_preserve_free(
                              state.preserve_entry, state.preserve_size) != 0;
            if (preserve_ok) {
                state.preserve_entry = 0;
                state.preserve_size = 0;
            }
        }
        /* T0-T7 persist while the F0 remains active. Once its cave is no
         * longer reachable, zero/free the one private T-register/TFLAGS bank. */
        if (code_ok && preserve_ok && have_temp) {
            temp_ok = xemu_cheat_external_preserve_free(
                          state.temp_entry, state.temp_size) != 0;
            if (temp_ok) {
                state.temp_entry = 0;
                state.temp_size = 0;
            }
        }
        released = code_ok && preserve_ok && temp_ok;
        if (released) {
            ClearReleasedFHookState(state);
        }
    }

    return released;
}

void CheatEngineWindow::RetireFHookResources(FHookState &state)
{
    if (!FHookHasTrackedEntries(state)) {
        return;
    }

    FHookState retired;
    retired.owner_block = state.owner_block;
    retired.hook_address = state.hook_address;
    retired.overwrite_length = state.overwrite_length;
    retired.external_entry = state.external_entry;
    retired.allocation_size = state.allocation_size;
    retired.code_size = state.code_size;
    retired.preserve_entry = state.preserve_entry;
    retired.preserve_size = state.preserve_size;
    retired.temp_entry = state.temp_entry;
    retired.temp_size = state.temp_size;
    retired.retired_may_be_referenced = state.retired_may_be_referenced;
    retired.resume_points_valid = state.resume_points_valid;
    retired.resume_points = std::move(state.resume_points);
    retired.retire_backoff_ticks = 0;
    /* Never reclaim a cave on the same UI action that restored the hook.
     * At the normal 10 Hz engine rate, two skipped retirement ticks provide a
     * short grace window for guest interrupt/exception frames to unwind. */
    retired.retire_skip_ticks = 2;

    /* The active hook state keeps its hook/original-byte identity, but no
     * longer owns the old allocations. This is what allows A OFF -> B ON at
     * the same hook address without B waiting for A's cave to be reclaimed. */
    state.external_entry = 0;
    state.allocation_size = 0;
    state.code_size = 0;
    state.preserve_entry = 0;
    state.preserve_size = 0;
    state.temp_entry = 0;
    state.temp_size = 0;
    state.retired_may_be_referenced = false;
    state.resume_points_valid = false;
    state.resume_points.clear();
    state.retire_backoff_ticks = 0;
    state.retire_skip_ticks = 0;

    if (FHookHasTrackedEntries(retired)) {
        m_retired_f_hooks.push_back(std::move(retired));
    }
}

void CheatEngineWindow::ReleaseRetiredFHooks()
{
    if (m_retired_f_hooks.empty()) {
        return;
    }

    bool have_due = false;
    for (FHookState &state : m_retired_f_hooks) {
        if (state.retire_skip_ticks != 0) {
            --state.retire_skip_ticks;
        } else {
            have_due = true;
        }
    }
    if (!have_due) {
        return;
    }

    /* Pause at most once for all caves that are due this tick. Caves that are
     * still referenced use a small exponential tick backoff (100 ms -> 200 ->
     * 400 -> 800 -> 1 s at the normal 10 Hz engine rate), avoiding repeated
     * stack scans while preserving prompt eventual reclamation. */
    TypeFGuestPauseGuard guest_pause;
    auto it = m_retired_f_hooks.begin();
    while (it != m_retired_f_hooks.end()) {
        if (it->retire_skip_ticks != 0) {
            ++it;
            continue;
        }
        if (ReleaseFHookCaveIfSafe(*it) || !FHookHasTrackedEntries(*it)) {
            it = m_retired_f_hooks.erase(it);
        } else {
            const uint8_t next = it->retire_backoff_ticks == 0
                                     ? 1
                                     : (uint8_t)std::min<int>(
                                           10, it->retire_backoff_ticks * 2);
            it->retire_backoff_ticks = next;
            it->retire_skip_ticks = next;
            ++it;
        }
    }
}


void CheatEngineWindow::DeactivateFHook(uint64_t key)
{
    auto it = m_f_hooks.find(key);
    if (it == m_f_hooks.end()) {
        return;
    }

    FHookState &state = it->second;
    if (!state.installed && !FHookHasTrackedEntries(state)) {
        return;
    }

    TypeFGuestPauseGuard guest_pause;

    if (state.installed) {
        if (state.original_bytes.empty() ||
            !xemu_cheat_patch_virtual(state.hook_address,
                                      state.original_bytes.data(),
                                      state.original_bytes.size())) {
            return;
        }
        state.installed = false;
        InvalidateFTempBankCache();
        /* The hook really was reachable before restoration. EIP may still be
         * inside the cave, or the guest may have a saved resume EIP into it. */
        state.retired_may_be_referenced = true;
    }

    /* Once the original hook is restored, a cave that was ever reachable is
     * always detached into the retired queue first. Do not free/reuse it on the
     * same UI action: guest interrupt/exception frames may still contain a
     * resume EIP into the old cave even when the currently-visible EIP is
     * elsewhere. Failed pre-hook allocations were never reachable and may
     * still be reclaimed immediately. */
    if (FHookHasResources(state)) {
        if (state.retired_may_be_referenced) {
            RetireFHookResources(state);
        } else if (!ReleaseFHookCaveIfSafe(state)) {
            RetireFHookResources(state);
        }
    }

}

void CheatEngineWindow::DeactivateFHooksForBlock(size_t owner_block)
{
    bool have_block_cave = false;
    for (const auto &entry : m_f_hooks) {
        if (entry.second.owner_block == owner_block &&
            (entry.second.installed || FHookHasTrackedEntries(entry.second))) {
            have_block_cave = true;
            break;
        }
    }
    if (!have_block_cave) {
        return;
    }

    TypeFGuestPauseGuard guest_pause;
    for (auto &entry : m_f_hooks) {
        if (entry.second.owner_block == owner_block) {
            DeactivateFHook(entry.first);
        }
    }
}

void CheatEngineWindow::DeactivateLiveFHooks()
{
    bool has_preentry_blocks = false;
    for (const CheatBlock &block : m_blocks) {
        if (block.preentry) {
            has_preentry_blocks = true;
            break;
        }
    }

    if (!has_preentry_blocks) {
        /* Preserve the historical fast path for ordinary code files: before
         * PREENTRY existed every tracked Cheat Engine hook was live-owned. */
        DeactivateAllFHooks();
        return;
    }

    m_f_deactivate_scratch.clear();
    for (const auto &entry : m_f_hooks) {
        const size_t owner = entry.second.owner_block;
        if (owner < m_blocks.size() && !m_blocks[owner].preentry &&
            (entry.second.installed || FHookHasTrackedEntries(entry.second))) {
            m_f_deactivate_scratch.emplace_back(owner, entry.first);
        }
    }
    if (m_f_deactivate_scratch.empty()) {
        return;
    }

    TypeFGuestPauseGuard guest_pause;
    for (const auto &pending : m_f_deactivate_scratch) {
        DeactivateFHook(pending.second);
    }
}

void CheatEngineWindow::DeactivateAllFHooks()
{
    bool have_cave = false;
    for (const auto &entry : m_f_hooks) {
        if (entry.second.installed || FHookHasTrackedEntries(entry.second)) {
            have_cave = true;
            break;
        }
    }
    if (!have_cave) {
        return;
    }

    TypeFGuestPauseGuard guest_pause;
    for (auto &entry : m_f_hooks) {
        DeactivateFHook(entry.first);
    }
}

void CheatEngineWindow::ForgetFHookOwnershipForNewGuest()
{
    /* CurrentGameManager has already identified a different guest/XBE. Never
     * restore bytes captured from the previous address space into the new one;
     * only forget stale ownership and reset Debug Tools' private cave arena. */
    m_f_hooks.clear();
    m_retired_f_hooks.clear();
    m_f_deactivate_scratch.clear();
    m_active_f_hooks_scratch.clear();
    InvalidateFTempBankCache();
    xemu_cheat_external_code_reset_allocations();
}

