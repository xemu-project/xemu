/*
 * Semihosting Console Test
 *
 * Copyright (c) 2019 Linaro Ltd
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdint.h>
#include <minilib.h>

#define SYS_READC 0x7

uintptr_t __semi_call(uintptr_t type, uintptr_t arg0)
{
    register uintptr_t t asm("x0") = type;
    register uintptr_t a0 asm("x1") = arg0;
    asm("hlt 0xf000"
        : "=r" (t)
        : "r" (t), "r" (a0));

    return t;
}

int main(void)
{
    char c;

    ml_printf("Semihosting Console Test\n");
    ml_printf("hit X to exit:");

    do {
        c = __semi_call(SYS_READC, 0);
        __sys_outc(c);
    } while (c != 'X');

    return 0;
}
