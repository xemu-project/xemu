/* Disassemble SH instructions.
   Copyright 1993, 1994, 1995, 1997, 1998, 2000, 2001, 2002, 2003, 2004
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.  */

#include "qemu/osdep.h"
#include "disas/dis-asm.h"

#define DEFINE_TABLE

typedef enum
  {
    HEX_0,
    HEX_1,
    HEX_2,
    HEX_3,
    HEX_4,
    HEX_5,
    HEX_6,
    HEX_7,
    HEX_8,
    HEX_9,
    HEX_A,
    HEX_B,
    HEX_C,
    HEX_D,
    HEX_E,
    HEX_F,
    HEX_XX00,
    HEX_00YY,
    REG_N,
    REG_N_D,     /* nnn0 */
    REG_N_B01,   /* nn01 */
    REG_M,
    SDT_REG_N,
    REG_NM,
    REG_B,
    BRANCH_12,
    BRANCH_8,
    IMM0_4,
    IMM0_4BY2,
    IMM0_4BY4,
    IMM1_4,
    IMM1_4BY2,
    IMM1_4BY4,
    PCRELIMM_8BY2,
    PCRELIMM_8BY4,
    IMM0_8,
    IMM0_8BY2,
    IMM0_8BY4,
    IMM1_8,
    IMM1_8BY2,
    IMM1_8BY4,
    PPI,
    NOPX,
    NOPY,
    MOVX,
    MOVY,
    MOVX_NOPY,
    MOVY_NOPX,
    PSH,
    PMUL,
    PPI3,
    PPI3NC,
    PDC,
    PPIC,
    REPEAT,
    IMM0_3c,	/* xxxx 0iii */
    IMM0_3s,	/* xxxx 1iii */
    IMM0_3Uc,	/* 0iii xxxx */
    IMM0_3Us,	/* 1iii xxxx */
    IMM0_20_4,
    IMM0_20,	/* follows IMM0_20_4 */
    IMM0_20BY8,	/* follows IMM0_20_4 */
    DISP0_12,
    DISP0_12BY2,
    DISP0_12BY4,
    DISP0_12BY8,
    DISP1_12,
    DISP1_12BY2,
    DISP1_12BY4,
    DISP1_12BY8
  }
sh_nibble_type;

typedef enum
  {
    A_END,
    A_BDISP12,
    A_BDISP8,
    A_DEC_M,
    A_DEC_N,
    A_DISP_GBR,
    A_PC,
    A_DISP_PC,
    A_DISP_PC_ABS,
    A_DISP_REG_M,
    A_DISP_REG_N,
    A_GBR,
    A_IMM,
    A_INC_M,
    A_INC_N,
    A_IND_M,
    A_IND_N,
    A_IND_R0_REG_M,
    A_IND_R0_REG_N,
    A_MACH,
    A_MACL,
    A_PR,
    A_R0,
    A_R0_GBR,
    A_REG_M,
    A_REG_N,
    A_REG_B,
    A_SR,
    A_VBR,
    A_TBR,
    A_DISP_TBR,
    A_DISP2_TBR,
    A_DEC_R15,
    A_INC_R15,
    A_MOD,
    A_RE,
    A_RS,
    A_DSR,
    DSP_REG_M,
    DSP_REG_N,
    DSP_REG_X,
    DSP_REG_Y,
    DSP_REG_E,
    DSP_REG_F,
    DSP_REG_G,
    DSP_REG_A_M,
    DSP_REG_AX,
    DSP_REG_XY,
    DSP_REG_AY,
    DSP_REG_YX,
    AX_INC_N,
    AY_INC_N,
    AXY_INC_N,
    AYX_INC_N,
    AX_IND_N,
    AY_IND_N,
    AXY_IND_N,
    AYX_IND_N,
    AX_PMOD_N,
    AXY_PMOD_N,
    AY_PMOD_N,
    AYX_PMOD_N,
    AS_DEC_N,
    AS_INC_N,
    AS_IND_N,
    AS_PMOD_N,
    A_A0,
    A_X0,
    A_X1,
    A_Y0,
    A_Y1,
    A_SSR,
    A_SPC,
    A_SGR,
    A_DBR,
    F_REG_N,
    F_REG_M,
    D_REG_N,
    D_REG_M,
    X_REG_N, /* Only used for argument parsing.  */
    X_REG_M, /* Only used for argument parsing.  */
    DX_REG_N,
    DX_REG_M,
    V_REG_N,
    V_REG_M,
    XMTRX_M4,
    F_FR0,
    FPUL_N,
    FPUL_M,
    FPSCR_N,
    FPSCR_M
  }
sh_arg_type;

typedef enum
  {
    A_A1_NUM =   5,
    A_A0_NUM =   7,
    A_X0_NUM, A_X1_NUM, A_Y0_NUM, A_Y1_NUM,
    A_M0_NUM, A_A1G_NUM, A_M1_NUM, A_A0G_NUM
  }
sh_dsp_reg_nums;

#define arch_sh1_base	0x0001
#define arch_sh2_base	0x0002
#define arch_sh3_base	0x0004
#define arch_sh4_base	0x0008
#define arch_sh4a_base	0x0010
#define arch_sh2a_base  0x0020

/* This is an annotation on instruction types, but we abuse the arch
   field in instructions to denote it.  */
#define arch_op32       0x00100000 /* This is a 32-bit opcode.  */

#define arch_sh_no_mmu	0x04000000
#define arch_sh_has_mmu 0x08000000
#define arch_sh_no_co	0x10000000 /* neither FPU nor DSP co-processor */
#define arch_sh_sp_fpu	0x20000000 /* single precision FPU */
#define arch_sh_dp_fpu	0x40000000 /* double precision FPU */
#define arch_sh_has_dsp	0x80000000


#define arch_sh_base_mask 0x0000003f
#define arch_opann_mask   0x00100000
#define arch_sh_mmu_mask  0x0c000000
#define arch_sh_co_mask   0xf0000000


#define arch_sh1	(arch_sh1_base|arch_sh_no_mmu|arch_sh_no_co)
#define arch_sh2	(arch_sh2_base|arch_sh_no_mmu|arch_sh_no_co)
#define arch_sh2a	(arch_sh2a_base|arch_sh_no_mmu|arch_sh_dp_fpu)
#define arch_sh2a_nofpu	(arch_sh2a_base|arch_sh_no_mmu|arch_sh_no_co)
#define arch_sh2e	(arch_sh2_base|arch_sh2a_base|arch_sh_no_mmu|arch_sh_sp_fpu)
#define arch_sh_dsp	(arch_sh2_base|arch_sh_no_mmu|arch_sh_has_dsp)
#define arch_sh3_nommu	(arch_sh3_base|arch_sh_no_mmu|arch_sh_no_co)
#define arch_sh3	(arch_sh3_base|arch_sh_has_mmu|arch_sh_no_co)
#define arch_sh3e	(arch_sh3_base|arch_sh_has_mmu|arch_sh_sp_fpu)
#define arch_sh3_dsp	(arch_sh3_base|arch_sh_has_mmu|arch_sh_has_dsp)
#define arch_sh4	(arch_sh4_base|arch_sh_has_mmu|arch_sh_dp_fpu)
#define arch_sh4a	(arch_sh4a_base|arch_sh_has_mmu|arch_sh_dp_fpu)
#define arch_sh4al_dsp	(arch_sh4a_base|arch_sh_has_mmu|arch_sh_has_dsp)
#define arch_sh4_nofpu	(arch_sh4_base|arch_sh_has_mmu|arch_sh_no_co)
#define arch_sh4a_nofpu	(arch_sh4a_base|arch_sh_has_mmu|arch_sh_no_co)
#define arch_sh4_nommu_nofpu (arch_sh4_base|arch_sh_no_mmu|arch_sh_no_co)

#define SH_MERGE_ARCH_SET(SET1, SET2) ((SET1) & (SET2))
#define SH_VALID_BASE_ARCH_SET(SET) (((SET) & arch_sh_base_mask) != 0)
#define SH_VALID_MMU_ARCH_SET(SET)  (((SET) & arch_sh_mmu_mask) != 0)
#define SH_VALID_CO_ARCH_SET(SET)   (((SET) & arch_sh_co_mask) != 0)
#define SH_VALID_ARCH_SET(SET) \
  (SH_VALID_BASE_ARCH_SET (SET) \
   && SH_VALID_MMU_ARCH_SET (SET) \
   && SH_VALID_CO_ARCH_SET (SET))
#define SH_MERGE_ARCH_SET_VALID(SET1, SET2) \
  SH_VALID_ARCH_SET (SH_MERGE_ARCH_SET (SET1, SET2))

#define SH_ARCH_SET_HAS_FPU(SET) \
  (((SET) & (arch_sh_sp_fpu | arch_sh_dp_fpu)) != 0)
#define SH_ARCH_SET_HAS_DSP(SET) \
  (((SET) & arch_sh_has_dsp) != 0)

/* This is returned from the functions below when an error occurs
   (in addition to a call to BFD_FAIL). The value should allow
   the tools to continue to function in most cases - there may
   be some confusion between DSP and FPU etc.  */
#define SH_ARCH_UNKNOWN_ARCH 0xffffffff

/* Below are the 'architecture sets'.
   They describe the following inheritance graph:

                SH1
                 |
                SH2
   .------------'|`--------------------.
  /              |                      \
SH-DSP          SH3-nommu               SH2E
 |               |`--------.             |
 |               |          \            |
 |              SH3     SH4-nommu-nofpu  |
 |               |           |           |
 | .------------'|`----------+---------. |
 |/                         /           \|
 |               | .-------'             |
 |               |/                      |
SH3-dsp         SH4-nofpu               SH3E
 |               |`--------------------. |
 |               |                      \|
 |              SH4A-nofpu              SH4
 | .------------' `--------------------. |
 |/                                     \|
SH4AL-dsp                               SH4A

*/

/* Central branches */
#define arch_sh1_up       (arch_sh1 | arch_sh2_up)
#define arch_sh2_up       (arch_sh2 | arch_sh2e_up | arch_sh2a_nofpu_up | arch_sh3_nommu_up | arch_sh_dsp_up)
#define arch_sh3_nommu_up (arch_sh3_nommu | arch_sh3_up | arch_sh4_nommu_nofpu_up)
#define arch_sh3_up       (arch_sh3 | arch_sh3e_up | arch_sh3_dsp_up | arch_sh4_nofp_up)
#define arch_sh4_nommu_nofpu_up (arch_sh4_nommu_nofpu | arch_sh4_nofp_up)
#define arch_sh4_nofp_up  (arch_sh4_nofpu | arch_sh4_up | arch_sh4a_nofp_up)
#define arch_sh4a_nofp_up (arch_sh4a_nofpu | arch_sh4a_up | arch_sh4al_dsp_up)

/* Right branch */
#define arch_sh2e_up (arch_sh2e | arch_sh2a_up | arch_sh3e_up)
#define arch_sh3e_up (arch_sh3e | arch_sh4_up)
#define arch_sh4_up  (arch_sh4 | arch_sh4a_up)
#define arch_sh4a_up (arch_sh4a)

/* Left branch */
#define arch_sh_dsp_up    (arch_sh_dsp | arch_sh3_dsp_up)
#define arch_sh3_dsp_up   (arch_sh3_dsp | arch_sh4al_dsp_up)
#define arch_sh4al_dsp_up (arch_sh4al_dsp)

/* SH 2a branched off SH2e, adding a lot but not all of SH4 and SH4a.  */
#define arch_sh2a_up        (arch_sh2a)
#define arch_sh2a_nofpu_up  (arch_sh2a_nofpu | arch_sh2a_up)


typedef struct
{
  const char *name;
  sh_arg_type arg[4];
  sh_nibble_type nibbles[9];
  unsigned int arch;
} sh_opcode_info;

#ifdef DEFINE_TABLE

static const sh_opcode_info sh_table[] =
  {
/* 0111nnnni8*1.... add #<imm>,<REG_N>  */{"add",{A_IMM,A_REG_N},{HEX_7,REG_N,IMM0_8}, arch_sh1_up},

/* 0011nnnnmmmm1100 add <REG_M>,<REG_N> */{"add",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_C}, arch_sh1_up},

/* 0011nnnnmmmm1110 addc <REG_M>,<REG_N>*/{"addc",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_E}, arch_sh1_up},

/* 0011nnnnmmmm1111 addv <REG_M>,<REG_N>*/{"addv",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_F}, arch_sh1_up},

/* 11001001i8*1.... and #<imm>,R0       */{"and",{A_IMM,A_R0},{HEX_C,HEX_9,IMM0_8}, arch_sh1_up},

/* 0010nnnnmmmm1001 and <REG_M>,<REG_N> */{"and",{ A_REG_M,A_REG_N},{HEX_2,REG_N,REG_M,HEX_9}, arch_sh1_up},

/* 11001101i8*1.... and.b #<imm>,@(R0,GBR)*/{"and.b",{A_IMM,A_R0_GBR},{HEX_C,HEX_D,IMM0_8}, arch_sh1_up},

/* 1010i12......... bra <bdisp12>       */{"bra",{A_BDISP12},{HEX_A,BRANCH_12}, arch_sh1_up},

/* 1011i12......... bsr <bdisp12>       */{"bsr",{A_BDISP12},{HEX_B,BRANCH_12}, arch_sh1_up},

/* 10001001i8p1.... bt <bdisp8>         */{"bt",{A_BDISP8},{HEX_8,HEX_9,BRANCH_8}, arch_sh1_up},

/* 10001011i8p1.... bf <bdisp8>         */{"bf",{A_BDISP8},{HEX_8,HEX_B,BRANCH_8}, arch_sh1_up},

/* 10001101i8p1.... bt.s <bdisp8>       */{"bt.s",{A_BDISP8},{HEX_8,HEX_D,BRANCH_8}, arch_sh2_up},

/* 10001101i8p1.... bt/s <bdisp8>       */{"bt/s",{A_BDISP8},{HEX_8,HEX_D,BRANCH_8}, arch_sh2_up},

/* 10001111i8p1.... bf.s <bdisp8>       */{"bf.s",{A_BDISP8},{HEX_8,HEX_F,BRANCH_8}, arch_sh2_up},

/* 10001111i8p1.... bf/s <bdisp8>       */{"bf/s",{A_BDISP8},{HEX_8,HEX_F,BRANCH_8}, arch_sh2_up},

/* 0000000010001000 clrdmxy             */{"clrdmxy",{0},{HEX_0,HEX_0,HEX_8,HEX_8}, arch_sh4al_dsp_up},

/* 0000000000101000 clrmac              */{"clrmac",{0},{HEX_0,HEX_0,HEX_2,HEX_8}, arch_sh1_up},

/* 0000000001001000 clrs                */{"clrs",{0},{HEX_0,HEX_0,HEX_4,HEX_8}, arch_sh1_up},

/* 0000000000001000 clrt                */{"clrt",{0},{HEX_0,HEX_0,HEX_0,HEX_8}, arch_sh1_up},

/* 10001000i8*1.... cmp/eq #<imm>,R0    */{"cmp/eq",{A_IMM,A_R0},{HEX_8,HEX_8,IMM0_8}, arch_sh1_up},

/* 0011nnnnmmmm0000 cmp/eq <REG_M>,<REG_N>*/{"cmp/eq",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_0}, arch_sh1_up},

/* 0011nnnnmmmm0011 cmp/ge <REG_M>,<REG_N>*/{"cmp/ge",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_3}, arch_sh1_up},

/* 0011nnnnmmmm0111 cmp/gt <REG_M>,<REG_N>*/{"cmp/gt",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_7}, arch_sh1_up},

/* 0011nnnnmmmm0110 cmp/hi <REG_M>,<REG_N>*/{"cmp/hi",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_6}, arch_sh1_up},

/* 0011nnnnmmmm0010 cmp/hs <REG_M>,<REG_N>*/{"cmp/hs",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_2}, arch_sh1_up},

/* 0100nnnn00010101 cmp/pl <REG_N>      */{"cmp/pl",{A_REG_N},{HEX_4,REG_N,HEX_1,HEX_5}, arch_sh1_up},

/* 0100nnnn00010001 cmp/pz <REG_N>      */{"cmp/pz",{A_REG_N},{HEX_4,REG_N,HEX_1,HEX_1}, arch_sh1_up},

/* 0010nnnnmmmm1100 cmp/str <REG_M>,<REG_N>*/{"cmp/str",{ A_REG_M,A_REG_N},{HEX_2,REG_N,REG_M,HEX_C}, arch_sh1_up},

/* 0010nnnnmmmm0111 div0s <REG_M>,<REG_N>*/{"div0s",{ A_REG_M,A_REG_N},{HEX_2,REG_N,REG_M,HEX_7}, arch_sh1_up},

/* 0000000000011001 div0u               */{"div0u",{0},{HEX_0,HEX_0,HEX_1,HEX_9}, arch_sh1_up},

/* 0011nnnnmmmm0100 div1 <REG_M>,<REG_N>*/{"div1",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_4}, arch_sh1_up},

/* 0110nnnnmmmm1110 exts.b <REG_M>,<REG_N>*/{"exts.b",{ A_REG_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_E}, arch_sh1_up},

/* 0110nnnnmmmm1111 exts.w <REG_M>,<REG_N>*/{"exts.w",{ A_REG_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_F}, arch_sh1_up},

/* 0110nnnnmmmm1100 extu.b <REG_M>,<REG_N>*/{"extu.b",{ A_REG_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_C}, arch_sh1_up},

/* 0110nnnnmmmm1101 extu.w <REG_M>,<REG_N>*/{"extu.w",{ A_REG_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_D}, arch_sh1_up},

/* 0000nnnn11100011 icbi @<REG_N>       */{"icbi",{A_IND_N},{HEX_0,REG_N,HEX_E,HEX_3}, arch_sh4a_nofp_up},

/* 0100nnnn00101011 jmp @<REG_N>        */{"jmp",{A_IND_N},{HEX_4,REG_N,HEX_2,HEX_B}, arch_sh1_up},

/* 0100nnnn00001011 jsr @<REG_N>        */{"jsr",{A_IND_N},{HEX_4,REG_N,HEX_0,HEX_B}, arch_sh1_up},

/* 0100nnnn00001110 ldc <REG_N>,SR      */{"ldc",{A_REG_N,A_SR},{HEX_4,REG_N,HEX_0,HEX_E}, arch_sh1_up},

/* 0100nnnn00011110 ldc <REG_N>,GBR     */{"ldc",{A_REG_N,A_GBR},{HEX_4,REG_N,HEX_1,HEX_E}, arch_sh1_up},

/* 0100nnnn00111010 ldc <REG_N>,SGR     */{"ldc",{A_REG_N,A_SGR},{HEX_4,REG_N,HEX_3,HEX_A}, arch_sh4_nommu_nofpu_up},

/* 0100mmmm01001010 ldc <REG_M>,TBR     */{"ldc",{A_REG_M,A_TBR},{HEX_4,REG_M,HEX_4,HEX_A}, arch_sh2a_nofpu_up},

/* 0100nnnn00101110 ldc <REG_N>,VBR     */{"ldc",{A_REG_N,A_VBR},{HEX_4,REG_N,HEX_2,HEX_E}, arch_sh1_up},

/* 0100nnnn01011110 ldc <REG_N>,MOD     */{"ldc",{A_REG_N,A_MOD},{HEX_4,REG_N,HEX_5,HEX_E}, arch_sh_dsp_up},

/* 0100nnnn01111110 ldc <REG_N>,RE     */{"ldc",{A_REG_N,A_RE},{HEX_4,REG_N,HEX_7,HEX_E}, arch_sh_dsp_up},

/* 0100nnnn01101110 ldc <REG_N>,RS     */{"ldc",{A_REG_N,A_RS},{HEX_4,REG_N,HEX_6,HEX_E}, arch_sh_dsp_up},

/* 0100nnnn00111110 ldc <REG_N>,SSR     */{"ldc",{A_REG_N,A_SSR},{HEX_4,REG_N,HEX_3,HEX_E}, arch_sh3_nommu_up},

/* 0100nnnn01001110 ldc <REG_N>,SPC     */{"ldc",{A_REG_N,A_SPC},{HEX_4,REG_N,HEX_4,HEX_E}, arch_sh3_nommu_up},

/* 0100nnnn11111010 ldc <REG_N>,DBR     */{"ldc",{A_REG_N,A_DBR},{HEX_4,REG_N,HEX_F,HEX_A}, arch_sh4_nommu_nofpu_up},

/* 0100nnnn1xxx1110 ldc <REG_N>,Rn_BANK */{"ldc",{A_REG_N,A_REG_B},{HEX_4,REG_N,REG_B,HEX_E}, arch_sh3_nommu_up},

/* 0100nnnn00000111 ldc.l @<REG_N>+,SR  */{"ldc.l",{A_INC_N,A_SR},{HEX_4,REG_N,HEX_0,HEX_7}, arch_sh1_up},

/* 0100nnnn00010111 ldc.l @<REG_N>+,GBR */{"ldc.l",{A_INC_N,A_GBR},{HEX_4,REG_N,HEX_1,HEX_7}, arch_sh1_up},

/* 0100nnnn00100111 ldc.l @<REG_N>+,VBR */{"ldc.l",{A_INC_N,A_VBR},{HEX_4,REG_N,HEX_2,HEX_7}, arch_sh1_up},

/* 0100nnnn00110110 ldc.l @<REG_N>+,SGR */{"ldc.l",{A_INC_N,A_SGR},{HEX_4,REG_N,HEX_3,HEX_6}, arch_sh4_nommu_nofpu_up},

/* 0100nnnn01010111 ldc.l @<REG_N>+,MOD */{"ldc.l",{A_INC_N,A_MOD},{HEX_4,REG_N,HEX_5,HEX_7}, arch_sh_dsp_up},

/* 0100nnnn01110111 ldc.l @<REG_N>+,RE */{"ldc.l",{A_INC_N,A_RE},{HEX_4,REG_N,HEX_7,HEX_7}, arch_sh_dsp_up},

/* 0100nnnn01100111 ldc.l @<REG_N>+,RS */{"ldc.l",{A_INC_N,A_RS},{HEX_4,REG_N,HEX_6,HEX_7}, arch_sh_dsp_up},

/* 0100nnnn00110111 ldc.l @<REG_N>+,SSR */{"ldc.l",{A_INC_N,A_SSR},{HEX_4,REG_N,HEX_3,HEX_7}, arch_sh3_nommu_up},

/* 0100nnnn01000111 ldc.l @<REG_N>+,SPC */{"ldc.l",{A_INC_N,A_SPC},{HEX_4,REG_N,HEX_4,HEX_7}, arch_sh3_nommu_up},

/* 0100nnnn11110110 ldc.l @<REG_N>+,DBR */{"ldc.l",{A_INC_N,A_DBR},{HEX_4,REG_N,HEX_F,HEX_6}, arch_sh4_nommu_nofpu_up},

/* 0100nnnn1xxx0111 ldc.l <REG_N>,Rn_BANK */{"ldc.l",{A_INC_N,A_REG_B},{HEX_4,REG_N,REG_B,HEX_7}, arch_sh3_nommu_up},

/* 0100mmmm00110100 ldrc <REG_M>        */{"ldrc",{A_REG_M},{HEX_4,REG_M,HEX_3,HEX_4}, arch_sh4al_dsp_up},
/* 10001010i8*1.... ldrc #<imm>         */{"ldrc",{A_IMM},{HEX_8,HEX_A,IMM0_8}, arch_sh4al_dsp_up},

/* 10001110i8p2.... ldre @(<disp>,PC)	*/{"ldre",{A_DISP_PC},{HEX_8,HEX_E,PCRELIMM_8BY2}, arch_sh_dsp_up},

/* 10001100i8p2.... ldrs @(<disp>,PC)	*/{"ldrs",{A_DISP_PC},{HEX_8,HEX_C,PCRELIMM_8BY2}, arch_sh_dsp_up},

/* 0100nnnn00001010 lds <REG_N>,MACH    */{"lds",{A_REG_N,A_MACH},{HEX_4,REG_N,HEX_0,HEX_A}, arch_sh1_up},

/* 0100nnnn00011010 lds <REG_N>,MACL    */{"lds",{A_REG_N,A_MACL},{HEX_4,REG_N,HEX_1,HEX_A}, arch_sh1_up},

/* 0100nnnn00101010 lds <REG_N>,PR      */{"lds",{A_REG_N,A_PR},{HEX_4,REG_N,HEX_2,HEX_A}, arch_sh1_up},

/* 0100nnnn01101010 lds <REG_N>,DSR	*/{"lds",{A_REG_N,A_DSR},{HEX_4,REG_N,HEX_6,HEX_A}, arch_sh_dsp_up},

/* 0100nnnn01111010 lds <REG_N>,A0	*/{"lds",{A_REG_N,A_A0},{HEX_4,REG_N,HEX_7,HEX_A}, arch_sh_dsp_up},

/* 0100nnnn10001010 lds <REG_N>,X0	*/{"lds",{A_REG_N,A_X0},{HEX_4,REG_N,HEX_8,HEX_A}, arch_sh_dsp_up},

/* 0100nnnn10011010 lds <REG_N>,X1	*/{"lds",{A_REG_N,A_X1},{HEX_4,REG_N,HEX_9,HEX_A}, arch_sh_dsp_up},

/* 0100nnnn10101010 lds <REG_N>,Y0	*/{"lds",{A_REG_N,A_Y0},{HEX_4,REG_N,HEX_A,HEX_A}, arch_sh_dsp_up},

/* 0100nnnn10111010 lds <REG_N>,Y1	*/{"lds",{A_REG_N,A_Y1},{HEX_4,REG_N,HEX_B,HEX_A}, arch_sh_dsp_up},

/* 0100nnnn01011010 lds <REG_N>,FPUL    */{"lds",{A_REG_M,FPUL_N},{HEX_4,REG_M,HEX_5,HEX_A}, arch_sh2e_up},

/* 0100nnnn01101010 lds <REG_M>,FPSCR   */{"lds",{A_REG_M,FPSCR_N},{HEX_4,REG_M,HEX_6,HEX_A}, arch_sh2e_up},

/* 0100nnnn00000110 lds.l @<REG_N>+,MACH*/{"lds.l",{A_INC_N,A_MACH},{HEX_4,REG_N,HEX_0,HEX_6}, arch_sh1_up},

/* 0100nnnn00010110 lds.l @<REG_N>+,MACL*/{"lds.l",{A_INC_N,A_MACL},{HEX_4,REG_N,HEX_1,HEX_6}, arch_sh1_up},

/* 0100nnnn00100110 lds.l @<REG_N>+,PR  */{"lds.l",{A_INC_N,A_PR},{HEX_4,REG_N,HEX_2,HEX_6}, arch_sh1_up},

/* 0100nnnn01100110 lds.l @<REG_N>+,DSR	*/{"lds.l",{A_INC_N,A_DSR},{HEX_4,REG_N,HEX_6,HEX_6}, arch_sh_dsp_up},

/* 0100nnnn01110110 lds.l @<REG_N>+,A0	*/{"lds.l",{A_INC_N,A_A0},{HEX_4,REG_N,HEX_7,HEX_6}, arch_sh_dsp_up},

/* 0100nnnn10000110 lds.l @<REG_N>+,X0	*/{"lds.l",{A_INC_N,A_X0},{HEX_4,REG_N,HEX_8,HEX_6}, arch_sh_dsp_up},

/* 0100nnnn10010110 lds.l @<REG_N>+,X1	*/{"lds.l",{A_INC_N,A_X1},{HEX_4,REG_N,HEX_9,HEX_6}, arch_sh_dsp_up},

/* 0100nnnn10100110 lds.l @<REG_N>+,Y0	*/{"lds.l",{A_INC_N,A_Y0},{HEX_4,REG_N,HEX_A,HEX_6}, arch_sh_dsp_up},

/* 0100nnnn10110110 lds.l @<REG_N>+,Y1	*/{"lds.l",{A_INC_N,A_Y1},{HEX_4,REG_N,HEX_B,HEX_6}, arch_sh_dsp_up},

/* 0100nnnn01010110 lds.l @<REG_M>+,FPUL*/{"lds.l",{A_INC_M,FPUL_N},{HEX_4,REG_M,HEX_5,HEX_6}, arch_sh2e_up},

/* 0100nnnn01100110 lds.l @<REG_M>+,FPSCR*/{"lds.l",{A_INC_M,FPSCR_N},{HEX_4,REG_M,HEX_6,HEX_6}, arch_sh2e_up},

/* 0000000000111000 ldtlb               */{"ldtlb",{0},{HEX_0,HEX_0,HEX_3,HEX_8}, arch_sh3_up},

/* 0100nnnnmmmm1111 mac.w @<REG_M>+,@<REG_N>+*/{"mac.w",{A_INC_M,A_INC_N},{HEX_4,REG_N,REG_M,HEX_F}, arch_sh1_up},

/* 1110nnnni8*1.... mov #<imm>,<REG_N>  */{"mov",{A_IMM,A_REG_N},{HEX_E,REG_N,IMM0_8}, arch_sh1_up},

/* 0110nnnnmmmm0011 mov <REG_M>,<REG_N> */{"mov",{ A_REG_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_3}, arch_sh1_up},

/* 0000nnnnmmmm0100 mov.b <REG_M>,@(R0,<REG_N>)*/{"mov.b",{ A_REG_M,A_IND_R0_REG_N},{HEX_0,REG_N,REG_M,HEX_4}, arch_sh1_up},

/* 0010nnnnmmmm0100 mov.b <REG_M>,@-<REG_N>*/{"mov.b",{ A_REG_M,A_DEC_N},{HEX_2,REG_N,REG_M,HEX_4}, arch_sh1_up},

/* 0010nnnnmmmm0000 mov.b <REG_M>,@<REG_N>*/{"mov.b",{ A_REG_M,A_IND_N},{HEX_2,REG_N,REG_M,HEX_0}, arch_sh1_up},

/* 10000100mmmmi4*1 mov.b @(<disp>,<REG_M>),R0*/{"mov.b",{A_DISP_REG_M,A_R0},{HEX_8,HEX_4,REG_M,IMM0_4}, arch_sh1_up},

/* 11000100i8*1.... mov.b @(<disp>,GBR),R0*/{"mov.b",{A_DISP_GBR,A_R0},{HEX_C,HEX_4,IMM0_8}, arch_sh1_up},

/* 0000nnnnmmmm1100 mov.b @(R0,<REG_M>),<REG_N>*/{"mov.b",{A_IND_R0_REG_M,A_REG_N},{HEX_0,REG_N,REG_M,HEX_C}, arch_sh1_up},

/* 0110nnnnmmmm0100 mov.b @<REG_M>+,<REG_N>*/{"mov.b",{A_INC_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_4}, arch_sh1_up},

/* 0110nnnnmmmm0000 mov.b @<REG_M>,<REG_N>*/{"mov.b",{A_IND_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_0}, arch_sh1_up},

/* 10000000mmmmi4*1 mov.b R0,@(<disp>,<REG_M>)*/{"mov.b",{A_R0,A_DISP_REG_M},{HEX_8,HEX_0,REG_M,IMM1_4}, arch_sh1_up},

/* 11000000i8*1.... mov.b R0,@(<disp>,GBR)*/{"mov.b",{A_R0,A_DISP_GBR},{HEX_C,HEX_0,IMM1_8}, arch_sh1_up},

/* 0100nnnn10001011 mov.b R0,@<REG_N>+ */{"mov.b",{A_R0,A_INC_N},{HEX_4,REG_N,HEX_8,HEX_B}, arch_sh2a_nofpu_up},
/* 0100nnnn11001011 mov.b @-<REG_M>,R0 */{"mov.b",{A_DEC_M,A_R0},{HEX_4,REG_M,HEX_C,HEX_B}, arch_sh2a_nofpu_up},
/* 0011nnnnmmmm0001 0000dddddddddddd mov.b <REG_M>,@(<DISP12>,<REG_N>) */
{"mov.b",{A_REG_M,A_DISP_REG_N},{HEX_3,REG_N,REG_M,HEX_1,HEX_0,DISP1_12}, arch_sh2a_nofpu_up | arch_op32},
/* 0011nnnnmmmm0001 0100dddddddddddd mov.b @(<DISP12>,<REG_M>),<REG_N> */
{"mov.b",{A_DISP_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_1,HEX_4,DISP0_12}, arch_sh2a_nofpu_up | arch_op32},
/* 0001nnnnmmmmi4*4 mov.l <REG_M>,@(<disp>,<REG_N>)*/{"mov.l",{ A_REG_M,A_DISP_REG_N},{HEX_1,REG_N,REG_M,IMM1_4BY4}, arch_sh1_up},

/* 0000nnnnmmmm0110 mov.l <REG_M>,@(R0,<REG_N>)*/{"mov.l",{ A_REG_M,A_IND_R0_REG_N},{HEX_0,REG_N,REG_M,HEX_6}, arch_sh1_up},

/* 0010nnnnmmmm0110 mov.l <REG_M>,@-<REG_N>*/{"mov.l",{ A_REG_M,A_DEC_N},{HEX_2,REG_N,REG_M,HEX_6}, arch_sh1_up},

/* 0010nnnnmmmm0010 mov.l <REG_M>,@<REG_N>*/{"mov.l",{ A_REG_M,A_IND_N},{HEX_2,REG_N,REG_M,HEX_2}, arch_sh1_up},

/* 0101nnnnmmmmi4*4 mov.l @(<disp>,<REG_M>),<REG_N>*/{"mov.l",{A_DISP_REG_M,A_REG_N},{HEX_5,REG_N,REG_M,IMM0_4BY4}, arch_sh1_up},

/* 11000110i8*4.... mov.l @(<disp>,GBR),R0*/{"mov.l",{A_DISP_GBR,A_R0},{HEX_C,HEX_6,IMM0_8BY4}, arch_sh1_up},

/* 1101nnnni8p4.... mov.l @(<disp>,PC),<REG_N>*/{"mov.l",{A_DISP_PC,A_REG_N},{HEX_D,REG_N,PCRELIMM_8BY4}, arch_sh1_up},

/* 0000nnnnmmmm1110 mov.l @(R0,<REG_M>),<REG_N>*/{"mov.l",{A_IND_R0_REG_M,A_REG_N},{HEX_0,REG_N,REG_M,HEX_E}, arch_sh1_up},

/* 0110nnnnmmmm0110 mov.l @<REG_M>+,<REG_N>*/{"mov.l",{A_INC_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_6}, arch_sh1_up},

/* 0110nnnnmmmm0010 mov.l @<REG_M>,<REG_N>*/{"mov.l",{A_IND_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_2}, arch_sh1_up},

/* 11000010i8*4.... mov.l R0,@(<disp>,GBR)*/{"mov.l",{A_R0,A_DISP_GBR},{HEX_C,HEX_2,IMM1_8BY4}, arch_sh1_up},

/* 0100nnnn10101011 mov.l R0,@<REG_N>+ */{"mov.l",{A_R0,A_INC_N},{HEX_4,REG_N,HEX_A,HEX_B}, arch_sh2a_nofpu_up},
/* 0100nnnn11001011 mov.l @-<REG_M>,R0 */{"mov.l",{A_DEC_M,A_R0},{HEX_4,REG_M,HEX_E,HEX_B}, arch_sh2a_nofpu_up},
/* 0011nnnnmmmm0001 0010dddddddddddd mov.l <REG_M>,@(<DISP12>,<REG_N>) */
{"mov.l",{A_REG_M,A_DISP_REG_N},{HEX_3,REG_N,REG_M,HEX_1,HEX_2,DISP1_12BY4}, arch_sh2a_nofpu_up | arch_op32},
/* 0011nnnnmmmm0001 0110dddddddddddd mov.l @(<DISP12>,<REG_M>),<REG_N> */
{"mov.l",{A_DISP_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_1,HEX_6,DISP0_12BY4}, arch_sh2a_nofpu_up | arch_op32},
/* 0000nnnnmmmm0101 mov.w <REG_M>,@(R0,<REG_N>)*/{"mov.w",{ A_REG_M,A_IND_R0_REG_N},{HEX_0,REG_N,REG_M,HEX_5}, arch_sh1_up},

/* 0010nnnnmmmm0101 mov.w <REG_M>,@-<REG_N>*/{"mov.w",{ A_REG_M,A_DEC_N},{HEX_2,REG_N,REG_M,HEX_5}, arch_sh1_up},

/* 0010nnnnmmmm0001 mov.w <REG_M>,@<REG_N>*/{"mov.w",{ A_REG_M,A_IND_N},{HEX_2,REG_N,REG_M,HEX_1}, arch_sh1_up},

/* 10000101mmmmi4*2 mov.w @(<disp>,<REG_M>),R0*/{"mov.w",{A_DISP_REG_M,A_R0},{HEX_8,HEX_5,REG_M,IMM0_4BY2}, arch_sh1_up},

/* 11000101i8*2.... mov.w @(<disp>,GBR),R0*/{"mov.w",{A_DISP_GBR,A_R0},{HEX_C,HEX_5,IMM0_8BY2}, arch_sh1_up},

/* 1001nnnni8p2.... mov.w @(<disp>,PC),<REG_N>*/{"mov.w",{A_DISP_PC,A_REG_N},{HEX_9,REG_N,PCRELIMM_8BY2}, arch_sh1_up},

/* 0000nnnnmmmm1101 mov.w @(R0,<REG_M>),<REG_N>*/{"mov.w",{A_IND_R0_REG_M,A_REG_N},{HEX_0,REG_N,REG_M,HEX_D}, arch_sh1_up},

/* 0110nnnnmmmm0101 mov.w @<REG_M>+,<REG_N>*/{"mov.w",{A_INC_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_5}, arch_sh1_up},

/* 0110nnnnmmmm0001 mov.w @<REG_M>,<REG_N>*/{"mov.w",{A_IND_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_1}, arch_sh1_up},

/* 10000001mmmmi4*2 mov.w R0,@(<disp>,<REG_M>)*/{"mov.w",{A_R0,A_DISP_REG_M},{HEX_8,HEX_1,REG_M,IMM1_4BY2}, arch_sh1_up},

/* 11000001i8*2.... mov.w R0,@(<disp>,GBR)*/{"mov.w",{A_R0,A_DISP_GBR},{HEX_C,HEX_1,IMM1_8BY2}, arch_sh1_up},

/* 0100nnnn10011011 mov.w R0,@<REG_N>+ */{"mov.w",{A_R0,A_INC_N},{HEX_4,REG_N,HEX_9,HEX_B}, arch_sh2a_nofpu_up},
/* 0100nnnn11011011 mov.w @-<REG_M>,R0 */{"mov.w",{A_DEC_M,A_R0},{HEX_4,REG_M,HEX_D,HEX_B}, arch_sh2a_nofpu_up},
/* 0011nnnnmmmm0001 0001dddddddddddd mov.w <REG_M>,@(<DISP12>,<REG_N>) */
{"mov.w",{A_REG_M,A_DISP_REG_N},{HEX_3,REG_N,REG_M,HEX_1,HEX_1,DISP1_12BY2}, arch_sh2a_nofpu_up | arch_op32},
/* 0011nnnnmmmm0001 0101dddddddddddd mov.w @(<DISP12>,<REG_M>),<REG_N> */
{"mov.w",{A_DISP_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_1,HEX_5,DISP0_12BY2}, arch_sh2a_nofpu_up | arch_op32},
/* 11000111i8p4.... mova @(<disp>,PC),R0*/{"mova",{A_DISP_PC,A_R0},{HEX_C,HEX_7,PCRELIMM_8BY4}, arch_sh1_up},
/* 0000nnnn11000011 movca.l R0,@<REG_N> */{"movca.l",{A_R0,A_IND_N},{HEX_0,REG_N,HEX_C,HEX_3}, arch_sh4_nommu_nofpu_up},

/* 0000nnnn01110011 movco.l r0,@<REG_N> */{"movco.l",{A_R0,A_IND_N},{HEX_0,REG_N,HEX_7,HEX_3}, arch_sh4a_nofp_up},
/* 0000mmmm01100011 movli.l @<REG_M>,r0 */{"movli.l",{A_IND_M,A_R0},{HEX_0,REG_M,HEX_6,HEX_3}, arch_sh4a_nofp_up},

/* 0000nnnn00101001 movt <REG_N>        */{"movt",{A_REG_N},{HEX_0,REG_N,HEX_2,HEX_9}, arch_sh1_up},

/* 0100mmmm10101001 movua.l @<REG_M>,r0 */{"movua.l",{A_IND_M,A_R0},{HEX_4,REG_M,HEX_A,HEX_9}, arch_sh4a_nofp_up},
/* 0100mmmm11101001 movua.l @<REG_M>+,r0 */{"movua.l",{A_INC_M,A_R0},{HEX_4,REG_M,HEX_E,HEX_9}, arch_sh4a_nofp_up},

/* 0010nnnnmmmm1111 muls.w <REG_M>,<REG_N>*/{"muls.w",{ A_REG_M,A_REG_N},{HEX_2,REG_N,REG_M,HEX_F}, arch_sh1_up},
/* 0010nnnnmmmm1111 muls <REG_M>,<REG_N>*/{"muls",{ A_REG_M,A_REG_N},{HEX_2,REG_N,REG_M,HEX_F}, arch_sh1_up},

/* 0000nnnnmmmm0111 mul.l <REG_M>,<REG_N>*/{"mul.l",{ A_REG_M,A_REG_N},{HEX_0,REG_N,REG_M,HEX_7}, arch_sh2_up},

/* 0010nnnnmmmm1110 mulu.w <REG_M>,<REG_N>*/{"mulu.w",{ A_REG_M,A_REG_N},{HEX_2,REG_N,REG_M,HEX_E}, arch_sh1_up},
/* 0010nnnnmmmm1110 mulu <REG_M>,<REG_N>*/{"mulu",{ A_REG_M,A_REG_N},{HEX_2,REG_N,REG_M,HEX_E}, arch_sh1_up},

/* 0110nnnnmmmm1011 neg <REG_M>,<REG_N> */{"neg",{ A_REG_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_B}, arch_sh1_up},

/* 0110nnnnmmmm1010 negc <REG_M>,<REG_N>*/{"negc",{ A_REG_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_A}, arch_sh1_up},

/* 0000000000001001 nop                 */{"nop",{0},{HEX_0,HEX_0,HEX_0,HEX_9}, arch_sh1_up},

/* 0110nnnnmmmm0111 not <REG_M>,<REG_N> */{"not",{ A_REG_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_7}, arch_sh1_up},
/* 0000nnnn10010011 ocbi @<REG_N>       */{"ocbi",{A_IND_N},{HEX_0,REG_N,HEX_9,HEX_3}, arch_sh4_nommu_nofpu_up},

/* 0000nnnn10100011 ocbp @<REG_N>       */{"ocbp",{A_IND_N},{HEX_0,REG_N,HEX_A,HEX_3}, arch_sh4_nommu_nofpu_up},

/* 0000nnnn10110011 ocbwb @<REG_N>      */{"ocbwb",{A_IND_N},{HEX_0,REG_N,HEX_B,HEX_3}, arch_sh4_nommu_nofpu_up},


/* 11001011i8*1.... or #<imm>,R0        */{"or",{A_IMM,A_R0},{HEX_C,HEX_B,IMM0_8}, arch_sh1_up},

/* 0010nnnnmmmm1011 or <REG_M>,<REG_N>  */{"or",{ A_REG_M,A_REG_N},{HEX_2,REG_N,REG_M,HEX_B}, arch_sh1_up},

/* 11001111i8*1.... or.b #<imm>,@(R0,GBR)*/{"or.b",{A_IMM,A_R0_GBR},{HEX_C,HEX_F,IMM0_8}, arch_sh1_up},

/* 0000nnnn10000011 pref @<REG_N>       */{"pref",{A_IND_N},{HEX_0,REG_N,HEX_8,HEX_3}, arch_sh4_nommu_nofpu_up | arch_sh2a_nofpu_up},

/* 0000nnnn11010011 prefi @<REG_N>      */{"prefi",{A_IND_N},{HEX_0,REG_N,HEX_D,HEX_3}, arch_sh4a_nofp_up},

/* 0100nnnn00100100 rotcl <REG_N>       */{"rotcl",{A_REG_N},{HEX_4,REG_N,HEX_2,HEX_4}, arch_sh1_up},

/* 0100nnnn00100101 rotcr <REG_N>       */{"rotcr",{A_REG_N},{HEX_4,REG_N,HEX_2,HEX_5}, arch_sh1_up},

/* 0100nnnn00000100 rotl <REG_N>        */{"rotl",{A_REG_N},{HEX_4,REG_N,HEX_0,HEX_4}, arch_sh1_up},

/* 0100nnnn00000101 rotr <REG_N>        */{"rotr",{A_REG_N},{HEX_4,REG_N,HEX_0,HEX_5}, arch_sh1_up},

/* 0000000000101011 rte                 */{"rte",{0},{HEX_0,HEX_0,HEX_2,HEX_B}, arch_sh1_up},

/* 0000000000001011 rts                 */{"rts",{0},{HEX_0,HEX_0,HEX_0,HEX_B}, arch_sh1_up},

/* 0000000010011000 setdmx              */{"setdmx",{0},{HEX_0,HEX_0,HEX_9,HEX_8}, arch_sh4al_dsp_up},
/* 0000000011001000 setdmy              */{"setdmy",{0},{HEX_0,HEX_0,HEX_C,HEX_8}, arch_sh4al_dsp_up},

/* 0000000001011000 sets                */{"sets",{0},{HEX_0,HEX_0,HEX_5,HEX_8}, arch_sh1_up},
/* 0000000000011000 sett                */{"sett",{0},{HEX_0,HEX_0,HEX_1,HEX_8}, arch_sh1_up},

/* 0100nnnn00010100 setrc <REG_N>       */{"setrc",{A_REG_N},{HEX_4,REG_N,HEX_1,HEX_4}, arch_sh_dsp_up},

/* 10000010i8*1.... setrc #<imm>        */{"setrc",{A_IMM},{HEX_8,HEX_2,IMM0_8}, arch_sh_dsp_up},

/* repeat start end <REG_N>       	*/{"repeat",{A_DISP_PC,A_DISP_PC,A_REG_N},{REPEAT,REG_N,HEX_1,HEX_4}, arch_sh_dsp_up},

/* repeat start end #<imm>        	*/{"repeat",{A_DISP_PC,A_DISP_PC,A_IMM},{REPEAT,HEX_2,IMM0_8,HEX_8}, arch_sh_dsp_up},

/* 0100nnnnmmmm1100 shad <REG_M>,<REG_N>*/{"shad",{ A_REG_M,A_REG_N},{HEX_4,REG_N,REG_M,HEX_C}, arch_sh3_nommu_up | arch_sh2a_nofpu_up},

/* 0100nnnnmmmm1101 shld <REG_M>,<REG_N>*/{"shld",{ A_REG_M,A_REG_N},{HEX_4,REG_N,REG_M,HEX_D}, arch_sh3_nommu_up | arch_sh2a_nofpu_up},

/* 0100nnnn00100000 shal <REG_N>        */{"shal",{A_REG_N},{HEX_4,REG_N,HEX_2,HEX_0}, arch_sh1_up},

/* 0100nnnn00100001 shar <REG_N>        */{"shar",{A_REG_N},{HEX_4,REG_N,HEX_2,HEX_1}, arch_sh1_up},

/* 0100nnnn00000000 shll <REG_N>        */{"shll",{A_REG_N},{HEX_4,REG_N,HEX_0,HEX_0}, arch_sh1_up},

/* 0100nnnn00101000 shll16 <REG_N>      */{"shll16",{A_REG_N},{HEX_4,REG_N,HEX_2,HEX_8}, arch_sh1_up},

/* 0100nnnn00001000 shll2 <REG_N>       */{"shll2",{A_REG_N},{HEX_4,REG_N,HEX_0,HEX_8}, arch_sh1_up},

/* 0100nnnn00011000 shll8 <REG_N>       */{"shll8",{A_REG_N},{HEX_4,REG_N,HEX_1,HEX_8}, arch_sh1_up},

/* 0100nnnn00000001 shlr <REG_N>        */{"shlr",{A_REG_N},{HEX_4,REG_N,HEX_0,HEX_1}, arch_sh1_up},

/* 0100nnnn00101001 shlr16 <REG_N>      */{"shlr16",{A_REG_N},{HEX_4,REG_N,HEX_2,HEX_9}, arch_sh1_up},

/* 0100nnnn00001001 shlr2 <REG_N>       */{"shlr2",{A_REG_N},{HEX_4,REG_N,HEX_0,HEX_9}, arch_sh1_up},

/* 0100nnnn00011001 shlr8 <REG_N>       */{"shlr8",{A_REG_N},{HEX_4,REG_N,HEX_1,HEX_9}, arch_sh1_up},

/* 0000000000011011 sleep               */{"sleep",{0},{HEX_0,HEX_0,HEX_1,HEX_B}, arch_sh1_up},

/* 0000nnnn00000010 stc SR,<REG_N>      */{"stc",{A_SR,A_REG_N},{HEX_0,REG_N,HEX_0,HEX_2}, arch_sh1_up},

/* 0000nnnn00010010 stc GBR,<REG_N>     */{"stc",{A_GBR,A_REG_N},{HEX_0,REG_N,HEX_1,HEX_2}, arch_sh1_up},

/* 0000nnnn00100010 stc VBR,<REG_N>     */{"stc",{A_VBR,A_REG_N},{HEX_0,REG_N,HEX_2,HEX_2}, arch_sh1_up},

/* 0000nnnn01010010 stc MOD,<REG_N>     */{"stc",{A_MOD,A_REG_N},{HEX_0,REG_N,HEX_5,HEX_2}, arch_sh_dsp_up},

/* 0000nnnn01110010 stc RE,<REG_N>     */{"stc",{A_RE,A_REG_N},{HEX_0,REG_N,HEX_7,HEX_2}, arch_sh_dsp_up},

/* 0000nnnn01100010 stc RS,<REG_N>     */{"stc",{A_RS,A_REG_N},{HEX_0,REG_N,HEX_6,HEX_2}, arch_sh_dsp_up},

/* 0000nnnn00110010 stc SSR,<REG_N>     */{"stc",{A_SSR,A_REG_N},{HEX_0,REG_N,HEX_3,HEX_2}, arch_sh3_nommu_up},

/* 0000nnnn01000010 stc SPC,<REG_N>     */{"stc",{A_SPC,A_REG_N},{HEX_0,REG_N,HEX_4,HEX_2}, arch_sh3_nommu_up},

/* 0000nnnn00111010 stc SGR,<REG_N>     */{"stc",{A_SGR,A_REG_N},{HEX_0,REG_N,HEX_3,HEX_A}, arch_sh4_nommu_nofpu_up},

/* 0000nnnn11111010 stc DBR,<REG_N>     */{"stc",{A_DBR,A_REG_N},{HEX_0,REG_N,HEX_F,HEX_A}, arch_sh4_nommu_nofpu_up},

/* 0000nnnn1xxx0010 stc Rn_BANK,<REG_N> */{"stc",{A_REG_B,A_REG_N},{HEX_0,REG_N,REG_B,HEX_2}, arch_sh3_nommu_up},

/* 0000nnnn01001010 stc TBR,<REG_N> */ {"stc",{A_TBR,A_REG_N},{HEX_0,REG_N,HEX_4,HEX_A}, arch_sh2a_nofpu_up},

/* 0100nnnn00000011 stc.l SR,@-<REG_N>  */{"stc.l",{A_SR,A_DEC_N},{HEX_4,REG_N,HEX_0,HEX_3}, arch_sh1_up},

/* 0100nnnn00100011 stc.l VBR,@-<REG_N> */{"stc.l",{A_VBR,A_DEC_N},{HEX_4,REG_N,HEX_2,HEX_3}, arch_sh1_up},

/* 0100nnnn01010011 stc.l MOD,@-<REG_N> */{"stc.l",{A_MOD,A_DEC_N},{HEX_4,REG_N,HEX_5,HEX_3}, arch_sh_dsp_up},

/* 0100nnnn01110011 stc.l RE,@-<REG_N>  */{"stc.l",{A_RE,A_DEC_N},{HEX_4,REG_N,HEX_7,HEX_3}, arch_sh_dsp_up},

/* 0100nnnn01100011 stc.l RS,@-<REG_N>  */{"stc.l",{A_RS,A_DEC_N},{HEX_4,REG_N,HEX_6,HEX_3}, arch_sh_dsp_up},

/* 0100nnnn00110011 stc.l SSR,@-<REG_N> */{"stc.l",{A_SSR,A_DEC_N},{HEX_4,REG_N,HEX_3,HEX_3}, arch_sh3_nommu_up},

/* 0100nnnn01000011 stc.l SPC,@-<REG_N> */{"stc.l",{A_SPC,A_DEC_N},{HEX_4,REG_N,HEX_4,HEX_3}, arch_sh3_nommu_up},

/* 0100nnnn00010011 stc.l GBR,@-<REG_N> */{"stc.l",{A_GBR,A_DEC_N},{HEX_4,REG_N,HEX_1,HEX_3}, arch_sh1_up},

/* 0100nnnn00110010 stc.l SGR,@-<REG_N> */{"stc.l",{A_SGR,A_DEC_N},{HEX_4,REG_N,HEX_3,HEX_2}, arch_sh4_nommu_nofpu_up},

/* 0100nnnn11110010 stc.l DBR,@-<REG_N> */{"stc.l",{A_DBR,A_DEC_N},{HEX_4,REG_N,HEX_F,HEX_2}, arch_sh4_nommu_nofpu_up},

/* 0100nnnn1xxx0011 stc.l Rn_BANK,@-<REG_N> */{"stc.l",{A_REG_B,A_DEC_N},{HEX_4,REG_N,REG_B,HEX_3}, arch_sh3_nommu_up},

/* 0000nnnn00001010 sts MACH,<REG_N>    */{"sts",{A_MACH,A_REG_N},{HEX_0,REG_N,HEX_0,HEX_A}, arch_sh1_up},

/* 0000nnnn00011010 sts MACL,<REG_N>    */{"sts",{A_MACL,A_REG_N},{HEX_0,REG_N,HEX_1,HEX_A}, arch_sh1_up},

/* 0000nnnn00101010 sts PR,<REG_N>      */{"sts",{A_PR,A_REG_N},{HEX_0,REG_N,HEX_2,HEX_A}, arch_sh1_up},

/* 0000nnnn01101010 sts DSR,<REG_N>	*/{"sts",{A_DSR,A_REG_N},{HEX_0,REG_N,HEX_6,HEX_A}, arch_sh_dsp_up},

/* 0000nnnn01111010 sts A0,<REG_N>	*/{"sts",{A_A0,A_REG_N},{HEX_0,REG_N,HEX_7,HEX_A}, arch_sh_dsp_up},

/* 0000nnnn10001010 sts X0,<REG_N>	*/{"sts",{A_X0,A_REG_N},{HEX_0,REG_N,HEX_8,HEX_A}, arch_sh_dsp_up},

/* 0000nnnn10011010 sts X1,<REG_N>	*/{"sts",{A_X1,A_REG_N},{HEX_0,REG_N,HEX_9,HEX_A}, arch_sh_dsp_up},

/* 0000nnnn10101010 sts Y0,<REG_N>	*/{"sts",{A_Y0,A_REG_N},{HEX_0,REG_N,HEX_A,HEX_A}, arch_sh_dsp_up},

/* 0000nnnn10111010 sts Y1,<REG_N>	*/{"sts",{A_Y1,A_REG_N},{HEX_0,REG_N,HEX_B,HEX_A}, arch_sh_dsp_up},

/* 0000nnnn01011010 sts FPUL,<REG_N>    */{"sts",{FPUL_M,A_REG_N},{HEX_0,REG_N,HEX_5,HEX_A}, arch_sh2e_up},

/* 0000nnnn01101010 sts FPSCR,<REG_N>   */{"sts",{FPSCR_M,A_REG_N},{HEX_0,REG_N,HEX_6,HEX_A}, arch_sh2e_up},

/* 0100nnnn00000010 sts.l MACH,@-<REG_N>*/{"sts.l",{A_MACH,A_DEC_N},{HEX_4,REG_N,HEX_0,HEX_2}, arch_sh1_up},

/* 0100nnnn00010010 sts.l MACL,@-<REG_N>*/{"sts.l",{A_MACL,A_DEC_N},{HEX_4,REG_N,HEX_1,HEX_2}, arch_sh1_up},

/* 0100nnnn00100010 sts.l PR,@-<REG_N>  */{"sts.l",{A_PR,A_DEC_N},{HEX_4,REG_N,HEX_2,HEX_2}, arch_sh1_up},

/* 0100nnnn01100110 sts.l DSR,@-<REG_N>	*/{"sts.l",{A_DSR,A_DEC_N},{HEX_4,REG_N,HEX_6,HEX_2}, arch_sh_dsp_up},

/* 0100nnnn01110110 sts.l A0,@-<REG_N>	*/{"sts.l",{A_A0,A_DEC_N},{HEX_4,REG_N,HEX_7,HEX_2}, arch_sh_dsp_up},

/* 0100nnnn10000110 sts.l X0,@-<REG_N>	*/{"sts.l",{A_X0,A_DEC_N},{HEX_4,REG_N,HEX_8,HEX_2}, arch_sh_dsp_up},

/* 0100nnnn10010110 sts.l X1,@-<REG_N>	*/{"sts.l",{A_X1,A_DEC_N},{HEX_4,REG_N,HEX_9,HEX_2}, arch_sh_dsp_up},

/* 0100nnnn10100110 sts.l Y0,@-<REG_N>	*/{"sts.l",{A_Y0,A_DEC_N},{HEX_4,REG_N,HEX_A,HEX_2}, arch_sh_dsp_up},

/* 0100nnnn10110110 sts.l Y1,@-<REG_N>	*/{"sts.l",{A_Y1,A_DEC_N},{HEX_4,REG_N,HEX_B,HEX_2}, arch_sh_dsp_up},

/* 0100nnnn01010010 sts.l FPUL,@-<REG_N>*/{"sts.l",{FPUL_M,A_DEC_N},{HEX_4,REG_N,HEX_5,HEX_2}, arch_sh2e_up},

/* 0100nnnn01100010 sts.l FPSCR,@-<REG_N>*/{"sts.l",{FPSCR_M,A_DEC_N},{HEX_4,REG_N,HEX_6,HEX_2}, arch_sh2e_up},

/* 0011nnnnmmmm1000 sub <REG_M>,<REG_N> */{"sub",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_8}, arch_sh1_up},

/* 0011nnnnmmmm1010 subc <REG_M>,<REG_N>*/{"subc",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_A}, arch_sh1_up},

/* 0011nnnnmmmm1011 subv <REG_M>,<REG_N>*/{"subv",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_B}, arch_sh1_up},

/* 0110nnnnmmmm1000 swap.b <REG_M>,<REG_N>*/{"swap.b",{ A_REG_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_8}, arch_sh1_up},

/* 0110nnnnmmmm1001 swap.w <REG_M>,<REG_N>*/{"swap.w",{ A_REG_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_9}, arch_sh1_up},

/* 0000000010101011 synco               */{"synco",{0},{HEX_0,HEX_0,HEX_A,HEX_B}, arch_sh4a_nofp_up},

/* 0100nnnn00011011 tas.b @<REG_N>      */{"tas.b",{A_IND_N},{HEX_4,REG_N,HEX_1,HEX_B}, arch_sh1_up},

/* 11000011i8*1.... trapa #<imm>        */{"trapa",{A_IMM},{HEX_C,HEX_3,IMM0_8}, arch_sh1_up},

/* 11001000i8*1.... tst #<imm>,R0       */{"tst",{A_IMM,A_R0},{HEX_C,HEX_8,IMM0_8}, arch_sh1_up},

/* 0010nnnnmmmm1000 tst <REG_M>,<REG_N> */{"tst",{ A_REG_M,A_REG_N},{HEX_2,REG_N,REG_M,HEX_8}, arch_sh1_up},

/* 11001100i8*1.... tst.b #<imm>,@(R0,GBR)*/{"tst.b",{A_IMM,A_R0_GBR},{HEX_C,HEX_C,IMM0_8}, arch_sh1_up},

/* 11001010i8*1.... xor #<imm>,R0       */{"xor",{A_IMM,A_R0},{HEX_C,HEX_A,IMM0_8}, arch_sh1_up},

/* 0010nnnnmmmm1010 xor <REG_M>,<REG_N> */{"xor",{ A_REG_M,A_REG_N},{HEX_2,REG_N,REG_M,HEX_A}, arch_sh1_up},

/* 11001110i8*1.... xor.b #<imm>,@(R0,GBR)*/{"xor.b",{A_IMM,A_R0_GBR},{HEX_C,HEX_E,IMM0_8}, arch_sh1_up},

/* 0010nnnnmmmm1101 xtrct <REG_M>,<REG_N>*/{"xtrct",{ A_REG_M,A_REG_N},{HEX_2,REG_N,REG_M,HEX_D}, arch_sh1_up},

/* 0000nnnnmmmm0111 mul.l <REG_M>,<REG_N>*/{"mul.l",{ A_REG_M,A_REG_N},{HEX_0,REG_N,REG_M,HEX_7}, arch_sh1_up},

/* 0100nnnn00010000 dt <REG_N>          */{"dt",{A_REG_N},{HEX_4,REG_N,HEX_1,HEX_0}, arch_sh2_up},

/* 0011nnnnmmmm1101 dmuls.l <REG_M>,<REG_N>*/{"dmuls.l",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_D}, arch_sh2_up},

/* 0011nnnnmmmm0101 dmulu.l <REG_M>,<REG_N>*/{"dmulu.l",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_5}, arch_sh2_up},

/* 0000nnnnmmmm1111 mac.l @<REG_M>+,@<REG_N>+*/{"mac.l",{A_INC_M,A_INC_N},{HEX_0,REG_N,REG_M,HEX_F}, arch_sh2_up},

/* 0000nnnn00100011 braf <REG_N>       */{"braf",{A_REG_N},{HEX_0,REG_N,HEX_2,HEX_3}, arch_sh2_up},

/* 0000nnnn00000011 bsrf <REG_N>       */{"bsrf",{A_REG_N},{HEX_0,REG_N,HEX_0,HEX_3}, arch_sh2_up},

/* 111101nnmmmm0000 movs.w @-<REG_N>,<DSP_REG_M> */   {"movs.w",{A_DEC_N,DSP_REG_M},{HEX_F,SDT_REG_N,REG_M,HEX_0}, arch_sh_dsp_up},

/* 111101nnmmmm0001 movs.w @<REG_N>,<DSP_REG_M> */    {"movs.w",{A_IND_N,DSP_REG_M},{HEX_F,SDT_REG_N,REG_M,HEX_4}, arch_sh_dsp_up},

/* 111101nnmmmm0010 movs.w @<REG_N>+,<DSP_REG_M> */   {"movs.w",{A_INC_N,DSP_REG_M},{HEX_F,SDT_REG_N,REG_M,HEX_8}, arch_sh_dsp_up},

/* 111101nnmmmm0011 movs.w @<REG_N>+r8,<DSP_REG_M> */ {"movs.w",{AS_PMOD_N,DSP_REG_M},{HEX_F,SDT_REG_N,REG_M,HEX_C}, arch_sh_dsp_up},

/* 111101nnmmmm0100 movs.w <DSP_REG_M>,@-<REG_N> */   {"movs.w",{DSP_REG_M,A_DEC_N},{HEX_F,SDT_REG_N,REG_M,HEX_1}, arch_sh_dsp_up},

/* 111101nnmmmm0101 movs.w <DSP_REG_M>,@<REG_N> */    {"movs.w",{DSP_REG_M,A_IND_N},{HEX_F,SDT_REG_N,REG_M,HEX_5}, arch_sh_dsp_up},

/* 111101nnmmmm0110 movs.w <DSP_REG_M>,@<REG_N>+ */   {"movs.w",{DSP_REG_M,A_INC_N},{HEX_F,SDT_REG_N,REG_M,HEX_9}, arch_sh_dsp_up},

/* 111101nnmmmm0111 movs.w <DSP_REG_M>,@<REG_N>+r8 */ {"movs.w",{DSP_REG_M,AS_PMOD_N},{HEX_F,SDT_REG_N,REG_M,HEX_D}, arch_sh_dsp_up},

/* 111101nnmmmm1000 movs.l @-<REG_N>,<DSP_REG_M> */   {"movs.l",{A_DEC_N,DSP_REG_M},{HEX_F,SDT_REG_N,REG_M,HEX_2}, arch_sh_dsp_up},

/* 111101nnmmmm1001 movs.l @<REG_N>,<DSP_REG_M> */    {"movs.l",{A_IND_N,DSP_REG_M},{HEX_F,SDT_REG_N,REG_M,HEX_6}, arch_sh_dsp_up},

/* 111101nnmmmm1010 movs.l @<REG_N>+,<DSP_REG_M> */   {"movs.l",{A_INC_N,DSP_REG_M},{HEX_F,SDT_REG_N,REG_M,HEX_A}, arch_sh_dsp_up},

/* 111101nnmmmm1011 movs.l @<REG_N>+r8,<DSP_REG_M> */ {"movs.l",{AS_PMOD_N,DSP_REG_M},{HEX_F,SDT_REG_N,REG_M,HEX_E}, arch_sh_dsp_up},

/* 111101nnmmmm1100 movs.l <DSP_REG_M>,@-<REG_N> */   {"movs.l",{DSP_REG_M,A_DEC_N},{HEX_F,SDT_REG_N,REG_M,HEX_3}, arch_sh_dsp_up},

/* 111101nnmmmm1101 movs.l <DSP_REG_M>,@<REG_N> */    {"movs.l",{DSP_REG_M,A_IND_N},{HEX_F,SDT_REG_N,REG_M,HEX_7}, arch_sh_dsp_up},

/* 111101nnmmmm1110 movs.l <DSP_REG_M>,@<REG_N>+ */   {"movs.l",{DSP_REG_M,A_INC_N},{HEX_F,SDT_REG_N,REG_M,HEX_B}, arch_sh_dsp_up},

/* 111101nnmmmm1111 movs.l <DSP_REG_M>,@<REG_N>+r8 */ {"movs.l",{DSP_REG_M,AS_PMOD_N},{HEX_F,SDT_REG_N,REG_M,HEX_F}, arch_sh_dsp_up},

/* 0*0*0*00** nopx */ {"nopx",{0},{PPI,NOPX}, arch_sh_dsp_up},
/* *0*0*0**00 nopy */ {"nopy",{0},{PPI,NOPY}, arch_sh_dsp_up},
/* n*m*0*01** movx.w @<REG_N>,<DSP_REG_X> */    {"movx.w",{AX_IND_N,DSP_REG_X},{PPI,MOVX,HEX_1}, arch_sh_dsp_up},
/* n*m*0*10** movx.w @<REG_N>+,<DSP_REG_X> */   {"movx.w",{AX_INC_N,DSP_REG_X},{PPI,MOVX,HEX_2}, arch_sh_dsp_up},
/* n*m*0*11** movx.w @<REG_N>+r8,<DSP_REG_X> */ {"movx.w",{AX_PMOD_N,DSP_REG_X},{PPI,MOVX,HEX_3}, arch_sh_dsp_up},
/* n*m*1*01** movx.w <DSP_REG_M>,@<REG_N> */    {"movx.w",{DSP_REG_A_M,AX_IND_N},{PPI,MOVX,HEX_9}, arch_sh_dsp_up},
/* n*m*1*10** movx.w <DSP_REG_M>,@<REG_N>+ */   {"movx.w",{DSP_REG_A_M,AX_INC_N},{PPI,MOVX,HEX_A}, arch_sh_dsp_up},
/* n*m*1*11** movx.w <DSP_REG_M>,@<REG_N>+r8 */ {"movx.w",{DSP_REG_A_M,AX_PMOD_N},{PPI,MOVX,HEX_B}, arch_sh_dsp_up},

/* nnmm000100 movx.w @<REG_Axy>,<DSP_REG_XY> */ {"movx.w",{AXY_IND_N,DSP_REG_XY},{PPI,MOVX_NOPY,HEX_0,HEX_4}, arch_sh4al_dsp_up},
/* nnmm001000 movx.w @<REG_Axy>+,<DSP_REG_XY> */{"movx.w",{AXY_INC_N,DSP_REG_XY},{PPI,MOVX_NOPY,HEX_0,HEX_8}, arch_sh4al_dsp_up},
/* nnmm001100 movx.w @<REG_Axy>+r8,<DSP_REG_XY> */{"movx.w",{AXY_PMOD_N,DSP_REG_XY},{PPI,MOVX_NOPY,HEX_0,HEX_C}, arch_sh4al_dsp_up},
/* nnmm100100 movx.w <DSP_REG_AX>,@<REG_Axy> */ {"movx.w",{DSP_REG_AX,AXY_IND_N},{PPI,MOVX_NOPY,HEX_2,HEX_4}, arch_sh4al_dsp_up},
/* nnmm101000 movx.w <DSP_REG_AX>,@<REG_Axy>+ */{"movx.w",{DSP_REG_AX,AXY_INC_N},{PPI,MOVX_NOPY,HEX_2,HEX_8}, arch_sh4al_dsp_up},
/* nnmm101100 movx.w <DSP_REG_AX>,@<REG_Axy>+r8 */{"movx.w",{DSP_REG_AX,AXY_PMOD_N},{PPI,MOVX_NOPY,HEX_2,HEX_C}, arch_sh4al_dsp_up},

/* nnmm010100 movx.l @<REG_Axy>,<DSP_REG_XY> */ {"movx.l",{AXY_IND_N,DSP_REG_XY},{PPI,MOVX_NOPY,HEX_1,HEX_4}, arch_sh4al_dsp_up},
/* nnmm011000 movx.l @<REG_Axy>+,<DSP_REG_XY> */{"movx.l",{AXY_INC_N,DSP_REG_XY},{PPI,MOVX_NOPY,HEX_1,HEX_8}, arch_sh4al_dsp_up},
/* nnmm011100 movx.l @<REG_Axy>+r8,<DSP_REG_XY> */{"movx.l",{AXY_PMOD_N,DSP_REG_XY},{PPI,MOVX_NOPY,HEX_1,HEX_C}, arch_sh4al_dsp_up},
/* nnmm110100 movx.l <DSP_REG_AX>,@<REG_Axy> */ {"movx.l",{DSP_REG_AX,AXY_IND_N},{PPI,MOVX_NOPY,HEX_3,HEX_4}, arch_sh4al_dsp_up},
/* nnmm111000 movx.l <DSP_REG_AX>,@<REG_Axy>+ */{"movx.l",{DSP_REG_AX,AXY_INC_N},{PPI,MOVX_NOPY,HEX_3,HEX_8}, arch_sh4al_dsp_up},
/* nnmm111100 movx.l <DSP_REG_AX>,@<REG_Axy>+r8 */{"movx.l",{DSP_REG_AX,AXY_PMOD_N},{PPI,MOVX_NOPY,HEX_3,HEX_C}, arch_sh4al_dsp_up},

/* *n*m*0**01 movy.w @<REG_N>,<DSP_REG_Y> */    {"movy.w",{AY_IND_N,DSP_REG_Y},{PPI,MOVY,HEX_1}, arch_sh_dsp_up},
/* *n*m*0**10 movy.w @<REG_N>+,<DSP_REG_Y> */   {"movy.w",{AY_INC_N,DSP_REG_Y},{PPI,MOVY,HEX_2}, arch_sh_dsp_up},
/* *n*m*0**11 movy.w @<REG_N>+r9,<DSP_REG_Y> */ {"movy.w",{AY_PMOD_N,DSP_REG_Y},{PPI,MOVY,HEX_3}, arch_sh_dsp_up},
/* *n*m*1**01 movy.w <DSP_REG_M>,@<REG_N> */    {"movy.w",{DSP_REG_A_M,AY_IND_N},{PPI,MOVY,HEX_9}, arch_sh_dsp_up},
/* *n*m*1**10 movy.w <DSP_REG_M>,@<REG_N>+ */   {"movy.w",{DSP_REG_A_M,AY_INC_N},{PPI,MOVY,HEX_A}, arch_sh_dsp_up},
/* *n*m*1**11 movy.w <DSP_REG_M>,@<REG_N>+r9 */ {"movy.w",{DSP_REG_A_M,AY_PMOD_N},{PPI,MOVY,HEX_B}, arch_sh_dsp_up},

/* nnmm000001 movy.w @<REG_Ayx>,<DSP_REG_YX> */ {"movy.w",{AYX_IND_N,DSP_REG_YX},{PPI,MOVY_NOPX,HEX_0,HEX_1}, arch_sh4al_dsp_up},
/* nnmm000010 movy.w @<REG_Ayx>+,<DSP_REG_YX> */{"movy.w",{AYX_INC_N,DSP_REG_YX},{PPI,MOVY_NOPX,HEX_0,HEX_2}, arch_sh4al_dsp_up},
/* nnmm000011 movy.w @<REG_Ayx>+r8,<DSP_REG_YX> */{"movy.w",{AYX_PMOD_N,DSP_REG_YX},{PPI,MOVY_NOPX,HEX_0,HEX_3}, arch_sh4al_dsp_up},
/* nnmm010001 movy.w <DSP_REG_AY>,@<REG_Ayx> */ {"movy.w",{DSP_REG_AY,AYX_IND_N},{PPI,MOVY_NOPX,HEX_1,HEX_1}, arch_sh4al_dsp_up},
/* nnmm010010 movy.w <DSP_REG_AY>,@<REG_Ayx>+ */{"movy.w",{DSP_REG_AY,AYX_INC_N},{PPI,MOVY_NOPX,HEX_1,HEX_2}, arch_sh4al_dsp_up},
/* nnmm010011 movy.w <DSP_REG_AY>,@<REG_Ayx>+r8 */{"movy.w",{DSP_REG_AY,AYX_PMOD_N},{PPI,MOVY_NOPX,HEX_1,HEX_3}, arch_sh4al_dsp_up},

/* nnmm100001 movy.l @<REG_Ayx>,<DSP_REG_YX> */ {"movy.l",{AYX_IND_N,DSP_REG_YX},{PPI,MOVY_NOPX,HEX_2,HEX_1}, arch_sh4al_dsp_up},
/* nnmm100010 movy.l @<REG_Ayx>+,<DSP_REG_YX> */{"movy.l",{AYX_INC_N,DSP_REG_YX},{PPI,MOVY_NOPX,HEX_2,HEX_2}, arch_sh4al_dsp_up},
/* nnmm100011 movy.l @<REG_Ayx>+r8,<DSP_REG_YX> */{"movy.l",{AYX_PMOD_N,DSP_REG_YX},{PPI,MOVY_NOPX,HEX_2,HEX_3}, arch_sh4al_dsp_up},
/* nnmm110001 movy.l <DSP_REG_AY>,@<REG_Ayx> */ {"movy.l",{DSP_REG_AY,AYX_IND_N},{PPI,MOVY_NOPX,HEX_3,HEX_1}, arch_sh4al_dsp_up},
/* nnmm110010 movy.l <DSP_REG_AY>,@<REG_Ayx>+ */{"movy.l",{DSP_REG_AY,AYX_INC_N},{PPI,MOVY_NOPX,HEX_3,HEX_2}, arch_sh4al_dsp_up},
/* nnmm110011 movy.l <DSP_REG_AY>,@<REG_Ayx>+r8 */{"movy.l",{DSP_REG_AY,AYX_PMOD_N},{PPI,MOVY_NOPX,HEX_3,HEX_3}, arch_sh4al_dsp_up},

/* 01aaeeffxxyyggnn pmuls Se,Sf,Dg */ {"pmuls",{DSP_REG_E,DSP_REG_F,DSP_REG_G},{PPI,PMUL}, arch_sh_dsp_up},
/* 10100000xxyynnnn psubc <DSP_REG_X>,<DSP_REG_Y>,<DSP_REG_N> */
{"psubc",{DSP_REG_X,DSP_REG_Y,DSP_REG_N},{PPI,PPI3,HEX_A,HEX_0}, arch_sh_dsp_up},
/* 10110000xxyynnnn paddc <DSP_REG_X>,<DSP_REG_Y>,<DSP_REG_N> */
{"paddc",{DSP_REG_X,DSP_REG_Y,DSP_REG_N},{PPI,PPI3,HEX_B,HEX_0}, arch_sh_dsp_up},
/* 10000100xxyynnnn pcmp <DSP_REG_X>,<DSP_REG_Y> */
{"pcmp", {DSP_REG_X,DSP_REG_Y},{PPI,PPI3,HEX_8,HEX_4}, arch_sh_dsp_up},
/* 10100100xxyynnnn pwsb <DSP_REG_X>,<DSP_REG_Y>,<DSP_REG_N> */
{"pwsb", {DSP_REG_X,DSP_REG_Y,DSP_REG_N},{PPI,PPI3,HEX_A,HEX_4}, arch_sh_dsp_up},
/* 10110100xxyynnnn pwad <DSP_REG_X>,<DSP_REG_Y>,<DSP_REG_N> */
{"pwad", {DSP_REG_X,DSP_REG_Y,DSP_REG_N},{PPI,PPI3,HEX_B,HEX_4}, arch_sh_dsp_up},
/* 10001000xxyynnnn pabs <DSP_REG_X>,<DSP_REG_N> */
{"pabs", {DSP_REG_X,DSP_REG_N},{PPI,PPI3NC,HEX_8,HEX_8}, arch_sh_dsp_up},
/* 1000100!xx01nnnn pabs <DSP_REG_X>,<DSP_REG_N> */
{"pabs", {DSP_REG_X,DSP_REG_N},{PPI,PPIC,HEX_8,HEX_9,HEX_1}, arch_sh4al_dsp_up},
/* 10101000xxyynnnn pabs <DSP_REG_Y>,<DSP_REG_N> */
{"pabs", {DSP_REG_Y,DSP_REG_N},{PPI,PPI3NC,HEX_A,HEX_8}, arch_sh_dsp_up},
/* 1010100!01yynnnn pabs <DSP_REG_Y>,<DSP_REG_N> */
{"pabs", {DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_A,HEX_9,HEX_4}, arch_sh4al_dsp_up},
/* 10011000xxyynnnn prnd <DSP_REG_X>,<DSP_REG_N> */
{"prnd", {DSP_REG_X,DSP_REG_N},{PPI,PPI3NC,HEX_9,HEX_8}, arch_sh_dsp_up},
/* 1001100!xx01nnnn prnd <DSP_REG_X>,<DSP_REG_N> */
{"prnd", {DSP_REG_X,DSP_REG_N},{PPI,PPIC,HEX_9,HEX_9,HEX_1}, arch_sh4al_dsp_up},
/* 10111000xxyynnnn prnd <DSP_REG_Y>,<DSP_REG_N> */
{"prnd", {DSP_REG_Y,DSP_REG_N},{PPI,PPI3NC,HEX_B,HEX_8}, arch_sh_dsp_up},
/* 1011100!01yynnnn prnd <DSP_REG_Y>,<DSP_REG_N> */
{"prnd", {DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_B,HEX_9,HEX_4}, arch_sh4al_dsp_up},

{"dct",{0},{PPI,PDC,HEX_1}, arch_sh_dsp_up},
{"dcf",{0},{PPI,PDC,HEX_2}, arch_sh_dsp_up},

/* 10000001xxyynnnn pshl <DSP_REG_X>,<DSP_REG_Y>,<DSP_REG_N> */
{"pshl", {DSP_REG_X,DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_8,HEX_1}, arch_sh_dsp_up},
/* 00000iiiiiiinnnn pshl #<imm>,<DSP_REG_N> */ {"pshl",{A_IMM,DSP_REG_N},{PPI,PSH,HEX_0}, arch_sh_dsp_up},
/* 10010001xxyynnnn psha <DSP_REG_X>,<DSP_REG_Y>,<DSP_REG_N> */
{"psha", {DSP_REG_X,DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_9,HEX_1}, arch_sh_dsp_up},
/* 00010iiiiiiinnnn psha #<imm>,<DSP_REG_N> */ {"psha",{A_IMM,DSP_REG_N},{PPI,PSH,HEX_1}, arch_sh_dsp_up},
/* 10100001xxyynnnn psub <DSP_REG_X>,<DSP_REG_Y>,<DSP_REG_N> */
{"psub", {DSP_REG_X,DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_A,HEX_1}, arch_sh_dsp_up},
/* 10000101xxyynnnn psub <DSP_REG_Y>,<DSP_REG_X>,<DSP_REG_N> */
{"psub", {DSP_REG_Y,DSP_REG_X,DSP_REG_N},{PPI,PPIC,HEX_8,HEX_5}, arch_sh4al_dsp_up},
/* 10110001xxyynnnn padd <DSP_REG_X>,<DSP_REG_Y>,<DSP_REG_N> */
{"padd", {DSP_REG_X,DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_B,HEX_1}, arch_sh_dsp_up},
/* 10010101xxyynnnn pand <DSP_REG_X>,<DSP_REG_Y>,<DSP_REG_N> */
{"pand", {DSP_REG_X,DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_9,HEX_5}, arch_sh_dsp_up},
/* 10100101xxyynnnn pxor <DSP_REG_X>,<DSP_REG_Y>,<DSP_REG_N> */
{"pxor", {DSP_REG_X,DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_A,HEX_5}, arch_sh_dsp_up},
/* 10110101xxyynnnn por  <DSP_REG_X>,<DSP_REG_Y>,<DSP_REG_N> */
{"por",  {DSP_REG_X,DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_B,HEX_5}, arch_sh_dsp_up},
/* 10001001xxyynnnn pdec <DSP_REG_X>,<DSP_REG_N> */
{"pdec", {DSP_REG_X,DSP_REG_N},{PPI,PPIC,HEX_8,HEX_9}, arch_sh_dsp_up},
/* 10101001xxyynnnn pdec <DSP_REG_Y>,<DSP_REG_N> */
{"pdec", {DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_A,HEX_9}, arch_sh_dsp_up},
/* 10011001xx00nnnn pinc <DSP_REG_X>,<DSP_REG_N> */
{"pinc", {DSP_REG_X,DSP_REG_N},{PPI,PPIC,HEX_9,HEX_9,HEX_XX00}, arch_sh_dsp_up},
/* 1011100100yynnnn pinc <DSP_REG_Y>,<DSP_REG_N> */
{"pinc", {DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_B,HEX_9,HEX_00YY}, arch_sh_dsp_up},
/* 10001101xxyynnnn pclr <DSP_REG_N> */
{"pclr", {DSP_REG_N},{PPI,PPIC,HEX_8,HEX_D}, arch_sh_dsp_up},
/* 10011101xx00nnnn pdmsb <DSP_REG_X>,<DSP_REG_N> */
{"pdmsb", {DSP_REG_X,DSP_REG_N},{PPI,PPIC,HEX_9,HEX_D,HEX_XX00}, arch_sh_dsp_up},
/* 1011110100yynnnn pdmsb <DSP_REG_Y>,<DSP_REG_N> */
{"pdmsb", {DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_B,HEX_D,HEX_00YY}, arch_sh_dsp_up},
/* 11001001xxyynnnn pneg  <DSP_REG_X>,<DSP_REG_N> */
{"pneg",  {DSP_REG_X,DSP_REG_N},{PPI,PPIC,HEX_C,HEX_9}, arch_sh_dsp_up},
/* 11101001xxyynnnn pneg  <DSP_REG_Y>,<DSP_REG_N> */
{"pneg",  {DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_E,HEX_9}, arch_sh_dsp_up},
/* 11011001xxyynnnn pcopy <DSP_REG_X>,<DSP_REG_N> */
{"pcopy", {DSP_REG_X,DSP_REG_N},{PPI,PPIC,HEX_D,HEX_9}, arch_sh_dsp_up},
/* 11111001xxyynnnn pcopy <DSP_REG_Y>,<DSP_REG_N> */
{"pcopy", {DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_F,HEX_9}, arch_sh_dsp_up},
/* 11001101xxyynnnn psts MACH,<DSP_REG_N> */
{"psts", {A_MACH,DSP_REG_N},{PPI,PPIC,HEX_C,HEX_D}, arch_sh_dsp_up},
/* 11011101xxyynnnn psts MACL,<DSP_REG_N> */
{"psts", {A_MACL,DSP_REG_N},{PPI,PPIC,HEX_D,HEX_D}, arch_sh_dsp_up},
/* 11101101xxyynnnn plds <DSP_REG_N>,MACH */
{"plds", {DSP_REG_N,A_MACH},{PPI,PPIC,HEX_E,HEX_D}, arch_sh_dsp_up},
/* 11111101xxyynnnn plds <DSP_REG_N>,MACL */
{"plds", {DSP_REG_N,A_MACL},{PPI,PPIC,HEX_F,HEX_D}, arch_sh_dsp_up},
/* 10011101xx01zzzz pswap <DSP_REG_X>,<DSP_REG_N> */
{"pswap", {DSP_REG_X,DSP_REG_N},{PPI,PPIC,HEX_9,HEX_D,HEX_1}, arch_sh4al_dsp_up},
/* 1011110101yyzzzz pswap <DSP_REG_Y>,<DSP_REG_N> */
{"pswap", {DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_B,HEX_D,HEX_4}, arch_sh4al_dsp_up},

/* 1111nnnn01011101 fabs <F_REG_N>     */{"fabs",{F_REG_N},{HEX_F,REG_N,HEX_5,HEX_D}, arch_sh2e_up},
/* 1111nnn001011101 fabs <D_REG_N>     */{"fabs",{D_REG_N},{HEX_F,REG_N,HEX_5,HEX_D}, arch_sh4_up | arch_sh2a_up},

/* 1111nnnnmmmm0000 fadd <F_REG_M>,<F_REG_N>*/{"fadd",{F_REG_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_0}, arch_sh2e_up},
/* 1111nnn0mmm00000 fadd <D_REG_M>,<D_REG_N>*/{"fadd",{D_REG_M,D_REG_N},{HEX_F,REG_N,REG_M,HEX_0}, arch_sh4_up | arch_sh2a_up},

/* 1111nnnnmmmm0100 fcmp/eq <F_REG_M>,<F_REG_N>*/{"fcmp/eq",{F_REG_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_4}, arch_sh2e_up},
/* 1111nnn0mmm00100 fcmp/eq <D_REG_M>,<D_REG_N>*/{"fcmp/eq",{D_REG_M,D_REG_N},{HEX_F,REG_N,REG_M,HEX_4}, arch_sh4_up | arch_sh2a_up},

/* 1111nnnnmmmm0101 fcmp/gt <F_REG_M>,<F_REG_N>*/{"fcmp/gt",{F_REG_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_5}, arch_sh2e_up},
/* 1111nnn0mmm00101 fcmp/gt <D_REG_M>,<D_REG_N>*/{"fcmp/gt",{D_REG_M,D_REG_N},{HEX_F,REG_N,REG_M,HEX_5}, arch_sh4_up | arch_sh2a_up},

/* 1111nnn010111101 fcnvds <D_REG_N>,FPUL*/{"fcnvds",{D_REG_N,FPUL_M},{HEX_F,REG_N_D,HEX_B,HEX_D}, arch_sh4_up | arch_sh2a_up},

/* 1111nnn010101101 fcnvsd FPUL,<D_REG_N>*/{"fcnvsd",{FPUL_M,D_REG_N},{HEX_F,REG_N_D,HEX_A,HEX_D}, arch_sh4_up | arch_sh2a_up},

/* 1111nnnnmmmm0011 fdiv <F_REG_M>,<F_REG_N>*/{"fdiv",{F_REG_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_3}, arch_sh2e_up},
/* 1111nnn0mmm00011 fdiv <D_REG_M>,<D_REG_N>*/{"fdiv",{D_REG_M,D_REG_N},{HEX_F,REG_N,REG_M,HEX_3}, arch_sh4_up | arch_sh2a_up},

/* 1111nnmm11101101 fipr <V_REG_M>,<V_REG_N>*/{"fipr",{V_REG_M,V_REG_N},{HEX_F,REG_NM,HEX_E,HEX_D}, arch_sh4_up},

/* 1111nnnn10001101 fldi0 <F_REG_N>    */{"fldi0",{F_REG_N},{HEX_F,REG_N,HEX_8,HEX_D}, arch_sh2e_up},

/* 1111nnnn10011101 fldi1 <F_REG_N>    */{"fldi1",{F_REG_N},{HEX_F,REG_N,HEX_9,HEX_D}, arch_sh2e_up},

/* 1111nnnn00011101 flds <F_REG_N>,FPUL*/{"flds",{F_REG_N,FPUL_M},{HEX_F,REG_N,HEX_1,HEX_D}, arch_sh2e_up},

/* 1111nnnn00101101 float FPUL,<F_REG_N>*/{"float",{FPUL_M,F_REG_N},{HEX_F,REG_N,HEX_2,HEX_D}, arch_sh2e_up},
/* 1111nnn000101101 float FPUL,<D_REG_N>*/{"float",{FPUL_M,D_REG_N},{HEX_F,REG_N,HEX_2,HEX_D}, arch_sh4_up | arch_sh2a_up},

/* 1111nnnnmmmm1110 fmac FR0,<F_REG_M>,<F_REG_N>*/{"fmac",{F_FR0,F_REG_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_E}, arch_sh2e_up},

/* 1111nnnnmmmm1100 fmov <F_REG_M>,<F_REG_N>*/{"fmov",{F_REG_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_C}, arch_sh2e_up},
/* 1111nnn1mmmm1100 fmov <DX_REG_M>,<DX_REG_N>*/{"fmov",{DX_REG_M,DX_REG_N},{HEX_F,REG_N,REG_M,HEX_C}, arch_sh4_up | arch_sh2a_up},

/* 1111nnnnmmmm1000 fmov @<REG_M>,<F_REG_N>*/{"fmov",{A_IND_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_8}, arch_sh2e_up},
/* 1111nnn1mmmm1000 fmov @<REG_M>,<DX_REG_N>*/{"fmov",{A_IND_M,DX_REG_N},{HEX_F,REG_N,REG_M,HEX_8}, arch_sh4_up | arch_sh2a_up},

/* 1111nnnnmmmm1010 fmov <F_REG_M>,@<REG_N>*/{"fmov",{F_REG_M,A_IND_N},{HEX_F,REG_N,REG_M,HEX_A}, arch_sh2e_up},
/* 1111nnnnmmm11010 fmov <DX_REG_M>,@<REG_N>*/{"fmov",{DX_REG_M,A_IND_N},{HEX_F,REG_N,REG_M,HEX_A}, arch_sh4_up | arch_sh2a_up},

/* 1111nnnnmmmm1001 fmov @<REG_M>+,<F_REG_N>*/{"fmov",{A_INC_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_9}, arch_sh2e_up},
/* 1111nnn1mmmm1001 fmov @<REG_M>+,<DX_REG_N>*/{"fmov",{A_INC_M,DX_REG_N},{HEX_F,REG_N,REG_M,HEX_9}, arch_sh4_up | arch_sh2a_up},

/* 1111nnnnmmmm1011 fmov <F_REG_M>,@-<REG_N>*/{"fmov",{F_REG_M,A_DEC_N},{HEX_F,REG_N,REG_M,HEX_B}, arch_sh2e_up},
/* 1111nnnnmmm11011 fmov <DX_REG_M>,@-<REG_N>*/{"fmov",{DX_REG_M,A_DEC_N},{HEX_F,REG_N,REG_M,HEX_B}, arch_sh4_up | arch_sh2a_up},

/* 1111nnnnmmmm0110 fmov @(R0,<REG_M>),<F_REG_N>*/{"fmov",{A_IND_R0_REG_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_6}, arch_sh2e_up},
/* 1111nnn1mmmm0110 fmov @(R0,<REG_M>),<DX_REG_N>*/{"fmov",{A_IND_R0_REG_M,DX_REG_N},{HEX_F,REG_N,REG_M,HEX_6}, arch_sh4_up | arch_sh2a_up},

/* 1111nnnnmmmm0111 fmov <F_REG_M>,@(R0,<REG_N>)*/{"fmov",{F_REG_M,A_IND_R0_REG_N},{HEX_F,REG_N,REG_M,HEX_7}, arch_sh2e_up},
/* 1111nnnnmmm10111 fmov <DX_REG_M>,@(R0,<REG_N>)*/{"fmov",{DX_REG_M,A_IND_R0_REG_N},{HEX_F,REG_N,REG_M,HEX_7}, arch_sh4_up | arch_sh2a_up},

/* 1111nnn1mmmm1000 fmov.d @<REG_M>,<DX_REG_N>*/{"fmov.d",{A_IND_M,DX_REG_N},{HEX_F,REG_N,REG_M,HEX_8}, arch_sh4_up | arch_sh2a_up},

/* 1111nnnnmmm11010 fmov.d <DX_REG_M>,@<REG_N>*/{"fmov.d",{DX_REG_M,A_IND_N},{HEX_F,REG_N,REG_M,HEX_A}, arch_sh4_up | arch_sh2a_up},

/* 1111nnn1mmmm1001 fmov.d @<REG_M>+,<DX_REG_N>*/{"fmov.d",{A_INC_M,DX_REG_N},{HEX_F,REG_N,REG_M,HEX_9}, arch_sh4_up | arch_sh2a_up},

/* 1111nnnnmmm11011 fmov.d <DX_REG_M>,@-<REG_N>*/{"fmov.d",{DX_REG_M,A_DEC_N},{HEX_F,REG_N,REG_M,HEX_B}, arch_sh4_up | arch_sh2a_up},

/* 1111nnn1mmmm0110 fmov.d @(R0,<REG_M>),<DX_REG_N>*/{"fmov.d",{A_IND_R0_REG_M,DX_REG_N},{HEX_F,REG_N,REG_M,HEX_6}, arch_sh4_up | arch_sh2a_up},

/* 1111nnnnmmm10111 fmov.d <DX_REG_M>,@(R0,<REG_N>)*/{"fmov.d",{DX_REG_M,A_IND_R0_REG_N},{HEX_F,REG_N,REG_M,HEX_7}, arch_sh4_up | arch_sh2a_up},
/* 0011nnnnmmmm0001 0011dddddddddddd fmov.d <F_REG_M>,@(<DISP12>,<REG_N>) */
{"fmov.d",{DX_REG_M,A_DISP_REG_N},{HEX_3,REG_N,REG_M,HEX_1,HEX_3,DISP1_12BY8}, arch_sh2a_up | arch_op32},
/* 0011nnnnmmmm0001 0111dddddddddddd fmov.d @(<DISP12>,<REG_M>),F_REG_N */
{"fmov.d",{A_DISP_REG_M,DX_REG_N},{HEX_3,REG_N,REG_M,HEX_1,HEX_7,DISP0_12BY8}, arch_sh2a_up | arch_op32},

/* 1111nnnnmmmm1000 fmov.s @<REG_M>,<F_REG_N>*/{"fmov.s",{A_IND_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_8}, arch_sh2e_up},

/* 1111nnnnmmmm1010 fmov.s <F_REG_M>,@<REG_N>*/{"fmov.s",{F_REG_M,A_IND_N},{HEX_F,REG_N,REG_M,HEX_A}, arch_sh2e_up},

/* 1111nnnnmmmm1001 fmov.s @<REG_M>+,<F_REG_N>*/{"fmov.s",{A_INC_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_9}, arch_sh2e_up},

/* 1111nnnnmmmm1011 fmov.s <F_REG_M>,@-<REG_N>*/{"fmov.s",{F_REG_M,A_DEC_N},{HEX_F,REG_N,REG_M,HEX_B}, arch_sh2e_up},

/* 1111nnnnmmmm0110 fmov.s @(R0,<REG_M>),<F_REG_N>*/{"fmov.s",{A_IND_R0_REG_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_6}, arch_sh2e_up},

/* 1111nnnnmmmm0111 fmov.s <F_REG_M>,@(R0,<REG_N>)*/{"fmov.s",{F_REG_M,A_IND_R0_REG_N},{HEX_F,REG_N,REG_M,HEX_7}, arch_sh2e_up},
/* 0011nnnnmmmm0001 0011dddddddddddd fmov.s <F_REG_M>,@(<DISP12>,<REG_N>) */
{"fmov.s",{F_REG_M,A_DISP_REG_N},{HEX_3,REG_N,REG_M,HEX_1,HEX_3,DISP1_12BY4}, arch_sh2a_up | arch_op32},
/* 0011nnnnmmmm0001 0111dddddddddddd fmov.s @(<DISP12>,<REG_M>),F_REG_N */
{"fmov.s",{A_DISP_REG_M,F_REG_N},{HEX_3,REG_N,REG_M,HEX_1,HEX_7,DISP0_12BY4}, arch_sh2a_up | arch_op32},

/* 1111nnnnmmmm0010 fmul <F_REG_M>,<F_REG_N>*/{"fmul",{F_REG_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_2}, arch_sh2e_up},
/* 1111nnn0mmm00010 fmul <D_REG_M>,<D_REG_N>*/{"fmul",{D_REG_M,D_REG_N},{HEX_F,REG_N,REG_M,HEX_2}, arch_sh4_up | arch_sh2a_up},

/* 1111nnnn01001101 fneg <F_REG_N>     */{"fneg",{F_REG_N},{HEX_F,REG_N,HEX_4,HEX_D}, arch_sh2e_up},
/* 1111nnn001001101 fneg <D_REG_N>     */{"fneg",{D_REG_N},{HEX_F,REG_N,HEX_4,HEX_D}, arch_sh4_up | arch_sh2a_up},

/* 1111011111111101 fpchg               */{"fpchg",{0},{HEX_F,HEX_7,HEX_F,HEX_D}, arch_sh4a_up},

/* 1111101111111101 frchg               */{"frchg",{0},{HEX_F,HEX_B,HEX_F,HEX_D}, arch_sh4_up},

/* 1111nnn011111101 fsca FPUL,<D_REG_N> */{"fsca",{FPUL_M,D_REG_N},{HEX_F,REG_N_D,HEX_F,HEX_D}, arch_sh4_up},

/* 1111001111111101 fschg               */{"fschg",{0},{HEX_F,HEX_3,HEX_F,HEX_D}, arch_sh4_up | arch_sh2a_up},

/* 1111nnnn01101101 fsqrt <F_REG_N>    */{"fsqrt",{F_REG_N},{HEX_F,REG_N,HEX_6,HEX_D}, arch_sh3e_up | arch_sh2a_up},
/* 1111nnn001101101 fsqrt <D_REG_N>    */{"fsqrt",{D_REG_N},{HEX_F,REG_N,HEX_6,HEX_D}, arch_sh4_up | arch_sh2a_up},

/* 1111nnnn01111101 fsrra <F_REG_N>    */{"fsrra",{F_REG_N},{HEX_F,REG_N,HEX_7,HEX_D}, arch_sh4_up},

/* 1111nnnn00001101 fsts FPUL,<F_REG_N>*/{"fsts",{FPUL_M,F_REG_N},{HEX_F,REG_N,HEX_0,HEX_D}, arch_sh2e_up},

/* 1111nnnnmmmm0001 fsub <F_REG_M>,<F_REG_N>*/{"fsub",{F_REG_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_1}, arch_sh2e_up},
/* 1111nnn0mmm00001 fsub <D_REG_M>,<D_REG_N>*/{"fsub",{D_REG_M,D_REG_N},{HEX_F,REG_N,REG_M,HEX_1}, arch_sh4_up | arch_sh2a_up},

/* 1111nnnn00111101 ftrc <F_REG_N>,FPUL*/{"ftrc",{F_REG_N,FPUL_M},{HEX_F,REG_N,HEX_3,HEX_D}, arch_sh2e_up},
/* 1111nnnn00111101 ftrc <D_REG_N>,FPUL*/{"ftrc",{D_REG_N,FPUL_M},{HEX_F,REG_N,HEX_3,HEX_D}, arch_sh4_up | arch_sh2a_up},

/* 1111nn0111111101 ftrv XMTRX_M4,<V_REG_n>*/{"ftrv",{XMTRX_M4,V_REG_N},{HEX_F,REG_N_B01,HEX_F,HEX_D}, arch_sh4_up},

  /* 10000110nnnn0iii bclr #<imm>, <REG_N> */  {"bclr",{A_IMM, A_REG_N},{HEX_8,HEX_6,REG_N,IMM0_3c}, arch_sh2a_nofpu_up},
  /* 0011nnnn0iii1001 0000dddddddddddd bclr.b #<imm>,@(<DISP12>,<REG_N>) */
{"bclr.b",{A_IMM,A_DISP_REG_N},{HEX_3,REG_N,IMM0_3Uc,HEX_9,HEX_0,DISP1_12}, arch_sh2a_nofpu_up | arch_op32},
  /* 10000111nnnn1iii bld #<imm>, <REG_N> */   {"bld",{A_IMM, A_REG_N},{HEX_8,HEX_7,REG_N,IMM0_3s}, arch_sh2a_nofpu_up},
  /* 0011nnnn0iii1001 0011dddddddddddd bld.b #<imm>,@(<DISP12>,<REG_N>) */
{"bld.b",{A_IMM,A_DISP_REG_N},{HEX_3,REG_N,IMM0_3Uc,HEX_9,HEX_3,DISP1_12}, arch_sh2a_nofpu_up | arch_op32},
  /* 10000110nnnn1iii bset #<imm>, <REG_N> */  {"bset",{A_IMM, A_REG_N},{HEX_8,HEX_6,REG_N,IMM0_3s}, arch_sh2a_nofpu_up},
  /* 0011nnnn0iii1001 0001dddddddddddd bset.b #<imm>,@(<DISP12>,<REG_N>) */
{"bset.b",{A_IMM,A_DISP_REG_N},{HEX_3,REG_N,IMM0_3Uc,HEX_9,HEX_1,DISP1_12}, arch_sh2a_nofpu_up | arch_op32},
  /* 10000111nnnn0iii bst #<imm>, <REG_N> */   {"bst",{A_IMM, A_REG_N},{HEX_8,HEX_7,REG_N,IMM0_3c}, arch_sh2a_nofpu_up},
  /* 0011nnnn0iii1001 0010dddddddddddd bst.b #<imm>,@(<DISP12>,<REG_N>) */
{"bst.b",{A_IMM,A_DISP_REG_N},{HEX_3,REG_N,IMM0_3Uc,HEX_9,HEX_2,DISP1_12}, arch_sh2a_nofpu_up | arch_op32},
  /* 0100nnnn10010001 clips.b <REG_N> */       {"clips.b",{A_REG_N},{HEX_4,REG_N,HEX_9,HEX_1}, arch_sh2a_nofpu_up},
  /* 0100nnnn10010101 clips.w <REG_N> */       {"clips.w",{A_REG_N},{HEX_4,REG_N,HEX_9,HEX_5}, arch_sh2a_nofpu_up},
  /* 0100nnnn10000001 clipu.b <REG_N> */       {"clipu.b",{A_REG_N},{HEX_4,REG_N,HEX_8,HEX_1}, arch_sh2a_nofpu_up},
  /* 0100nnnn10000101 clipu.w <REG_N> */       {"clipu.w",{A_REG_N},{HEX_4,REG_N,HEX_8,HEX_5}, arch_sh2a_nofpu_up},
  /* 0100nnnn10010100 divs R0,<REG_N> */       {"divs",{A_R0,A_REG_N},{HEX_4,REG_N,HEX_9,HEX_4}, arch_sh2a_nofpu_up},
  /* 0100nnnn10000100 divu R0,<REG_N> */       {"divu",{A_R0,A_REG_N},{HEX_4,REG_N,HEX_8,HEX_4}, arch_sh2a_nofpu_up},
  /* 0100mmmm01001011 jsr/n @<REG_M>  */       {"jsr/n",{A_IND_M},{HEX_4,REG_M,HEX_4,HEX_B}, arch_sh2a_nofpu_up},
  /* 10000011dddddddd jsr/n @@(<disp>,TBR) */  {"jsr/n",{A_DISP2_TBR},{HEX_8,HEX_3,IMM0_8BY4}, arch_sh2a_nofpu_up},
  /* 0100mmmm11100101 ldbank @<REG_M>,R0 */    {"ldbank",{A_IND_M,A_R0},{HEX_4,REG_M,HEX_E,HEX_5}, arch_sh2a_nofpu_up},
  /* 0100mmmm11110001 movml.l <REG_M>,@-R15 */ {"movml.l",{A_REG_M,A_DEC_R15},{HEX_4,REG_M,HEX_F,HEX_1}, arch_sh2a_nofpu_up},
  /* 0100mmmm11110101 movml.l @R15+,<REG_M> */ {"movml.l",{A_INC_R15,A_REG_M},{HEX_4,REG_M,HEX_F,HEX_5}, arch_sh2a_nofpu_up},
  /* 0100mmmm11110000 movml.l <REG_M>,@-R15 */ {"movmu.l",{A_REG_M,A_DEC_R15},{HEX_4,REG_M,HEX_F,HEX_0}, arch_sh2a_nofpu_up},
  /* 0100mmmm11110100 movml.l @R15+,<REG_M> */ {"movmu.l",{A_INC_R15,A_REG_M},{HEX_4,REG_M,HEX_F,HEX_4}, arch_sh2a_nofpu_up},
  /* 0000nnnn00111001 movrt <REG_N> */         {"movrt",{A_REG_N},{HEX_0,REG_N,HEX_3,HEX_9}, arch_sh2a_nofpu_up},
  /* 0100nnnn10000000 mulr R0,<REG_N> */       {"mulr",{A_R0,A_REG_N},{HEX_4,REG_N,HEX_8,HEX_0}, arch_sh2a_nofpu_up},
  /* 0000000001101000 nott */                  {"nott",{A_END},{HEX_0,HEX_0,HEX_6,HEX_8}, arch_sh2a_nofpu_up},
  /* 0000000001011011 resbank */               {"resbank",{A_END},{HEX_0,HEX_0,HEX_5,HEX_B}, arch_sh2a_nofpu_up},
  /* 0000000001101011 rts/n */                 {"rts/n",{A_END},{HEX_0,HEX_0,HEX_6,HEX_B}, arch_sh2a_nofpu_up},
  /* 0000mmmm01111011 rtv/n <REG_M>*/          {"rtv/n",{A_REG_M},{HEX_0,REG_M,HEX_7,HEX_B}, arch_sh2a_nofpu_up},
  /* 0100nnnn11100001 stbank R0,@<REG_N>*/     {"stbank",{A_R0,A_IND_N},{HEX_4,REG_N,HEX_E,HEX_1}, arch_sh2a_nofpu_up},

/* 0011nnnn0iii1001 0100dddddddddddd band.b #<imm>,@(<DISP12>,<REG_N>) */
{"band.b",{A_IMM,A_DISP_REG_N},{HEX_3,REG_N,IMM0_3Uc,HEX_9,HEX_4,DISP1_12}, arch_sh2a_nofpu_up | arch_op32},
/* 0011nnnn0iii1001 1100dddddddddddd bandnot.b #<imm>,@(<DISP12>,<REG_N>) */
{"bandnot.b",{A_IMM,A_DISP_REG_N},{HEX_3,REG_N,IMM0_3Uc,HEX_9,HEX_C,DISP1_12}, arch_sh2a_nofpu_up | arch_op32},
/* 0011nnnn0iii1001 1011dddddddddddd bldnot.b #<imm>,@(<DISP12>,<REG_N>) */
{"bldnot.b",{A_IMM,A_DISP_REG_N},{HEX_3,REG_N,IMM0_3Uc,HEX_9,HEX_B,DISP1_12}, arch_sh2a_nofpu_up | arch_op32},
/* 0011nnnn0iii1001 0101dddddddddddd bor.b #<imm>,@(<DISP12>,<REG_N>) */
{"bor.b",{A_IMM,A_DISP_REG_N},{HEX_3,REG_N,IMM0_3Uc,HEX_9,HEX_5,DISP1_12}, arch_sh2a_nofpu_up | arch_op32},
/* 0011nnnn0iii1001 1101dddddddddddd bornot.b #<imm>,@(<DISP12>,<REG_N>) */
{"bornot.b",{A_IMM,A_DISP_REG_N},{HEX_3,REG_N,IMM0_3Uc,HEX_9,HEX_D,DISP1_12}, arch_sh2a_nofpu_up | arch_op32},
/* 0011nnnn0iii1001 0110dddddddddddd bxor.b #<imm>,@(<DISP12>,<REG_N>) */
{"bxor.b",{A_IMM,A_DISP_REG_N},{HEX_3,REG_N,IMM0_3Uc,HEX_9,HEX_6,DISP1_12}, arch_sh2a_nofpu_up | arch_op32},
/* 0000nnnniiii0000 iiiiiiiiiiiiiiii movi20 #<imm>,<REG_N> */
{"movi20",{A_IMM,A_REG_N},{HEX_0,REG_N,IMM0_20_4,HEX_0,IMM0_20}, arch_sh2a_nofpu_up | arch_op32},
/* 0000nnnniiii0001 iiiiiiiiiiiiiiii movi20s #<imm>,<REG_N> */
{"movi20s",{A_IMM,A_REG_N},{HEX_0,REG_N,IMM0_20_4,HEX_1,IMM0_20BY8}, arch_sh2a_nofpu_up | arch_op32},
/* 0011nnnnmmmm0001 1000dddddddddddd movu.b @(<DISP12>,<REG_M>),<REG_N> */
{"movu.b",{A_DISP_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_1,HEX_8,DISP0_12}, arch_sh2a_nofpu_up | arch_op32},
/* 0011nnnnmmmm0001 1001dddddddddddd movu.w @(<DISP12>,<REG_M>),<REG_N> */
{"movu.w",{A_DISP_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_1,HEX_9,DISP0_12BY2}, arch_sh2a_nofpu_up | arch_op32},

{ 0, {0}, {0}, 0 }
};

#endif

#ifdef ARCH_all
#define INCLUDE_SHMEDIA
#endif

static void
print_movxy (const sh_opcode_info *op, int rn, int rm,
             fprintf_function fprintf_fn, void *stream)
{
  int n;

  fprintf_fn (stream, "%s\t", op->name);
  for (n = 0; n < 2; n++)
    {
      switch (op->arg[n])
	{
	case A_IND_N:
	case AX_IND_N:
	case AXY_IND_N:
	case AY_IND_N:
	case AYX_IND_N:
	  fprintf_fn (stream, "@r%d", rn);
	  break;
	case A_INC_N:
	case AX_INC_N:
	case AXY_INC_N:
	case AY_INC_N:
	case AYX_INC_N:
	  fprintf_fn (stream, "@r%d+", rn);
	  break;
	case AX_PMOD_N:
	case AXY_PMOD_N:
	  fprintf_fn (stream, "@r%d+r8", rn);
	  break;
	case AY_PMOD_N:
	case AYX_PMOD_N:
	  fprintf_fn (stream, "@r%d+r9", rn);
	  break;
	case DSP_REG_A_M:
	  fprintf_fn (stream, "a%c", '0' + rm);
	  break;
	case DSP_REG_X:
	  fprintf_fn (stream, "x%c", '0' + rm);
	  break;
	case DSP_REG_Y:
	  fprintf_fn (stream, "y%c", '0' + rm);
	  break;
	case DSP_REG_AX:
	  fprintf_fn (stream, "%c%c",
		      (rm & 1) ? 'x' : 'a',
		      (rm & 2) ? '1' : '0');
	  break;
	case DSP_REG_XY:
	  fprintf_fn (stream, "%c%c",
		      (rm & 1) ? 'y' : 'x',
		      (rm & 2) ? '1' : '0');
	  break;
	case DSP_REG_AY:
	  fprintf_fn (stream, "%c%c",
		      (rm & 2) ? 'y' : 'a',
		      (rm & 1) ? '1' : '0');
	  break;
	case DSP_REG_YX:
	  fprintf_fn (stream, "%c%c",
		      (rm & 2) ? 'x' : 'y',
		      (rm & 1) ? '1' : '0');
	  break;
	default:
	  abort ();
	}
      if (n == 0)
	fprintf_fn (stream, ",");
    }
}

/* Print a double data transfer insn.  INSN is just the lower three
   nibbles of the insn, i.e. field a and the bit that indicates if
   a parallel processing insn follows.
   Return nonzero if a field b of a parallel processing insns follows.  */

static void
print_insn_ddt (int insn, struct disassemble_info *info)
{
  fprintf_function fprintf_fn = info->fprintf_func;
  void *stream = info->stream;

  /* If this is just a nop, make sure to emit something.  */
  if (insn == 0x000)
    fprintf_fn (stream, "nopx\tnopy");

  /* If a parallel processing insn was printed before,
     and we got a non-nop, emit a tab.  */
  if ((insn & 0x800) && (insn & 0x3ff))
    fprintf_fn (stream, "\t");

  /* Check if either the x or y part is invalid.  */
  if (((insn & 0xc) == 0 && (insn & 0x2a0))
      || ((insn & 3) == 0 && (insn & 0x150)))
    if (info->mach != bfd_mach_sh_dsp
        && info->mach != bfd_mach_sh3_dsp)
      {
	static const sh_opcode_info *first_movx, *first_movy;
	const sh_opcode_info *op;
	int is_movy;

	if (! first_movx)
	  {
	    for (first_movx = sh_table; first_movx->nibbles[1] != MOVX_NOPY;)
	      first_movx++;
	    for (first_movy = first_movx; first_movy->nibbles[1] != MOVY_NOPX;)
	      first_movy++;
	  }

	is_movy = ((insn & 3) != 0);

	if (is_movy)
	  op = first_movy;
	else
	  op = first_movx;

	while (op->nibbles[2] != (unsigned) ((insn >> 4) & 3)
	       || op->nibbles[3] != (unsigned) (insn & 0xf))
	  op++;

	print_movxy (op,
		     (4 * ((insn & (is_movy ? 0x200 : 0x100)) == 0)
		      + 2 * is_movy
		      + 1 * ((insn & (is_movy ? 0x100 : 0x200)) != 0)),
		     (insn >> 6) & 3,
		     fprintf_fn, stream);
      }
    else
      fprintf_fn (stream, ".word 0x%x", insn);
  else
    {
      static const sh_opcode_info *first_movx, *first_movy;
      const sh_opcode_info *opx, *opy;
      unsigned int insn_x, insn_y;

      if (! first_movx)
	{
	  for (first_movx = sh_table; first_movx->nibbles[1] != MOVX;)
	    first_movx++;
	  for (first_movy = first_movx; first_movy->nibbles[1] != MOVY;)
	    first_movy++;
	}
      insn_x = (insn >> 2) & 0xb;
      if (insn_x)
	{
	  for (opx = first_movx; opx->nibbles[2] != insn_x;)
	    opx++;
	  print_movxy (opx, ((insn >> 9) & 1) + 4, (insn >> 7) & 1,
		       fprintf_fn, stream);
	}
      insn_y = (insn & 3) | ((insn >> 1) & 8);
      if (insn_y)
	{
	  if (insn_x)
	    fprintf_fn (stream, "\t");
	  for (opy = first_movy; opy->nibbles[2] != insn_y;)
	    opy++;
	  print_movxy (opy, ((insn >> 8) & 1) + 6, (insn >> 6) & 1,
		       fprintf_fn, stream);
	}
    }
}

static void
print_dsp_reg (int rm, fprintf_function fprintf_fn, void *stream)
{
  switch (rm)
    {
    case A_A1_NUM:
      fprintf_fn (stream, "a1");
      break;
    case A_A0_NUM:
      fprintf_fn (stream, "a0");
      break;
    case A_X0_NUM:
      fprintf_fn (stream, "x0");
      break;
    case A_X1_NUM:
      fprintf_fn (stream, "x1");
      break;
    case A_Y0_NUM:
      fprintf_fn (stream, "y0");
      break;
    case A_Y1_NUM:
      fprintf_fn (stream, "y1");
      break;
    case A_M0_NUM:
      fprintf_fn (stream, "m0");
      break;
    case A_A1G_NUM:
      fprintf_fn (stream, "a1g");
      break;
    case A_M1_NUM:
      fprintf_fn (stream, "m1");
      break;
    case A_A0G_NUM:
      fprintf_fn (stream, "a0g");
      break;
    default:
      fprintf_fn (stream, "0x%x", rm);
      break;
    }
}

static void
print_insn_ppi (int field_b, struct disassemble_info *info)
{
  static const char *sx_tab[] = { "x0", "x1", "a0", "a1" };
  static const char *sy_tab[] = { "y0", "y1", "m0", "m1" };
  fprintf_function fprintf_fn = info->fprintf_func;
  void *stream = info->stream;
  unsigned int nib1, nib2, nib3;
  unsigned int altnib1, nib4;
  const char *dc = NULL;
  const sh_opcode_info *op;

  if ((field_b & 0xe800) == 0)
    {
      fprintf_fn (stream, "psh%c\t#%d,",
		  field_b & 0x1000 ? 'a' : 'l',
		  (field_b >> 4) & 127);
      print_dsp_reg (field_b & 0xf, fprintf_fn, stream);
      return;
    }
  if ((field_b & 0xc000) == 0x4000 && (field_b & 0x3000) != 0x1000)
    {
      static const char *du_tab[] = { "x0", "y0", "a0", "a1" };
      static const char *se_tab[] = { "x0", "x1", "y0", "a1" };
      static const char *sf_tab[] = { "y0", "y1", "x0", "a1" };
      static const char *sg_tab[] = { "m0", "m1", "a0", "a1" };

      if (field_b & 0x2000)
	{
	  fprintf_fn (stream, "p%s %s,%s,%s\t",
		      (field_b & 0x1000) ? "add" : "sub",
		      sx_tab[(field_b >> 6) & 3],
		      sy_tab[(field_b >> 4) & 3],
		      du_tab[(field_b >> 0) & 3]);
	}
      else if ((field_b & 0xf0) == 0x10
	       && info->mach != bfd_mach_sh_dsp
	       && info->mach != bfd_mach_sh3_dsp)
	{
	  fprintf_fn (stream, "pclr %s \t", du_tab[(field_b >> 0) & 3]);
	}
      else if ((field_b & 0xf3) != 0)
	{
	  fprintf_fn (stream, ".word 0x%x\t", field_b);
	}
      fprintf_fn (stream, "pmuls%c%s,%s,%s",
		  field_b & 0x2000 ? ' ' : '\t',
		  se_tab[(field_b >> 10) & 3],
		  sf_tab[(field_b >>  8) & 3],
		  sg_tab[(field_b >>  2) & 3]);
      return;
    }

  nib1 = PPIC;
  nib2 = field_b >> 12 & 0xf;
  nib3 = field_b >> 8 & 0xf;
  nib4 = field_b >> 4 & 0xf;
  switch (nib3 & 0x3)
    {
    case 0:
      dc = "";
      nib1 = PPI3;
      break;
    case 1:
      dc = "";
      break;
    case 2:
      dc = "dct ";
      nib3 -= 1;
      break;
    case 3:
      dc = "dcf ";
      nib3 -= 2;
      break;
    }
  if (nib1 == PPI3)
    altnib1 = PPI3NC;
  else
    altnib1 = nib1;
  for (op = sh_table; op->name; op++)
    {
      if ((op->nibbles[1] == nib1 || op->nibbles[1] == altnib1)
	  && op->nibbles[2] == nib2
	  && op->nibbles[3] == nib3)
	{
	  int n;

	  switch (op->nibbles[4])
	    {
	    case HEX_0:
	      break;
	    case HEX_XX00:
	      if ((nib4 & 3) != 0)
		continue;
	      break;
	    case HEX_1:
	      if ((nib4 & 3) != 1)
		continue;
	      break;
	    case HEX_00YY:
	      if ((nib4 & 0xc) != 0)
		continue;
	      break;
	    case HEX_4:
	      if ((nib4 & 0xc) != 4)
		continue;
	      break;
	    default:
	      abort ();
	    }
	  fprintf_fn (stream, "%s%s\t", dc, op->name);
	  for (n = 0; n < 3 && op->arg[n] != A_END; n++)
	    {
	      if (n && op->arg[1] != A_END)
		fprintf_fn (stream, ",");
	      switch (op->arg[n])
		{
		case DSP_REG_N:
		  print_dsp_reg (field_b & 0xf, fprintf_fn, stream);
		  break;
		case DSP_REG_X:
		  fprintf_fn (stream, "%s", sx_tab[(field_b >> 6) & 3]);
		  break;
		case DSP_REG_Y:
		  fprintf_fn (stream, "%s", sy_tab[(field_b >> 4) & 3]);
		  break;
		case A_MACH:
		  fprintf_fn (stream, "mach");
		  break;
		case A_MACL:
		  fprintf_fn (stream, "macl");
		  break;
		default:
		  abort ();
		}
	    }
	  return;
	}
    }
  /* Not found.  */
  fprintf_fn (stream, ".word 0x%x", field_b);
}

/* FIXME mvs: movx insns print as ".word 0x%03x", insn & 0xfff
   (ie. the upper nibble is missing).  */
int
print_insn_sh (bfd_vma memaddr, struct disassemble_info *info)
{
  fprintf_function fprintf_fn = info->fprintf_func;
  void *stream = info->stream;
  unsigned char insn[4];
  unsigned char nibs[8];
  int status;
  bfd_vma relmask = ~(bfd_vma) 0;
  const sh_opcode_info *op;
  unsigned int target_arch;
  int allow_op32;

  switch (info->mach)
    {
    case bfd_mach_sh:
      target_arch = arch_sh1;
      break;
    case bfd_mach_sh4:
      target_arch = arch_sh4;
      break;
    case bfd_mach_sh5:
#ifdef INCLUDE_SHMEDIA
      status = print_insn_sh64 (memaddr, info);
      if (status != -2)
	return status;
#endif
      /* When we get here for sh64, it's because we want to disassemble
	 SHcompact, i.e. arch_sh4.  */
      target_arch = arch_sh4;
      break;
    default:
      fprintf (stderr, "sh architecture not supported\n");
      return -1;
    }

  status = info->read_memory_func (memaddr, insn, 2, info);

  if (status != 0)
    {
      info->memory_error_func (status, memaddr, info);
      return -1;
    }

  if (info->endian == BFD_ENDIAN_LITTLE)
    {
      nibs[0] = (insn[1] >> 4) & 0xf;
      nibs[1] = insn[1] & 0xf;

      nibs[2] = (insn[0] >> 4) & 0xf;
      nibs[3] = insn[0] & 0xf;
    }
  else
    {
      nibs[0] = (insn[0] >> 4) & 0xf;
      nibs[1] = insn[0] & 0xf;

      nibs[2] = (insn[1] >> 4) & 0xf;
      nibs[3] = insn[1] & 0xf;
    }
  status = info->read_memory_func (memaddr + 2, insn + 2, 2, info);
  if (status != 0)
    allow_op32 = 0;
  else
    {
      allow_op32 = 1;

      if (info->endian == BFD_ENDIAN_LITTLE)
	{
	  nibs[4] = (insn[3] >> 4) & 0xf;
	  nibs[5] = insn[3] & 0xf;

	  nibs[6] = (insn[2] >> 4) & 0xf;
	  nibs[7] = insn[2] & 0xf;
	}
      else
	{
	  nibs[4] = (insn[2] >> 4) & 0xf;
	  nibs[5] = insn[2] & 0xf;

	  nibs[6] = (insn[3] >> 4) & 0xf;
	  nibs[7] = insn[3] & 0xf;
	}
    }

  if (nibs[0] == 0xf && (nibs[1] & 4) == 0
      && SH_MERGE_ARCH_SET_VALID (target_arch, arch_sh_dsp_up))
    {
      if (nibs[1] & 8)
	{
	  int field_b;

	  status = info->read_memory_func (memaddr + 2, insn, 2, info);

	  if (status != 0)
	    {
	      info->memory_error_func (status, memaddr + 2, info);
	      return -1;
	    }

	  if (info->endian == BFD_ENDIAN_LITTLE)
	    field_b = insn[1] << 8 | insn[0];
	  else
	    field_b = insn[0] << 8 | insn[1];

	  print_insn_ppi (field_b, info);
	  print_insn_ddt ((nibs[1] << 8) | (nibs[2] << 4) | nibs[3], info);
	  return 4;
	}
      print_insn_ddt ((nibs[1] << 8) | (nibs[2] << 4) | nibs[3], info);
      return 2;
    }
  for (op = sh_table; op->name; op++)
    {
      int n;
      int imm = 0;
      int rn = 0;
      int rm = 0;
      int rb = 0;
      int disp_pc;
      bfd_vma disp_pc_addr = 0;
      int disp = 0;
      int has_disp = 0;
      int max_n = SH_MERGE_ARCH_SET (op->arch, arch_op32) ? 8 : 4;

      if (!allow_op32
	  && SH_MERGE_ARCH_SET (op->arch, arch_op32))
	goto fail;

      if (!SH_MERGE_ARCH_SET_VALID (op->arch, target_arch))
	goto fail;
      for (n = 0; n < max_n; n++)
	{
	  int i = op->nibbles[n];

	  if (i < 16)
	    {
	      if (nibs[n] == i)
		continue;
	      goto fail;
	    }
	  switch (i)
	    {
	    case BRANCH_8:
	      imm = (nibs[2] << 4) | (nibs[3]);
	      if (imm & 0x80)
		imm |= ~0xff;
	      imm = ((char) imm) * 2 + 4;
	      goto ok;
	    case BRANCH_12:
	      imm = ((nibs[1]) << 8) | (nibs[2] << 4) | (nibs[3]);
	      if (imm & 0x800)
		imm |= ~0xfff;
	      imm = imm * 2 + 4;
	      goto ok;
	    case IMM0_3c:
	      if (nibs[3] & 0x8)
		goto fail;
	      imm = nibs[3] & 0x7;
	      break;
	    case IMM0_3s:
	      if (!(nibs[3] & 0x8))
		goto fail;
	      imm = nibs[3] & 0x7;
	      break;
	    case IMM0_3Uc:
	      if (nibs[2] & 0x8)
		goto fail;
	      imm = nibs[2] & 0x7;
	      break;
	    case IMM0_3Us:
	      if (!(nibs[2] & 0x8))
		goto fail;
	      imm = nibs[2] & 0x7;
	      break;
	    case DISP0_12:
	    case DISP1_12:
	      disp = (nibs[5] << 8) | (nibs[6] << 4) | nibs[7];
	      has_disp = 1;
	      goto ok;
	    case DISP0_12BY2:
	    case DISP1_12BY2:
	      disp = ((nibs[5] << 8) | (nibs[6] << 4) | nibs[7]) << 1;
	      relmask = ~(bfd_vma) 1;
	      has_disp = 1;
	      goto ok;
	    case DISP0_12BY4:
	    case DISP1_12BY4:
	      disp = ((nibs[5] << 8) | (nibs[6] << 4) | nibs[7]) << 2;
	      relmask = ~(bfd_vma) 3;
	      has_disp = 1;
	      goto ok;
	    case DISP0_12BY8:
	    case DISP1_12BY8:
	      disp = ((nibs[5] << 8) | (nibs[6] << 4) | nibs[7]) << 3;
	      relmask = ~(bfd_vma) 7;
	      has_disp = 1;
	      goto ok;
	    case IMM0_20_4:
	      break;
	    case IMM0_20:
	      imm = ((nibs[2] << 16) | (nibs[4] << 12) | (nibs[5] << 8)
		     | (nibs[6] << 4) | nibs[7]);
	      if (imm & 0x80000)
		imm -= 0x100000;
	      goto ok;
	    case IMM0_20BY8:
	      imm = ((nibs[2] << 16) | (nibs[4] << 12) | (nibs[5] << 8)
		     | (nibs[6] << 4) | nibs[7]);
	      imm <<= 8;
	      if (imm & 0x8000000)
		imm -= 0x10000000;
	      goto ok;
	    case IMM0_4:
	    case IMM1_4:
	      imm = nibs[3];
	      goto ok;
	    case IMM0_4BY2:
	    case IMM1_4BY2:
	      imm = nibs[3] << 1;
	      goto ok;
	    case IMM0_4BY4:
	    case IMM1_4BY4:
	      imm = nibs[3] << 2;
	      goto ok;
	    case IMM0_8:
	    case IMM1_8:
	      imm = (nibs[2] << 4) | nibs[3];
	      disp = imm;
	      has_disp = 1;
	      if (imm & 0x80)
		imm -= 0x100;
	      goto ok;
	    case PCRELIMM_8BY2:
	      imm = ((nibs[2] << 4) | nibs[3]) << 1;
	      relmask = ~(bfd_vma) 1;
	      goto ok;
	    case PCRELIMM_8BY4:
	      imm = ((nibs[2] << 4) | nibs[3]) << 2;
	      relmask = ~(bfd_vma) 3;
	      goto ok;
	    case IMM0_8BY2:
	    case IMM1_8BY2:
	      imm = ((nibs[2] << 4) | nibs[3]) << 1;
	      goto ok;
	    case IMM0_8BY4:
	    case IMM1_8BY4:
	      imm = ((nibs[2] << 4) | nibs[3]) << 2;
	      goto ok;
	    case REG_N_D:
	      if ((nibs[n] & 1) != 0)
		goto fail;
	      /* fall through */
	    case REG_N:
	      rn = nibs[n];
	      break;
	    case REG_M:
	      rm = nibs[n];
	      break;
	    case REG_N_B01:
	      if ((nibs[n] & 0x3) != 1 /* binary 01 */)
		goto fail;
	      rn = (nibs[n] & 0xc) >> 2;
	      break;
	    case REG_NM:
	      rn = (nibs[n] & 0xc) >> 2;
	      rm = (nibs[n] & 0x3);
	      break;
	    case REG_B:
	      rb = nibs[n] & 0x07;
	      break;
	    case SDT_REG_N:
	      /* sh-dsp: single data transfer.  */
	      rn = nibs[n];
	      if ((rn & 0xc) != 4)
		goto fail;
	      rn = rn & 0x3;
	      rn |= (!(rn & 2)) << 2;
	      break;
	    case PPI:
	    case REPEAT:
	      goto fail;
	    default:
	      abort ();
	    }
	}

    ok:
      /* sh2a has D_REG but not X_REG.  We don't know the pattern
	 doesn't match unless we check the output args to see if they
	 make sense.  */
      if (target_arch == arch_sh2a
	  && ((op->arg[0] == DX_REG_M && (rm & 1) != 0)
	      || (op->arg[1] == DX_REG_N && (rn & 1) != 0)))
	goto fail;

      fprintf_fn (stream, "%s\t", op->name);
      disp_pc = 0;
      for (n = 0; n < 3 && op->arg[n] != A_END; n++)
	{
	  if (n && op->arg[1] != A_END)
	    fprintf_fn (stream, ",");
	  switch (op->arg[n])
	    {
	    case A_IMM:
	      fprintf_fn (stream, "#%d", imm);
	      break;
	    case A_R0:
	      fprintf_fn (stream, "r0");
	      break;
	    case A_REG_N:
	      fprintf_fn (stream, "r%d", rn);
	      break;
	    case A_INC_N:
	    case AS_INC_N:
	      fprintf_fn (stream, "@r%d+", rn);
	      break;
	    case A_DEC_N:
	    case AS_DEC_N:
	      fprintf_fn (stream, "@-r%d", rn);
	      break;
	    case A_IND_N:
	    case AS_IND_N:
	      fprintf_fn (stream, "@r%d", rn);
	      break;
	    case A_DISP_REG_N:
	      fprintf_fn (stream, "@(%d,r%d)", has_disp?disp:imm, rn);
	      break;
	    case AS_PMOD_N:
	      fprintf_fn (stream, "@r%d+r8", rn);
	      break;
	    case A_REG_M:
	      fprintf_fn (stream, "r%d", rm);
	      break;
	    case A_INC_M:
	      fprintf_fn (stream, "@r%d+", rm);
	      break;
	    case A_DEC_M:
	      fprintf_fn (stream, "@-r%d", rm);
	      break;
	    case A_IND_M:
	      fprintf_fn (stream, "@r%d", rm);
	      break;
	    case A_DISP_REG_M:
	      fprintf_fn (stream, "@(%d,r%d)", has_disp?disp:imm, rm);
	      break;
	    case A_REG_B:
	      fprintf_fn (stream, "r%d_bank", rb);
	      break;
	    case A_DISP_PC:
	      disp_pc = 1;
	      disp_pc_addr = imm + 4 + (memaddr & relmask);
	      (*info->print_address_func) (disp_pc_addr, info);
	      break;
	    case A_IND_R0_REG_N:
	      fprintf_fn (stream, "@(r0,r%d)", rn);
	      break;
	    case A_IND_R0_REG_M:
	      fprintf_fn (stream, "@(r0,r%d)", rm);
	      break;
	    case A_DISP_GBR:
	      fprintf_fn (stream, "@(%d,gbr)", has_disp?disp:imm);
	      break;
	    case A_TBR:
	      fprintf_fn (stream, "tbr");
	      break;
	    case A_DISP2_TBR:
	      fprintf_fn (stream, "@@(%d,tbr)", has_disp?disp:imm);
	      break;
	    case A_INC_R15:
	      fprintf_fn (stream, "@r15+");
	      break;
	    case A_DEC_R15:
	      fprintf_fn (stream, "@-r15");
	      break;
	    case A_R0_GBR:
	      fprintf_fn (stream, "@(r0,gbr)");
	      break;
	    case A_BDISP12:
	    case A_BDISP8:
                {
                    bfd_vma addr;
                    addr = imm + memaddr;
                    (*info->print_address_func) (addr, info);
                }
	      break;
	    case A_SR:
	      fprintf_fn (stream, "sr");
	      break;
	    case A_GBR:
	      fprintf_fn (stream, "gbr");
	      break;
	    case A_VBR:
	      fprintf_fn (stream, "vbr");
	      break;
	    case A_DSR:
	      fprintf_fn (stream, "dsr");
	      break;
	    case A_MOD:
	      fprintf_fn (stream, "mod");
	      break;
	    case A_RE:
	      fprintf_fn (stream, "re");
	      break;
	    case A_RS:
	      fprintf_fn (stream, "rs");
	      break;
	    case A_A0:
	      fprintf_fn (stream, "a0");
	      break;
	    case A_X0:
	      fprintf_fn (stream, "x0");
	      break;
	    case A_X1:
	      fprintf_fn (stream, "x1");
	      break;
	    case A_Y0:
	      fprintf_fn (stream, "y0");
	      break;
	    case A_Y1:
	      fprintf_fn (stream, "y1");
	      break;
	    case DSP_REG_M:
	      print_dsp_reg (rm, fprintf_fn, stream);
	      break;
	    case A_SSR:
	      fprintf_fn (stream, "ssr");
	      break;
	    case A_SPC:
	      fprintf_fn (stream, "spc");
	      break;
	    case A_MACH:
	      fprintf_fn (stream, "mach");
	      break;
	    case A_MACL:
	      fprintf_fn (stream, "macl");
	      break;
	    case A_PR:
	      fprintf_fn (stream, "pr");
	      break;
	    case A_SGR:
	      fprintf_fn (stream, "sgr");
	      break;
	    case A_DBR:
	      fprintf_fn (stream, "dbr");
	      break;
	    case F_REG_N:
	      fprintf_fn (stream, "fr%d", rn);
	      break;
	    case F_REG_M:
	      fprintf_fn (stream, "fr%d", rm);
	      break;
	    case DX_REG_N:
	      if (rn & 1)
		{
		  fprintf_fn (stream, "xd%d", rn & ~1);
		  break;
		}
	      /* fallthrough */
	    case D_REG_N:
	      fprintf_fn (stream, "dr%d", rn);
	      break;
	    case DX_REG_M:
	      if (rm & 1)
		{
		  fprintf_fn (stream, "xd%d", rm & ~1);
		  break;
		}
	      /* fallthrough */
	    case D_REG_M:
	      fprintf_fn (stream, "dr%d", rm);
	      break;
	    case FPSCR_M:
	    case FPSCR_N:
	      fprintf_fn (stream, "fpscr");
	      break;
	    case FPUL_M:
	    case FPUL_N:
	      fprintf_fn (stream, "fpul");
	      break;
	    case F_FR0:
	      fprintf_fn (stream, "fr0");
	      break;
	    case V_REG_N:
	      fprintf_fn (stream, "fv%d", rn * 4);
	      break;
	    case V_REG_M:
	      fprintf_fn (stream, "fv%d", rm * 4);
	      break;
	    case XMTRX_M4:
	      fprintf_fn (stream, "xmtrx");
	      break;
	    default:
	      abort ();
	    }
	}

#if 0
      /* This code prints instructions in delay slots on the same line
         as the instruction which needs the delay slots.  This can be
         confusing, since other disassembler don't work this way, and
         it means that the instructions are not all in a line.  So I
         disabled it.  Ian.  */
      if (!(info->flags & 1)
	  && (op->name[0] == 'j'
	      || (op->name[0] == 'b'
		  && (op->name[1] == 'r'
		      || op->name[1] == 's'))
	      || (op->name[0] == 'r' && op->name[1] == 't')
	      || (op->name[0] == 'b' && op->name[2] == '.')))
	{
	  info->flags |= 1;
	  fprintf_fn (stream, "\t(slot ");
	  print_insn_sh (memaddr + 2, info);
	  info->flags &= ~1;
	  fprintf_fn (stream, ")");
	  return 4;
	}
#endif

      if (disp_pc && strcmp (op->name, "mova") != 0)
	{
	  int size;
	  bfd_byte bytes[4];

	  if (relmask == ~(bfd_vma) 1)
	    size = 2;
	  else
	    size = 4;
	  status = info->read_memory_func (disp_pc_addr, bytes, size, info);
	  if (status == 0)
	    {
	      unsigned int val;

	      if (size == 2)
		{
		  if (info->endian == BFD_ENDIAN_LITTLE)
		    val = bfd_getl16 (bytes);
		  else
		    val = bfd_getb16 (bytes);
		}
	      else
		{
		  if (info->endian == BFD_ENDIAN_LITTLE)
		    val = bfd_getl32 (bytes);
		  else
		    val = bfd_getb32 (bytes);
		}
	      if ((*info->symbol_at_address_func) (val, info))
		{
		  fprintf_fn (stream, "\t! ");
		  (*info->print_address_func) (val, info);
		}
	      else
		fprintf_fn (stream, "\t! 0x%x", val);
	    }
	}

      return SH_MERGE_ARCH_SET (op->arch, arch_op32) ? 4 : 2;
    fail:
      ;

    }
  fprintf_fn (stream, ".word 0x%x%x%x%x", nibs[0], nibs[1], nibs[2], nibs[3]);
  return 2;
}
