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

#ifndef DSP_CPU_H
#define DSP_CPU_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "dsp_cpu_regs.h"

typedef enum {
    DSP_TRACE_MODE,
    DSP_DISASM_MODE
} dsp_trace_disasm_t;

typedef struct dsp_interrupt_s {
    const uint16_t inter;
    const uint16_t vectorAddr;
    const uint16_t periph;
    const char *name;
} dsp_interrupt_t;

typedef struct dsp_core_s dsp_core_t;

struct dsp_core_s {
    bool is_gp;
    bool is_idle;
    uint32_t cycle_count;

    /* DSP instruction Cycle counter */
    uint16_t instr_cycle;

    /* Registers */
    uint32_t pc;
    uint32_t registers[DSP_REG_MAX];

    /* stack[0=ssh], stack[1=ssl] */
    uint32_t stack[2][16];

    uint32_t xram[DSP_XRAM_SIZE];
    uint32_t yram[DSP_YRAM_SIZE];
    uint32_t pram[DSP_PRAM_SIZE];
    const void *pram_opcache[DSP_PRAM_SIZE];

    uint32_t mixbuffer[DSP_MIXBUFFER_SIZE];

    /* peripheral space, x:0xffff80-0xffffff */
    uint32_t periph[DSP_PERIPH_SIZE];

    /* Misc */
    uint32_t loop_rep;      /* executing rep ? */
    uint32_t pc_on_rep;     /* True if PC is on REP instruction */

    /* Interruptions */
    uint16_t interrupt_state;        /* NONE, FAST or LONG interrupt */
    uint16_t interrupt_instr_fetch;        /* vector of the current interrupt */
    uint16_t interrupt_save_pc;        /* save next pc value before interrupt */
    uint16_t interrupt_counter;        /* count number of pending interrupts */
    uint16_t interrupt_ipl_to_raise;     /* save the IPL level to save in the SR register */
    uint16_t interrupt_pipeline_count; /* used to prefetch correctly the 2 inter instructions */
    int16_t interrupt_ipl[12];     /* store the current IPL for each interrupt */
    uint16_t interrupt_is_pending[12];  /* store if interrupt is pending for each interrupt */

    /* callbacks */
    uint32_t (*read_peripheral)(dsp_core_t* core, uint32_t address);
    void (*write_peripheral)(dsp_core_t* core, uint32_t address, uint32_t value);

    /* runtime data */

    /* Instructions per second */
#ifdef DSP_COUNT_IPS
    uint32_t start_time;
#endif
    uint32_t num_inst;

    /* Length of current instruction */
    uint32_t cur_inst_len; /* =0:jump, >0:increment */
    /* Current instruction */
    uint32_t cur_inst;

    char str_disasm_memory[2][50];     /* Buffer for memory change text in disasm mode */
    uint32_t disasm_memory_ptr;        /* Pointer for memory change in disasm mode */

    bool exception_debugging;


    /* disasm data */

    /* Previous instruction */
    uint32_t disasm_prev_inst_pc;
    bool disasm_is_looping;

    /* Used to display dc instead of unknown instruction for illegal opcodes */
    dsp_trace_disasm_t disasm_mode;

    uint32_t disasm_cur_inst;
    uint16_t disasm_cur_inst_len;

    /* Current instruction */
    char disasm_str_instr[256];
    char disasm_str_instr2[523];
    char disasm_parallelmove_name[64];

    /**********************************
     *  Register change
     **********************************/

    uint32_t disasm_registers_save[64];
#ifdef DSP_DISASM_REG_PC
    uint32_t pc_save;
#endif

};

/* Functions */
void dsp56k_reset_cpu(dsp_core_t* dsp);		/* Set dsp_core to use */
void dsp56k_execute_instruction(dsp_core_t* dsp);	/* Execute 1 instruction */

uint32_t dsp56k_read_memory(dsp_core_t* dsp, int space, uint32_t address);
void dsp56k_write_memory(dsp_core_t* dsp, int space, uint32_t address, uint32_t value);

/* Interrupt relative functions */
void dsp56k_add_interrupt(dsp_core_t* dsp, uint16_t inter);

#endif	/* DSP_CPU_H */
