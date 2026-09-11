/*
 * MCPX DSP emulator
 *
 * Copyright (c) 2015 espes
 * Copyright (c) 2020-2026 Matt Borgerson
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
#include "qemu/atomic.h"
#include "dsp.h"
#include "debug.h"
#include "ep_yrom.h"
#include "trace.h"

#include <dsp56300.h>

#define INTERRUPT_ABORT_FRAME (1 << 0)
#define INTERRUPT_START_FRAME (1 << 1)
#define INTERRUPT_DMA_EOL (1 << 7)

/* Map the snapshot's interrupt indices (the retired C interpreter's) to
 * architectural IVT slots. Architectural slot = vectorAddr / 2 (per
 * DSP56300FM Table 2-2). */
static const int snapshot_to_arch[4] = {
    0, /* RESET:       vec $00, slot 0 */
    2, /* ILLEGAL:     vec $04, slot 2 */
    1, /* STACK_ERROR: vec $02, slot 1 */
    4, /* TRAP:        vec $08, slot 4 */
};

static int g_gp_frame_count = 0;

/* Incremented from whichever thread is running the core: two writers. */
uint64_t g_dsp_gp_halts, g_dsp_ep_halts;

/*
 * Peripheral I/O, reached from the core through its callback region.
 */

/* The core's clock: frames ticked (dsp_frame_tick) plus the cycles retired
 * in the current frame. */
static uint64_t dsp_cycles_total(DSPState *dsp)
{
    return dsp->cycles_base + dsp56300_cycle_count(dsp->jit);
}

static uint32_t read_peripheral(void *opaque, uint32_t address)
{
    DSPState *dsp = opaque;
    uint32_t v = 0xababa;
    switch (address) {
    case 0xFFFFB0:
        v = dsp->timer_ctl;
        break;
    case 0xFFFFB2:
        v = dsp->timer_period;
        break;
    case 0xFFFFB3:
        /* Core cycles since the $FFFFB1 write while enabled, else 0
         * (probed: reads 0 until the programs' timer init).
         *
         * GP only: the GP calibrates its frame length against this and the
         * frame-timer test measures it. The EP program watchdogs the timer
         * (P:$00B6: if the cycles between two of its resident-loop reads
         * reach ~8 frames it jumps to an error path and hangs), and the
         * wall-frame model here over-attributes cycles to the EP across the
         * frames it spends budget-limited or parked between kicks, tripping
         * that watchdog. Until the EP's per-frame cycle use is modelled
         * faithfully the EP reads 0, as it did before the timer existed, so
         * its watchdog stays dormant (dsp-cycle-model-status). */
        v = 0;
        if (dsp->is_gp && (dsp->timer_ctl & 1)) {
            v = (dsp_cycles_total(dsp) - dsp->timer_base) & 0xFFFFFF;
        }
        break;
    case 0xFFFFC5:
        v = dsp->interrupts | dsp->dma.pending_interrupts;
        if (dsp->dma.eol) {
            v |= INTERRUPT_DMA_EOL;
            dsp_dma_completion_seen(&dsp->dma);
        }
        break;
    case 0xFFFFD4:
        v = dsp_dma_read(&dsp->dma, DMA_NEXT_BLOCK);
        break;
    case 0xFFFFD5:
        v = dsp_dma_read(&dsp->dma, DMA_START_BLOCK);
        break;
    case 0xFFFFD6:
        v = dsp_dma_read(&dsp->dma, DMA_CONTROL);
        break;
    case 0xFFFFD7:
        v = dsp_dma_read(&dsp->dma, DMA_CONFIGURATION);
        break;
    }

    trace_dsp_read_peripheral(address, v);
    return v;
}

static void write_peripheral(void *opaque, uint32_t address, uint32_t value)
{
    DSPState *dsp = opaque;
    switch (address) {
    case 0xFFFFB0:
        dsp->timer_ctl = value;
        break;
    case 0xFFFFB1:
        /* Any write restarts the count (the programs write 1). */
        dsp->timer_base = dsp_cycles_total(dsp);
        break;
    case 0xFFFFB2:
        dsp->timer_period = value;
        break;
    case 0xFFFFC4:
        if (value & 1) {
            /* Frame-complete: the program's own stop. Counted so
             * the scheduler trace report can tell an idling core from one cut off
             * at its cycle budget. */
            if (dsp->is_gp) {
                qatomic_add(&g_dsp_gp_halts, 1);
            } else {
                qatomic_add(&g_dsp_ep_halts, 1);
            }
            dsp_set_halt_requested(dsp, true);
            dsp->halted_since_reset = true;
            /* Frame-complete is what releases a latched start (probed:
             * with a backlog, the bit appears one read after this write
             * and never on the acknowledge alone). */
            if (dsp->frame_starts_pending) {
                dsp->interrupts |= INTERRUPT_START_FRAME;
            }
        }
        break;
    case 0xFFFFC5:
        if (!dsp->is_gp && value == 0x80) {
            mcpx_apu_ep_snapshot_on_eol_clear(dsp);
        }
        dsp->interrupts &= ~value;
        if (value & INTERRUPT_START_FRAME) {
            /* The program consumed one start. The next owed one stays
             * invisible until its frame-complete (see $FFFFC4). Before the
             * first frame-complete the program is clearing the bit as part
             * of its init, not consuming a start: a start that arrived
             * during a slow init is still owed to the first real frame. */
            if (dsp->frame_starts_pending && dsp->halted_since_reset) {
                dsp->frame_starts_pending--;
            }
        }
        dsp->dma.pending_interrupts &= ~value;
        if (value & INTERRUPT_DMA_EOL) {
            dsp->dma.eol = false;
        }
        break;
    case 0xFFFFD4:
        dsp_dma_write(&dsp->dma, DMA_NEXT_BLOCK, value);
        break;
    case 0xFFFFD5:
        dsp_dma_write(&dsp->dma, DMA_START_BLOCK, value);
        break;
    case 0xFFFFD6:
        dsp_dma_write(&dsp->dma, DMA_CONTROL, value);
        break;
    case 0xFFFFD7:
        dsp_dma_write(&dsp->dma, DMA_CONFIGURATION, value);
        break;
    }

    trace_dsp_write_peripheral(address, value);
}

/* EP on-chip Y data ROM (Y:$0800-$0FFF). Read-only: writes are ignored, as on
 * hardware. Only mapped for the EP core (the GP has no such ROM). */
static uint32_t read_yrom(void *opaque, uint32_t address)
{
    return ep_yrom[address - DSP_YROM_BASE];
}

static void write_yrom(void *opaque, uint32_t address, uint32_t value)
{
}

/*
 * The DMA engine's view of the core's memory.
 */

static uint32_t dma_mem_read(void *opaque, int space, uint32_t addr)
{
    return dsp56300_read_memory((const Dsp56300Jit *)opaque,
                                (Dsp56300MemSpace)space, addr);
}

static void dma_mem_write(void *opaque, int space, uint32_t addr,
                          uint32_t value)
{
    dsp56300_write_memory((Dsp56300Jit *)opaque, (Dsp56300MemSpace)space, addr,
                          value);
}

static void dma_mem_read_run(void *opaque, int space, uint32_t addr,
                             uint32_t *out, uint32_t count)
{
    dsp56300_read_memory_run((const Dsp56300Jit *)opaque,
                             (Dsp56300MemSpace)space, addr, out, count);
}

static void dma_mem_write_run(void *opaque, int space, uint32_t addr,
                              const uint32_t *vals, uint32_t count)
{
    dsp56300_write_memory_run((Dsp56300Jit *)opaque, (Dsp56300MemSpace)space,
                              addr, vals, count);
}

static Dsp56300Jit *dsp_create_core(DSPState *dsp)
{
    /* Peripheral (internal I/O) region, common to both cores. Unmapped
     * addresses (the gaps between regions) read 0, matching hardware. */
    const Dsp56300MemoryRegion periph_region = {
        .start = 0xFFFF80,
        .end = 0x1000000,
        .kind = DSP56300_REGION_CALLBACK,
        .data = { .callback = { .opaque = dsp,
                                .read = read_peripheral,
                                .write = write_peripheral } } };

    Dsp56300MemoryRegion x_regions[3];
    Dsp56300MemoryRegion y_regions[2];
    uint32_t x_count, y_count;

    if (dsp->is_gp) {
        /* GP X: XRAM, the mixbuf alias at [0x1400, 0x1800), peripherals. */
        x_regions[0] = (Dsp56300MemoryRegion){
            .start = 0x0000, .end = DSP_GP_XRAM_SIZE,
            .kind = DSP56300_REGION_BUFFER,
            .data = { .buffer = { .base = dsp->xram, .offset = 0 } } };
        x_regions[1] = (Dsp56300MemoryRegion){
            .start = 0x1400, .end = 0x1800,
            .kind = DSP56300_REGION_BUFFER,
            .data = { .buffer = { .base = dsp->xram, .offset = 0xC00 } } };
        x_regions[2] = periph_region;
        x_count = 3;
        /* GP Y: YRAM [0, 0x800); no ROM. */
        y_regions[0] = (Dsp56300MemoryRegion){
            .start = 0x0000, .end = DSP_GP_YRAM_SIZE,
            .kind = DSP56300_REGION_BUFFER,
            .data = { .buffer = { .base = dsp->yram, .offset = 0 } } };
        y_count = 1;
    } else {
        /* EP X: XRAM [0, 0xC00) only (no mixbuffer); higher X reads 0. */
        x_regions[0] = (Dsp56300MemoryRegion){
            .start = 0x0000, .end = DSP_EP_XRAM_SIZE,
            .kind = DSP56300_REGION_BUFFER,
            .data = { .buffer = { .base = dsp->xram, .offset = 0 } } };
        x_regions[1] = periph_region;
        x_count = 2;
        /* EP Y: RAM [0, 0x100), then on-chip data ROM [0x800, 0x1000)
         * (Dolby/AC3 encode tables); the $0100-$07FF gap reads 0. */
        y_regions[0] = (Dsp56300MemoryRegion){
            .start = 0x0000, .end = DSP_EP_YRAM_SIZE,
            .kind = DSP56300_REGION_BUFFER,
            .data = { .buffer = { .base = dsp->yram, .offset = 0 } } };
        y_regions[1] = (Dsp56300MemoryRegion){
            .start = DSP_YROM_BASE, .end = DSP_YROM_BASE + DSP_YROM_SIZE,
            .kind = DSP56300_REGION_CALLBACK,
            .data = { .callback = { .opaque = dsp,
                                    .read = read_yrom,
                                    .write = write_yrom } } };
        y_count = 2;
    }

    /* P-space: PRAM [0, 0x1000) */
    Dsp56300MemoryRegion p_regions[1] = {
        { .start = 0x0000,
          .end = 0x1000,
          .kind = DSP56300_REGION_BUFFER,
          .data = { .buffer = { .base = dsp->pram, .offset = 0 } } },
    };

    Dsp56300CreateInfo info = {
        .memory_map = {
            .x_regions = x_regions,
            .x_count = x_count,
            .y_regions = y_regions,
            .y_count = y_count,
            .p_regions = p_regions,
            .p_count = ARRAY_SIZE(p_regions),
        },
    };

    return dsp56300_create(&info);
}

DSPState *dsp_init(void *rw_opaque, dsp_scratch_rw_func scratch_rw,
                   dsp_fifo_rw_func fifo_rw, bool is_gp)
{
    DSPState *dsp = g_new0(DSPState, 1);
    dsp->is_gp = is_gp;
    dsp->core.is_gp = is_gp;

    dsp->xram = g_new(uint32_t, DSP_XRAM_SIZE);
    memset(dsp->xram, 0xCA, DSP_XRAM_SIZE * sizeof(uint32_t));
    dsp->yram = g_new(uint32_t, DSP_YRAM_SIZE);
    memset(dsp->yram, 0xCA, DSP_YRAM_SIZE * sizeof(uint32_t));
    dsp->pram = g_new(uint32_t, DSP_PRAM_SIZE);
    memset(dsp->pram, 0xCA, DSP_PRAM_SIZE * sizeof(uint32_t));
    dsp->jit = dsp_create_core(dsp);

    dsp->dma.rw_opaque = rw_opaque;
    dsp->dma.scratch_rw = scratch_rw;
    dsp->dma.fifo_rw = fifo_rw;
    dsp->dma.is_gp = is_gp;
    dsp->dma.mem_opaque = dsp->jit;
    dsp->dma.mem_read = dma_mem_read;
    dsp->dma.mem_write = dma_mem_write;
    dsp->dma.mem_read_run = dma_mem_read_run;
    dsp->dma.mem_write_run = dma_mem_write_run;

    dsp_reset(dsp);

    return dsp;
}

void dsp_destroy(DSPState *dsp)
{
    dsp56300_destroy(dsp->jit);
    g_free(dsp->xram);
    g_free(dsp->yram);
    g_free(dsp->pram);
    g_free(dsp);
}

void dsp_reset(DSPState *dsp)
{
    /* A start owed to the program being reset is not owed to the next one:
     * silicon loses an interrupt that lands in reset, and carrying it over
     * runs the new program one frame ahead of the frame counter. */
    dsp->frame_starts_pending = 0;
    dsp->halted_since_reset = false;
    dsp->timer_ctl = 0;
    dsp->timer_period = 0;
    dsp->timer_base = dsp_cycles_total(dsp);
    /* The block reset restarts the DMA engine idle: a STOP the outgoing
     * program issued does not leave STOPPED for the next program to read
     * (probed: DMA_CONTROL reads 0 on a fresh boot after a stopped chain). */
    dsp->dma.control = 0;
    dsp->dma.dma_read_count = 0;
    /* Nor does an EOL a FREEZE held back reach the next program: an
     * UNFREEZE alone after a reboot saw no EOL on silicon. */
    dsp->dma.eol_held = false;
    dsp56300_reset(dsp->jit);
}

void dsp_step(DSPState *dsp)
{
    dsp56300_step(dsp->jit);
}

void dsp_run(DSPState *dsp, int cycles)
{
    dsp56300_run(dsp->jit, cycles);

    /* WAIT/STOP park the core until an interrupt (power_state 1/2) without
     * consuming cycles; nothing further can happen within this frame, so
     * report the frame complete. The run loop only idles on the $FFFFC4
     * flag; a program that parks with WAIT would otherwise spin the
     * frame's halt-wait loop forever, wedging the APU thread while it
     * holds the APU lock. */
    if (!dsp56300_halt_requested(dsp->jit)) {
        Dsp56300State ss;
        dsp56300_get_state(dsp->jit, &ss);
        if (ss.power_state != 0) {
            dsp56300_set_halt_requested(dsp->jit, true);
        }
    }
}

void dsp_bootstrap(DSPState *dsp)
{
    /* Scratch memory is DMA'd into PRAM by the boot ROM. */
    dsp->dma.scratch_rw(dsp->dma.rw_opaque, (uint8_t *)dsp->pram, 0,
                        0x800 * 4, false);
    for (int i = 0; i < 0x800; i++) {
        if (dsp->pram[i] & 0xff000000) {
            DPRINTF("Bootstrap %04x: %08x\n", i, dsp->pram[i]);
            dsp->pram[i] &= 0x00ffffff;
        }
    }
    dsp56300_invalidate_cache(dsp->jit);
}

void dsp_start_frame(DSPState *dsp)
{
    if (dsp->is_gp) {
        g_gp_frame_count++;
    }
    /* Every tick latches; the core sees it once it has completed a frame
     * (a parked WAIT/STOP core counts as complete, see dsp_run). A core
     * still working sees it at its next frame-complete. */
    dsp->frame_starts_pending++;
    if (dsp_get_halt_requested(dsp)) {
        dsp->interrupts |= INTERRUPT_START_FRAME;
    }
}

/* Whether a frame start is signalled that the program has not yet consumed
 * (it clears the bit when it begins the frame): a core that halted with
 * this set still owes that frame's work. */
bool dsp_frame_start_pending(DSPState *dsp)
{
    return dsp->interrupts & INTERRUPT_START_FRAME;
}

void dsp_get_registers(DSPState *dsp, uint32_t out[64])
{
    Dsp56300State ss;

    dsp56300_get_state(dsp->jit, &ss);
    memcpy(out, ss.registers, 64 * sizeof(uint32_t));
}

void dsp_get_pc_sp(DSPState *dsp, uint32_t *pc, uint32_t *sp,
                   uint32_t ssh[16])
{
    Dsp56300State ss;

    dsp56300_get_state(dsp->jit, &ss);
    *pc = ss.pc;
    *sp = ss.registers[DSP56300_REG_SP];
    memcpy(ssh, ss.stack[0], 16 * sizeof(uint32_t));
}

static Dsp56300MemSpace mem_space(char space)
{
    return (space == 'X') ? DSP56300_MEM_SPACE_X :
           (space == 'Y') ? DSP56300_MEM_SPACE_Y :
                            DSP56300_MEM_SPACE_P;
}

uint32_t dsp_read_memory(DSPState *dsp, char space, uint32_t address)
{
    return dsp56300_read_memory(dsp->jit, mem_space(space), address);
}

void dsp_write_memory(DSPState *dsp, char space, uint32_t address,
                      uint32_t value)
{
    dsp56300_write_memory(dsp->jit, mem_space(space), address, value);
}

bool dsp_get_halt_requested(DSPState *dsp)
{
    return dsp56300_halt_requested(dsp->jit);
}

void dsp_set_halt_requested(DSPState *dsp, bool idle)
{
    dsp56300_set_halt_requested(dsp->jit, idle);
}

uint32_t dsp_get_cycle_count(DSPState *dsp)
{
    return dsp56300_cycle_count(dsp->jit);
}

void dsp_frame_tick(DSPState *dsp)
{
    /* The retired count within a frame never exceeds the frame's budget,
     * so the timer stays monotonic across the tick. */
    dsp->cycles_base += DSP_FRAME_CYCLES;
    dsp56300_set_cycle_count(dsp->jit, 0);
}

void dsp_set_cycle_count(DSPState *dsp, uint32_t count)
{
    dsp56300_set_cycle_count(dsp->jit, count);
}

void dsp_invalidate_opcache(DSPState *dsp)
{
    dsp56300_invalidate_cache(dsp->jit);
}

void dsp_jit_get_stats(DSPState *dsp, uint64_t *compiles, uint64_t *compile_ns,
                       uint64_t *compile_ns_worst,
                       uint64_t *invalidations, uint64_t *cache_hits,
                       uint64_t *retained, uint64_t *block_entries,
                       uint64_t *code_bytes)
{
    Dsp56300JitStats st;
    dsp56300_get_jit_stats(dsp->jit, &st);
    *compiles = st.compiles;
    *compile_ns = st.compile_ns;
    *compile_ns_worst = st.compile_ns_worst;
    *invalidations = st.invalidations;
    *cache_hits = st.cache_hits;
    *retained = st.retained;
    *block_entries = st.block_entries;
    *code_bytes = st.code_bytes;
}

/* Per-start-PC block-entry histogram from the library's JIT profiler. The
 * first call is what enables profiling, so a caller wanting a steady-state
 * window issues one early and throws the file away. */
void dsp_jit_dump_block_profile(DSPState *dsp, const char *path)
{
    dsp56300_dump_profile(dsp->jit, path);
}

/*
 * Snapshot sync: core -> DspCoreState (before VM save / debug)
 */
void dsp_sync_to_vm(DSPState *dsp)
{
    Dsp56300Jit *jit = dsp->jit;
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
        int slot = snapshot_to_arch[i];
        vm->interrupt_ipl[i] = (int16_t)ss.interrupts.ipl[slot];
        vm->interrupt_is_pending[i] = (ss.interrupts.pending_bits[slot / 64] >> (slot % 64)) & 1;
        vm->interrupt_counter += vm->interrupt_is_pending[i];
    }
    dsp->save_cycles = ss.cycle_budget;

    /* Register/stack arrays from bulk state */
    memcpy(vm->registers, ss.registers, DSP_REG_MAX * sizeof(uint32_t));
    memcpy(vm->stack[0], ss.stack[0], 16 * sizeof(uint32_t));
    memcpy(vm->stack[1], ss.stack[1], 16 * sizeof(uint32_t));

    /* Memory arrays from the C-side owned buffers */
    memcpy(vm->pram, dsp->pram, DSP_PRAM_SIZE * sizeof(uint32_t));
    memcpy(vm->xram, dsp->xram, DSP_XRAM_SIZE * sizeof(uint32_t));
    memcpy(vm->yram, dsp->yram, DSP_YRAM_SIZE * sizeof(uint32_t));
    /* Mixbuffer is aliased at xram[0xC00] */
    memcpy(vm->mixbuffer, dsp->xram + 0xC00,
           DSP_MIXBUFFER_SIZE * sizeof(uint32_t));
    /* Peripheral state: read current values via callbacks */
    for (int i = 0; i < DSP_PERIPH_SIZE; i++) {
        vm->periph[i] =
            dsp56300_read_memory(jit, DSP56300_MEM_SPACE_X, 0xFFFF80 + i);
    }
}

/*
 * Snapshot sync: DspCoreState -> core (after VM load)
 */
void dsp_sync_from_vm(DSPState *dsp)
{
    Dsp56300Jit *jit = dsp->jit;
    DspCoreState *vm = &dsp->core;

    /* Memory arrays into the C-side owned buffers */
    memcpy(dsp->pram, vm->pram, DSP_PRAM_SIZE * sizeof(uint32_t));
    memcpy(dsp->xram, vm->xram, DSP_XRAM_SIZE * sizeof(uint32_t));
    memcpy(dsp->yram, vm->yram, DSP_YRAM_SIZE * sizeof(uint32_t));
    /* Mixbuffer is aliased at xram[0xC00] */
    memcpy(dsp->xram + 0xC00, vm->mixbuffer,
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
        int slot = snapshot_to_arch[i];
        ss.interrupts.ipl[slot] = (int8_t)vm->interrupt_ipl[i];
        ss.interrupts.pending_bits[slot / 64] |=
            (uint64_t)(vm->interrupt_is_pending[i] != 0) << (slot % 64);
    }
    memcpy(ss.registers, vm->registers, DSP_REG_MAX * sizeof(uint32_t));
    memcpy(ss.stack[0], vm->stack[0], 16 * sizeof(uint32_t));
    memcpy(ss.stack[1], vm->stack[1], 16 * sizeof(uint32_t));

    /* Widen legacy 16-bit register values from old snapshots, which the
     * C interpreter wrote with 16-bit R/N/M/SSH/SSL/LA/LC registers.
     * M: $FFFF meant linear addressing at 16-bit; the core needs $FFFFFF.
     * N: Signed offsets need sign extension from 16-bit to 24-bit.
     */
    for (int i = 0; i < 8; i++) {
        if (ss.registers[DSP56300_REG_M0 + i] == 0x00FFFF) {
            ss.registers[DSP56300_REG_M0 + i] = 0x00FFFFFF;
        }
        if (ss.registers[DSP56300_REG_N0 + i] & 0x8000) {
            ss.registers[DSP56300_REG_N0 + i] |= 0xFF0000;
        }
    }

    dsp56300_set_state(jit, &ss);

    dsp56300_invalidate_cache(jit);
}
