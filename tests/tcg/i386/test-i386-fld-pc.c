// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Test x87 precision control for loads and arithmetic.
 */

#include <stdint.h>
#include <stdio.h>

union x87_value {
    struct {
        uint64_t significand;
        uint16_t sign_exp;
    };
    long double value;
};

static const double load_input = 314159265.35897934;
static const double add_input = 16777216.0;
static const double one = 1.0;

int main(void)
{
    union x87_value load_result;
    union x87_value add_result;
    uint16_t old_cw;
    uint16_t single_cw;
    int ret = 0;

    __asm__ volatile("fnstcw %0" : "=m"(old_cw));
    single_cw = old_cw & ~0xf00;
    __asm__ volatile("fldcw %0" : : "m"(single_cw));

    __asm__ volatile("fldl %1; fstpt %0"
                     : "=m"(load_result.value)
                     : "m"(load_input)
                     : "st");
    __asm__ volatile("fldl %1; faddl %2; fstpt %0"
                     : "=m"(add_result.value)
                     : "m"(add_input), "m"(one)
                     : "st");

    __asm__ volatile("fldcw %0" : : "m"(old_cw));

    if (load_result.significand != UINT64_C(0x95cd850adf309000) ||
        load_result.sign_exp != 0x401b) {
        printf("FAIL: FLD rounded by precision control\n");
        ret = 1;
    }
    if (add_result.significand != UINT64_C(0x8000000000000000) ||
        add_result.sign_exp != 0x4017) {
        printf("FAIL: FADD ignored precision control\n");
        ret = 1;
    }

    return ret;
}
