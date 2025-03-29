/*
 * MCPX DSP emulator
 *
 * Copyright (c) 2015 espes
 * Copyright (c) 2020-2025 Matt Borgerson
 *
 * Adapted from Hatari DSP M56001 emulation
 * (C) 2001-2008 ARAnyM developer team
 * Adaption to Hatari (C) 2008 by Thomas Huth
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "qemu/osdep.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include "dsp_cpu.h"
#include "dsp_dma.h"
#include "dsp_state.h"

#include "dsp.h"
#include "debug.h"

#if DEBUG_DSP
#define BITMASK(x)  ((1<<(x))-1)

/**
 * Output memory values between given addresses in given DSP address space.
 * Return next DSP address value.
 */
uint32_t dsp_disasm_memory(DSPState* dsp, uint32_t dsp_memdump_addr, uint32_t dsp_memdump_upper, char space)
{
    uint32_t mem, value;

    for (mem = dsp_memdump_addr; mem <= dsp_memdump_upper; mem++) {
        value = dsp_read_memory(dsp, space, mem);
        printf("%04x  %06x\n", mem, value);
    }
    return dsp_memdump_upper+1;
}

/**
 * Show information on DSP core state which isn't
 * shown by any of the other commands (dd, dm, dr).
 */
void dsp_info(DSPState* dsp)
{
    int i, j;
    const char *stackname[] = { "SSH", "SSL" };

    printf("DSP core information:\n");

    for (i = 0; i < ARRAY_SIZE(stackname); i++) {
        printf("- %s stack:", stackname[i]);
        for (j = 0; j < ARRAY_SIZE(dsp->core.stack[0]); j++) {
            printf(" %04x", dsp->core.stack[i][j]);
        }
        printf("\n");
    }

    printf("- Interrupt IPL:");
    for (i = 0; i < ARRAY_SIZE(dsp->core.interrupt_ipl); i++) {
        printf(" %04x", dsp->core.interrupt_ipl[i]);
    }
    printf("\n");

    printf("- Pending ints: ");
    for (i = 0; i < ARRAY_SIZE(dsp->core.interrupt_is_pending); i++) {
        printf(" %04hx", dsp->core.interrupt_is_pending[i]);
    }
    printf("\n");
}

/**
 * Show DSP register contents
 */
void dsp_print_registers(DSPState* dsp)
{
    uint32_t i;

    printf("A: A2: %02x  A1: %06x  A0: %06x\n",
        dsp->core.registers[DSP_REG_A2], dsp->core.registers[DSP_REG_A1], dsp->core.registers[DSP_REG_A0]);
    printf("B: B2: %02x  B1: %06x  B0: %06x\n",
        dsp->core.registers[DSP_REG_B2], dsp->core.registers[DSP_REG_B1], dsp->core.registers[DSP_REG_B0]);

    printf("X: X1: %06x  X0: %06x\n", dsp->core.registers[DSP_REG_X1], dsp->core.registers[DSP_REG_X0]);
    printf("Y: Y1: %06x  Y0: %06x\n", dsp->core.registers[DSP_REG_Y1], dsp->core.registers[DSP_REG_Y0]);

    for (i=0; i<8; i++) {
        printf("R%01x: %04x   N%01x: %04x   M%01x: %04x\n",
            i, dsp->core.registers[DSP_REG_R0+i],
            i, dsp->core.registers[DSP_REG_N0+i],
            i, dsp->core.registers[DSP_REG_M0+i]);
    }

    printf("LA: %04x   LC: %04x   PC: %04x\n", dsp->core.registers[DSP_REG_LA], dsp->core.registers[DSP_REG_LC], dsp->core.pc);
    printf("SR: %04x  OMR: %02x\n", dsp->core.registers[DSP_REG_SR], dsp->core.registers[DSP_REG_OMR]);
    printf("SP: %02x    SSH: %04x  SSL: %04x\n",
        dsp->core.registers[DSP_REG_SP], dsp->core.registers[DSP_REG_SSH], dsp->core.registers[DSP_REG_SSL]);
}


/**
 * Get given DSP register address and required bit mask.
 * Works for A0-2, B0-2, LA, LC, M0-7, N0-7, R0-7, X0-1, Y0-1, PC, SR, SP,
 * OMR, SSH & SSL registers, but note that the SP, SSH & SSL registers
 * need special handling (in DSP*SetRegister()) when they are set.
 * Return the register width in bits or zero for an error.
 */
int dsp_get_register_address(DSPState* dsp, const char *regname, uint32_t **addr, uint32_t *mask)
{
#define MAX_REGNAME_LEN 4
    typedef struct {
        const char name[MAX_REGNAME_LEN];
        uint32_t *addr;
        size_t bits;
        uint32_t mask;
    } reg_addr_t;

    /* sorted by name so that this can be bisected */
    const reg_addr_t registers[] = {

        /* 56-bit A register */
        { "A0",  &dsp->core.registers[DSP_REG_A0],  32, BITMASK(24) },
        { "A1",  &dsp->core.registers[DSP_REG_A1],  32, BITMASK(24) },
        { "A2",  &dsp->core.registers[DSP_REG_A2],  32, BITMASK(8) },

        /* 56-bit B register */
        { "B0",  &dsp->core.registers[DSP_REG_B0],  32, BITMASK(24) },
        { "B1",  &dsp->core.registers[DSP_REG_B1],  32, BITMASK(24) },
        { "B2",  &dsp->core.registers[DSP_REG_B2],  32, BITMASK(8) },

        /* 16-bit LA & LC registers */
        { "LA",  &dsp->core.registers[DSP_REG_LA],  32, BITMASK(16) },
        { "LC",  &dsp->core.registers[DSP_REG_LC],  32, BITMASK(16) },

        /* 16-bit M registers */
        { "M0",  &dsp->core.registers[DSP_REG_M0],  32, BITMASK(16) },
        { "M1",  &dsp->core.registers[DSP_REG_M1],  32, BITMASK(16) },
        { "M2",  &dsp->core.registers[DSP_REG_M2],  32, BITMASK(16) },
        { "M3",  &dsp->core.registers[DSP_REG_M3],  32, BITMASK(16) },
        { "M4",  &dsp->core.registers[DSP_REG_M4],  32, BITMASK(16) },
        { "M5",  &dsp->core.registers[DSP_REG_M5],  32, BITMASK(16) },
        { "M6",  &dsp->core.registers[DSP_REG_M6],  32, BITMASK(16) },
        { "M7",  &dsp->core.registers[DSP_REG_M7],  32, BITMASK(16) },

        /* 16-bit N registers */
        { "N0",  &dsp->core.registers[DSP_REG_N0],  32, BITMASK(16) },
        { "N1",  &dsp->core.registers[DSP_REG_N1],  32, BITMASK(16) },
        { "N2",  &dsp->core.registers[DSP_REG_N2],  32, BITMASK(16) },
        { "N3",  &dsp->core.registers[DSP_REG_N3],  32, BITMASK(16) },
        { "N4",  &dsp->core.registers[DSP_REG_N4],  32, BITMASK(16) },
        { "N5",  &dsp->core.registers[DSP_REG_N5],  32, BITMASK(16) },
        { "N6",  &dsp->core.registers[DSP_REG_N6],  32, BITMASK(16) },
        { "N7",  &dsp->core.registers[DSP_REG_N7],  32, BITMASK(16) },

        { "OMR", &dsp->core.registers[DSP_REG_OMR], 32, 0x5f },

        /* 16-bit program counter */
        { "PC",  (uint32_t*)(&dsp->core.pc),  24, BITMASK(24) },

        /* 16-bit DSP R (address) registers */
        { "R0",  &dsp->core.registers[DSP_REG_R0],  32, BITMASK(16) },
        { "R1",  &dsp->core.registers[DSP_REG_R1],  32, BITMASK(16) },
        { "R2",  &dsp->core.registers[DSP_REG_R2],  32, BITMASK(16) },
        { "R3",  &dsp->core.registers[DSP_REG_R3],  32, BITMASK(16) },
        { "R4",  &dsp->core.registers[DSP_REG_R4],  32, BITMASK(16) },
        { "R5",  &dsp->core.registers[DSP_REG_R5],  32, BITMASK(16) },
        { "R6",  &dsp->core.registers[DSP_REG_R6],  32, BITMASK(16) },
        { "R7",  &dsp->core.registers[DSP_REG_R7],  32, BITMASK(16) },

        { "SSH", &dsp->core.registers[DSP_REG_SSH], 32, BITMASK(16) },
        { "SSL", &dsp->core.registers[DSP_REG_SSL], 32, BITMASK(16) },
        { "SP",  &dsp->core.registers[DSP_REG_SP],  32, BITMASK(6) },

        /* 16-bit status register */
        { "SR",  &dsp->core.registers[DSP_REG_SR],  32, 0xefff },

        /* 48-bit X register */
        { "X0",  &dsp->core.registers[DSP_REG_X0],  32, BITMASK(24) },
        { "X1",  &dsp->core.registers[DSP_REG_X1],  32, BITMASK(24) },

        /* 48-bit Y register */
        { "Y0",  &dsp->core.registers[DSP_REG_Y0],  32, BITMASK(24) },
        { "Y1",  &dsp->core.registers[DSP_REG_Y1],  32, BITMASK(24) }
    };
    /* left, right, middle, direction */
    int l, r, m, dir = 0;
    unsigned int i, len;
    char reg[MAX_REGNAME_LEN];

    for (i = 0; i < sizeof(reg) && regname[i]; i++) {
        reg[i] = toupper(regname[i]);
    }
    if (i < 2 || regname[i]) {
        /* too short or longer than any of the names */
        return 0;
    }
    len = i;

    /* bisect */
    l = 0;
    r = ARRAY_SIZE(registers) - 1;
    do {
        m = (l+r) >> 1;
        for (i = 0; i < len; i++) {
            dir = (int)reg[i] - registers[m].name[i];
            if (dir) {
                break;
            }
        }
        if (dir == 0) {
            *addr = registers[m].addr;
            *mask = registers[m].mask;
            return registers[m].bits;
        }
        if (dir < 0) {
            r = m-1;
        } else {
            l = m+1;
        }
    } while (l <= r);
#undef MAX_REGNAME_LEN
    return 0;
}


/**
 * Set given DSP register value, return false if unknown register given
 */
bool dsp_disasm_set_register(DSPState* dsp, const char *arg, uint32_t value)
{
    uint32_t *addr, mask, sp_value;
    int bits;

    /* first check registers needing special handling... */
    if (arg[0]=='S' || arg[0]=='s') {
        if (arg[1]=='P' || arg[1]=='p') {
            dsp->core.registers[DSP_REG_SP] = value & BITMASK(6);
            value &= BITMASK(4);
            dsp->core.registers[DSP_REG_SSH] = dsp->core.stack[0][value];
            dsp->core.registers[DSP_REG_SSL] = dsp->core.stack[1][value];
            return true;
        }
        if (arg[1]=='S' || arg[1]=='s') {
            sp_value = dsp->core.registers[DSP_REG_SP] & BITMASK(4);
            if (arg[2]=='H' || arg[2]=='h') {
                if (sp_value == 0) {
                    dsp->core.registers[DSP_REG_SSH] = 0;
                    dsp->core.stack[0][sp_value] = 0;
                } else {
                    dsp->core.registers[DSP_REG_SSH] = value & BITMASK(16);
                    dsp->core.stack[0][sp_value] = value & BITMASK(16);
                }
                return true;
            }
            if (arg[2]=='L' || arg[2]=='l') {
                if (sp_value == 0) {
                    dsp->core.registers[DSP_REG_SSL] = 0;
                    dsp->core.stack[1][sp_value] = 0;
                } else {
                    dsp->core.registers[DSP_REG_SSL] = value & BITMASK(16);
                    dsp->core.stack[1][sp_value] = value & BITMASK(16);
                }
                return true;
            }
        }
    }

    /* ...then registers where address & mask are enough */
    bits = dsp_get_register_address(dsp, arg, &addr, &mask);
    switch (bits) {
    case 32:
        *addr = value & mask;
        return true;
    case 16:
        *(uint16_t*)addr = value & mask;
        return true;
    }
    return false;
}

#endif
