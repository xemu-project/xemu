/*
 * MCPX DSP DMA
 *
 * Copyright (c) 2015 espes
 * Copyright (c) 2018-2026 Matt Borgerson
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

/* Bulk variants covering a run of consecutive addresses in one space, so a
 * linear node crosses into the DSP once per window rather than once per
 * word. Optional: a backend that leaves these NULL gets the per-word path. */
typedef void (*dsp_dma_mem_read_run_func)(
    void *opaque, int space, uint32_t addr, uint32_t *out, uint32_t count);
typedef void (*dsp_dma_mem_write_run_func)(
    void *opaque, int space, uint32_t addr, const uint32_t *vals,
    uint32_t count);

typedef struct DSPDMAState {
    /* DSP memory access (backend-agnostic) */
    void *mem_opaque;
    dsp_dma_mem_read_func mem_read;
    dsp_dma_mem_write_func mem_write;
    dsp_dma_mem_read_run_func mem_read_run;
    dsp_dma_mem_write_run_func mem_write_run;

    /* System memory access */
    void *rw_opaque;
    dsp_scratch_rw_func scratch_rw;
    dsp_fifo_rw_func fifo_rw;
    bool is_gp;

    /* Per-node completion interrupts selected by control bits 2-3 (silicon:
     * value n raises interrupt-status bit 3+n at $FFFFC5). Cleared by the
     * DSP writing those bits back, like the EOL bit. */
    uint32_t pending_interrupts;

    uint32_t configuration;
    uint32_t control;
    uint32_t start_block;
    uint32_t next_block;

    bool error;
    bool eol;

    /* Reads of DMA_CONTROL while RUNNING; the third reads the engine as
     * idle (see dsp_dma_read). */
    uint32_t dma_read_count;

    /* Bounce buffer for one node's payload, grown on demand. Per engine:
     * the GP and EP run their DMA from their own threads, so a shared
     * buffer is a data race and a use-after-free on reallocation. */
    uint8_t *scratch_buf;
    size_t scratch_buf_size;

    /* Word staging for the bulk path, grown on demand; per engine for the
     * same reasons as scratch_buf. */
    uint32_t *word_buf;
    size_t word_buf_words;
} DSPDMAState;

uint32_t dsp_dma_read(DSPDMAState *s, DSPDMARegister reg);
/* The DSP has read the interrupt status with EOL set: the engine is idle. */
void dsp_dma_completion_seen(DSPDMAState *s);
void dsp_dma_write(DSPDMAState *s, DSPDMARegister reg, uint32_t v);

#endif
