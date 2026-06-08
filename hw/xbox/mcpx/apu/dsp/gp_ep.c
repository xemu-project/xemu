/*
 * QEMU MCPX Audio Processing Unit implementation
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2018-2019 Jannik Vogel
 * Copyright (c) 2019-2025 Matt Borgerson
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

#include "hw/xbox/mcpx/apu/apu_int.h"

static const int16_t ep_silence[256][2] = { 0 };

void mcpx_apu_update_dsp_preference(MCPXAPUState *d)
{
    static int last_known_preference = -1;

    if (last_known_preference == (int)g_config.audio.use_dsp) {
        return;
    }

    if (g_config.audio.use_dsp) {
        d->monitor.point = MCPX_APU_DEBUG_MON_GP_OR_EP;
        d->gp.realtime = true;
        d->ep.realtime = true;
    } else {
        d->monitor.point = MCPX_APU_DEBUG_MON_VP;
        d->gp.realtime = false;
        d->ep.realtime = false;
    }

    last_known_preference = g_config.audio.use_dsp;
}

static void scatter_gather_rw(MCPXAPUState *d, hwaddr sge_base,
                              unsigned int max_sge, uint8_t *ptr, uint32_t addr,
                              size_t len, bool dir)
{
    unsigned int page_entry = addr / TARGET_PAGE_SIZE;
    unsigned int offset_in_page = addr % TARGET_PAGE_SIZE;
    unsigned int bytes_to_copy = TARGET_PAGE_SIZE - offset_in_page;

    while (len > 0) {
        assert(page_entry <= max_sge);

        uint32_t prd_address = ldl_le_phys(&address_space_memory,
                                           sge_base + page_entry * 8 + 0);
        // uint32_t prd_control = ldl_le_phys(&address_space_memory,
        //                                     sge_base + page_entry * 8 + 4);

        hwaddr paddr = prd_address + offset_in_page;

        if (bytes_to_copy > len) {
            bytes_to_copy = len;
        }

        assert(paddr + bytes_to_copy < memory_region_size(d->ram));

        if (dir) {
            memcpy(&d->ram_ptr[paddr], ptr, bytes_to_copy);
            memory_region_set_dirty(d->ram, paddr, bytes_to_copy);
        } else {
            memcpy(ptr, &d->ram_ptr[paddr], bytes_to_copy);
        }

        ptr += bytes_to_copy;
        len -= bytes_to_copy;

        /* After the first iteration, we are page aligned */
        page_entry += 1;
        bytes_to_copy = TARGET_PAGE_SIZE;
        offset_in_page = 0;
    }
}

static void gp_scratch_rw(void *opaque, uint8_t *ptr, uint32_t addr, size_t len,
                          bool dir)
{
    MCPXAPUState *d = opaque;
    // fprintf(stderr, "GP %s scratch 0x%x bytes (0x%x words) at %x (0x%x words)\n", dir ? "writing to" : "reading from", len, len/4, addr, addr/4);
    scatter_gather_rw(d, d->regs[NV_PAPU_GPSADDR], d->regs[NV_PAPU_GPSMAXSGE],
                      ptr, addr, len, dir);
}

static void ep_scratch_rw(void *opaque, uint8_t *ptr, uint32_t addr, size_t len,
                          bool dir)
{
    MCPXAPUState *d = opaque;
    // fprintf(stderr, "EP %s scratch 0x%x bytes (0x%x words) at %x (0x%x words)\n", dir ? "writing to" : "reading from", len, len/4, addr, addr/4);
    scatter_gather_rw(d, d->regs[NV_PAPU_EPSADDR], d->regs[NV_PAPU_EPSMAXSGE],
                      ptr, addr, len, dir);
}

static uint32_t circular_scatter_gather_rw(MCPXAPUState *d, hwaddr sge_base,
                                           unsigned int max_sge, uint8_t *ptr,
                                           uint32_t base, uint32_t end,
                                           uint32_t cur, size_t len, bool dir)
{
    while (len > 0) {
        unsigned int bytes_to_copy = end - cur;

        if (bytes_to_copy > len) {
            bytes_to_copy = len;
        }

        DPRINTF("circular scatter gather %s in range 0x%x - 0x%x at 0x%x of "
                "length 0x%x / 0x%lx bytes\n",
                dir ? "write" : "read", base, end, cur, bytes_to_copy, len);

        assert((cur >= base) && ((cur + bytes_to_copy) <= end));
        scatter_gather_rw(d, sge_base, max_sge, ptr, cur, bytes_to_copy, dir);

        ptr += bytes_to_copy;
        len -= bytes_to_copy;

        /* After the first iteration we might have to wrap */
        cur += bytes_to_copy;
        if (cur >= end) {
            assert(cur == end);
            cur = base;
        }
    }

    return cur;
}

static void gp_fifo_rw(void *opaque, uint8_t *ptr, unsigned int index,
                       size_t len, bool dir)
{
    MCPXAPUState *d = opaque;
    uint32_t base;
    uint32_t end;
    hwaddr cur_reg;
    if (dir) {
        assert(index < GP_OUTPUT_FIFO_COUNT);
        base = GET_MASK(d->regs[NV_PAPU_GPOFBASE0 + 0x10 * index],
                        NV_PAPU_GPOFBASE0_VALUE);
        end = GET_MASK(d->regs[NV_PAPU_GPOFEND0 + 0x10 * index],
                       NV_PAPU_GPOFEND0_VALUE);
        cur_reg = NV_PAPU_GPOFCUR0 + 0x10 * index;
    } else {
        assert(index < GP_INPUT_FIFO_COUNT);
        base = GET_MASK(d->regs[NV_PAPU_GPIFBASE0 + 0x10 * index],
                        NV_PAPU_GPOFBASE0_VALUE);
        end = GET_MASK(d->regs[NV_PAPU_GPIFEND0 + 0x10 * index],
                       NV_PAPU_GPOFEND0_VALUE);
        cur_reg = NV_PAPU_GPIFCUR0 + 0x10 * index;
    }

    uint32_t cur = GET_MASK(d->regs[cur_reg], NV_PAPU_GPOFCUR0_VALUE);

    // fprintf(stderr, "GP %s fifo #%d, base = %x, end = %x, cur = %x, len = %x\n",
    //     dir ? "writing to" : "reading from", index,
    //     base, end, cur, len);

    /* DSP hangs if current >= end; but forces current >= base */
    assert(cur < end);
    if (cur < base) {
        cur = base;
    }

    cur = circular_scatter_gather_rw(d,
        d->regs[NV_PAPU_GPFADDR], d->regs[NV_PAPU_GPFMAXSGE],
        ptr, base, end, cur, len, dir);

    SET_MASK(d->regs[cur_reg], NV_PAPU_GPOFCUR0_VALUE, cur);
}

static bool ep_sink_samples(MCPXAPUState *d, uint8_t *ptr, size_t len)
{
    if (d->monitor.point == MCPX_APU_DEBUG_MON_AC97) {
        return false;
    } else if ((d->monitor.point == MCPX_APU_DEBUG_MON_EP) ||
        (d->monitor.point == MCPX_APU_DEBUG_MON_GP_OR_EP)) {
        assert(len == sizeof(d->monitor.frame_buf));
        memcpy(d->monitor.frame_buf, ptr, len);
    }

    return true;
}

static void ep_fifo_rw(void *opaque, uint8_t *ptr, unsigned int index,
                       size_t len, bool dir)
{
    MCPXAPUState *d = opaque;
    uint32_t base;
    uint32_t end;
    hwaddr cur_reg;
    if (dir) {
        assert(index < EP_OUTPUT_FIFO_COUNT);
        base = GET_MASK(d->regs[NV_PAPU_EPOFBASE0 + 0x10 * index],
                        NV_PAPU_GPOFBASE0_VALUE);
        end = GET_MASK(d->regs[NV_PAPU_EPOFEND0 + 0x10 * index],
                       NV_PAPU_GPOFEND0_VALUE);
        cur_reg = NV_PAPU_EPOFCUR0 + 0x10 * index;
    } else {
        assert(index < EP_INPUT_FIFO_COUNT);
        base = GET_MASK(d->regs[NV_PAPU_EPIFBASE0 + 0x10 * index],
                        NV_PAPU_GPOFBASE0_VALUE);
        end = GET_MASK(d->regs[NV_PAPU_EPIFEND0 + 0x10 * index],
                       NV_PAPU_GPOFEND0_VALUE);
        cur_reg = NV_PAPU_EPIFCUR0 + 0x10 * index;
    }

    uint32_t cur = GET_MASK(d->regs[cur_reg], NV_PAPU_GPOFCUR0_VALUE);

    // fprintf(stderr, "EP %s fifo #%d, base = %x, end = %x, cur = %x, len = %x\n",
    //     dir ? "writing to" : "reading from", index,
    //     base, end, cur, len);

    if (dir && index == 0) {
        bool did_sink = ep_sink_samples(d, ptr, len);
        if (did_sink) {
            /* Since we are sinking, push silence out */
            assert(len <= sizeof(ep_silence));
            ptr = (uint8_t*)ep_silence;
        }
    }

    /* DSP hangs if current >= end; but forces current >= base */
    if (cur >= end) {
        cur = cur % (end - base);
    }
    if (cur < base) {
        cur = base;
    }

    cur = circular_scatter_gather_rw(d,
        d->regs[NV_PAPU_EPFADDR], d->regs[NV_PAPU_EPFMAXSGE],
        ptr, base, end, cur, len, dir);

    SET_MASK(d->regs[cur_reg], NV_PAPU_GPOFCUR0_VALUE, cur);
}

static void proc_rst_write(DSPState *dsp, uint32_t oldval, uint32_t val)
{
    if (!(val & NV_PAPU_GPRST_GPRST) || !(val & NV_PAPU_GPRST_GPDSPRST)) {
        dsp_reset(dsp);
    } else if (
        (!(oldval & NV_PAPU_GPRST_GPRST) || !(oldval & NV_PAPU_GPRST_GPDSPRST))
        && ((val & NV_PAPU_GPRST_GPRST) && (val & NV_PAPU_GPRST_GPDSPRST))) {
        dsp_bootstrap(dsp);
    }
}

/* Global Processor - programmable DSP */
static uint64_t gp_read(void *opaque, hwaddr addr, unsigned int size)
{
    MCPXAPUState *d = opaque;

    assert(size == 4);
    assert(addr % 4 == 0);

    uint64_t r = 0;
    switch (addr) {
    case NV_PAPU_GPXMEM ... NV_PAPU_GPXMEM + 0x1000 * 4 - 1: {
        uint32_t xaddr = (addr - NV_PAPU_GPXMEM) / 4;
        r = dsp_read_memory(d->gp.dsp, 'X', xaddr);
        // fprintf(stderr, "read GP NV_PAPU_GPXMEM [%x] -> %x\n", xaddr, r);
        break;
    }
    case NV_PAPU_GPMIXBUF ... NV_PAPU_GPMIXBUF + 0x400 * 4 - 1: {
        uint32_t xaddr = (addr - NV_PAPU_GPMIXBUF) / 4;
        r = dsp_read_memory(d->gp.dsp, 'X', GP_DSP_MIXBUF_BASE + xaddr);
        // fprintf(stderr, "read GP NV_PAPU_GPMIXBUF [%x] -> %x\n", xaddr, r);
        break;
    }
    case NV_PAPU_GPYMEM ... NV_PAPU_GPYMEM + 0x800 * 4 - 1: {
        uint32_t yaddr = (addr - NV_PAPU_GPYMEM) / 4;
        r = dsp_read_memory(d->gp.dsp, 'Y', yaddr);
        // fprintf(stderr, "read GP NV_PAPU_GPYMEM [%x] -> %x\n", yaddr, r);
        break;
    }
    case NV_PAPU_GPPMEM ... NV_PAPU_GPPMEM + 0x1000 * 4 - 1: {
        uint32_t paddr = (addr - NV_PAPU_GPPMEM) / 4;
        r = dsp_read_memory(d->gp.dsp, 'P', paddr);
        // fprintf(stderr, "read GP NV_PAPU_GPPMEM [%x] -> %x\n", paddr, r);
        break;
    }
    default:
        r = d->gp.regs[addr];
        break;
    }
    DPRINTF("mcpx apu GP: read [0x%" HWADDR_PRIx "] -> 0x%lx\n", addr, r);

    return r;
}

static void gp_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size)
{
    MCPXAPUState *d = opaque;

    qemu_mutex_lock(&d->lock);

    assert(size == 4);
    assert(addr % 4 == 0);

    DPRINTF("mcpx apu GP: [0x%" HWADDR_PRIx "] = 0x%lx\n", addr, val);

    switch (addr) {
    case NV_PAPU_GPXMEM ... NV_PAPU_GPXMEM + 0x1000 * 4 - 1: {
        uint32_t xaddr = (addr - NV_PAPU_GPXMEM) / 4;
        // fprintf(stderr, "gp write xmem %x = %x\n", xaddr, val);
        dsp_write_memory(d->gp.dsp, 'X', xaddr, val);
        break;
    }
    case NV_PAPU_GPMIXBUF ... NV_PAPU_GPMIXBUF + 0x400 * 4 - 1: {
        uint32_t xaddr = (addr - NV_PAPU_GPMIXBUF) / 4;
        // fprintf(stderr, "gp write xmixbuf %x = %x\n", xaddr, val);
        dsp_write_memory(d->gp.dsp, 'X', GP_DSP_MIXBUF_BASE + xaddr, val);
        break;
    }
    case NV_PAPU_GPYMEM ... NV_PAPU_GPYMEM + 0x800 * 4 - 1: {
        uint32_t yaddr = (addr - NV_PAPU_GPYMEM) / 4;
        // fprintf(stderr, "gp write ymem %x = %x\n", yaddr, val);
        dsp_write_memory(d->gp.dsp, 'Y', yaddr, val);
        break;
    }
    case NV_PAPU_GPPMEM ... NV_PAPU_GPPMEM + 0x1000 * 4 - 1: {
        uint32_t paddr = (addr - NV_PAPU_GPPMEM) / 4;
        // fprintf(stderr, "gp write pmem %x = %x\n", paddr, val);
        dsp_write_memory(d->gp.dsp, 'P', paddr, val);
        break;
    }
    case NV_PAPU_GPRST:
        proc_rst_write(d->gp.dsp, d->gp.regs[NV_PAPU_GPRST], val);
        d->gp.regs[NV_PAPU_GPRST] = val;
        break;
    default:
        d->gp.regs[addr] = val;
        break;
    }

    qemu_mutex_unlock(&d->lock);
}

const MemoryRegionOps gp_ops = {
    .read = gp_read,
    .write = gp_write,
};

/* Encode Processor - encoding DSP */
static uint64_t ep_read(void *opaque, hwaddr addr, unsigned int size)
{
    MCPXAPUState *d = opaque;

    assert(size == 4);
    assert(addr % 4 == 0);

    uint64_t r = 0;
    switch (addr) {
    case NV_PAPU_EPXMEM ... NV_PAPU_EPXMEM + 0xC00 * 4 - 1: {
        uint32_t xaddr = (addr - NV_PAPU_EPXMEM) / 4;
        r = dsp_read_memory(d->ep.dsp, 'X', xaddr);
        // fprintf(stderr, "read EP  NV_PAPU_EPXMEM [%x] -> %x\n", xaddr, r);
        break;
    }
    case NV_PAPU_EPYMEM ... NV_PAPU_EPYMEM + 0x100 * 4 - 1: {
        uint32_t yaddr = (addr - NV_PAPU_EPYMEM) / 4;
        r = dsp_read_memory(d->ep.dsp, 'Y', yaddr);
        // fprintf(stderr, "read EP  NV_PAPU_EPYMEM [%x] -> %x\n", yaddr, r);
        break;
    }
    case NV_PAPU_EPPMEM ... NV_PAPU_EPPMEM + 0x1000 * 4 - 1: {
        uint32_t paddr = (addr - NV_PAPU_EPPMEM) / 4;
        r = dsp_read_memory(d->ep.dsp, 'P', paddr);
        // fprintf(stderr, "read EP  NV_PAPU_EPPMEM [%x] -> %x\n", paddr, r);
        break;
    }
    default:
        r = d->ep.regs[addr];
        break;
    }
    DPRINTF("mcpx apu EP: read [0x%" HWADDR_PRIx "] -> 0x%lx\n", addr, r);

    return r;
}

static void ep_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size)
{
    MCPXAPUState *d = opaque;

    qemu_mutex_lock(&d->lock);

    assert(size == 4);
    assert(addr % 4 == 0);

    DPRINTF("mcpx apu EP: [0x%" HWADDR_PRIx "] = 0x%lx\n", addr, val);

    switch (addr) {
    case NV_PAPU_EPXMEM ... NV_PAPU_EPXMEM + 0xC00 * 4 - 1: {
        uint32_t xaddr = (addr - NV_PAPU_EPXMEM) / 4;
        dsp_write_memory(d->ep.dsp, 'X', xaddr, val);
        // fprintf(stderr, "ep write xmem %x = %x\n", xaddr, val);
        break;
    }
    case NV_PAPU_EPYMEM ... NV_PAPU_EPYMEM + 0x100 * 4 - 1: {
        uint32_t yaddr = (addr - NV_PAPU_EPYMEM) / 4;
        dsp_write_memory(d->ep.dsp, 'Y', yaddr, val);
        // fprintf(stderr, "ep write ymem %x = %x\n", yaddr, val);
        break;
    }
    case NV_PAPU_EPPMEM ... NV_PAPU_EPPMEM + 0x1000 * 4 - 1: {
        uint32_t paddr = (addr - NV_PAPU_EPPMEM) / 4;
        // fprintf(stderr, "ep write pmem %x = %x\n", paddr, val);
        dsp_write_memory(d->ep.dsp, 'P', paddr, val);
        break;
    }
    case NV_PAPU_EPRST:
        proc_rst_write(d->ep.dsp, d->ep.regs[NV_PAPU_EPRST], val);
        d->ep.regs[NV_PAPU_EPRST] = val;
        d->ep_frame_div = 0; /* FIXME: Still unsure about frame sync */
        break;
    default:
        d->ep.regs[addr] = val;
        break;
    }

    qemu_mutex_unlock(&d->lock);
}

const MemoryRegionOps ep_ops = {
    .read = ep_read,
    .write = ep_write,
};

void mcpx_apu_dsp_frame(MCPXAPUState *d, float mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME])
{
    /* Write VP results to the GP DSP MIXBUF */
    for (int mixbin = 0; mixbin < NUM_MIXBINS; mixbin++) {
        uint32_t base = GP_DSP_MIXBUF_BASE + mixbin * NUM_SAMPLES_PER_FRAME;
        for (int sample = 0; sample < NUM_SAMPLES_PER_FRAME; sample++) {
            dsp_write_memory(d->gp.dsp, 'X', base + sample,
                             float_to_24b(mixbins[mixbin][sample]));
        }
    }

    bool ep_enabled = (d->ep.regs[NV_PAPU_EPRST] & NV_PAPU_GPRST_GPRST) &&
                      (d->ep.regs[NV_PAPU_EPRST] & NV_PAPU_GPRST_GPDSPRST);

    /* Run GP */
    if ((d->gp.regs[NV_PAPU_GPRST] & NV_PAPU_GPRST_GPRST) &&
        (d->gp.regs[NV_PAPU_GPRST] & NV_PAPU_GPRST_GPDSPRST)) {
        dsp_start_frame(d->gp.dsp);
        d->gp.dsp->core.is_idle = false;
        d->gp.dsp->core.cycle_count = 0;
        do {
            dsp_run(d->gp.dsp, 1000);
        } while (!d->gp.dsp->core.is_idle && d->gp.realtime);
        g_dbg.gp.cycles = d->gp.dsp->core.cycle_count;

        if ((d->monitor.point == MCPX_APU_DEBUG_MON_GP) ||
            (d->monitor.point == MCPX_APU_DEBUG_MON_GP_OR_EP && !ep_enabled)) {
            int off = (d->ep_frame_div % 8) * NUM_SAMPLES_PER_FRAME;
            for (int i = 0; i < NUM_SAMPLES_PER_FRAME; i++) {
                uint32_t l = dsp_read_memory(d->gp.dsp, 'X', 0x1400 + i);
                d->monitor.frame_buf[off + i][0] = l >> 8;
                uint32_t r =
                    dsp_read_memory(d->gp.dsp, 'X', 0x1400 + 1 * 0x20 + i);
                d->monitor.frame_buf[off + i][1] = r >> 8;
            }
        }
    }

    /* Run EP */
    if ((d->ep.regs[NV_PAPU_EPRST] & NV_PAPU_GPRST_GPRST) &&
        (d->ep.regs[NV_PAPU_EPRST] & NV_PAPU_GPRST_GPDSPRST)) {
        if (d->ep_frame_div % 8 == 0) {
            dsp_start_frame(d->ep.dsp);
            d->ep.dsp->core.is_idle = false;
            d->ep.dsp->core.cycle_count = 0;
            do {
                dsp_run(d->ep.dsp, 1000);
            } while (!d->ep.dsp->core.is_idle && d->ep.realtime);
            g_dbg.ep.cycles = d->ep.dsp->core.cycle_count;
        }
    }
}

void mcpx_apu_dsp_init(MCPXAPUState *d)
{
    d->gp.dsp = dsp_init(d, gp_scratch_rw, gp_fifo_rw);
    for (int i = 0; i < DSP_PRAM_SIZE; i++) {
        d->gp.dsp->core.pram[i] = 0xCACACACA;
    }
    memset(d->gp.dsp->core.pram_opcache, 0,
           sizeof(d->gp.dsp->core.pram_opcache));
    d->gp.dsp->is_gp = true;
    d->gp.dsp->core.is_gp = true;
    d->gp.dsp->core.is_idle = false;
    d->gp.dsp->core.cycle_count = 0;

    d->ep.dsp = dsp_init(d, ep_scratch_rw, ep_fifo_rw);
    for (int i = 0; i < DSP_PRAM_SIZE; i++) {
        d->ep.dsp->core.pram[i] = 0xCACACACA;
    }
    memset(d->ep.dsp->core.pram_opcache, 0,
           sizeof(d->ep.dsp->core.pram_opcache));
    for (int i = 0; i < DSP_XRAM_SIZE; i++) {
        d->ep.dsp->core.xram[i] = 0xCACACACA;
    }
    for (int i = 0; i < DSP_YRAM_SIZE; i++) {
        d->ep.dsp->core.yram[i] = 0xCACACACA;
    }
    d->ep.dsp->is_gp = false;
    d->ep.dsp->core.is_gp = false;
    d->ep.dsp->core.is_idle = false;
    d->ep.dsp->core.cycle_count = 0;

    /* Until DSP is more performant, a switch to decide whether or not we should
     * use the full audio pipeline or not.
     */
    mcpx_apu_update_dsp_preference(d);
}
