/*
 * MCPX DSP DMA
 *
 * Copyright (c) 2015 espes
 * Copyright (c) 2020-2021 Matt Borgerson
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

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include <stddef.h>
#include "qemu/compiler.h"
#include "dsp_dma.h"
#include "dsp_state.h"


#define DMA_CONFIGURATION_AUTOSTART (1 << 0)
#define DMA_CONFIGURATION_AUTOREADY (1 << 1)
#define DMA_CONFIGURATION_IOC_CLEAR (1 << 2)
#define DMA_CONFIGURATION_EOL_CLEAR (1 << 3)
#define DMA_CONFIGURATION_ERR_CLEAR (1 << 4)

#define DMA_CONTROL_ACTION 0x7
#define DMA_CONTROL_ACTION_NOP 0
#define DMA_CONTROL_ACTION_START 1
#define DMA_CONTROL_ACTION_STOP 2
#define DMA_CONTROL_ACTION_FREEZE 3
#define DMA_CONTROL_ACTION_UNFREEZE 4
#define DMA_CONTROL_ACTION_ABORT 5
#define DMA_CONTROL_FROZEN (1 << 3)
#define DMA_CONTROL_RUNNING (1 << 4)
#define DMA_CONTROL_STOPPED (1 << 5)

#define NODE_POINTER_VAL 0x3fff
#define NODE_POINTER_EOL (1 << 14)

#define NODE_CONTROL_DIRECTION (1 << 1)


// #define DEBUG
#ifdef DEBUG
# define DPRINTF(s, ...) printf(s, ## __VA_ARGS__)
#else
# define DPRINTF(s, ...) do { } while (0)
#endif

#ifdef DEBUG
const char *buffer_names[] = {
    "fifo0",            /* 0x0 */
    "fifo1",            /* 0x1 */
    "fifo2",            /* 0x2 */
    "fifo3",            /* 0x3 */
    "<unknown-0x4>",    /* 0x4 */
    "<unknown-0x5>",    /* 0x5 */
    "<unknown-0x6>",    /* 0x6 */
    "<unknown-0x7>",    /* 0x7 */
    "<unknown-0x8>",    /* 0x8 */
    "<unknown-0x9>",    /* 0x9 */
    "<unknown-0xa>",    /* 0xA */
    "<unknown-0xb>",    /* 0xB */
    "<unknown-0xc>",    /* 0xC */
    "<unknown-0xd>",    /* 0xD */
    "scratch-circular", /* 0xE */
    "scratch"           /* 0xF */
};

const char *format_names[] = {
    "8 bit",         /* 0x0 */
    "16 bit",        /* 0x1 */
    "24 bit msb",    /* 0x2 */
    "32 bit",        /* 0x3 */
    "<invalid-0x4>", /* 0x4 */
    "<invalid-0x5>", /* 0x5 */
    "24 bit lsb",    /* 0x6 */
    "<invalid-0x7>"  /* 0x7 */
};

const char *space_names[] = {
    "x", /* DSP_SPACE_X, 0x0 */
    "y", /* DSP_SPACE_Y, 0x1 */
    "p"  /* DSP_SPACE_P, 0x2 */
};

#endif

#define MIN(a,b) (((a)<(b))?(a):(b))

static void scratch_circular_copy(
    DSPDMAState *s,
    uint32_t     scratch_base,
    uint32_t    *scratch_offset,
    uint32_t     scratch_size,
    uint32_t     transfer_size,
    uint8_t     *scratch_buf,
    int          direction)
{
    if (*scratch_offset >= scratch_size) {
        // fprintf(stderr, "Initial scratch offset exceeds scratch size! Wrapping\n");
        *scratch_offset = 0;
    }

    uint32_t buf_offset = 0;

    while (transfer_size > 0) {
        size_t bytes_until_wrap = scratch_size - *scratch_offset;
        size_t chunk_size = MIN(transfer_size, bytes_until_wrap);
        uint32_t scratch_addr = scratch_base + *scratch_offset;

        // R/W to scratch memory from chunk in buffer
        s->scratch_rw(s->rw_opaque, &scratch_buf[buf_offset], scratch_addr, chunk_size, direction);

        // Advance scratch pointer, wrap if we've reached the end
        *scratch_offset += chunk_size;
        if (*scratch_offset >= scratch_size) {
            *scratch_offset = 0;
        }

        transfer_size -= chunk_size;
        buf_offset += chunk_size;
    }
}

static void dsp_dma_run(DSPDMAState *s)
{
    if (!(s->control & DMA_CONTROL_RUNNING)
        || (s->control & DMA_CONTROL_FROZEN)) {
        return;
    }

    // DSPState *dsp = container_of(s, DSPState, dma);

    while (!(s->next_block & NODE_POINTER_EOL)) {
        uint32_t addr = s->next_block & NODE_POINTER_VAL;
        uint32_t block_addr = 0;
        int block_space = DSP_SPACE_X;

        if (addr < 0x1800) {
            assert(addr+6 < 0x1800);
            block_space = DSP_SPACE_X;
            block_addr = addr;
        } else if (addr >= 0x1800 && addr < 0x2000) { //?
            assert(addr+6 < 0x2000);
            block_space = DSP_SPACE_Y;
            block_addr = addr - 0x1800;
        } else if (addr >= 0x2800 && addr < 0x3800) { //?
            assert(addr+6 < 0x3800);
            block_space = DSP_SPACE_P;
            block_addr = addr - 0x2800;
        } else {
            assert(false);
        }

        uint32_t next_block = dsp56k_read_memory(s->core, block_space, block_addr);
        uint32_t control = dsp56k_read_memory(s->core, block_space, block_addr+1);
        uint32_t count = dsp56k_read_memory(s->core, block_space, block_addr+2);
        uint32_t dsp_offset = dsp56k_read_memory(s->core, block_space, block_addr+3);
        uint32_t scratch_offset = dsp56k_read_memory(s->core, block_space, block_addr+4);
        uint32_t scratch_base = dsp56k_read_memory(s->core, block_space, block_addr+5);
        uint32_t scratch_size = dsp56k_read_memory(s->core, block_space, block_addr+6)+1;

        s->next_block = next_block;
        if (s->next_block & NODE_POINTER_EOL) {
            s->eol = true;
        }

        /* Decode control word */
        bool     dsp_interleave          = (control >> 0) & 1;
        bool     direction               = control & NODE_CONTROL_DIRECTION;
        uint32_t unk2                    = (control >>  2) & 0x3;
        bool     buffer_offset_writeback = (control >>  4) & 1;
        uint32_t buf_id                  = (control >>  5) & 0xf;
        // bool     unk9                    = (control >>  9) & 1; /* FIXME: What does this do? */
        uint32_t format                  = (control >> 10) & 0x7;
        bool     unk13                   = (control >> 13) & 1;
        // uint32_t dsp_step                = (control >> 14) & 0x3FF; // FIXME

        /* Check for unhandled control settings */
        assert(unk2 == 0x0);
        assert(unk13 == false);

        /* Decode count for interleaved mode */
        uint32_t channel_count = (count & 0xF) + 1;
        uint32_t block_count = count >> 4;

        unsigned int item_size = 4;
        uint32_t item_mask = 0xffffffff;
        // bool lsb = (format == 6); // FIXME

        switch(format) {
        case 1:
            item_size = 2;
            item_mask = 0x0000ffff;
            break;
        case 2:
        case 6:
            item_size = 4;
            item_mask = 0x00ffffff;
            break;
        default:
            fprintf(stderr, "Unknown dsp dma format: 0x%x\n", format);
            assert(false);
            break;
        }

        size_t scratch_addr = scratch_base + scratch_offset;
        uint32_t mem_address = 0;
        int mem_space = DSP_SPACE_X;

        if (dsp_offset < 0x1800) {
            assert(dsp_offset+count < 0x1800);
            mem_space = DSP_SPACE_X;
            mem_address = dsp_offset;
        } else if (dsp_offset >= 0x1800 && dsp_offset < 0x2000) { //?
            assert(dsp_offset+count < 0x2000);
            mem_space = DSP_SPACE_Y;
            mem_address = dsp_offset - 0x1800;
        } else if (dsp_offset >= 0x2800 && dsp_offset < 0x3800) { //?
            assert(dsp_offset+count < 0x3800);
            mem_space = DSP_SPACE_P;
            mem_address = dsp_offset - 0x2800;
        } else {
            fprintf(stderr, "Attempt to access %08x\n", dsp_offset);
            assert(false);
        }

        size_t transfer_size = count * item_size;

        // FIXME: Remove this intermediate buffer
        static uint8_t *scratch_buf = NULL;
        static ssize_t scratch_buf_size = -1;
        if (count * item_size > scratch_buf_size) {
            scratch_buf_size = count * item_size;
            scratch_buf = malloc(scratch_buf_size);
        }

        if (direction) {
            if (dsp_interleave) {
                // FIXME: Above xfer size calculation instead of
                // overwriting here
                transfer_size = block_count * item_size * channel_count;

                // Interleave samples
                for (int i = 0; i < block_count; i++) {
                    for (int ch = 0; ch < channel_count; ch++) {
                        uint32_t v = dsp56k_read_memory(s->core,
                            mem_space, mem_address+ch*block_count+i);
                        switch(item_size) {
                        case 2:
                            *(uint16_t*)(scratch_buf + i*2*channel_count + ch*2) = v >> 8;
                            break;
                        case 4:
                            *(uint32_t*)(scratch_buf + i*4*channel_count + ch*4) = v;
                            break;
                        default:
                            assert(false);
                            break;
                        }
                    }
                }
            } else {
                for (int i = 0; i < count; i++) {
                    uint32_t v = dsp56k_read_memory(s->core, mem_space, mem_address+i);
                    switch(item_size) {
                    case 2:
                        *(uint16_t*)(scratch_buf + i*2) = v >> 8;
                        break;
                    case 4:
                        *(uint32_t*)(scratch_buf + i*4) = v;
                        break;
                    default:
                        assert(false);
                        break;
                    }
                }

            }

            /* FIXME: Move to function; then reuse for both directions */
            switch (buf_id) {
            case 0x0:
            case 0x1:
            case 0x2:
            case 0x3:
                s->fifo_rw(s->rw_opaque, scratch_buf, buf_id, transfer_size, 1);
                break;
            case 0xE:
                scratch_circular_copy(s, scratch_base, &scratch_offset, scratch_size, transfer_size, scratch_buf, 1);
                break;
            case 0xF:
                s->scratch_rw(s->rw_opaque, scratch_buf, scratch_addr, transfer_size, 1);
                break;
            default:
                fprintf(stderr, "Unknown DSP DMA buffer: 0x%x\n", buf_id);
                assert(false);
                break;
            }
        } else {
            assert(!dsp_interleave);

            if (buf_id == 0xe) {
                scratch_circular_copy(s, scratch_base, &scratch_offset, scratch_size, transfer_size, scratch_buf, 0);
            } else if (buf_id == 0xf) {
                s->scratch_rw(s->rw_opaque, scratch_buf, scratch_addr, transfer_size, 0);
            } else {
                fprintf(stderr, "Unhandled DSP DMA buffer: 0x%x\n", buf_id);
                assert(false);
            }

            for (int i = 0; i < count; i++) {
                uint32_t v;
                switch(item_size) {
                case 2:
                    v = *(uint16_t*)(scratch_buf + i*2) << 8;
                    break;
                case 4:
                    v = (*(uint32_t*)(scratch_buf + i*4)) & item_mask;
                    break;
                default:
                    v = 0;
                    assert(false);
                    break;
                }

                dsp56k_write_memory(s->core, mem_space, mem_address+i, v);
            }
        }

        if (buffer_offset_writeback) {
            dsp56k_write_memory(s->core, block_space, block_addr+4, scratch_offset);
        }

    }
}

uint32_t dsp_dma_read(DSPDMAState *s, DSPDMARegister reg)
{
    switch (reg) {
    case DMA_CONFIGURATION:
        return s->configuration;
    case DMA_CONTROL:
        return s->control;
    case DMA_START_BLOCK:
        return s->start_block;
    case DMA_NEXT_BLOCK:
        return s->next_block;
    default:
        assert(false);
    }
    return 0;
}

void dsp_dma_write(DSPDMAState *s, DSPDMARegister reg, uint32_t v)
{
    switch (reg) {
    case DMA_CONFIGURATION:
        s->configuration = v;
        break;
    case DMA_CONTROL:
        switch(v & DMA_CONTROL_ACTION) {
        case DMA_CONTROL_ACTION_START:
            s->control |= DMA_CONTROL_RUNNING;
            s->control &= ~DMA_CONTROL_STOPPED;
            break;
        case DMA_CONTROL_ACTION_STOP:
            s->control |= DMA_CONTROL_STOPPED;
            s->control &= ~DMA_CONTROL_RUNNING;
            break;
        case DMA_CONTROL_ACTION_FREEZE:
            s->control |= DMA_CONTROL_FROZEN;
            break;
        case DMA_CONTROL_ACTION_UNFREEZE:
            s->control &= ~DMA_CONTROL_FROZEN;
            break;
        default:
            assert(false);
            break;
        }
        dsp_dma_run(s);

        // IDK about this, but need to stop somehow?
        // s->control |= DMA_CONTROL_STOPPED;
        break;
    case DMA_START_BLOCK:
        s->start_block = v;
        break;
    case DMA_NEXT_BLOCK:
        s->next_block = v;
        break;
    default:
        assert(false);
    }
}

