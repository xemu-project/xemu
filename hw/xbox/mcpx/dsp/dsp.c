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
#include "dsp_cpu.h"
#include "dsp_dma.h"
#include "dsp_state.h"
#include "dsp.h"
#include "debug.h"
#include "trace.h"

/* Defines */
#define BITMASK(x)  ((1<<(x))-1)

#define INTERRUPT_ABORT_FRAME (1 << 0)
#define INTERRUPT_START_FRAME (1 << 1)
#define INTERRUPT_DMA_EOL (1 << 7)

static uint32_t read_peripheral(dsp_core_t* core, uint32_t address);
static void write_peripheral(dsp_core_t* core, uint32_t address, uint32_t value);

DSPState *dsp_init(void *rw_opaque,
                   dsp_scratch_rw_func scratch_rw,
                   dsp_fifo_rw_func fifo_rw)
{
    DPRINTF("dsp_init\n");

    DSPState* dsp = (DSPState*)malloc(sizeof(DSPState));
    memset(dsp, 0, sizeof(*dsp));

    dsp->core.read_peripheral = read_peripheral;
    dsp->core.write_peripheral = write_peripheral;

    dsp->dma.core = &dsp->core;
    dsp->dma.rw_opaque = rw_opaque;
    dsp->dma.scratch_rw = scratch_rw;
    dsp->dma.fifo_rw = fifo_rw;

    dsp_reset(dsp);

    return dsp;
}

void dsp_reset(DSPState* dsp)
{
    dsp56k_reset_cpu(&dsp->core);
    dsp->save_cycles = 0;
}

void dsp_destroy(DSPState* dsp)
{
    free(dsp);
}

static uint32_t read_peripheral(dsp_core_t* core, uint32_t address) {
    DSPState* dsp = container_of(core, DSPState, core);

    uint32_t v = 0xababa;
    switch(address) {
    case 0xFFFFB3:
        v = 0; // core->num_inst; // ??
        break;
    case 0xFFFFC5:
        v = dsp->interrupts;
        if (dsp->dma.eol) {
            v |= INTERRUPT_DMA_EOL;
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

static void write_peripheral(dsp_core_t* core, uint32_t address, uint32_t value) {
    DSPState* dsp = container_of(core, DSPState, core);

    switch(address) {
    case 0xFFFFC4:
        if (value & 1) {
            core->is_idle = true;
        }
        break;
    case 0xFFFFC5:
        dsp->interrupts &= ~value;
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


void dsp_step(DSPState* dsp)
{
    dsp56k_execute_instruction(&dsp->core);
}

void dsp_run(DSPState* dsp, int cycles)
{
    dsp->save_cycles += cycles;

    if (dsp->save_cycles <= 0) return;

    int dma_timer = 0;

    while (dsp->save_cycles > 0)
    {
        dsp56k_execute_instruction(&dsp->core);
        dsp->save_cycles -= dsp->core.instr_cycle;
        dsp->core.cycle_count++;

        if (dsp->dma.control & DMA_CONTROL_RUNNING) {
            dma_timer++;
        }

        if (dma_timer > 2) {
            dma_timer = 0;
            dsp->dma.control &= ~DMA_CONTROL_RUNNING;
            dsp->dma.control |= DMA_CONTROL_STOPPED;
        }

        if (dsp->core.is_idle) break;
    }

    /* FIXME: DMA timing be done cleaner. Xbox enables running
     * then polls to make sure its running. But we complete DMA instantaneously,
     * so when is it supposed to be signaled that it stopped? Maybe just wait at
     * least one cycle? How long does hardware wait?
     */
}

void dsp_bootstrap(DSPState* dsp)
{
    // scratch memory is dma'd in to pram by the bootrom
    dsp->dma.scratch_rw(dsp->dma.rw_opaque,
        (uint8_t*)dsp->core.pram, 0, 0x800*4, false);
    for (int i = 0; i < 0x800; i++) {
        if (dsp->core.pram[i] & 0xff000000) {
            DPRINTF("Bootstrap %04x: %08x\n", i, dsp->core.pram[i]);
            dsp->core.pram[i] &= 0x00ffffff;
        }
    }
    memset(dsp->core.pram_opcache, 0, sizeof(dsp->core.pram_opcache));
}

void dsp_start_frame(DSPState* dsp)
{
    dsp->interrupts |= INTERRUPT_START_FRAME;
}

uint32_t dsp_read_memory(DSPState* dsp, char space, uint32_t address)
{
    int space_id;

    switch (space) {
    case 'X':
        space_id = DSP_SPACE_X;
        break;
    case 'Y':
        space_id = DSP_SPACE_Y;
        break;
    case 'P':
        space_id = DSP_SPACE_P;
        break;
    default:
        assert(false);
        return 0;
    }

    return dsp56k_read_memory(&dsp->core, space_id, address);
}

void dsp_write_memory(DSPState* dsp, char space, uint32_t address, uint32_t value)
{
    int space_id;

    switch (space) {
    case 'X':
        space_id = DSP_SPACE_X;
        break;
    case 'Y':
        space_id = DSP_SPACE_Y;
        break;
    case 'P':
        space_id = DSP_SPACE_P;
        break;
    default:
        assert(false);
        return;
    }

    dsp56k_write_memory(&dsp->core, space_id, address, value);
}
