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

#include "breakpoint-conditions.hh"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <sstream>
#include <utility>

namespace {

static std::string trim_copy(const std::string &text)
{
    size_t first = 0;
    while (first < text.size() &&
           std::isspace(static_cast<unsigned char>(text[first]))) {
        ++first;
    }

    size_t last = text.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(text[last - 1]))) {
        --last;
    }
    return text.substr(first, last - first);
}

static std::string uppercase_copy(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::toupper(ch));
                   });
    return text;
}

static bool parse_register(const std::string &text,
                           XemuBreakpointConditionRegister &reg)
{
    static const std::pair<const char *, XemuBreakpointConditionRegister>
        kRegisters[] = {
            {"EAX", XemuBreakpointConditionRegister::EAX},
            {"EBX", XemuBreakpointConditionRegister::EBX},
            {"ECX", XemuBreakpointConditionRegister::ECX},
            {"EDX", XemuBreakpointConditionRegister::EDX},
            {"ESI", XemuBreakpointConditionRegister::ESI},
            {"EDI", XemuBreakpointConditionRegister::EDI},
            {"ESP", XemuBreakpointConditionRegister::ESP},
            {"EBP", XemuBreakpointConditionRegister::EBP},
            {"EIP", XemuBreakpointConditionRegister::EIP},
            {"PC", XemuBreakpointConditionRegister::PC},
            {"EFLAGS", XemuBreakpointConditionRegister::EFLAGS},
            {"CR0", XemuBreakpointConditionRegister::CR0},
            {"CR2", XemuBreakpointConditionRegister::CR2},
            {"CR3", XemuBreakpointConditionRegister::CR3},
            {"CR4", XemuBreakpointConditionRegister::CR4},
            {"CS", XemuBreakpointConditionRegister::CS},
            {"DS", XemuBreakpointConditionRegister::DS},
            {"ES", XemuBreakpointConditionRegister::ES},
            {"FS", XemuBreakpointConditionRegister::FS},
            {"GS", XemuBreakpointConditionRegister::GS},
            {"SS", XemuBreakpointConditionRegister::SS},
        };

    const std::string name = uppercase_copy(trim_copy(text));
    for (const auto &entry : kRegisters) {
        if (name == entry.first) {
            reg = entry.second;
            return true;
        }
    }
    return false;
}

static bool parse_hex_u32(const std::string &text, uint32_t &value)
{
    std::string input = trim_copy(text);
    if (input.size() >= 2 && input[0] == '0' &&
        (input[1] == 'x' || input[1] == 'X')) {
        input.erase(0, 2);
    }
    if (input.empty() || input.size() > 8) {
        return false;
    }
    for (char ch : input) {
        if (!std::isxdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
    }

    errno = 0;
    char *end = nullptr;
    const unsigned long parsed = std::strtoul(input.c_str(), &end, 16);
    if (errno != 0 || end == nullptr || *end != '\0' || parsed > 0xFFFFFFFFul) {
        return false;
    }
    value = static_cast<uint32_t>(parsed);
    return true;
}

static uint32_t register_value(XemuBreakpointConditionRegister reg,
                               const XemuCheatX86Registers &r)
{
    switch (reg) {
    case XemuBreakpointConditionRegister::EAX: return r.eax;
    case XemuBreakpointConditionRegister::EBX: return r.ebx;
    case XemuBreakpointConditionRegister::ECX: return r.ecx;
    case XemuBreakpointConditionRegister::EDX: return r.edx;
    case XemuBreakpointConditionRegister::ESI: return r.esi;
    case XemuBreakpointConditionRegister::EDI: return r.edi;
    case XemuBreakpointConditionRegister::ESP: return r.esp;
    case XemuBreakpointConditionRegister::EBP: return r.ebp;
    case XemuBreakpointConditionRegister::EIP: return r.eip;
    case XemuBreakpointConditionRegister::PC: return r.pc;
    case XemuBreakpointConditionRegister::EFLAGS: return r.eflags;
    case XemuBreakpointConditionRegister::CR0: return r.cr0;
    case XemuBreakpointConditionRegister::CR2: return r.cr2;
    case XemuBreakpointConditionRegister::CR3: return r.cr3;
    case XemuBreakpointConditionRegister::CR4: return r.cr4;
    case XemuBreakpointConditionRegister::CS: return r.cs;
    case XemuBreakpointConditionRegister::DS: return r.ds;
    case XemuBreakpointConditionRegister::ES: return r.es;
    case XemuBreakpointConditionRegister::FS: return r.fs;
    case XemuBreakpointConditionRegister::GS: return r.gs;
    case XemuBreakpointConditionRegister::SS: return r.ss;
    }
    return 0;
}

} // namespace

bool xemu_breakpoint_conditions_parse(
    const std::string &text,
    std::vector<XemuBreakpointCondition> &conditions,
    std::string &error)
{
    static const struct {
        const char *text;
        XemuBreakpointConditionOperator op;
    } kOperators[] = {
        {"==", XemuBreakpointConditionOperator::Equal},
        {"!=", XemuBreakpointConditionOperator::NotEqual},
        {"<=", XemuBreakpointConditionOperator::LessEqual},
        {">=", XemuBreakpointConditionOperator::GreaterEqual},
        {"<", XemuBreakpointConditionOperator::Less},
        {">", XemuBreakpointConditionOperator::Greater},
    };

    conditions.clear();
    error.clear();

    std::istringstream input(text);
    std::string line;
    size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        line = trim_copy(line);
        if (line.empty()) {
            continue;
        }

        size_t operator_pos = std::string::npos;
        const char *operator_text = nullptr;
        XemuBreakpointConditionOperator condition_op =
            XemuBreakpointConditionOperator::Equal;
        for (const auto &candidate : kOperators) {
            const size_t pos = line.find(candidate.text);
            if (pos != std::string::npos) {
                operator_pos = pos;
                operator_text = candidate.text;
                condition_op = candidate.op;
                break;
            }
        }
        if (operator_text == nullptr) {
            error = "Line " + std::to_string(line_number) +
                    ": Invalid operator. Use ==, !=, <, <=, >, or >=.";
            conditions.clear();
            return false;
        }

        const std::string lhs = trim_copy(line.substr(0, operator_pos));
        const std::string rhs = trim_copy(
            line.substr(operator_pos + std::char_traits<char>::length(operator_text)));
        if (rhs.find_first_of("=!<>") != std::string::npos) {
            error = "Line " + std::to_string(line_number) +
                    ": Invalid operator. Use ==, !=, <, <=, >, or >=.";
            conditions.clear();
            return false;
        }
        XemuBreakpointCondition condition;
        if (!parse_register(lhs, condition.reg)) {
            error = "Line " + std::to_string(line_number) +
                    ": Unknown register \"" + lhs + "\".";
            conditions.clear();
            return false;
        }
        if (!parse_hex_u32(rhs, condition.value)) {
            error = "Line " + std::to_string(line_number) +
                    ": Invalid hexadecimal value \"" + rhs + "\".";
            conditions.clear();
            return false;
        }
        condition.op = condition_op;
        conditions.push_back(condition);
    }

    return true;
}

bool xemu_breakpoint_conditions_evaluate(
    const std::vector<XemuBreakpointCondition> &conditions,
    const XemuCheatX86Registers &regs)
{
    for (const XemuBreakpointCondition &condition : conditions) {
        const uint32_t lhs = register_value(condition.reg, regs);
        const uint32_t rhs = condition.value;
        bool matched = false;
        switch (condition.op) {
        case XemuBreakpointConditionOperator::Equal: matched = lhs == rhs; break;
        case XemuBreakpointConditionOperator::NotEqual: matched = lhs != rhs; break;
        case XemuBreakpointConditionOperator::Less: matched = lhs < rhs; break;
        case XemuBreakpointConditionOperator::LessEqual: matched = lhs <= rhs; break;
        case XemuBreakpointConditionOperator::Greater: matched = lhs > rhs; break;
        case XemuBreakpointConditionOperator::GreaterEqual: matched = lhs >= rhs; break;
        }
        if (!matched) {
            return false;
        }
    }
    return true;
}
