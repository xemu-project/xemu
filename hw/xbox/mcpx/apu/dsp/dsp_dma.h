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
#include <stddef.h>

typedef void (*dsp_scratch_rw_func)(
    void *opaque, uint8_t *ptr, uint32_t addr, size_t len, bool dir);
typedef void (*dsp_fifo_rw_func)(
    void *opaque, uint8_t *ptr, unsigned int index, size_t len, bool dir);

#define DMA_CONTROL_RUNNING (1 << 4)
#define DMA_CONTROL_STOPPED (1 << 5)

typedef enum DSPDMARegister {
    DMA_CONFIGURATION,
    DMA_CONTROL,
    DMA_START_BLOCK,
    DMA_NEXT_BLOCK,
} DSPDMARegister;

/* Memory access callbacks for backend-agnostic DMA */
typedef uint32_t (*dsp_dma_mem_read_func)(void *opaque, int space, uint32_t addr);
typedef void (*dsp_dma_mem_write_func)(void *opaque, int space, uint32_t addr, uint32_t value);

typedef struct DSPDMAState {
    /* DSP memory access (backend-agnostic) */
    void *mem_opaque;
    dsp_dma_mem_read_func mem_read;
    dsp_dma_mem_write_func mem_write;

    /* System memory access */
    void *rw_opaque;
    dsp_scratch_rw_func scratch_rw;
    dsp_fifo_rw_func fifo_rw;

    uint32_t configuration;
    uint32_t control;
    uint32_t start_block;
    uint32_t next_block;

    bool error;
    bool eol;

    /* DMA completion timer: counts reads of DMA_CONTROL while RUNNING.
     * After 3 reads, transitions RUNNING -> STOPPED. */
    uint32_t dma_read_count;
} DSPDMAState;

uint32_t dsp_dma_read(DSPDMAState *s, DSPDMARegister reg);
void dsp_dma_write(DSPDMAState *s, DSPDMARegister reg, uint32_t v);

#endif
