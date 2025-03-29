/*
 * DSP56300 emulator
 *
 * Copyright (c) 2015 espes
 *
 * Adapted from Hatari DSP M56001 emulation
 * (C) 2003-2008 ARAnyM developer team
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

#ifndef HW_XBOX_MCPX_DSP_DSP_CPU_REGS_H
#define HW_XBOX_MCPX_DSP_DSP_CPU_REGS_H

#define DSP_OMR_MA  0x00
#define DSP_OMR_MB  0x01
#define DSP_OMR_DE  0x02
#define DSP_OMR_SD  0x06
#define DSP_OMR_EA  0x07

#define DSP_SR_C    0x00
#define DSP_SR_V    0x01
#define DSP_SR_Z    0x02
#define DSP_SR_N    0x03
#define DSP_SR_U    0x04
#define DSP_SR_E    0x05
#define DSP_SR_L    0x06

#define DSP_SR_I0   0x08
#define DSP_SR_I1   0x09
#define DSP_SR_S0   0x0a
#define DSP_SR_S1   0x0b
#define DSP_SR_T    0x0d
#define DSP_SR_LF   0x0f

#define DSP_SP_SE   0x04
#define DSP_SP_UF   0x05

/* Registers numbers in dsp.registers[] */
#define DSP_REG_X0  0x04
#define DSP_REG_X1  0x05
#define DSP_REG_Y0  0x06
#define DSP_REG_Y1  0x07
#define DSP_REG_A0  0x08
#define DSP_REG_B0  0x09
#define DSP_REG_A2  0x0a
#define DSP_REG_B2  0x0b
#define DSP_REG_A1  0x0c
#define DSP_REG_B1  0x0d
#define DSP_REG_A   0x0e
#define DSP_REG_B   0x0f

#define DSP_REG_R0  0x10
#define DSP_REG_R1  0x11
#define DSP_REG_R2  0x12
#define DSP_REG_R3  0x13
#define DSP_REG_R4  0x14
#define DSP_REG_R5  0x15
#define DSP_REG_R6  0x16
#define DSP_REG_R7  0x17

#define DSP_REG_N0  0x18
#define DSP_REG_N1  0x19
#define DSP_REG_N2  0x1a
#define DSP_REG_N3  0x1b
#define DSP_REG_N4  0x1c
#define DSP_REG_N5  0x1d
#define DSP_REG_N6  0x1e
#define DSP_REG_N7  0x1f

#define DSP_REG_M0  0x20
#define DSP_REG_M1  0x21
#define DSP_REG_M2  0x22
#define DSP_REG_M3  0x23
#define DSP_REG_M4  0x24
#define DSP_REG_M5  0x25
#define DSP_REG_M6  0x26
#define DSP_REG_M7  0x27

#define DSP_REG_SR  0x39
#define DSP_REG_OMR 0x3a
#define DSP_REG_SP  0x3b
#define DSP_REG_SSH 0x3c
#define DSP_REG_SSL 0x3d
#define DSP_REG_LA  0x3e
#define DSP_REG_LC  0x3f

#define DSP_REG_NULL    0x00
#define DSP_REG_LCSAVE  0x30

#define DSP_REG_MAX 0x40

/* Memory spaces for dsp.ram[], dsp.rom[] */
#define DSP_SPACE_X 0x00
#define DSP_SPACE_Y 0x01
#define DSP_SPACE_P 0x02

#define DSP_XRAM_SIZE 4096
#define DSP_YRAM_SIZE 2048
#define DSP_PRAM_SIZE 4096

#define DSP_MIXBUFFER_BASE 0x001400
#define DSP_MIXBUFFER_SIZE 1024

#define DSP_PERIPH_BASE 0xFFFF80
#define DSP_PERIPH_SIZE 128

#define DSP_INTERRUPT_NONE      0x0
#define DSP_INTERRUPT_DISABLED  0x1
#define DSP_INTERRUPT_LONG      0x2

#define DSP_INTER_RESET         0x0
#define DSP_INTER_ILLEGAL       0x1
#define DSP_INTER_STACK_ERROR       0x2
#define DSP_INTER_TRACE         0x3
#define DSP_INTER_SWI           0x4
#define DSP_INTER_HOST_COMMAND      0x5
#define DSP_INTER_HOST_RCV_DATA     0x6
#define DSP_INTER_HOST_TRX_DATA     0x7
#define DSP_INTER_SSI_RCV_DATA_E    0x8
#define DSP_INTER_SSI_RCV_DATA      0x9
#define DSP_INTER_SSI_TRX_DATA_E    0xa
#define DSP_INTER_SSI_TRX_DATA      0xb

#endif
