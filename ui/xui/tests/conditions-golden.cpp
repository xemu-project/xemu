// v2.87 current regression ownership.
#include "breakpoint-conditions.hh"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

int main()
{
    std::vector<XemuBreakpointCondition> conditions;
    std::string error;

    assert(xemu_breakpoint_conditions_parse(
        "EAX == 12345678\nECX != 00000000\nESI >= 00001000\n",
        conditions, error));
    assert(error.empty());
    assert(conditions.size() == 3);

    XemuCheatX86Registers regs = {};
    regs.eax = 0x12345678u;
    regs.ecx = 1u;
    regs.esi = 0x1000u;
    assert(xemu_breakpoint_conditions_evaluate(conditions, regs));
    regs.ecx = 0;
    assert(!xemu_breakpoint_conditions_evaluate(conditions, regs));

    assert(xemu_breakpoint_conditions_parse(
        "eax==0x12345678\ncr3 < FFFFFFFF\n",
        conditions, error));
    regs.eax = 0x12345678u;
    regs.cr3 = 0x00123000u;
    assert(xemu_breakpoint_conditions_evaluate(conditions, regs));

    assert(xemu_breakpoint_conditions_parse(
        "EAX <= 10\nEBX > 0F\nEDX >= 20\nEDI < 30\n",
        conditions, error));
    regs.eax = 0x10u;
    regs.ebx = 0x10u;
    regs.edx = 0x20u;
    regs.edi = 0x2Fu;
    assert(xemu_breakpoint_conditions_evaluate(conditions, regs));
    regs.edi = 0x30u;
    assert(!xemu_breakpoint_conditions_evaluate(conditions, regs));

    assert(xemu_breakpoint_conditions_parse("\n\n", conditions, error));
    assert(conditions.empty());
    assert(xemu_breakpoint_conditions_evaluate(conditions, regs));

    assert(!xemu_breakpoint_conditions_parse("ABC == 1", conditions, error));
    assert(error.find("Unknown register") != std::string::npos);
    assert(!xemu_breakpoint_conditions_parse("EAX === 1", conditions, error));
    assert(error.find("Invalid operator") != std::string::npos);
    assert(!xemu_breakpoint_conditions_parse("EAX ~~ 1", conditions, error));
    assert(error.find("Invalid operator") != std::string::npos);
    assert(!xemu_breakpoint_conditions_parse("EAX == 100000000", conditions, error));
    assert(error.find("Invalid hexadecimal value") != std::string::npos);

    std::cout << "PASS: breakpoint condition parser/evaluator\n";
    return 0;
}
