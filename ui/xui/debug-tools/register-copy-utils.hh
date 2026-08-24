//
// xemu RAW Cheat Engine - Current-register clipboard formatting
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#pragma once

#include "cheat-engine-memory.h"

#include <cstdio>
#include <string>

namespace xemu_register_copy {

inline std::string BuildAllCurrentRegistersText(
    const XemuCheatX86Registers &regs,
    const XemuCheatX86ExtraRegisters &extra)
{
    std::string text;
    text.reserve(1536);

    auto append_u32 = [&](const char *name, uint32_t value) {
        char line[64];
        std::snprintf(line, sizeof(line), "%s\t%08X\n", name, value);
        text += line;
    };
    auto append_u64 = [&](const char *name, uint64_t value) {
        char line[72];
        std::snprintf(line, sizeof(line), "%s\t%016llX\n", name,
                      (unsigned long long)value);
        text += line;
    };

    append_u32("EAX", regs.eax);
    append_u32("EBX", regs.ebx);
    append_u32("ECX", regs.ecx);
    append_u32("EDX", regs.edx);
    append_u32("ESI", regs.esi);
    append_u32("EDI", regs.edi);
    append_u32("ESP", regs.esp);
    append_u32("EBP", regs.ebp);
    append_u32("EIP", regs.eip);
    append_u32("PC", regs.pc);
    append_u32("EFLAGS", regs.eflags);
    append_u32("CR3", regs.cr3);
    append_u32("CR0", regs.cr0);
    append_u32("CR2", regs.cr2);
    append_u32("CR4", regs.cr4);
    append_u32("CS", regs.cs);
    append_u32("DS", regs.ds);
    append_u32("SS", regs.ss);
    append_u32("ES", regs.es);
    append_u32("FS", regs.fs);
    append_u32("GS", regs.gs);

    for (unsigned i = 0; i < 8; ++i) {
        char name[8];
        char line[72];
        std::snprintf(name, sizeof(name), "ST%u", i);
        std::snprintf(line, sizeof(line), "%s\t%04X%016llX\n", name,
                      (unsigned)extra.st_high[i],
                      (unsigned long long)extra.st_low[i]);
        text += line;
    }
    append_u32("FCTRL", extra.fctrl);
    append_u32("FSTAT", extra.fstat);
    {
        char line[64];
        std::snprintf(line, sizeof(line), "TOP\t%u\n", (unsigned)extra.fp_top);
        text += line;
    }
    append_u32("FOP", extra.fop);

    for (unsigned i = 0; i < 8; ++i) {
        char name[8];
        std::snprintf(name, sizeof(name), "MM%u", i);
        append_u64(name, extra.mmx[i]);
    }

    for (unsigned i = 0; i < 8; ++i) {
        char line[96];
        std::snprintf(line, sizeof(line),
                      "XMM%u\t%08X %08X %08X %08X\n", i,
                      extra.xmm[i][3], extra.xmm[i][2],
                      extra.xmm[i][1], extra.xmm[i][0]);
        text += line;
    }
    append_u32("MXCSR", extra.mxcsr);

    if (!text.empty()) {
        text.pop_back(); // No trailing newline in clipboard payload.
    }
    return text;
}

} // namespace xemu_register_copy
