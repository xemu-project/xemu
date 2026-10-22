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
#include "dsp_internal.h"
#include "trace.h"
#include "ui/xemu-settings.h"

/* Defines */
#define BITMASK(x) ((1 << (x)) - 1)

#define INTERRUPT_ABORT_FRAME (1 << 0)
#define INTERRUPT_START_FRAME (1 << 1)
#define INTERRUPT_DMA_EOL (1 << 7)

static int g_gp_frame_count = 0;

/*
 * Shared peripheral I/O helpers, used by both backends via callbacks.
 */

uint32_t read_peripheral(DSPState *dsp, uint32_t address)
{
    uint32_t v = 0xababa;
    switch (address) {
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

void write_peripheral(DSPState *dsp, uint32_t address, uint32_t value)
{
    switch (address) {
    case 0xFFFFC4:
        if (value & 1) {
            dsp_set_halt_requested(dsp, true);
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

void dsp_start_frame_impl(DSPState *dsp)
{
    dsp->interrupts |= INTERRUPT_START_FRAME;
}

DSPState *dsp_init(void *rw_opaque, dsp_scratch_rw_func scratch_rw,
                   dsp_fifo_rw_func fifo_rw, bool is_gp)
{
    DSPState *dsp = g_new0(DSPState, 1);
    dsp->is_gp = is_gp;
    dsp->core.is_gp = is_gp;

    dsp->dma.rw_opaque = rw_opaque;
    dsp->dma.scratch_rw = scratch_rw;
    dsp->dma.fifo_rw = fifo_rw;

    if (g_config.audio.use_dsp_jit) {
        dsp_jit_init(dsp);
    } else {
        dsp_c_init(dsp);
    }

    dsp_reset(dsp);

    return dsp;
}

void dsp_destroy(DSPState *dsp)
{
    dsp->ops->finalize(dsp);
    g_free(dsp);
}

void dsp_reset(DSPState *dsp)
{
    dsp->ops->reset(dsp);
}

void dsp_step(DSPState *dsp)
{
    dsp->ops->step(dsp);
}

void dsp_run(DSPState *dsp, int cycles)
{
    dsp->ops->run(dsp, cycles);
}

void dsp_bootstrap(DSPState *dsp)
{
    dsp->ops->bootstrap(dsp);
}

void dsp_start_frame(DSPState *dsp)
{
    if (dsp->is_gp) {
        g_gp_frame_count++;
    }
    dsp->ops->start_frame(dsp);
}

uint32_t dsp_read_memory(DSPState *dsp, char space, uint32_t address)
{
    return dsp->ops->read_memory(dsp, space, address);
}

void dsp_write_memory(DSPState *dsp, char space, uint32_t address,
                      uint32_t value)
{
    dsp->ops->write_memory(dsp, space, address, value);
}

bool dsp_get_halt_requested(DSPState *dsp)
{
    return dsp->ops->get_halt_requested(dsp);
}

void dsp_set_halt_requested(DSPState *dsp, bool idle)
{
    dsp->ops->set_halt_requested(dsp, idle);
}

uint32_t dsp_get_cycle_count(DSPState *dsp)
{
    return dsp->ops->get_cycle_count(dsp);
}

void dsp_set_cycle_count(DSPState *dsp, uint32_t count)
{
    dsp->ops->set_cycle_count(dsp, count);
}

void dsp_invalidate_opcache(DSPState *dsp)
{
    dsp->ops->invalidate_opcache(dsp);
}

void dsp_sync_to_vm(DSPState *dsp)
{
    dsp->ops->sync_to_vm(dsp);
}

void dsp_sync_from_vm(DSPState *dsp)
{
    dsp->ops->sync_from_vm(dsp);
}

void dsp_set_engine(DSPState *dsp, bool use_jit)
{
    bool currently_jit = (dsp->ops == &jit_dsp_ops);
    if (use_jit == currently_jit) {
        return;
    }

    dsp_sync_to_vm(dsp);
    dsp->ops->finalize(dsp);

    if (use_jit) {
        dsp_jit_init(dsp);
    } else {
        dsp_c_init(dsp);
    }

    dsp_sync_from_vm(dsp);
}
