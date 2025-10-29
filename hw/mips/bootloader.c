/*
 * Utility for QEMU MIPS to generate it's simple bootloader
 *
 * Instructions used here are carefully selected to keep compatibility with
 * MIPS Release 6.
 *
 * Copyright (C) 2020 Jiaxun Yang <jiaxun.yang@flygoat.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/bitops.h"
#include "cpu.h"
#include "hw/mips/bootloader.h"

typedef enum bl_reg {
    BL_REG_ZERO = 0,
    BL_REG_AT = 1,
    BL_REG_V0 = 2,
    BL_REG_V1 = 3,
    BL_REG_A0 = 4,
    BL_REG_A1 = 5,
    BL_REG_A2 = 6,
    BL_REG_A3 = 7,
    BL_REG_T0 = 8,
    BL_REG_T1 = 9,
    BL_REG_T2 = 10,
    BL_REG_T3 = 11,
    BL_REG_T4 = 12,
    BL_REG_T5 = 13,
    BL_REG_T6 = 14,
    BL_REG_T7 = 15,
    BL_REG_S0 = 16,
    BL_REG_S1 = 17,
    BL_REG_S2 = 18,
    BL_REG_S3 = 19,
    BL_REG_S4 = 20,
    BL_REG_S5 = 21,
    BL_REG_S6 = 22,
    BL_REG_S7 = 23,
    BL_REG_T8 = 24,
    BL_REG_T9 = 25,
    BL_REG_K0 = 26,
    BL_REG_K1 = 27,
    BL_REG_GP = 28,
    BL_REG_SP = 29,
    BL_REG_FP = 30,
    BL_REG_RA = 31,
} bl_reg;

static bool bootcpu_supports_isa(uint64_t isa_mask)
{
    return cpu_supports_isa(&MIPS_CPU(first_cpu)->env, isa_mask);
}

static void st_nm32_p(void **ptr, uint32_t insn)
{
    uint16_t *p = *ptr;

    stw_p(p, insn >> 16);
    p++;
    stw_p(p, insn >> 0);
    p++;

    *ptr = p;
}

/* Base types */
static void bl_gen_nop(void **ptr)
{
    if (bootcpu_supports_isa(ISA_NANOMIPS32)) {
        st_nm32_p(ptr, 0x8000c000);
    } else {
        uint32_t *p = *ptr;

        stl_p(p, 0);
        p++;
        *ptr = p;
    }
}

static void bl_gen_r_type(void **ptr, uint8_t opcode,
                          bl_reg rs, bl_reg rt, bl_reg rd,
                          uint8_t shift, uint8_t funct)
{
    uint32_t *p = *ptr;
    uint32_t insn = 0;

    insn = deposit32(insn, 26, 6, opcode);
    insn = deposit32(insn, 21, 5, rs);
    insn = deposit32(insn, 16, 5, rt);
    insn = deposit32(insn, 11, 5, rd);
    insn = deposit32(insn, 6, 5, shift);
    insn = deposit32(insn, 0, 6, funct);

    stl_p(p, insn);
    p++;

    *ptr = p;
}

static void bl_gen_i_type(void **ptr, uint8_t opcode,
                          bl_reg rs, bl_reg rt, uint16_t imm)
{
    uint32_t *p = *ptr;
    uint32_t insn = 0;

    insn = deposit32(insn, 26, 6, opcode);
    insn = deposit32(insn, 21, 5, rs);
    insn = deposit32(insn, 16, 5, rt);
    insn = deposit32(insn, 0, 16, imm);

    stl_p(p, insn);
    p++;

    *ptr = p;
}

/* Single instructions */
static void bl_gen_dsll(void **p, bl_reg rd, bl_reg rt, uint8_t sa)
{
    if (bootcpu_supports_isa(ISA_MIPS3)) {
        bl_gen_r_type(p, 0, 0, rt, rd, sa, 0x38);
    } else {
        g_assert_not_reached(); /* unsupported */
    }
}

static void bl_gen_jalr(void **p, bl_reg rs)
{
    if (bootcpu_supports_isa(ISA_NANOMIPS32)) {
        uint32_t insn = 0;

        insn = deposit32(insn, 26, 6, 0b010010); /* JALRC */
        insn = deposit32(insn, 21, 5, BL_REG_RA);
        insn = deposit32(insn, 16, 5, rs);

        st_nm32_p(p, insn);
    } else {
        bl_gen_r_type(p, 0, rs, 0, BL_REG_RA, 0, 0x09);
    }
}

static void bl_gen_lui_nm(void **ptr, bl_reg rt, uint32_t imm20)
{
    uint32_t insn = 0;

    assert(extract32(imm20, 0, 20) == imm20);
    insn = deposit32(insn, 26, 6, 0b111000);
    insn = deposit32(insn, 21, 5, rt);
    insn = deposit32(insn, 12, 9, extract32(imm20, 0, 9));
    insn = deposit32(insn, 2, 10, extract32(imm20, 9, 10));
    insn = deposit32(insn, 0, 1, sextract32(imm20, 19, 1));

    st_nm32_p(ptr, insn);
}

static void bl_gen_lui(void **p, bl_reg rt, uint16_t imm)
{
    /* R6: It's a alias of AUI with RS = 0 */
    bl_gen_i_type(p, 0x0f, 0, rt, imm);
}

static void bl_gen_ori_nm(void **ptr, bl_reg rt, bl_reg rs, uint16_t imm12)
{
    uint32_t insn = 0;

    assert(extract32(imm12, 0, 12) == imm12);
    insn = deposit32(insn, 26, 6, 0b100000);
    insn = deposit32(insn, 21, 5, rt);
    insn = deposit32(insn, 16, 5, rs);
    insn = deposit32(insn, 0, 12, imm12);

    st_nm32_p(ptr, insn);
}

static void bl_gen_ori(void **p, bl_reg rt, bl_reg rs, uint16_t imm)
{
    bl_gen_i_type(p, 0x0d, rs, rt, imm);
}

static void bl_gen_sw_nm(void **ptr, bl_reg rt, uint8_t rs, uint16_t ofs12)
{
    uint32_t insn = 0;

    assert(extract32(ofs12, 0, 12) == ofs12);
    insn = deposit32(insn, 26, 6, 0b100001);
    insn = deposit32(insn, 21, 5, rt);
    insn = deposit32(insn, 16, 5, rs);
    insn = deposit32(insn, 12, 4, 0b1001);
    insn = deposit32(insn, 0, 12, ofs12);

    st_nm32_p(ptr, insn);
}

static void bl_gen_sw(void **p, bl_reg rt, uint8_t base, uint16_t offset)
{
    if (bootcpu_supports_isa(ISA_NANOMIPS32)) {
        bl_gen_sw_nm(p, rt, base, offset);
    } else {
        bl_gen_i_type(p, 0x2b, base, rt, offset);
    }
}

static void bl_gen_sd(void **p, bl_reg rt, uint8_t base, uint16_t offset)
{
    if (bootcpu_supports_isa(ISA_MIPS3)) {
        bl_gen_i_type(p, 0x3f, base, rt, offset);
    } else {
        g_assert_not_reached(); /* unsupported */
    }
}

/* Pseudo instructions */
static void bl_gen_li(void **p, bl_reg rt, uint32_t imm)
{
    if (bootcpu_supports_isa(ISA_NANOMIPS32)) {
        bl_gen_lui_nm(p, rt, extract32(imm, 12, 20));
        bl_gen_ori_nm(p, rt, rt, extract32(imm, 0, 12));
    } else {
        bl_gen_lui(p, rt, extract32(imm, 16, 16));
        bl_gen_ori(p, rt, rt, extract32(imm, 0, 16));
    }
}

static void bl_gen_dli(void **p, bl_reg rt, uint64_t imm)
{
    bl_gen_li(p, rt, extract64(imm, 32, 32));
    bl_gen_dsll(p, rt, rt, 16);
    bl_gen_ori(p, rt, rt, extract64(imm, 16, 16));
    bl_gen_dsll(p, rt, rt, 16);
    bl_gen_ori(p, rt, rt, extract64(imm, 0, 16));
}

static void bl_gen_load_ulong(void **p, bl_reg rt, target_ulong imm)
{
    if (bootcpu_supports_isa(ISA_MIPS3)) {
        bl_gen_dli(p, rt, imm); /* 64bit */
    } else {
        bl_gen_li(p, rt, imm); /* 32bit */
    }
}

/* Helpers */
void bl_gen_jump_to(void **p, target_ulong jump_addr)
{
    bl_gen_load_ulong(p, BL_REG_T9, jump_addr);
    bl_gen_jalr(p, BL_REG_T9);
    bl_gen_nop(p); /* delay slot */
}

void bl_gen_jump_kernel(void **p,
                        bool set_sp, target_ulong sp,
                        bool set_a0, target_ulong a0,
                        bool set_a1, target_ulong a1,
                        bool set_a2, target_ulong a2,
                        bool set_a3, target_ulong a3,
                        target_ulong kernel_addr)
{
    if (set_sp) {
        bl_gen_load_ulong(p, BL_REG_SP, sp);
    }
    if (set_a0) {
        bl_gen_load_ulong(p, BL_REG_A0, a0);
    }
    if (set_a1) {
        bl_gen_load_ulong(p, BL_REG_A1, a1);
    }
    if (set_a2) {
        bl_gen_load_ulong(p, BL_REG_A2, a2);
    }
    if (set_a3) {
        bl_gen_load_ulong(p, BL_REG_A3, a3);
    }

    bl_gen_jump_to(p, kernel_addr);
}

void bl_gen_write_ulong(void **p, target_ulong addr, target_ulong val)
{
    bl_gen_load_ulong(p, BL_REG_K0, val);
    bl_gen_load_ulong(p, BL_REG_K1, addr);
    if (bootcpu_supports_isa(ISA_MIPS3)) {
        bl_gen_sd(p, BL_REG_K0, BL_REG_K1, 0x0);
    } else {
        bl_gen_sw(p, BL_REG_K0, BL_REG_K1, 0x0);
    }
}

void bl_gen_write_u32(void **p, target_ulong addr, uint32_t val)
{
    bl_gen_li(p, BL_REG_K0, val);
    bl_gen_load_ulong(p, BL_REG_K1, addr);
    bl_gen_sw(p, BL_REG_K0, BL_REG_K1, 0x0);
}

void bl_gen_write_u64(void **p, target_ulong addr, uint64_t val)
{
    bl_gen_dli(p, BL_REG_K0, val);
    bl_gen_load_ulong(p, BL_REG_K1, addr);
    bl_gen_sd(p, BL_REG_K0, BL_REG_K1, 0x0);
}
