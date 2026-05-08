/*
 * MCPX DSP DMA
 *
 * Copyright (c) 2015 espes
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

#ifndef DSP_DMA_H
#define DSP_DMA_H

#include <stdint.h>
#include <stdbool.h>

#include "dsp.h"
#include "dsp_cpu.h"

#define DMA_CONTROL_RUNNING (1 << 4)
#define DMA_CONTROL_STOPPED (1 << 5)

typedef enum DSPDMARegister {
    DMA_CONFIGURATION,
    DMA_CONTROL,
    DMA_START_BLOCK,
    DMA_NEXT_BLOCK,
} DSPDMARegister;

typedef struct DSPDMAState {
    dsp_core_t* core;

    void *rw_opaque;
    dsp_scratch_rw_func scratch_rw;
    dsp_fifo_rw_func fifo_rw;

    uint32_t configuration;
    uint32_t control;
    uint32_t start_block;
    uint32_t next_block;

    bool error;
    bool eol;
} DSPDMAState;

uint32_t dsp_dma_read(DSPDMAState *s, DSPDMARegister reg);
void dsp_dma_write(DSPDMAState *s, DSPDMARegister reg, uint32_t v);

#endif
