/*
 * MCPX DSP emulator - Rust JIT backend
 *
 * Copyright (c) 2026 Matt Borgerson
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
#include "dsp_internal.h"
#include "debug.h"

#include <dsp56300.h>

/* Map C interpreter interrupt indices to architectural IVT slots.
 * Architectural slot = vectorAddr / 2 (per DSP56300FM Table 2-2). */
static const int cinterp_to_arch[4] = {
    0, /* RESET:       vec $00, slot 0 */
    2, /* ILLEGAL:     vec $04, slot 2 */
    1, /* STACK_ERROR: vec $02, slot 1 */
    4, /* TRAP:        vec $08, slot 4 */
};

typedef struct JitBackend {
    Dsp56300Jit *jit;
    uint32_t *xram;
    uint32_t *yram;
    uint32_t *pram;
} JitBackend;

static JitBackend *jit_be(DSPState *dsp)
{
    return (JitBackend *)dsp->backend;
}

static uint32_t jit_dma_mem_read(void *opaque, int space, uint32_t addr)
{
    return dsp56300_read_memory((const Dsp56300Jit *)opaque,
                                (Dsp56300MemSpace)space, addr);
}

static void jit_dma_mem_write(void *opaque, int space, uint32_t addr,
                              uint32_t value)
{
    dsp56300_write_memory((Dsp56300Jit *)opaque, (Dsp56300MemSpace)space, addr,
                          value);
}

static uint32_t jit_read_peripheral(void *opaque, uint32_t address)
{
    return read_peripheral((DSPState *)opaque, address);
}

static void jit_write_peripheral(void *opaque, uint32_t address, uint32_t value)
{
    write_peripheral((DSPState *)opaque, address, value);
}

static void dsp_jit_reset(DSPState *dsp)
{
    dsp56300_reset(jit_be(dsp)->jit);
}

static void dsp_jit_step(DSPState *dsp)
{
    dsp56300_step(jit_be(dsp)->jit);
}

static void dsp_jit_run(DSPState *dsp, int cycles)
{
    dsp56300_run(jit_be(dsp)->jit, cycles);
}

static void dsp_jit_bootstrap(DSPState *dsp)
{
    JitBackend *be = jit_be(dsp);

    /* Load scratch memory into PRAM (C-side owned buffer) */
    dsp->dma.scratch_rw(dsp->dma.rw_opaque, (uint8_t *)be->pram, 0, 0x800 * 4,
                        false);
    for (int i = 0; i < 0x800; i++) {
        if (be->pram[i] & 0xff000000) {
            DPRINTF("Bootstrap %04x: %08x\n", i, be->pram[i]);
            be->pram[i] &= 0x00ffffff;
        }
    }
    dsp56300_invalidate_cache(be->jit);
}

static uint32_t dsp_jit_read_memory(DSPState *dsp, char space, uint32_t addr)
{
    Dsp56300MemSpace space_id = (space == 'X') ? DSP56300_MEM_SPACE_X :
                                (space == 'Y') ? DSP56300_MEM_SPACE_Y :
                                                 DSP56300_MEM_SPACE_P;
    return dsp56300_read_memory(jit_be(dsp)->jit, space_id, addr);
}

static void dsp_jit_write_memory(DSPState *dsp, char space, uint32_t addr,
                                 uint32_t value)
{
    Dsp56300MemSpace space_id = (space == 'X') ? DSP56300_MEM_SPACE_X :
                                (space == 'Y') ? DSP56300_MEM_SPACE_Y :
                                                 DSP56300_MEM_SPACE_P;
    dsp56300_write_memory(jit_be(dsp)->jit, space_id, addr, value);
}

static bool dsp_jit_get_halt_requested(DSPState *dsp)
{
    return dsp56300_halt_requested(jit_be(dsp)->jit);
}

static void dsp_jit_set_halt_requested(DSPState *dsp, bool idle)
{
    dsp56300_set_halt_requested(jit_be(dsp)->jit, idle);
}

static uint32_t dsp_jit_get_cycle_count(DSPState *dsp)
{
    return dsp56300_cycle_count(jit_be(dsp)->jit);
}

static void dsp_jit_set_cycle_count(DSPState *dsp, uint32_t count)
{
    dsp56300_set_cycle_count(jit_be(dsp)->jit, count);
}

static void dsp_jit_invalidate_opcache(DSPState *dsp)
{
    dsp56300_invalidate_cache(jit_be(dsp)->jit);
}

/*
 * Sync: JIT -> DspCoreState (before VM save / debug)
 */
static void dsp_jit_sync_to_vm(DSPState *dsp)
{
    JitBackend *be = jit_be(dsp);
    Dsp56300Jit *jit = be->jit;
    DspCoreState *vm = &dsp->core;

    /* Scalar state via bulk struct */
    Dsp56300State ss;
    dsp56300_get_state(jit, &ss);
    vm->pc = ss.pc;
    vm->cur_inst_len = ss.pc_advance;
    vm->loop_rep = (uint32_t)ss.loop_rep;
    vm->pc_on_rep = (uint32_t)ss.pc_on_rep;
    vm->cycle_count = ss.cycle_count;
    vm->halt_requested = ss.halt_requested;
    vm->interrupt_state = ss.interrupts.state;
    vm->interrupt_instr_fetch = (uint16_t)ss.interrupts.vector_addr;
    vm->interrupt_save_pc = (uint16_t)ss.interrupts.saved_pc;
    vm->interrupt_ipl_to_raise = ss.interrupts.ipl_to_raise;
    vm->interrupt_pipeline_count = ss.interrupts.pipeline_stage;
    vm->interrupt_counter = 0;
    for (int i = 0; i < 4; i++) {
        int slot = cinterp_to_arch[i];
        vm->interrupt_ipl[i] = (int16_t)ss.interrupts.ipl[slot];
        vm->interrupt_is_pending[i] = (ss.interrupts.pending_bits[slot / 64] >> (slot % 64)) & 1;
        vm->interrupt_counter += vm->interrupt_is_pending[i];
    }
    dsp->save_cycles = ss.cycle_budget;

    /* Register/stack arrays from bulk state */
    memcpy(vm->registers, ss.registers, DSP_REG_MAX * sizeof(uint32_t));
    memcpy(vm->stack[0], ss.stack[0], 16 * sizeof(uint32_t));
    memcpy(vm->stack[1], ss.stack[1], 16 * sizeof(uint32_t));

    /* Memory arrays from C-side owned buffers */
    memcpy(vm->pram, be->pram, DSP_PRAM_SIZE * sizeof(uint32_t));
    memcpy(vm->xram, be->xram, DSP_XRAM_SIZE * sizeof(uint32_t));
    memcpy(vm->yram, be->yram, DSP_YRAM_SIZE * sizeof(uint32_t));
    /* Mixbuffer is aliased at xram[0xC00] */
    memcpy(vm->mixbuffer, be->xram + 0xC00,
           DSP_MIXBUFFER_SIZE * sizeof(uint32_t));
    /* Peripheral state: read current values via callbacks */
    for (int i = 0; i < DSP_PERIPH_SIZE; i++) {
        vm->periph[i] =
            dsp56300_read_memory(jit, DSP56300_MEM_SPACE_X, 0xFFFF80 + i);
    }
}

/*
 * Sync: DspCoreState -> JIT (after VM load / engine switch)
 */
static void dsp_jit_sync_from_vm(DSPState *dsp)
{
    JitBackend *be = jit_be(dsp);
    Dsp56300Jit *jit = be->jit;
    DspCoreState *vm = &dsp->core;

    /* Memory arrays into C-side owned buffers */
    memcpy(be->pram, vm->pram, DSP_PRAM_SIZE * sizeof(uint32_t));
    memcpy(be->xram, vm->xram, DSP_XRAM_SIZE * sizeof(uint32_t));
    memcpy(be->yram, vm->yram, DSP_YRAM_SIZE * sizeof(uint32_t));
    /* Mixbuffer is aliased at xram[0xC00] */
    memcpy(be->xram + 0xC00, vm->mixbuffer,
           DSP_MIXBUFFER_SIZE * sizeof(uint32_t));

    /* Scalar state via bulk struct */
    Dsp56300State ss = {
        .pc = vm->pc,
        .pc_advance = vm->cur_inst_len,
        .pc_on_rep = (bool)vm->pc_on_rep,
        .cycle_count = vm->cycle_count,
        .cycle_budget = dsp->save_cycles,
        .loop_rep = (bool)vm->loop_rep,
        .halt_requested = vm->halt_requested,
        .power_state = 0,
        .registers = {0},
        .stack = {{0}},
        .interrupts = {
            .state = (uint8_t)vm->interrupt_state,
            .vector_addr = vm->interrupt_instr_fetch,
            .saved_pc = vm->interrupt_save_pc,
            .ipl_to_raise = (uint8_t)vm->interrupt_ipl_to_raise,
            .pipeline_stage = (uint8_t)vm->interrupt_pipeline_count,
        },
    };
    ss.interrupts.pending_bits[0] = 0;
    ss.interrupts.pending_bits[1] = 0;
    for (int i = 0; i < 4; i++) {
        int slot = cinterp_to_arch[i];
        ss.interrupts.ipl[slot] = (int8_t)vm->interrupt_ipl[i];
        ss.interrupts.pending_bits[slot / 64] |=
            (uint64_t)(vm->interrupt_is_pending[i] != 0) << (slot % 64);
    }
    memcpy(ss.registers, vm->registers, DSP_REG_MAX * sizeof(uint32_t));
    memcpy(ss.stack[0], vm->stack[0], 16 * sizeof(uint32_t));
    memcpy(ss.stack[1], vm->stack[1], 16 * sizeof(uint32_t));

    /* Widen legacy 16-bit register values from old snapshots.
     * The C interpreter used 16-bit R/N/M/SSH/SSL/LA/LC registers.
     * M: $FFFF meant linear addressing at 16-bit; JIT needs $FFFFFF.
     * N: Signed offsets need sign extension from 16-bit to 24-bit.
     */
    for (int i = 0; i < 8; i++) {
        if (ss.registers[DSP_REG_M0 + i] == 0x00FFFF) {
            ss.registers[DSP_REG_M0 + i] = 0x00FFFFFF;
        }
        if (ss.registers[DSP_REG_N0 + i] & 0x8000) {
            ss.registers[DSP_REG_N0 + i] |= 0xFF0000;
        }
    }

    dsp56300_set_state(jit, &ss);

    dsp56300_invalidate_cache(jit);
}

static JitBackend *dsp_create_jit_backend(DSPState *dsp)
{
    JitBackend *be = g_new0(JitBackend, 1);

    /* Allocate memory buffers owned by the C side */
    be->xram = g_new(uint32_t, DSP_XRAM_SIZE);
    memset(be->xram, 0xCA, DSP_XRAM_SIZE * sizeof(uint32_t));
    be->yram = g_new(uint32_t, DSP_YRAM_SIZE);
    memset(be->yram, 0xCA, DSP_YRAM_SIZE * sizeof(uint32_t));
    be->pram = g_new(uint32_t, DSP_PRAM_SIZE);
    memset(be->pram, 0xCA, DSP_PRAM_SIZE * sizeof(uint32_t));

    /* X-space: XRAM [0, 0x1000), mixbuf alias [0x1400, 0x1800), peripherals */
    Dsp56300MemoryRegion x_regions[3] = {
        { .start = 0x0000,
          .end = 0x1000,
          .kind = DSP56300_REGION_BUFFER,
          .data = { .buffer = { .base = be->xram, .offset = 0 } } },
        { .start = 0x1400,
          .end = 0x1800,
          .kind = DSP56300_REGION_BUFFER,
          .data = { .buffer = { .base = be->xram, .offset = 0xC00 } } },
        { .start = 0xFFFF80,
          .end = 0x1000000,
          .kind = DSP56300_REGION_CALLBACK,
          .data = { .callback = { .opaque = dsp,
                                  .read = jit_read_peripheral,
                                  .write = jit_write_peripheral } } },
    };

    /* Y-space: YRAM [0, 0x800) */
    Dsp56300MemoryRegion y_regions[1] = {
        { .start = 0x0000,
          .end = 0x0800,
          .kind = DSP56300_REGION_BUFFER,
          .data = { .buffer = { .base = be->yram, .offset = 0 } } },
    };

    /* P-space: PRAM [0, 0x1000) */
    Dsp56300MemoryRegion p_regions[1] = {
        { .start = 0x0000,
          .end = 0x1000,
          .kind = DSP56300_REGION_BUFFER,
          .data = { .buffer = { .base = be->pram, .offset = 0 } } },
    };

    Dsp56300CreateInfo info = {
        .memory_map = {
            .x_regions = x_regions,
            .x_count = ARRAY_SIZE(x_regions),
            .y_regions = y_regions,
            .y_count = ARRAY_SIZE(y_regions),
            .p_regions = p_regions,
            .p_count = ARRAY_SIZE(p_regions),
        },
    };

    be->jit = dsp56300_create(&info);
    return be;
}

void dsp_jit_init(DSPState *dsp)
{
    JitBackend *be = dsp_create_jit_backend(dsp);
    dsp->backend = be;
    dsp->ops = &jit_dsp_ops;

    dsp->dma.mem_opaque = be->jit;
    dsp->dma.mem_read = jit_dma_mem_read;
    dsp->dma.mem_write = jit_dma_mem_write;
}

static void dsp_jit_finalize(DSPState *dsp)
{
    JitBackend *be = jit_be(dsp);
    dsp56300_destroy(be->jit);
    g_free(be->xram);
    g_free(be->yram);
    g_free(be->pram);
    g_free(be);
    dsp->backend = NULL;
}

const DSPOps jit_dsp_ops = {
    .finalize = dsp_jit_finalize,
    .reset = dsp_jit_reset,
    .step = dsp_jit_step,
    .run = dsp_jit_run,
    .bootstrap = dsp_jit_bootstrap,
    .start_frame = dsp_start_frame_impl,
    .read_memory = dsp_jit_read_memory,
    .write_memory = dsp_jit_write_memory,
    .get_halt_requested = dsp_jit_get_halt_requested,
    .set_halt_requested = dsp_jit_set_halt_requested,
    .get_cycle_count = dsp_jit_get_cycle_count,
    .set_cycle_count = dsp_jit_set_cycle_count,
    .invalidate_opcache = dsp_jit_invalidate_opcache,
    .sync_to_vm = dsp_jit_sync_to_vm,
    .sync_from_vm = dsp_jit_sync_from_vm,
};
