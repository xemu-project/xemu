//
// xemu RAW Cheat Engine - conditional breakpoint expressions
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

#include <cstdint>
#include <string>
#include <vector>

enum class XemuBreakpointConditionRegister {
    EAX = 0,
    EBX,
    ECX,
    EDX,
    ESI,
    EDI,
    ESP,
    EBP,
    EIP,
    PC,
    EFLAGS,
    CR0,
    CR2,
    CR3,
    CR4,
    CS,
    DS,
    ES,
    FS,
    GS,
    SS,
};

enum class XemuBreakpointConditionOperator {
    Equal = 0,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
};

struct XemuBreakpointCondition {
    XemuBreakpointConditionRegister reg = XemuBreakpointConditionRegister::EAX;
    XemuBreakpointConditionOperator op = XemuBreakpointConditionOperator::Equal;
    uint32_t value = 0;
};

/* Parse one condition per non-empty line. Values are hexadecimal; an optional
 * 0x prefix is accepted. Multiple lines are ANDed when evaluated. */
bool xemu_breakpoint_conditions_parse(
    const std::string &text,
    std::vector<XemuBreakpointCondition> &conditions,
    std::string &error);

bool xemu_breakpoint_conditions_evaluate(
    const std::vector<XemuBreakpointCondition> &conditions,
    const XemuCheatX86Registers &regs);
