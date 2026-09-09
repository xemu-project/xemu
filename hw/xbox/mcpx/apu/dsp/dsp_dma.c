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

#include "qemu/osdep.h"
#include "qemu/atomic.h"
#include "qemu/compiler.h"
#include "debug.h"
#include "dsp_dma.h"
#include "dsp_dma_regs.h"
#include "dsp_mem.h"
#include "trace.h"

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

/* Node descriptors and transfer data share one flat DSP address space:
 *   $0000-$17FF -> X, $1800-$1FFF -> Y, $2800-$37FF -> P.
 * Everything else is unmapped. Silicon reads unmapped addresses as zero and
 * drops writes to them, silently - no error, no abort - and a transfer that
 * runs off the end of a window simply continues into the next one (probed:
 * dsp_offset $17F8 for 32 words read the tail of X and then Y:$0000+). So
 * addresses are mapped per access instead of range-checked per node. */
static bool dsp_dma_map(uint32_t addr, int *space, uint32_t *offset)
{
    if (addr < 0x1800) {
        *space = DSP_SPACE_X;
        *offset = addr;
    } else if (addr < 0x2000) {
        *space = DSP_SPACE_Y;
        *offset = addr - 0x1800;
    } else if (addr >= 0x2800 && addr < 0x3800) {
        *space = DSP_SPACE_P;
        *offset = addr - 0x2800;
    } else if (addr & 0x4000) {
        /* Unmapped like every other hole, in every context probed. The AC3
         * encoder relies on it: its resident command $0c builds a bit-14
         * FIFO #1 node for each IEC 61937 stuffing run, so the microcode
         * emits zeros without keeping any. Decoding the flag as
         * X:(offset & $3FFF) instead makes those transfers carry a stale
         * frame image behind a valid-looking preamble. */
        return false;
    } else {
        return false;
    }
    return true;
}

static uint32_t dsp_dma_mem_read(DSPDMAState *s, uint32_t addr)
{
    int space;
    uint32_t offset;

    if (!dsp_dma_map(addr, &space, &offset)) {
        return 0;
    }
    return s->mem_read(s->mem_opaque, space, offset);
}

/* Words this engine has landed in P memory, each of which can dirty a
 * translated block; in the scheduler trace report. Both cores' engines
 * write it, from whichever thread is running them. */
uint64_t g_dsp_dma_p_writes;

static void dsp_dma_mem_write(DSPDMAState *s, uint32_t addr, uint32_t value)
{
    int space;
    uint32_t offset;

    if (dsp_dma_map(addr, &space, &offset)) {
        if (space == DSP_SPACE_P) {
            qatomic_add(&g_dsp_dma_p_writes, 1);
        }
        s->mem_write(s->mem_opaque, space, offset, value);
    }
}

/* Buffer bytes per item, and DSP words consumed per item. Format 3 carries a
 * 32-bit sample in a *pair* of DSP words; formats 4, 5 and 7 are rejected by
 * the engine (see dsp_dma_format_valid). */
typedef struct DSPDMAFormat {
    uint8_t item_size;
    uint8_t words_per_item;
} DSPDMAFormat;

static const DSPDMAFormat dsp_dma_formats[8] = {
    [0] = { 1, 1 }, /* 8 bit, offset binary */
    [1] = { 2, 1 }, /* 16 bit */
    [2] = { 4, 1 }, /* 24 bit, MSB-aligned in a 32-bit slot */
    [3] = { 4, 2 }, /* 32 bit, from a DSP word pair */
    [6] = { 4, 1 }, /* 24 bit, LSB-aligned in a 32-bit slot */
};

static bool dsp_dma_format_valid(uint32_t format)
{
    return dsp_dma_formats[format].item_size != 0;
}

/* Format 0 is not a truncation. Silicon rounds on bit 15, saturates, and
 * emits offset binary: $008000 -> $81 (not $80), $7FFF00 -> $FF (saturates
 * rather than wrapping to $00), $800000 -> $00, $FE8000 -> $7F. */
static uint8_t dsp_dma_pack8(uint32_t word)
{
    int32_t v = (int32_t)((word & 0xffffff) ^ 0x800000) - 0x800000;
    int32_t r = (v + 0x8000) >> 16;

    if (r > 127) {
        r = 127;
    }
    return (uint8_t)(r + 128);
}

/* Serialize one item's DSP words into buffer bytes. Byte orders are as
 * probed on silicon; note that format 2 is MSB-aligned (a leading zero
 * byte), which is what distinguishes it from format 6. */
static void dsp_dma_pack_item(uint32_t format, const uint32_t *words,
                              uint8_t *out)
{
    uint32_t w = words[0] & 0xffffff;

    switch (format) {
    case 0:
        out[0] = dsp_dma_pack8(w);
        break;
    case 1:
        out[0] = (w >> 8) & 0xff;
        out[1] = (w >> 16) & 0xff;
        break;
    case 2:
        out[0] = 0;
        out[1] = w & 0xff;
        out[2] = (w >> 8) & 0xff;
        out[3] = (w >> 16) & 0xff;
        break;
    case 3: {
        uint32_t w1 = words[1] & 0xffffff;
        out[0] = (w >> 16) & 0xff;
        out[1] = w1 & 0xff;
        out[2] = (w1 >> 8) & 0xff;
        out[3] = (w1 >> 16) & 0xff;
        break;
    }
    case 6:
        out[0] = w & 0xff;
        out[1] = (w >> 8) & 0xff;
        out[2] = (w >> 16) & 0xff;
        out[3] = 0;
        break;
    }
}

/* Inverse of dsp_dma_pack_item. The format-3 pairing is probed in this
 * direction too (an all-$EE buffer produced $EE0000, $EEEEEE word pairs);
 * the format-0 expansion is the natural inverse and is not probed. */
static void dsp_dma_unpack_item(uint32_t format, const uint8_t *in,
                                uint32_t *words)
{
    switch (format) {
    case 0:
        words[0] = (uint32_t)(in[0] ^ 0x80) << 16;
        break;
    case 1:
        words[0] = ((uint32_t)in[1] << 16) | ((uint32_t)in[0] << 8);
        break;
    case 2:
        words[0] = ((uint32_t)in[3] << 16) | ((uint32_t)in[2] << 8) | in[1];
        break;
    case 3:
        words[0] = (uint32_t)in[0] << 16;
        words[1] = ((uint32_t)in[3] << 16) | ((uint32_t)in[2] << 8) | in[1];
        break;
    case 6:
        words[0] = ((uint32_t)in[2] << 16) | ((uint32_t)in[1] << 8) | in[0];
        break;
    }
}

/* DSP address of word `w` of item `item`. Linear transfers advance by
 * dsp_step per word; interleaved transfers place channel `ch` of block `b`
 * at dsp_offset + (b + ch*dsp_step) * words_per_item (probed with step !=
 * block count, so the stride is the control word's, not the block count,
 * and with the word-pair format, whose items land as intact pairs rather
 * than overlapping by a word). */
static uint32_t dsp_dma_item_addr(uint32_t dsp_offset, uint32_t dsp_step,
                                  bool interleave, uint32_t channel_count,
                                  uint32_t words_per_item, uint32_t item,
                                  uint32_t w)
{
    if (interleave) {
        uint32_t block = item / channel_count;
        uint32_t ch = item % channel_count;
        return dsp_offset + (block + ch * dsp_step) * words_per_item + w;
    }
    return dsp_offset + (item * words_per_item + w) * dsp_step;
}

/* End (exclusive) of the mapped-or-unmapped window containing addr, so a
 * consecutive address run splits into segments that stay in one space.
 * Mirrors dsp_dma_map's regions. */
static uint32_t dsp_dma_window_end(uint32_t addr)
{
    if (addr < 0x1800) {
        return 0x1800;
    }
    if (addr < 0x2000) {
        return 0x2000;
    }
    if (addr < 0x2800) {
        return 0x2800;
    }
    if (addr < 0x3800) {
        return 0x3800;
    }
    return (addr | 0x3fff) + 1;
}

/* Bulk transfer staging: one mem_*_run call per mapped window instead of a
 * mapped call per word. Unmapped stretches keep silicon's behaviour - reads
 * as zero, writes dropped - and a run continues across window boundaries
 * exactly as the per-word path does. */
static void dsp_dma_words_read(DSPDMAState *s, uint32_t addr, uint32_t *out,
                               uint32_t count)
{
    while (count) {
        int space;
        uint32_t offset;
        uint32_t n = MIN(count, dsp_dma_window_end(addr) - addr);

        if (dsp_dma_map(addr, &space, &offset)) {
            s->mem_read_run(s->mem_opaque, space, offset, out, n);
        } else {
            memset(out, 0, n * sizeof(*out));
        }
        addr += n;
        out += n;
        count -= n;
    }
}

static void dsp_dma_words_write(DSPDMAState *s, uint32_t addr,
                                const uint32_t *vals, uint32_t count)
{
    while (count) {
        int space;
        uint32_t offset;
        uint32_t n = MIN(count, dsp_dma_window_end(addr) - addr);

        if (dsp_dma_map(addr, &space, &offset)) {
            if (space == DSP_SPACE_P) {
                qatomic_add(&g_dsp_dma_p_writes, n);
            }
            s->mem_write_run(s->mem_opaque, space, offset, vals, n);
        }
        addr += n;
        vals += n;
        count -= n;
    }
}

/* A chain whose next-pointer loops back on itself runs forever on silicon
 * (probed: DMA_CONTROL still reads RUNNING after the node has been replayed
 * indefinitely) - harmless there, because the DSP keeps executing and can
 * issue STOP. xemu runs the chain synchronously inside the MMIO write, so
 * the same node would hang the emulator; bail out instead, leaving RUNNING
 * set as silicon does. */
#define DSP_DMA_MAX_NODES 4096

static void dsp_dma_run(DSPDMAState *s)
{
    unsigned int nodes = 0;

    if (!(s->control & DMA_CONTROL_RUNNING)
        || (s->control & DMA_CONTROL_FROZEN)) {
        return;
    }

    /* Only the EOL bit ends a chain. A pointer of $0000 fetches the
     * descriptor at X:$0000 like any other address (probed: a next pointer
     * of zero ran the node seeded there, and an unmapped pointer reads a
     * zero descriptor whose zero next pointer does the same). */
    while (!(s->next_block & NODE_POINTER_EOL)) {
        uint32_t addr = s->next_block & NODE_POINTER_VAL;

        if (++nodes > DSP_DMA_MAX_NODES) {
            fprintf(stderr, "DSP DMA: chain exceeded %d nodes at %04x; "
                            "abandoning it (looping descriptor?)\n",
                    DSP_DMA_MAX_NODES, addr);
            break;
        }

        /* The descriptor is seven consecutive words; a chain visits dozens
         * of nodes a kick, so read it in one crossing rather than seven. */
        uint32_t desc[7];
        if (s->mem_read_run) {
            dsp_dma_words_read(s, addr, desc, 7);
        } else {
            for (int i = 0; i < 7; i++) {
                desc[i] = dsp_dma_mem_read(s, addr + i);
            }
        }
        uint32_t next_block     = desc[0];
        uint32_t control        = desc[1];
        uint32_t count          = desc[2];
        uint32_t dsp_offset     = desc[3];
        uint32_t scratch_offset = desc[4];
        uint32_t scratch_base   = desc[5];
        uint32_t scratch_size   = desc[6] + 1;

        s->next_block = next_block;
        if (s->next_block & NODE_POINTER_EOL) {
            s->eol = true;
        }

        trace_dsp_dma_node(s->is_gp ? "GP" : "EP", addr, next_block, control,
                           count, dsp_offset, scratch_offset, scratch_base,
                           scratch_size);

        /* Control word, as probed: intr_sel (bits 2-3) selects the
         * completion interrupt, value n != 0 raising interrupt-status bit
         * 3+n when the node completes; dsp_step (bits 14+) is the DSP
         * address stride, per word for linear transfers (0 = re-access one
         * address) and per channel for interleaved ones. Bits 9 and 13
         * showed no effect and are ignored. */
        bool     dsp_interleave          = (control >> 0) & 1;
        bool     direction               = control & NODE_CONTROL_DIRECTION;
        uint32_t intr_sel                = (control >>  2) & 0x3;
        bool     buffer_offset_writeback = (control >>  4) & 1;
        uint32_t buf_id                  = (control >>  5) & 0xf;
        uint32_t format                  = (control >> 10) & 0x7;
        uint32_t dsp_step                = (control >> 14) & 0x3FF;

        /* Buffer ids $4-$7 and $C-$D are rejected outright, as are formats
         * 4, 5 and 7: silicon transfers nothing, raises the error bit, and
         * still latches EOL, leaving the chain to continue. */
        bool buf_valid = buf_id <= 0x3 || (buf_id >= 0x8 && buf_id <= 0xb) ||
                         buf_id == 0xe || buf_id == 0xf;
        if (!buf_valid || !dsp_dma_format_valid(format)) {
            s->error = true;
            s->pending_interrupts |= DMA_INTERRUPT_ERROR;
            continue;
        }

        uint32_t item_size = dsp_dma_formats[format].item_size;
        uint32_t words_per_item = dsp_dma_formats[format].words_per_item;

        uint32_t channel_count = dsp_interleave ? (count & 0xF) + 1 : 1;
        uint32_t items = dsp_interleave ? (count >> 4) * channel_count : count;
        size_t transfer_size = (size_t)items * item_size;

        if (transfer_size == 0) {
            goto node_done;
        }

        // FIXME: Remove this intermediate buffer
        if (transfer_size > s->scratch_buf_size) {
            s->scratch_buf_size = transfer_size;
            s->scratch_buf = g_realloc(s->scratch_buf, s->scratch_buf_size);
        }
        uint8_t *scratch_buf = s->scratch_buf;

        /* A linear step-1 node touches one consecutive DSP address run, and
         * that is what the overlay loads and sample moves all are. Stage the
         * words and cross into the DSP once per mapped window; strides,
         * interleave, and backends without bulk callbacks keep the scalar
         * path. */
        uint32_t total_words = items * words_per_item;
        bool bulk = !dsp_interleave && dsp_step == 1
            && s->mem_read_run && s->mem_write_run;
        if (bulk && total_words > s->word_buf_words) {
            s->word_buf_words = total_words;
            s->word_buf =
                g_realloc(s->word_buf, total_words * sizeof(uint32_t));
        }

        /* Scratch ($F, and the $8-$B aliases) is addressed by scratch_offset
         * alone; scratch_base is ignored (probed). */
        if (direction) {
            if (bulk) {
                dsp_dma_words_read(s, dsp_offset, s->word_buf, total_words);
                for (uint32_t i = 0; i < items; i++) {
                    dsp_dma_pack_item(format,
                                      &s->word_buf[i * words_per_item],
                                      &scratch_buf[i * item_size]);
                }
            } else {
                for (uint32_t i = 0; i < items; i++) {
                    uint32_t words[2] = { 0, 0 };
                    for (uint32_t w = 0; w < words_per_item; w++) {
                        words[w] = dsp_dma_mem_read(s, dsp_dma_item_addr(
                            dsp_offset, dsp_step, dsp_interleave,
                            channel_count, words_per_item, i, w));
                    }
                    dsp_dma_pack_item(format, words,
                                      &scratch_buf[i * item_size]);
                }
            }
        }

        switch (buf_id) {
        case 0x0 ... 0x3:
            s->fifo_rw(s->rw_opaque, scratch_buf, buf_id, transfer_size,
                       direction);
            break;
        case 0xE:
            scratch_circular_copy(s, scratch_base, &scratch_offset,
                                  scratch_size, transfer_size, scratch_buf,
                                  direction);
            break;
        default: /* $8-$B behave as $F on silicon */
            s->scratch_rw(s->rw_opaque, scratch_buf, scratch_offset,
                          transfer_size, direction);
            break;
        }

        if (!direction) {
            if (bulk) {
                for (uint32_t i = 0; i < items; i++) {
                    dsp_dma_unpack_item(format, &scratch_buf[i * item_size],
                                        &s->word_buf[i * words_per_item]);
                }
                dsp_dma_words_write(s, dsp_offset, s->word_buf, total_words);
            } else {
                for (uint32_t i = 0; i < items; i++) {
                    uint32_t words[2] = { 0, 0 };
                    dsp_dma_unpack_item(format, &scratch_buf[i * item_size],
                                        words);
                    for (uint32_t w = 0; w < words_per_item; w++) {
                        dsp_dma_mem_write(s, dsp_dma_item_addr(
                            dsp_offset, dsp_step, dsp_interleave,
                            channel_count, words_per_item, i, w), words[w]);
                    }
                }
            }
        }

node_done:
        if (buffer_offset_writeback) {
            dsp_dma_mem_write(s, addr + 4, scratch_offset);
        }

        if (intr_sel) {
            s->pending_interrupts |= 1 << (3 + intr_sel);
        }
    }
}

void dsp_dma_completion_seen(DSPDMAState *s)
{
    if (s->eol) {
        s->control &= ~DMA_CONTROL_RUNNING;
        s->dma_read_count = 0;
    }
}

uint32_t dsp_dma_read(DSPDMAState *s, DSPDMARegister reg)
{
    switch (reg) {
    case DMA_CONFIGURATION:
        return s->configuration;
    case DMA_CONTROL:
        /* The chain ran inside the START write, but silicon takes
         * microseconds, and the microcode confirms the start by polling
         * RUNNING before it waits for EOL. So RUNNING stays readable until
         * the program has looked at the engine: two reads here, or one of
         * the interrupt status with EOL set (dsp_dma_completion_seen).
         * Then the register reads 0, as probed after every completed
         * chain: not RUNNING, and not STOPPED either. */
        if (s->control & DMA_CONTROL_RUNNING) {
            s->dma_read_count++;
            if (s->dma_read_count > 2) {
                s->control &= ~DMA_CONTROL_RUNNING;
                s->dma_read_count = 0;
            }
        }
        return s->control;
    case DMA_START_BLOCK:
        return s->start_block;
    case DMA_NEXT_BLOCK:
        return s->next_block;
    default:
        break;
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
            s->dma_read_count = 0;
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
            /* NOP, ABORT and the unassigned encodings are silent no-ops on
             * silicon: nothing starts, DMA_CONTROL reads back 0 and
             * NEXT_BLOCK keeps the pointer the DSP wrote. */
            break;
        }
        dsp_dma_run(s);
        break;
    case DMA_START_BLOCK:
        s->start_block = v;
        break;
    case DMA_NEXT_BLOCK:
        s->next_block = v;
        break;
    default:
        break;
    }
}

