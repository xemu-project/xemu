/*
 * MCPX DSP emulator
 *
 * Copyright (c) 2015 espes
 * Copyright (c) 2018-2026 Matt Borgerson
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

#ifndef DSP_H
#define DSP_H

#include <stdint.h>
#include <stdbool.h>

#include "dsp_mem.h"
#include "dsp_dma.h"

typedef struct DSPState DSPState;
typedef struct Dsp56300Jit Dsp56300Jit;

/* Register file width, the layout Dsp56300State and the snapshot share. */
#define DSP_REG_MAX 64

/*
 * The core's state as saved and loaded: the JIT syncs to and from this
 * struct around a snapshot.
 *
 * TODO: The layout is the retired C interpreter's, kept so existing
 * snapshots still load. It has 16-bit interrupt fields and only 4 interrupt
 * slots, losing fidelity against the JIT's 24-bit addresses and 128 IVT
 * slots. Simplify it to match Dsp56300State and bump the vmstate version.
 */
typedef struct DspCoreState {
    uint32_t pc;
    uint32_t registers[DSP_REG_MAX];
    uint32_t stack[2][16];

    uint32_t xram[DSP_XRAM_SIZE];
    uint32_t yram[DSP_YRAM_SIZE];
    uint32_t pram[DSP_PRAM_SIZE];
    uint32_t mixbuffer[DSP_MIXBUFFER_SIZE];
    uint32_t periph[DSP_PERIPH_SIZE];

    uint32_t cycle_count;
    uint16_t instr_cycle;
    bool halt_requested;
    bool is_gp;

    uint32_t loop_rep;
    uint32_t pc_on_rep;
    uint32_t cur_inst;
    uint32_t cur_inst_len;

    uint16_t interrupt_state;
    uint16_t interrupt_instr_fetch;
    uint16_t interrupt_save_pc;
    uint16_t interrupt_counter;
    uint16_t interrupt_ipl_to_raise;
    uint16_t interrupt_pipeline_count;
    int16_t interrupt_ipl[12]; /* only [0..3] used; size 12 for snapshot compat */
    uint16_t interrupt_is_pending[12]; /* only [0..3] used; size 12 for snapshot compat */
} DspCoreState;

struct DSPState {
    Dsp56300Jit *jit;
    /* The core's memory, owned here and mapped into the JIT. */
    uint32_t *xram;
    uint32_t *yram;
    uint32_t *pram;

    DspCoreState core;
    DSPDMAState dma;
    int save_cycles;

    uint32_t interrupts;
    /* Frame starts signalled but not yet consumed by the program. Silicon's
     * cores never fall behind their edge-latched start; here a core can, so
     * starts are counted (capped) and re-armed as the program consumes
     * each one. */
    uint32_t frame_starts_pending, frame_starts_dropped;
    /* Set by the program's first frame-complete after a reset. Until then
     * an acknowledge of the start-frame bit clears the bit without
     * consuming a latched start: the program is still initialising. */
    bool halted_since_reset;

    bool is_gp;
};

DSPState *dsp_init(void *rw_opaque, dsp_scratch_rw_func scratch_rw,
                   dsp_fifo_rw_func fifo_rw, bool is_gp);
void dsp_destroy(DSPState *dsp);
void dsp_reset(DSPState *dsp);

void dsp_step(DSPState *dsp);
void dsp_run(DSPState *dsp, int cycles);

void dsp_bootstrap(DSPState *dsp);
void dsp_start_frame(DSPState *dsp);
void dsp_get_registers(DSPState *dsp, uint32_t out[64]);
void dsp_get_pc_sp(DSPState *dsp, uint32_t *pc, uint32_t *sp,
                   uint32_t ssh[16]);

uint32_t dsp_read_memory(DSPState *dsp, char space, uint32_t addr);
void dsp_write_memory(DSPState *dsp, char space, uint32_t address,
                      uint32_t value);

bool dsp_get_halt_requested(DSPState *dsp);
void dsp_set_halt_requested(DSPState *dsp, bool idle);
uint32_t dsp_get_cycle_count(DSPState *dsp);
void dsp_set_cycle_count(DSPState *dsp, uint32_t count);
void dsp_invalidate_opcache(DSPState *dsp);

/* Snapshot synchronization: the core's state to/from DspCoreState. */
void dsp_sync_to_vm(DSPState *dsp);
bool dsp_frame_start_pending(DSPState *dsp);
uint32_t dsp_frame_starts_dropped(DSPState *dsp);
/* Frame-complete flags counted per core (dsp.c) and DMA words landed in P
 * memory (dsp_dma.c), for the scheduler trace report (gp_ep.c). */
extern uint64_t g_dsp_gp_halts, g_dsp_ep_halts, g_dsp_dma_p_writes;
/* The JIT's translation counters. */
void dsp_jit_get_stats(DSPState *dsp, uint64_t *compiles, uint64_t *compile_ns,
                       uint64_t *compile_ns_worst,
                       uint64_t *invalidations, uint64_t *cache_hits,
                       uint64_t *retained, uint64_t *block_entries,
                       uint64_t *code_bytes);
/* The JIT's per-start-PC block-entry histogram (the first dump is what
 * enables profiling). */
void dsp_jit_dump_block_profile(DSPState *dsp, const char *path);
/* gp_ep.c: the MCPX_EP_SNAPSHOT node trigger, see there. */
void mcpx_apu_ep_snapshot_on_eol_clear(DSPState *dsp);
void dsp_sync_from_vm(DSPState *dsp);

#endif /* DSP_H */
