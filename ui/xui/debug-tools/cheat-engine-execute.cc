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

bool CheatEngineWindow::ReadGuest(GuestAddressSpace space, uint32_t address,
                                  void *buffer, size_t size)
{
    return xemu_cheat_memory_read(space == GuestAddressSpace::Virtual,
                                  address, buffer, size) != 0;
}

bool CheatEngineWindow::WriteGuest(GuestAddressSpace space, uint32_t address,
                                   const void *buffer, size_t size)
{
    return xemu_cheat_memory_write(space == GuestAddressSpace::Virtual,
                                   address, buffer, size) != 0;
}

bool CheatEngineWindow::ReadValue(GuestAddressSpace space, uint32_t address,
                                  size_t size, uint32_t &value)
{
    uint8_t bytes[4] = {};
    if (size == 0 || size > sizeof(bytes) ||
        !ReadGuest(space, address, bytes, size)) {
        return false;
    }

    value = bytes[0];
    if (size >= 2) {
        value |= (uint32_t)bytes[1] << 8;
    }
    if (size >= 3) {
        value |= (uint32_t)bytes[2] << 16;
    }
    if (size >= 4) {
        value |= (uint32_t)bytes[3] << 24;
    }
    return true;
}

bool CheatEngineWindow::WriteValue(GuestAddressSpace space, uint32_t address,
                                   size_t size, uint32_t value)
{
    uint8_t bytes[4] = {
        (uint8_t)(value & 0xFF),
        (uint8_t)((value >> 8) & 0xFF),
        (uint8_t)((value >> 16) & 0xFF),
        (uint8_t)((value >> 24) & 0xFF),
    };
    return size > 0 && size <= sizeof(bytes) &&
           WriteGuest(space, address, bytes, size);
}

uint32_t CheatEngineWindow::Type9Count(const RawCode &code)
{
    return code.command & 0x00FFFFFFu;
}

size_t CheatEngineWindow::LogicalSpan(const std::vector<RawCode> &codes,
                                      size_t index) const
{
    if (index >= codes.size()) {
        return 0;
    }

    const RawCode &code = codes[index];
    uint32_t type = code.command >> 28;

    if (type == 0x9) {
        uint32_t count = Type9Count(code);
        if (count == 0) {
            return codes.size() - index;
        }
        size_t available = codes.size() - index - 1;
        return 1 + std::min<size_t>(count, available);
    }

    // D/E own their guarded raw lines for experimental logical skipping.
    if (type == 0xD || type == 0xE) {
        uint32_t count = (code.value >> 24) & 0xFF;
        size_t available = codes.size() - index - 1;
        return 1 + std::min<size_t>(count, available);
    }

    if (type == 0x3) {
        uint32_t subtype = (code.command >> 20) & 0xF;
        return (subtype == 0x4 || subtype == 0x5)
                   ? std::min<size_t>(2, codes.size() - index)
                   : 1;
    }

    if (type == 0x4 || type == 0x5) {
        // Types 4/5 own one continuation RAW line.
        return std::min<size_t>(2, codes.size() - index);
    }

    if (type == 0xA) {
        // xemu type A owns enough continuation RAW lines to supply Z bytes.
        // Each continuation contains exactly eight literal bytes.
        const uint32_t byte_count = code.value;
        if (byte_count == 0) {
            return 1;
        }
        const size_t data_lines = (size_t)(byte_count / 8u) +
                                  (byte_count % 8u != 0 ? 1u : 0u);
        const size_t span = 1u + data_lines;
        return std::min(span, codes.size() - index);
    }

    if (type == 0xF) {
        /* F0/F1 bodies are captured inside the header RawCode through their
         * DEADCODE directive, so the complete cave is one logical command. */
        return 1;
    }

    if (type == 0x6) {
        if (index + 1 >= codes.size()) {
            return 1;
        }
        uint32_t offsets = codes[index + 1].command & 0xFFFFu;
        if (offsets == 0) {
            offsets = 1;
        }
        // Header + descriptor/first-offset line + two offsets per extra line.
        size_t span = 2 + (size_t)(offsets / 2);
        return std::min(span, codes.size() - index);
    }

    // 0/1/2/7 are one-line commands.
    return 1;
}

size_t CheatEngineWindow::SkipLogicalCommands(const std::vector<RawCode> &codes,
                                              size_t start,
                                              uint32_t count) const
{
    size_t pos = start;
    for (uint32_t n = 0; n < count && pos < codes.size(); ++n) {
        size_t span = LogicalSpan(codes, pos);
        pos += std::max<size_t>(span, 1);
    }
    return std::min(pos, codes.size());
}

bool CheatEngineWindow::ExecuteBasicWrite(const RawCode &code,
                                         GuestAddressSpace active_space,
                                         uint32_t active_base)
{
    const uint32_t type = code.command >> 28;
    const size_t size = type == 0 ? 1 : (type == 1 ? 2 : 4);
    const uint32_t address = active_base + (code.command & 0x0FFFFFFFu);
    return WriteValue(active_space, address, size, code.value);
}

bool CheatEngineWindow::ExecuteArithmetic(const CheatBlock &block, size_t index,
                                          GuestAddressSpace active_space,
                                          uint32_t active_base,
                                          size_t &next_index)
{
    const RawCode &code = block.codes[index];
    // CodeBreaker-style arithmetic:
    //   30T0VVVV AAAAAAAA
    // T=0/1 byte +/-; T=2/3 halfword +/-; T=4/5 word +/-
    // with the 32-bit amount in word 1 of the following raw line.
    const uint32_t subtype = (code.command >> 20) & 0xF;
    const uint32_t address = active_base + (code.value & 0x0FFFFFFFu);
    size_t size = 0;
    bool subtract = false;
    uint32_t amount = 0;

    switch (subtype) {
    case 0x0:
        size = 1;
        amount = code.command & 0xFFu;
        break;
    case 0x1:
        size = 1;
        subtract = true;
        amount = code.command & 0xFFu;
        break;
    case 0x2:
        size = 2;
        amount = code.command & 0xFFFFu;
        break;
    case 0x3:
        size = 2;
        subtract = true;
        amount = code.command & 0xFFFFu;
        break;
    case 0x4:
    case 0x5:
        if (index + 1 >= block.codes.size()) {
            m_last_runtime_message =
                "Truncated 32-bit type-3 code on source line " +
                std::to_string(code.source_line);
            return false;
        }
        size = 4;
        subtract = subtype == 0x5;
        amount = block.codes[index + 1].command;
        next_index = index + 2;
        break;
    default:
        m_last_runtime_message =
            "Unsupported type-3 subtype on source line " +
            std::to_string(code.source_line);
        return false;
    }

    uint32_t current = 0;
    if (!ReadValue(active_space, address, size, current)) {
        return false;
    }
    const uint32_t mask = size == 1 ? 0xFFu :
                          (size == 2 ? 0xFFFFu : 0xFFFFFFFFu);
    const uint32_t result = subtract ? current - amount : current + amount;
    return WriteValue(active_space, address, size, result & mask);
}

bool CheatEngineWindow::ExecuteSerial(const CheatBlock &block, size_t index,
                                      GuestAddressSpace active_space,
                                      uint32_t active_base,
                                      size_t &next_index)
{
    const RawCode &code = block.codes[index];
    // 4AAAAAAA NNNNSSSS
    // VVVVVVVV IIIIIIII
    if (index + 1 >= block.codes.size()) {
        m_last_runtime_message =
            "Truncated type-4 serial code on source line " +
            std::to_string(code.source_line);
        return false;
    }

    const uint32_t count = (code.value >> 16) & 0xFFFFu;
    const uint32_t step_words = code.value & 0xFFFFu;
    uint32_t address = active_base + (code.command & 0x0FFFFFFFu);
    uint32_t serial_value = block.codes[index + 1].command;
    const uint32_t increment = block.codes[index + 1].value;
    next_index = index + 2;

    for (uint32_t n = 0; n < count; ++n) {
        if (!WriteValue(active_space, address, 4, serial_value)) {
            return false;
        }
        address += step_words * 4u;
        serial_value += increment;
    }
    return true;
}

bool CheatEngineWindow::ExecuteCopy(const CheatBlock &block, size_t index,
                                    GuestAddressSpace active_space,
                                    uint32_t active_base,
                                    size_t &next_index)
{
    const RawCode &code = block.codes[index];
    // 5AAAAAAA NNNNNNNN
    // DDDDDDDD ????????
    // Both source and destination use the active type-9 context.
    if (index + 1 >= block.codes.size()) {
        m_last_runtime_message =
            "Truncated type-5 copy code on source line " +
            std::to_string(code.source_line);
        return false;
    }

    if (code.value > 16u * 1024u * 1024u) {
        m_last_runtime_message =
            "Type-5 copy exceeds 16 MiB safety limit on source line " +
            std::to_string(code.source_line);
        next_index = index + 2;
        return false;
    }

    const uint32_t src = active_base + (code.command & 0x0FFFFFFFu);
    const uint32_t dst = active_base +
                         (block.codes[index + 1].command & 0x0FFFFFFFu);
    const size_t length = (size_t)code.value;
    std::vector<uint8_t> data(length);
    next_index = index + 2;
    if (length == 0) {
        return true;
    }
    return ReadGuest(active_space, src, data.data(), length) &&
           WriteGuest(active_space, dst, data.data(), length);
}

bool CheatEngineWindow::ExecutePointer(const CheatBlock &block, size_t index,
                                       GuestAddressSpace active_space,
                                       uint32_t active_base,
                                       size_t &next_index)
{
    const RawCode &code = block.codes[index];
    // 6AAAAAAA VVVVVVVV
    // 000TNNNN OOOOOOOO
    // OOOOOOOO OOOOOOOO ...
    // Type-9 selects physical/virtual addressing for the base and every
    // intermediate pointer. Full 32-bit Xbox pointers are kept.
    if (index + 1 >= block.codes.size()) {
        m_last_runtime_message =
            "Truncated type-6 pointer code on source line " +
            std::to_string(code.source_line);
        return false;
    }

    const RawCode &desc = block.codes[index + 1];
    const uint32_t write_type = (desc.command >> 16) & 0xFu;
    uint32_t offset_count = desc.command & 0xFFFFu;
    if (offset_count == 0) {
        offset_count = 1;
    }
    const size_t total_lines = 2 + (size_t)(offset_count / 2);
    next_index = std::min(block.codes.size(), index + total_lines);

    if (write_type > 2) {
        m_last_runtime_message =
            "Unsupported type-6 write size on source line " +
            std::to_string(code.source_line);
        return false;
    }
    if (offset_count > 256) {
        m_last_runtime_message =
            "Type-6 pointer has more than 256 offsets on source line " +
            std::to_string(code.source_line);
        return false;
    }
    if (index + total_lines > block.codes.size()) {
        m_last_runtime_message =
            "Truncated type-6 pointer offsets on source line " +
            std::to_string(code.source_line);
        return false;
    }

    std::vector<uint32_t> offsets;
    offsets.reserve(offset_count);
    offsets.push_back(desc.value);
    size_t p = index + 2;
    while (offsets.size() < offset_count) {
        offsets.push_back(block.codes[p].command);
        if (offsets.size() < offset_count) {
            offsets.push_back(block.codes[p].value);
        }
        ++p;
    }

    uint32_t ptr = 0;
    const uint32_t base_address = active_base +
                                  (code.command & 0x0FFFFFFFu);
    if (!ReadValue(active_space, base_address, 4, ptr)) {
        return false;
    }

    uint32_t target = 0;
    for (size_t n = 0; n < offsets.size(); ++n) {
        if (ptr == 0) {
            return false;
        }
        target = ptr + offsets[n];
        if (n + 1 < offsets.size() &&
            !ReadValue(active_space, target, 4, ptr)) {
            return false;
        }
    }

    const size_t size = write_type == 0 ? 1 :
                        (write_type == 1 ? 2 : 4);
    return WriteValue(active_space, target, size, code.value);
}

bool CheatEngineWindow::ExecuteBitwise(const RawCode &code,
                                       GuestAddressSpace active_space,
                                       uint32_t active_base)
{
    // 7AAAAAAA 00T0VVVV
    const uint32_t op = (code.value >> 20) & 0xFu;
    const uint32_t address = active_base + (code.command & 0x0FFFFFFFu);
    const size_t size = (op & 1u) ? 2 : 1;
    const uint32_t rhs = size == 1 ? (code.value & 0xFFu)
                                   : (code.value & 0xFFFFu);

    if (op > 5) {
        m_last_runtime_message =
            "Unsupported type-7 operation on source line " +
            std::to_string(code.source_line);
        return false;
    }

    uint32_t current = 0;
    if (!ReadValue(active_space, address, size, current)) {
        return false;
    }

    uint32_t result = 0;
    switch (op) {
    case 0:
    case 1:
        result = current | rhs;
        break;
    case 2:
    case 3:
        result = current & rhs;
        break;
    case 4:
    case 5:
        result = current ^ rhs;
        break;
    }
    return WriteValue(active_space, address, size, result);
}

bool CheatEngineWindow::PrepareAddressContext(
    const RawCode &code, const std::vector<AddressContext> &contexts,
    uint32_t active_base, AddressContext &next_context, bool &push_context)
{
    const uint32_t mode = (code.command >> 24) & 0xF;
    const uint32_t count = Type9Count(code);
    const uint32_t operand = code.value;
    const uint32_t source = active_base + operand;

    next_context.remaining = count;
    next_context.until_end = count == 0;

    switch (mode) {
    case 0x0: // virtual direct base
        next_context.space = GuestAddressSpace::Virtual;
        next_context.base = contexts.empty() ? operand : source;
        push_context = true;
        return true;
    case 0x1: // physical direct base
        next_context.space = GuestAddressSpace::Physical;
        next_context.base = contexts.empty() ? operand : source;
        push_context = true;
        return true;
    case 0x2: { // virtual pointer base
        uint32_t ptr = 0;
        next_context.space = GuestAddressSpace::Virtual;
        if (!ReadValue(GuestAddressSpace::Virtual,
                       contexts.empty() ? operand : source, 4, ptr)) {
            return false;
        }
        next_context.base = ptr;
        push_context = true;
        return true;
    }
    case 0x3: { // physical pointer base
        uint32_t ptr = 0;
        next_context.space = GuestAddressSpace::Physical;
        if (!ReadValue(GuestAddressSpace::Physical,
                       contexts.empty() ? operand : source, 4, ptr)) {
            return false;
        }
        next_context.base = ptr;
        push_context = true;
        return true;
    }
    default:
        m_last_runtime_message =
            "Unsupported type-9 mode on source line " +
            std::to_string(code.source_line);
        return false;
    }
}

bool CheatEngineWindow::ExecuteRawBytes(const CheatBlock &block, size_t index,
                                        GuestAddressSpace active_space,
                                        uint32_t active_base,
                                        size_t &next_index)
{
    const RawCode &code = block.codes[index];
    // xemu variable-length raw-byte fill/write:
    //   AXXXXXXX ZZZZZZZZ
    //   DDDDDDDD DDDDDDDD  <- 8 literal bytes
    const uint32_t byte_count = code.value;
    const size_t data_lines =
        byte_count == 0 ? 0 :
        (size_t)(byte_count / 8u) + (byte_count % 8u != 0 ? 1u : 0u);
    const size_t total_lines = 1u + data_lines;
    next_index = std::min(block.codes.size(), index + total_lines);

    if (byte_count == 0) {
        m_last_runtime_message =
            "Type-A byte count must be greater than zero on source line " +
            std::to_string(code.source_line);
        return false;
    }
    if (data_lines > block.codes.size() - index - 1u) {
        m_last_runtime_message =
            "Truncated type-A raw-byte write on source line " +
            std::to_string(code.source_line) + " (needs " +
            std::to_string(data_lines) + " continuation line" +
            (data_lines == 1 ? "" : "s") + ")";
        return false;
    }

    const uint32_t address = active_base + (code.command & 0x0FFFFFFFu);
    size_t bytes_remaining = (size_t)byte_count;
    uint32_t write_address = address;

    for (size_t line_index = 0; line_index < data_lines; ++line_index) {
        const RawCode &data = block.codes[index + 1u + line_index];
        const uint8_t bytes[8] = {
            (uint8_t)((data.command >> 24) & 0xFFu),
            (uint8_t)((data.command >> 16) & 0xFFu),
            (uint8_t)((data.command >> 8) & 0xFFu),
            (uint8_t)(data.command & 0xFFu),
            (uint8_t)((data.value >> 24) & 0xFFu),
            (uint8_t)((data.value >> 16) & 0xFFu),
            (uint8_t)((data.value >> 8) & 0xFFu),
            (uint8_t)(data.value & 0xFFu),
        };
        const size_t chunk = std::min<size_t>(bytes_remaining, 8u);
        if (!WriteGuest(active_space, write_address, bytes, chunk)) {
            return false;
        }
        write_address += (uint32_t)chunk;
        bytes_remaining -= chunk;
    }
    return true;
}

bool CheatEngineWindow::ExecuteTypeF(
    size_t block_index, size_t code_index, const RawCode &code,
    GuestAddressSpace active_space, uint32_t active_base,
    std::vector<uint64_t> &active_hooks)
{
    const uint32_t subtype = (code.command >> 24) & 0xFu;
    const uint64_t hook_key = ((uint64_t)block_index << 32) |
                              (uint32_t)code_index;

    const uint32_t flags = code.command & 0x00FFFFFFu;
    if (flags != 0) {
        m_last_runtime_message =
            "Type-F low 24 bits are reserved and must be 000000 on source line " +
            std::to_string(code.source_line);
        return false;
    }
    if (subtype != 0x0 && subtype != 0x1) {
        m_last_runtime_message =
            "Unsupported Type-F subtype on source line " +
            std::to_string(code.source_line);
        return false;
    }
    if (!code.f_terminated) {
        m_last_runtime_message =
            "Type-F block is missing DEADCODE (source line " +
            std::to_string(code.source_line) + ")";
        return false;
    }
    if (active_space != GuestAddressSpace::Virtual) {
        m_last_runtime_message =
            "Type-F hooks require a Virtual address context on source line " +
            std::to_string(code.source_line);
        return false;
    }

    const uint32_t hook_address = active_base + code.value;

    if (!code.f_precompiled || !code.f_precompile_ok) {
        if (subtype == 0x0) {
            m_last_runtime_message = "Type-F0 assembler error";
            if (code.f_precompile_error_line > 0) {
                m_last_runtime_message += " on source line " +
                    std::to_string(code.f_precompile_error_line);
            }
        } else {
            m_last_runtime_message =
                "Type-F1 raw-hex error on source line " +
                std::to_string(code.f_precompile_error_line > 0
                                   ? code.f_precompile_error_line
                                   : code.source_line);
        }
        if (!code.f_precompile_error.empty()) {
            m_last_runtime_message += ": " + code.f_precompile_error;
        }
        return false;
    }

    const std::vector<XemuCheatAsmLine> *f0_source =
        subtype == 0x0 ? &code.f_body : nullptr;

    /* The parse-time probe/signature is immutable for this RawCode.  Normal
     * 10 Hz ticks now reach InstallFHook's installed/signature fast path
     * without rebuilding strings, reassembling F0, or reparsing F1 bytes. */
    if (!InstallFHook(block_index, hook_key, hook_address,
                      code.f_probe_code, code.f_probe_data, f0_source,
                      code.f_uses_preserve, code.f_preserve_bytes,
                      code.f_uses_temp, code.f_temp_bytes,
                      code.f_definition_signature)) {
        if (m_last_runtime_message.empty()) {
            m_last_runtime_message =
                "Type-F hook installation failed on source line " +
                std::to_string(code.source_line);
        }
        return false;
    }

    active_hooks.push_back(hook_key);
    return true;
}

bool CheatEngineWindow::ExecuteConditional(
    size_t block_index, const CheatBlock &block, size_t index,
    const RawCode &code, const std::vector<AddressContext> &contexts,
    GuestAddressSpace active_space, uint32_t active_base,
    size_t &next_index)
{
    const uint32_t type = code.command >> 28;
    const uint32_t n = (code.value >> 24) & 0xFF;
    const uint32_t test = (code.value >> 20) & 0x7;
    const uint32_t field = (code.value >> 16) & 0xF;
    const bool is_8bit = (field & 0x2) != 0;
    const bool requested_virtual = (field & 0x1) != 0;
    const uint32_t compare_value = code.value & 0xFFFF;

    GuestAddressSpace compare_space;
    uint32_t compare_address;
    if (!contexts.empty()) {
        compare_space = active_space;
        compare_address = active_base + (code.command & 0x0FFFFFFFu);
    } else {
        compare_space = requested_virtual ? GuestAddressSpace::Virtual
                                          : GuestAddressSpace::Physical;
        compare_address = code.command & 0x0FFFFFFFu;
    }

    uint32_t memory_value = 0;
    const bool runtime_ok = ReadValue(compare_space, compare_address,
                                      is_8bit ? 1 : 2, memory_value);

    bool condition = false;
    if (runtime_ok) {
        const uint32_t rhs = is_8bit ? (compare_value & 0xFF) : compare_value;
        switch (test) {
        case 0: condition = memory_value == rhs; break;
        case 1: condition = memory_value != rhs; break;
        case 2: condition = memory_value < rhs; break;
        case 3: condition = memory_value > rhs; break;
        case 4: condition = (memory_value & rhs) == 0; break;
        case 5: condition = (memory_value & rhs) != 0; break;
        case 6: condition = (memory_value | rhs) == 0; break;
        case 7: condition = (memory_value | rhs) != 0; break;
        }
    }

    bool guard_active = condition;
    if (type == 0xE) {
        const uint64_t key = ((uint64_t)block_index << 32) | (uint32_t)index;
        SwitchState &state = m_switches[key];
        if (runtime_ok && condition && !state.previous_condition) {
            state.on = !state.on;
        }
        state.previous_condition = runtime_ok && condition;
        guard_active = state.on;
    }

    if (!runtime_ok) {
        guard_active = false;
    }

    if (!guard_active && n > 0) {
        if (m_code_aware_skip) {
            next_index = SkipLogicalCommands(block.codes, index + 1, n);
        } else {
            next_index = std::min(block.codes.size(),
                                  index + 1 + (size_t)n);
        }
    }
    return runtime_ok;
}

void CheatEngineWindow::ExecuteBlock(size_t block_index, CheatBlock &block)
{
    m_address_context_scratch.clear();
    std::vector<AddressContext> &contexts = m_address_context_scratch;
    m_active_f_hooks_scratch.clear();
    std::vector<uint64_t> &f_active_hooks = m_active_f_hooks_scratch;

    auto consume_one_physical_line = [&]() {
        for (auto &ctx : contexts) {
            if (!ctx.until_end && ctx.remaining > 0) {
                --ctx.remaining;
            }
        }
        contexts.erase(std::remove_if(contexts.begin(), contexts.end(),
                                      [](const AddressContext &ctx) {
                                          return !ctx.until_end &&
                                                 ctx.remaining == 0;
                                      }),
                       contexts.end());
    };

    auto consume_skipped_range = [&](size_t from, size_t to) {
        for (size_t p = from; p < to; ++p) {
            consume_one_physical_line();
        }
    };

    size_t i = 0;
    while (i < block.codes.size()) {
        const RawCode &code = block.codes[i];
        const uint32_t type = code.command >> 28;
        const size_t contexts_before = contexts.size();

        GuestAddressSpace active_space = GuestAddressSpace::Virtual;
        uint32_t active_base = 0;
        if (!contexts.empty()) {
            active_space = contexts.back().space;
            active_base = contexts.back().base;
        }

        bool runtime_ok = true;
        bool push_context = false;
        AddressContext next_context;
        size_t next_i = i + 1;

        if (type <= 0x2) {
            runtime_ok = ExecuteBasicWrite(code, active_space, active_base);
        } else if (type == 0x3) {
            runtime_ok = ExecuteArithmetic(block, i, active_space, active_base,
                                           next_i);
        } else if (type == 0x4) {
            runtime_ok = ExecuteSerial(block, i, active_space, active_base,
                                       next_i);
        } else if (type == 0x5) {
            runtime_ok = ExecuteCopy(block, i, active_space, active_base,
                                     next_i);
        } else if (type == 0x6) {
            runtime_ok = ExecutePointer(block, i, active_space, active_base,
                                        next_i);
        } else if (type == 0x7) {
            runtime_ok = ExecuteBitwise(code, active_space, active_base);
        } else if (type == 0x9) {
            runtime_ok = PrepareAddressContext(code, contexts, active_base,
                                               next_context, push_context);
        } else if (type == 0xA) {
            runtime_ok = ExecuteRawBytes(block, i, active_space, active_base,
                                         next_i);
        } else if (type == 0xF) {
            runtime_ok = ExecuteTypeF(block_index, i, code, active_space,
                                      active_base, f_active_hooks);
        } else if (type == 0xD || type == 0xE) {
            runtime_ok = ExecuteConditional(block_index, block, i, code,
                                            contexts, active_space, active_base,
                                            next_i);
        } else {
            // Types 8/B/C are CodeBreaker setup/master-engine families.
            // They are intentionally not mapped to Xbox memory operations yet.
            runtime_ok = false;
            char msg[192];
            std::snprintf(msg, sizeof(msg),
                          "Unsupported RAW type %X on source line %d",
                          type, code.source_line);
            m_last_runtime_message = msg;
        }

        // The current physical line consumes any contexts that were active
        // before it. A type-9 context starts *after* its own header line.
        for (size_t c = 0; c < contexts_before && c < contexts.size(); ++c) {
            if (!contexts[c].until_end && contexts[c].remaining > 0) {
                --contexts[c].remaining;
            }
        }
        contexts.erase(std::remove_if(contexts.begin(), contexts.end(),
                                      [](const AddressContext &ctx) {
                                          return !ctx.until_end &&
                                                 ctx.remaining == 0;
                                      }),
                       contexts.end());

        if (push_context) {
            contexts.push_back(next_context);
        }

        if (!runtime_ok && m_last_runtime_message.empty()) {
            m_last_runtime_message = "Memory access failed on source line " +
                                     std::to_string(code.source_line);
        }

        if (next_i > i + 1) {
            // Continuation lines consumed by 3/4/5/6/A, or lines skipped by D/E,
            // still consume outer type-9 raw-line scopes. They are data/skip
            // lines here, so a type-9-looking continuation is never activated.
            consume_skipped_range(i + 1, next_i);
        }

        i = next_i;
    }

    /* F hooks are persistent machine-code patches, unlike ordinary periodic
     * writes. If an F0/F1 command was skipped by D/E this tick, restore its
     * original bytes so conditions control hooks predictably. */
    for (auto &entry : m_f_hooks) {
        if (entry.second.owner_block == block_index && entry.second.installed &&
            !std::binary_search(f_active_hooks.begin(), f_active_hooks.end(),
                                entry.first)) {
            DeactivateFHook(entry.first);
        }
    }
}

// UI/frontend methods are owned by cheat-engine-ui.cc.

