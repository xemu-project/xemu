// v2.87 current regression ownership.
#include "register-copy-utils.hh"

#include <cstdlib>
#include <iostream>
#include <string>

static void expect_equal(const std::string &actual, const std::string &expected)
{
    if (actual != expected) {
        std::cerr << "register clipboard text mismatch\nEXPECTED:\n"
                  << expected << "\nACTUAL:\n" << actual << "\n";
        std::exit(1);
    }
}

int main()
{
    XemuCheatX86Registers r = {};
    r.eax = 0x11111111; r.ebx = 0x22222222;
    r.ecx = 0x33333333; r.edx = 0x44444444;
    r.esi = 0x55555555; r.edi = 0x66666666;
    r.esp = 0x77777777; r.ebp = 0x88888888;
    r.eip = 0x99999999; r.pc = 0xAAAAAAAA;
    r.eflags = 0xBBBBBBBB; r.cr3 = 0xCCCCCCCC;
    r.cr0 = 0xDDDDDDDD; r.cr2 = 0xEEEEEEEE;
    r.cr4 = 0xFFFFFFFF; r.cs = 0x00000001;
    r.ds = 0x00000002; r.ss = 0x00000003;
    r.es = 0x00000004; r.fs = 0x00000005; r.gs = 0x00000006;

    XemuCheatX86ExtraRegisters x = {};
    for (unsigned i = 0; i < 8; ++i) {
        x.st_high[i] = (uint16_t)(0x1000 + i);
        x.st_low[i] = 0x1111111111111111ULL + i;
        x.mmx[i] = 0x2222222222222222ULL + i;
        x.xmm[i][0] = 0x10000000 + i;
        x.xmm[i][1] = 0x20000000 + i;
        x.xmm[i][2] = 0x30000000 + i;
        x.xmm[i][3] = 0x40000000 + i;
    }
    x.fctrl = 0x12345678;
    x.fstat = 0x23456789;
    x.fp_top = 7;
    x.fop = 0x3456789A;
    x.mxcsr = 0x456789AB;

    const std::string expected =
        "EAX\t11111111\n"
        "EBX\t22222222\n"
        "ECX\t33333333\n"
        "EDX\t44444444\n"
        "ESI\t55555555\n"
        "EDI\t66666666\n"
        "ESP\t77777777\n"
        "EBP\t88888888\n"
        "EIP\t99999999\n"
        "PC\tAAAAAAAA\n"
        "EFLAGS\tBBBBBBBB\n"
        "CR3\tCCCCCCCC\n"
        "CR0\tDDDDDDDD\n"
        "CR2\tEEEEEEEE\n"
        "CR4\tFFFFFFFF\n"
        "CS\t00000001\n"
        "DS\t00000002\n"
        "SS\t00000003\n"
        "ES\t00000004\n"
        "FS\t00000005\n"
        "GS\t00000006\n"
        "ST0\t10001111111111111111\n"
        "ST1\t10011111111111111112\n"
        "ST2\t10021111111111111113\n"
        "ST3\t10031111111111111114\n"
        "ST4\t10041111111111111115\n"
        "ST5\t10051111111111111116\n"
        "ST6\t10061111111111111117\n"
        "ST7\t10071111111111111118\n"
        "FCTRL\t12345678\n"
        "FSTAT\t23456789\n"
        "TOP\t7\n"
        "FOP\t3456789A\n"
        "MM0\t2222222222222222\n"
        "MM1\t2222222222222223\n"
        "MM2\t2222222222222224\n"
        "MM3\t2222222222222225\n"
        "MM4\t2222222222222226\n"
        "MM5\t2222222222222227\n"
        "MM6\t2222222222222228\n"
        "MM7\t2222222222222229\n"
        "XMM0\t40000000 30000000 20000000 10000000\n"
        "XMM1\t40000001 30000001 20000001 10000001\n"
        "XMM2\t40000002 30000002 20000002 10000002\n"
        "XMM3\t40000003 30000003 20000003 10000003\n"
        "XMM4\t40000004 30000004 20000004 10000004\n"
        "XMM5\t40000005 30000005 20000005 10000005\n"
        "XMM6\t40000006 30000006 20000006 10000006\n"
        "XMM7\t40000007 30000007 20000007 10000007\n"
        "MXCSR\t456789AB";

    expect_equal(xemu_register_copy::BuildAllCurrentRegistersText(r, x), expected);
    std::cout << "PASS: current-register clipboard formatting\n";
    return 0;
}
