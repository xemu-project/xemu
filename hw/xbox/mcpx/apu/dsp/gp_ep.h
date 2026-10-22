/*
 * QEMU MCPX Audio Processing Unit implementation
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2018-2019 Jannik Vogel
 * Copyright (c) 2019-2026 Matt Borgerson
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#ifndef HW_XBOX_MCPX_APU_GP_EP_H
#define HW_XBOX_MCPX_APU_GP_EP_H

#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/pci/pci.h"
#include "hw/xbox/mcpx/apu/apu_regs.h"

#include "dsp.h"

typedef struct MCPXAPUState MCPXAPUState;

typedef struct MCPXAPUGPState {
    MemoryRegion mmio;
    DSPState *dsp;
    uint32_t regs[0x10000];
} MCPXAPUGPState;

typedef struct MCPXAPUEPState {
    MemoryRegion mmio;
    DSPState *dsp;
    uint32_t regs[0x10000];
} MCPXAPUEPState;

extern const MemoryRegionOps gp_ops;
extern const MemoryRegionOps ep_ops;

void mcpx_apu_dsp_init(MCPXAPUState *d);
/* One SE frame of DSP work in three steps around the VP: begin does the
 * kick bookkeeping and releases the EP worker, gp fills the GP mixbuf from
 * the mixbins the VP just produced and releases the GP worker, end joins
 * both and reads their results. */
void mcpx_apu_dsp_frame_begin(MCPXAPUState *d);
void mcpx_apu_dsp_frame_gp(
    MCPXAPUState *d, float mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME]);
void mcpx_apu_dsp_frame_end(MCPXAPUState *d);
void mcpx_apu_dsp_stop_workers(void);

#endif
