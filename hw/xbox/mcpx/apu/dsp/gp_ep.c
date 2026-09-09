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
#include "qemu/cutils.h"
#include "qemu/timer.h"

static const int16_t ep_silence[256][2] = { 0 };

void mcpx_apu_update_dsp_preference(MCPXAPUState *d)
{
    static int last_known_dsp_pref = -1;

    if (last_known_dsp_pref != (int)g_config.audio.use_dsp) {
        if (g_config.audio.use_dsp) {
            d->monitor.point = MCPX_APU_DEBUG_MON_GP_OR_EP;
            d->gp.realtime = true;
            d->ep.realtime = true;
        } else {
            d->monitor.point = MCPX_APU_DEBUG_MON_VP;
            d->gp.realtime = false;
            d->ep.realtime = false;
        }
        /* MCPX_APU_MONITOR=ac97|vp|gp|ep|spdif|auto: pin the debug
         * monitor point from the environment - what the UI combo does,
         * for headless captures. `ep` is FIFO #0, the analog output;
         * `spdif` decodes the AC-3 stream on FIFO #1 (spdif.c). */
        const char *mon = getenv("MCPX_APU_MONITOR");
        if (mon) {
            if (!strcmp(mon, "ac97")) {
                d->monitor.point = MCPX_APU_DEBUG_MON_AC97;
            } else if (!strcmp(mon, "vp")) {
                d->monitor.point = MCPX_APU_DEBUG_MON_VP;
            } else if (!strcmp(mon, "gp")) {
                d->monitor.point = MCPX_APU_DEBUG_MON_GP;
            } else if (!strcmp(mon, "ep")) {
                d->monitor.point = MCPX_APU_DEBUG_MON_EP;
            } else if (!strcmp(mon, "spdif")) {
                d->monitor.point = MCPX_APU_DEBUG_MON_EP_SPDIF;
            }
        }
        /* MCPX_APU_MON_CHANNELS=<list>: solo/mute channels of the monitor's
         * layout - comma-separated names (l,r or fl,fr,fc,lfe,bl,br), channel
         * indices, or a hex mask (0x..). Unset = all channels. */
        const char *chs = getenv("MCPX_APU_MON_CHANNELS");
        d->monitor.channel_mask = 0x3F;
        if (chs) {
            static const char *const names[6] = { "fl", "fr", "fc", "lfe",
                                                  "bl", "br" };
            uint32_t mask = 0;
            unsigned long hex;
            if (!strncmp(chs, "0x", 2)) {
                if (qemu_strtoul(chs, NULL, 16, &hex) == 0) {
                    mask = hex;
                }
            } else {
                char buf[64];
                snprintf(buf, sizeof(buf), "%s", chs);
                char *tok = strtok(buf, ",");
                for (; tok; tok = strtok(NULL, ",")) {
                    if (!strcmp(tok, "l")) {
                        mask |= 1;
                    } else if (!strcmp(tok, "r")) {
                        mask |= 2;
                    } else if (*tok >= '0' && *tok <= '5' && !tok[1]) {
                        mask |= 1u << (*tok - '0');
                    } else {
                        for (int c = 0; c < 6; c++) {
                            if (!strcmp(tok, names[c])) {
                                mask |= 1u << c;
                            }
                        }
                    }
                }
            }
            if (mask) {
                d->monitor.channel_mask = mask;
            }
        }
        last_known_dsp_pref = g_config.audio.use_dsp;
    }
}

/* Walk an SGE table for a scratch or FIFO transfer. The table lives in guest
 * RAM at `sge_base`, one 8-byte entry per page, the entry's first word being
 * the physical page. The guest programs the table and its bounds through the
 * APU registers, which take the APU lock for exactly these words so a
 * transfer never sees a table change under it (see mcpx_apu_write); a table
 * that is stale or short is the guest's problem on silicon too, so an entry
 * past `max_sge` or a page outside RAM ends the transfer, not the emulator. */
static void scatter_gather_rw(MCPXAPUState *d, hwaddr sge_base,
                              unsigned int max_sge, uint8_t *ptr, uint32_t addr,
                              size_t len, bool dir)
{
    unsigned int page_entry = addr / TARGET_PAGE_SIZE;
    unsigned int offset_in_page = addr % TARGET_PAGE_SIZE;
    unsigned int bytes_to_copy = TARGET_PAGE_SIZE - offset_in_page;

    if (trace_event_get_state_backends(TRACE_MCPX_APU_DSP_SGE)) {
        uint32_t first_page_phys =
            mcpx_apu_ram_ldl(d, sge_base + page_entry * 8);
        trace_mcpx_apu_dsp_sge(sge_base, dir ? "wr" : "rd", addr,
                               (uint64_t)len,
                               first_page_phys + offset_in_page);
    }

    while (len > 0) {
        if (page_entry > max_sge) {
            return;
        }

        uint32_t prd_address = mcpx_apu_ram_ldl(d, sge_base + page_entry * 8);
        hwaddr paddr = prd_address + offset_in_page;

        if (bytes_to_copy > len) {
            bytes_to_copy = len;
        }
        if (paddr + bytes_to_copy > d->ram_size) {
            return;
        }

        if (dir) {
            memcpy(&d->ram_ptr[paddr], ptr, bytes_to_copy);
            mcpx_apu_ram_set_dirty(d, paddr, bytes_to_copy);
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
    scatter_gather_rw(d, d->regs[NV_PAPU_GPSADDR], d->regs[NV_PAPU_GPSMAXSGE],
                      ptr, addr, len, dir);
}

static bool core_resume_owed_frame(DSPState *dsp);

static void ep_scratch_rw(void *opaque, uint8_t *ptr, uint32_t addr, size_t len,
                          bool dir)
{
    MCPXAPUState *d = opaque;
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

    trace_mcpx_apu_dsp_fifo("GP", dir ? "wr" : "rd", index, base, end, cur,
                            (uint64_t)len);

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

/* EP output FIFO #0 is the analog stereo output - with Dolby Digital on, a
 * surround-encoded downmix, not the GP's front pair - as 256 S16 stereo
 * frames per EP frame. The `ep` monitor point plays it; every point but
 * AC97 sinks it (silence goes to the guest's ring). */
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

    trace_mcpx_apu_dsp_fifo("EP", dir ? "wr" : "rd", index, base, end, cur,
                            (uint64_t)len);

    /* With Dolby Digital enabled, FIFO #1 carries the EP AC3 encoder's
     * IEC 61937 bitstream. MCPX_EP_FIFO1_DUMP=<path> captures it raw;
     * the result plays with `ffplay -f spdif <path>`. */
    if (dir && index == 1) {
        static FILE *fifo1_dump;
        static bool fifo1_dump_checked;
        if (!fifo1_dump_checked) {
            const char *path = getenv("MCPX_EP_FIFO1_DUMP");
            if (path) {
                fifo1_dump = fopen(path, "wb");
                if (!fifo1_dump) {
                    fprintf(stderr, "MCPX_EP_FIFO1_DUMP: cannot open %s\n",
                            path);
                }
            }
            fifo1_dump_checked = true;
        }
        if (fifo1_dump) {
            fwrite(ptr, 1, len, fifo1_dump);
            fflush(fifo1_dump);
        }
        if (d->monitor.point == MCPX_APU_DEBUG_MON_EP_SPDIF) {
            mcpx_apu_spdif_feed(d, ptr, len);
        }
    }

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
        break;
    }
    case NV_PAPU_GPMIXBUF ... NV_PAPU_GPMIXBUF + 0x400 * 4 - 1: {
        uint32_t xaddr = (addr - NV_PAPU_GPMIXBUF) / 4;
        r = dsp_read_memory(d->gp.dsp, 'X', GP_DSP_MIXBUF_BASE + xaddr);
        break;
    }
    case NV_PAPU_GPYMEM ... NV_PAPU_GPYMEM + 0x800 * 4 - 1: {
        uint32_t yaddr = (addr - NV_PAPU_GPYMEM) / 4;
        r = dsp_read_memory(d->gp.dsp, 'Y', yaddr);
        break;
    }
    case NV_PAPU_GPPMEM ... NV_PAPU_GPPMEM + 0x1000 * 4 - 1: {
        uint32_t paddr = (addr - NV_PAPU_GPPMEM) / 4;
        r = dsp_read_memory(d->gp.dsp, 'P', paddr);
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

    mcpx_apu_guest_lock(d);

    assert(size == 4);
    assert(addr % 4 == 0);

    DPRINTF("mcpx apu GP: [0x%" HWADDR_PRIx "] = 0x%lx\n", addr, val);

    switch (addr) {
    case NV_PAPU_GPXMEM ... NV_PAPU_GPXMEM + 0x1000 * 4 - 1: {
        uint32_t xaddr = (addr - NV_PAPU_GPXMEM) / 4;
        dsp_write_memory(d->gp.dsp, 'X', xaddr, val);
        break;
    }
    case NV_PAPU_GPMIXBUF ... NV_PAPU_GPMIXBUF + 0x400 * 4 - 1: {
        uint32_t xaddr = (addr - NV_PAPU_GPMIXBUF) / 4;
        dsp_write_memory(d->gp.dsp, 'X', GP_DSP_MIXBUF_BASE + xaddr, val);
        break;
    }
    case NV_PAPU_GPYMEM ... NV_PAPU_GPYMEM + 0x800 * 4 - 1: {
        uint32_t yaddr = (addr - NV_PAPU_GPYMEM) / 4;
        dsp_write_memory(d->gp.dsp, 'Y', yaddr, val);
        break;
    }
    case NV_PAPU_GPPMEM ... NV_PAPU_GPPMEM + 0x1000 * 4 - 1: {
        uint32_t paddr = (addr - NV_PAPU_GPPMEM) / 4;
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

    mcpx_apu_guest_unlock(d);
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
        break;
    }
    case NV_PAPU_EPYMEM ... NV_PAPU_EPYMEM + 0x100 * 4 - 1: {
        uint32_t yaddr = (addr - NV_PAPU_EPYMEM) / 4;
        r = dsp_read_memory(d->ep.dsp, 'Y', yaddr);
        break;
    }
    case NV_PAPU_EPPMEM ... NV_PAPU_EPPMEM + 0x1000 * 4 - 1: {
        uint32_t paddr = (addr - NV_PAPU_EPPMEM) / 4;
        r = dsp_read_memory(d->ep.dsp, 'P', paddr);
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

    mcpx_apu_guest_lock(d);

    assert(size == 4);
    assert(addr % 4 == 0);

    DPRINTF("mcpx apu EP: [0x%" HWADDR_PRIx "] = 0x%lx\n", addr, val);

    switch (addr) {
    case NV_PAPU_EPXMEM ... NV_PAPU_EPXMEM + 0xC00 * 4 - 1: {
        uint32_t xaddr = (addr - NV_PAPU_EPXMEM) / 4;
        dsp_write_memory(d->ep.dsp, 'X', xaddr, val);
        break;
    }
    case NV_PAPU_EPYMEM ... NV_PAPU_EPYMEM + 0x100 * 4 - 1: {
        uint32_t yaddr = (addr - NV_PAPU_EPYMEM) / 4;
        dsp_write_memory(d->ep.dsp, 'Y', yaddr, val);
        break;
    }
    case NV_PAPU_EPPMEM ... NV_PAPU_EPPMEM + 0x1000 * 4 - 1: {
        uint32_t paddr = (addr - NV_PAPU_EPPMEM) / 4;
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

    mcpx_apu_guest_unlock(d);
}

const MemoryRegionOps ep_ops = {
    .read = ep_read,
    .write = ep_write,
};

/* Cycles handed to a core per dsp_run call: how often the run loop is
 * re-entered, and so how promptly a worker notices a waiting guest accessor
 * or the end of its frame. Per-call overhead is not measurable against the
 * generated code at this size. */
#define DSP_SLICE_CYCLES 1000

/* One worker thread per DSP core.
 *
 * On silicon the GP and EP are concurrent cores that hand data to each
 * other through scratch buffers in guest RAM; running them in turn makes a
 * frame's DSP cost the sum of the two and lets a spin-wait be satisfied
 * only at slice boundaries. The frame thread remains the barrier: it fills
 * the GP mixbuf, releases both workers, and waits for both before reading
 * any result, so nothing touches a core's state while its worker runs. The
 * cores are otherwise disjoint (DSPState, register windows, SGE tables);
 * where they meet, the same guest RAM through different SGE tables, the
 * programs' own handshake orders it: the GP writes one slice per frame
 * into the half of its output ring the EP is not reading, and the EP loads
 * the completed half at its kick (see mcpx_apu_dsp_frame_begin for how a
 * late GP frame is finished before that load).
 *
 * There is one APU, so these are file-static. */
typedef struct DSPWorker {
    MCPXAPUState *d;
    bool is_gp;
    const char *name;
    QemuThread thread;
    QemuMutex lock;
    QemuCond start_cond, done_cond;
    bool created;
    bool start_requested;
    bool busy;
    bool exiting;
    /* Frame thread -> worker */
    uint32_t budget;
    /* Worker -> frame thread, published before done_cond is signalled */
    bool wall_bound;
} DSPWorker;

static DSPWorker g_gp_worker = { .is_gp = true, .name = "mcpx.apu_gp" };
static DSPWorker g_ep_worker = { .is_gp = false, .name = "mcpx.apu_ep" };

/* Run one core until it signals frame-complete, exhausts its cycle budget, or
 * has held the thread long enough that the frame should end. */
static void run_one_core(DSPWorker *w)
{
    MCPXAPUState *d = w->d;
    DSPState *dsp = w->is_gp ? d->gp.dsp : d->ep.dsp;
    const int64_t slice_us = 2000;
    int64_t t0 = qemu_clock_get_us(QEMU_CLOCK_REALTIME);

    w->wall_bound = false;
    while (dsp_get_cycle_count(dsp) < w->budget) {
        if (dsp_get_halt_requested(dsp) &&
            !core_resume_owed_frame(dsp)) {
            break;
        }
        dsp_run(dsp, DSP_SLICE_CYCLES);
        if (qemu_clock_get_us(QEMU_CLOCK_REALTIME) - t0 >= slice_us) {
            w->wall_bound = true;
            break;
        }
        /* A guest-side accessor is blocked on the APU lock the frame thread
         * is holding while it waits for us. Stop between slices so it can be
         * handed over; the core keeps its state and resumes next frame. */
        if (qatomic_read(&d->lock_waiters) > 0) {
            break;
        }
    }
}

static void *dsp_worker_thread(void *opaque)
{
    DSPWorker *w = opaque;

    /* The scratch DMA resolves SGE entries outside RAM through the address
     * space, which reads the flat view under the RCU read lock. A thread
     * that is not in the registry is not waited for by synchronize_rcu(),
     * so that view can be freed while this thread is walking it. */
    rcu_register_thread();
    qemu_mutex_lock(&w->lock);
    for (;;) {
        while (!w->start_requested && !w->exiting) {
            qemu_cond_wait(&w->start_cond, &w->lock);
        }
        if (w->exiting) {
            break;
        }
        w->start_requested = false;
        qemu_mutex_unlock(&w->lock);

        run_one_core(w);

        qemu_mutex_lock(&w->lock);
        w->busy = false;
        qemu_cond_signal(&w->done_cond);
    }
    qemu_mutex_unlock(&w->lock);
    rcu_unregister_thread();
    return NULL;
}

static void dsp_worker_init(DSPWorker *w, MCPXAPUState *d)
{
    if (w->created) {
        return;
    }
    w->d = d;
    qemu_mutex_init(&w->lock);
    qemu_cond_init(&w->start_cond);
    qemu_cond_init(&w->done_cond);
    w->created = true;
    qemu_thread_create(&w->thread, w->name, dsp_worker_thread, w,
                       QEMU_THREAD_JOINABLE);
}

static void dsp_worker_start(DSPWorker *w, uint32_t budget)
{
    qemu_mutex_lock(&w->lock);
    w->budget = budget;
    w->busy = true;
    w->start_requested = true;
    qemu_cond_signal(&w->start_cond);
    qemu_mutex_unlock(&w->lock);
}

static void dsp_worker_wait(DSPWorker *w)
{
    qemu_mutex_lock(&w->lock);
    while (w->busy) {
        qemu_cond_wait(&w->done_cond, &w->lock);
    }
    qemu_mutex_unlock(&w->lock);
}

void mcpx_apu_dsp_stop_workers(void)
{
    DSPWorker *ws[] = { &g_gp_worker, &g_ep_worker };
    for (unsigned i = 0; i < ARRAY_SIZE(ws); i++) {
        DSPWorker *w = ws[i];
        if (!w->created) {
            continue;
        }
        qemu_mutex_lock(&w->lock);
        while (w->busy) {
            qemu_cond_wait(&w->done_cond, &w->lock);
        }
        w->exiting = true;
        qemu_cond_signal(&w->start_cond);
        qemu_mutex_unlock(&w->lock);
        qemu_thread_join(&w->thread);
        w->created = false;
    }
}

static void run_dsps(MCPXAPUState *d, bool gp_active, bool ep_active,
                     uint32_t gp_budget, uint32_t ep_budget)
{
    dsp_worker_init(&g_gp_worker, d);
    dsp_worker_init(&g_ep_worker, d);

    if (gp_active) {
        dsp_worker_start(&g_gp_worker, gp_budget);
    }
    if (ep_active) {
        dsp_worker_start(&g_ep_worker, ep_budget);
    }
}

/* Carried from begin to end. One APU, so file-static like the workers. */
static struct {
    bool gp_enabled, ep_enabled, ran;
    uint32_t gp_budget, ep_budget;
    /* The GP did not reach frame-complete by the end of the previous frame:
     * its output slice for that frame is still unwritten. */
    bool gp_late;
} g_frame;

/* A core that halted while its next frame start is already pending owes
 * that frame: let it continue within its budget instead of parking until
 * the next frame, which would leave it a frame behind for good. */
static bool core_resume_owed_frame(DSPState *dsp)
{
    if (dsp_get_halt_requested(dsp) && dsp_frame_start_pending(dsp)) {
        dsp_set_halt_requested(dsp, false);
        return true;
    }
    return false;
}

/* Join whatever run_dsps released. Separate from the start so the frame
 * thread has somewhere useful to be in between - see
 * mcpx_apu_dsp_frame_begin.
 *
 * The workers stop between slices when a guest accessor is waiting on the
 * APU lock this thread holds. Ending the frame there would leave the
 * stopped core's output unproduced and frame_end would consume the missing
 * samples as silence, so after the join: while a core is unfinished for
 * any reason but wall time, hand the lock over with both workers idle,
 * then re-release the unfinished cores onto the remainder of their frame. */
static void join_dsps(MCPXAPUState *d, bool gp_active, bool ep_active)
{
    if (gp_active) {
        dsp_worker_wait(&g_gp_worker);
    }
    if (ep_active) {
        dsp_worker_wait(&g_ep_worker);
    }

    for (;;) {
        bool gp_more = gp_active && !g_gp_worker.wall_bound &&
                       !dsp_get_halt_requested(d->gp.dsp) &&
                       dsp_get_cycle_count(d->gp.dsp) < g_frame.gp_budget;
        bool ep_more = ep_active && !g_ep_worker.wall_bound &&
                       !dsp_get_halt_requested(d->ep.dsp) &&
                       dsp_get_cycle_count(d->ep.dsp) < g_frame.ep_budget;
        if ((!gp_more && !ep_more) || d->pause_requested) {
            break;
        }
        while (!d->pause_requested && qatomic_read(&d->lock_waiters) > 0) {
            qemu_cond_timedwait(&d->cond, &d->lock, 1);
        }
        if (d->pause_requested) {
            break;
        }
        if (gp_more) {
            dsp_worker_start(&g_gp_worker, g_frame.gp_budget);
        }
        if (ep_more) {
            dsp_worker_start(&g_ep_worker, g_frame.ep_budget);
        }
        if (gp_more) {
            dsp_worker_wait(&g_gp_worker);
        }
        if (ep_more) {
            dsp_worker_wait(&g_ep_worker);
        }
    }
}

/* After the join: what the frame left behind for the next one. */
static void dsp_frame_account(MCPXAPUState *d, bool gp_active)
{
    g_frame.gp_late = gp_active && !dsp_get_halt_requested(d->gp.dsp);
}

void mcpx_apu_dsp_frame_begin(MCPXAPUState *d)
{
    bool ep_enabled = (d->ep.regs[NV_PAPU_EPRST] & NV_PAPU_GPRST_GPRST) &&
                      (d->ep.regs[NV_PAPU_EPRST] & NV_PAPU_GPRST_GPDSPRST);

    bool gp_enabled = (d->gp.regs[NV_PAPU_GPRST] & NV_PAPU_GPRST_GPRST) &&
                      (d->gp.regs[NV_PAPU_GPRST] & NV_PAPU_GPRST_GPDSPRST);

    g_frame.gp_enabled = gp_enabled;
    g_frame.ep_enabled = ep_enabled;
    g_frame.ran = gp_enabled || ep_enabled;

    if (gp_enabled) {
        dsp_start_frame(d->gp.dsp);
        dsp_set_halt_requested(d->gp.dsp, false);
        dsp_set_cycle_count(d->gp.dsp, 0);
    }
    if (ep_enabled) {
        /* The kick (start-frame interrupt + run-to-idle) fires every 8th
         * frame as on hardware; between kicks an EP that has not idled keeps
         * executing its per-frame slice, and an idled EP stays parked. The
         * first kick is in the release frame itself: the EP's first kick is
         * the encoder's initialisation pass and its second, eight frames on,
         * is the first to read the GP's output ring, by which time the GP
         * has filled exactly one 8-slice half. */
        if (d->ep_frame_div % 8 == 0) {
            dsp_start_frame(d->ep.dsp);
            dsp_set_halt_requested(d->ep.dsp, false);
        }
        dsp_set_cycle_count(d->ep.dsp, 0);
    }

    if (gp_enabled || ep_enabled) {
        /* Hardware budget: 150 MHz for one 32-sample frame at 48 kHz. The EP
         * gets the same per-frame slice every SE frame (its 8-frame kick
         * budget spread evenly) rather than one 8x slice on kick frames,
         * which is the hardware's own shape: the encoder's kick takes ~4.4
         * frames of continuous execution at 150 MHz and then idles. */
        const uint32_t hw_budget = 100000;
        /* MCPX_DSP_BUDGET=<gp>[,<ep>]: per-frame cycle caps, for tests that
         * want a core short of its per-frame need (a GP that cannot finish
         * a frame is how the late-frame path below is provoked). */
        static uint32_t gp_cap, ep_cap;
        static int budget_env = -1;
        if (budget_env < 0) {
            const char *v = getenv("MCPX_DSP_BUDGET");
            budget_env = v ? 1 : 0;
            gp_cap = ep_cap = hw_budget;
            if (v) {
                const char *end;
                unsigned long cap;
                if (qemu_strtoul(v, &end, 0, &cap) == 0) {
                    gp_cap = ep_cap = cap;
                    if (*end == ',' &&
                        qemu_strtoul(end + 1, NULL, 0, &cap) == 0) {
                        ep_cap = cap;
                    }
                }
            }
        }
        g_frame.gp_budget = d->gp.realtime ? gp_cap : 1000;
        g_frame.ep_budget = d->ep.realtime ? ep_cap : 1000;

        /* The EP runs from here, overlapped with the VP; the GP waits for
         * the VP (mcpx_apu_dsp_frame_gp). */
        run_dsps(d, false, ep_enabled, g_frame.gp_budget, g_frame.ep_budget);
    }
}

/* The VP has filled this frame's mixbins: hand them to the GP and release
 * it. The GP runs after the VP, not overlapped with it, because the two
 * meet in guest RAM inside a frame: the GP's DMA writes its effect returns
 * to scratch pages that VP voices read as sample data in the same frame,
 * and a VP reading a page the GP is rewriting mixes two frames' worth of
 * it - a discontinuity at the frame boundary, heard as a pop. The EP shares
 * no such page with the VP and runs from frame begin. */
void mcpx_apu_dsp_frame_gp(MCPXAPUState *d,
                           float mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME])
{
    if (!g_frame.gp_enabled) {
        return;
    }

    /* A GP that did not reach frame-complete last frame is parked part way
     * through it, still reading the mixbuf that frame was given. Writing
     * this frame's over it would finish the late frame on a mix of two
     * frames' input, and the owed frame that follows (core_resume_owed_
     * frame) would consume this frame's mixbuf a second time: a slice of
     * garbage and a repeated slice in the output ring. On a kick frame the
     * EP, loading the GP's last eight slices from their shared ring almost
     * at once, would also read the unwritten one - silicon has every slice
     * written by the frame boundary. Finish the late frame here first, on
     * its own mixbuf, bounded by the GP's budget. */
    if (g_frame.gp_late) {
        int64_t c0 = qemu_clock_get_us(QEMU_CLOCK_REALTIME);
        while (!dsp_get_halt_requested(d->gp.dsp) &&
               dsp_get_cycle_count(d->gp.dsp) < g_frame.gp_budget &&
               qemu_clock_get_us(QEMU_CLOCK_REALTIME) - c0 < 2000) {
            dsp_run(d->gp.dsp, DSP_SLICE_CYCLES);
        }
    }

    /* MCPX_APU_MIX_INJECT=1: overwrite the six mixbins with independent
     * per-channel sinusoids before the GP consumes them, so the encoder's
     * per-channel fidelity can be read from the decoded stream without the
     * content's own inter-channel correlation confounding it. Bin b gets a
     * distinct frequency at a fixed level; a bin the encoder handles well
     * reconstructs its own tone, a mishandled one leaks or decorrelates.
     * 2 = only bin 5, 3 = all bins with the tone order reversed. */
    static int inject = -1;
    if (inject < 0) {
        const char *v = getenv("MCPX_APU_MIX_INJECT");
        inject = v ? atoi(v) : 0;
    }
    if (inject) {
        static uint64_t phase;
        const double freq[NUM_MIXBINS] = { 300, 700, 1100, 1900, 3100, 5300 };
        for (int mixbin = 0; mixbin < NUM_MIXBINS; mixbin++) {
            for (int sample = 0; sample < NUM_SAMPLES_PER_FRAME; sample++) {
                double t = (double)(phase + sample) / 48000.0;
                double f =
                    freq[(inject == 3 && mixbin < 6) ? 5 - mixbin : mixbin];
                float v = (inject == 2 && mixbin != 5) ? 0.0f :
                    0.25f * sinf((float)(2.0 * M_PI * f * t));
                mixbins[mixbin][sample] = v;
            }
        }
        phase += NUM_SAMPLES_PER_FRAME;
    }

    /* Write VP results to the GP DSP MIXBUF */
    for (int mixbin = 0; mixbin < NUM_MIXBINS; mixbin++) {
        uint32_t base = GP_DSP_MIXBUF_BASE + mixbin * NUM_SAMPLES_PER_FRAME;
        for (int sample = 0; sample < NUM_SAMPLES_PER_FRAME; sample++) {
            dsp_write_memory(d->gp.dsp, 'X', base + sample,
                             float_to_24b(mixbins[mixbin][sample]));
        }
    }

    dsp_worker_start(&g_gp_worker, g_frame.gp_budget);
}

/* Join the cores and read their results. Everything here touches DSP state,
 * so it must not run while a worker does. */
void mcpx_apu_dsp_frame_end(MCPXAPUState *d)
{
    bool gp_enabled = g_frame.gp_enabled;
    bool ep_enabled = g_frame.ep_enabled;

    if (g_frame.ran) {
        join_dsps(d, gp_enabled, ep_enabled);
        dsp_frame_account(d, gp_enabled);
    }

    if (gp_enabled) {
        g_dbg.gp.cycles = dsp_get_cycle_count(d->gp.dsp);

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
    if (ep_enabled) {
        g_dbg.ep.cycles = dsp_get_cycle_count(d->ep.dsp);
    }
}

void mcpx_apu_dsp_init(MCPXAPUState *d)
{
    d->gp.dsp = dsp_init(d, gp_scratch_rw, gp_fifo_rw, true);
    dsp_set_halt_requested(d->gp.dsp, false);
    dsp_set_cycle_count(d->gp.dsp, 0);

    d->ep.dsp = dsp_init(d, ep_scratch_rw, ep_fifo_rw, false);
    dsp_set_halt_requested(d->ep.dsp, false);
    dsp_set_cycle_count(d->ep.dsp, 0);

    /* Until DSP is more performant, a switch to decide whether or not we should
     * use the full audio pipeline or not.
     */
    mcpx_apu_update_dsp_preference(d);
}
