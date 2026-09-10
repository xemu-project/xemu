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
#include "qemu/error-report.h"
#include "qemu/timer.h"

static const int16_t ep_silence[256][2] = { 0 };

void mcpx_apu_update_dsp_preference(MCPXAPUState *d)
{
    static int last_known_dsp_pref = -1;

    if (last_known_dsp_pref != (int)g_config.audio.use_dsp) {
        if (g_config.audio.use_dsp) {
            d->monitor.point = MCPX_APU_DEBUG_MON_EP;
            d->gp.realtime = true;
            d->ep.realtime = true;
        } else {
            d->monitor.point = MCPX_APU_DEBUG_MON_VP;
            d->gp.realtime = false;
            d->ep.realtime = false;
        }
        /* MCPX_APU_MONITOR=ac97|vp|gp|ep|spdif: pin the debug
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

static void ep_snapshot_node_done(MCPXAPUState *d, bool fifo1);
static bool core_resume_owed_frame(DSPState *dsp, uint64_t *counter);

static void ep_scratch_rw(void *opaque, uint8_t *ptr, uint32_t addr, size_t len,
                          bool dir)
{
    MCPXAPUState *d = opaque;
    scatter_gather_rw(d, d->regs[NV_PAPU_EPSADDR], d->regs[NV_PAPU_EPSMAXSGE],
                      ptr, addr, len, dir);
    ep_snapshot_node_done(d, false);
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

/* The mcpx_apu_dsp_sched trace event: once a second, what the DSP
 * scheduler got through - SE frames, EP kicks, cycles retired per core,
 * wall time in DSP code, and which bound ended each frame. Realtime needs
 * 1500 SE frames/s; the exit-reason split says whether a shortfall is the
 * wall bound, the cycle budget, or the cores not being kicked often enough.
 * Its per-slice timing and the EP stall sampling run only while the event
 * is enabled; the companion events (mcpx_apu_dsp_sched_*) fire from the
 * same report. */

/* Where the EP is when a frame ends with it still running. A core that is
 * working spreads over its code; one that is polling parks on a handful of
 * PCs. */
#define EP_STALL_PCS 512
static struct { uint32_t pc; uint64_t n; } g_ep_stall[EP_STALL_PCS];
static uint64_t g_ep_stall_samples, g_ep_stall_dropped;

/* Where in its kick the EP reaches the frame-complete flag. The EP is kicked
 * every 8th SE frame and given its 800k-cycle hardware budget as 8 slices of
 * 100k; this says whether it stops because it signalled done, or because the
 * budget ran out. */
static struct {
    uint64_t cycles, frames;      /* accumulated over the current kick */
    bool halted;                  /* has it signalled done this kick? */
    uint64_t n, cyc_sum, frm_sum, cyc_min, cyc_max, never;
} g_ep_kick;

/* MCPX_EP_SNAPSHOT=<path>[:<div>[:<node>]]: write the EP's whole working
 * state - P, X and Y memory, the register file, the hardware stack and the
 * PC - once, in the LOD dialect that dsp56300's `difftest run-to` reads
 * (`P|X|Y addr word`, `R idx word`, `SSH|SSL slot word`, `PC word`).
 *
 * With <div> alone (default 900, well into steady state) the state is taken
 * at the end of that frame. With <node> it is taken at a DMA boundary
 * instead: nodes are counted from the first FIFO #1 write at or after <div>
 * (the kick boundary - the output piece is a kick's last node), and once
 * node <node> has completed and the program clears its EOL ($FFFFC5 = $80)
 * the core is asked to leave its block, so the frame end that follows
 * writes the state with an exact PC. From there to the next DMA start the
 * program touches no peripheral, so the segment runs identically on any
 * engine, which is how a kick is bisected against silicon: xbtest's
 * run_ep_phase_hw.py runs the same snapshot on the console. */
static struct {
    int armed;          /* -1 unparsed, 0 off, 1 waiting, 2 pending, 3 done */
    unsigned at_div;
    int node;           /* -1: at the end of frame at_div; else a DMA node */
    int counting;       /* nodes since the kick boundary, -1 = not yet */
    char path[512];
} g_ep_snapshot = { .armed = -1, .at_div = 900, .node = -1, .counting = -1 };
static bool g_ep_snapshot_eol_watch;

static void ep_snapshot_init(void)
{
    if (g_ep_snapshot.armed >= 0) {
        return;
    }
    g_ep_snapshot.armed = 0;
    const char *v = getenv("MCPX_EP_SNAPSHOT");
    if (v) {
        char path[512];
        unsigned at = 900;
        int node = -1;
        if (sscanf(v, "%511[^:]:%u:%d", path, &at, &node) >= 1) {
            snprintf(g_ep_snapshot.path, sizeof(g_ep_snapshot.path), "%s",
                     path);
            g_ep_snapshot.at_div = at;
            g_ep_snapshot.node = node;
            g_ep_snapshot.armed = 1;
        }
    }
}

/* Every EP DMA node ends in exactly one scratch or FIFO callback; this is
 * that callback's tail. `fifo1` marks the output piece (FIFO #1 write). */
static void ep_snapshot_node_done(MCPXAPUState *d, bool fifo1)
{
    ep_snapshot_init();
    if (g_ep_snapshot.armed != 1 || g_ep_snapshot.node < 0) {
        return;
    }
    if (g_ep_snapshot.counting < 0) {
        if (fifo1 && d->ep_frame_div >= g_ep_snapshot.at_div) {
            g_ep_snapshot.counting = 0;
        }
        return;
    }
    if (g_ep_snapshot.counting == g_ep_snapshot.node) {
        g_ep_snapshot_eol_watch = true;
    }
    g_ep_snapshot.counting++;
}

/* Called from the EP's peripheral write path on $FFFFC5 = $80. */
void mcpx_apu_ep_snapshot_on_eol_clear(DSPState *dsp)
{
    if (g_ep_snapshot_eol_watch && g_ep_snapshot.armed == 1) {
        g_ep_snapshot_eol_watch = false;
        g_ep_snapshot.armed = 2;
        dsp_set_halt_requested(dsp, true);
    }
}

/* At a frame end, after the cores are joined. */
static void ep_snapshot(MCPXAPUState *d)
{
    ep_snapshot_init();
    if (g_ep_snapshot.armed == 1 && g_ep_snapshot.node < 0 &&
        d->ep_frame_div == g_ep_snapshot.at_div) {
        g_ep_snapshot.armed = 2;
    }
    if (g_ep_snapshot.armed != 2) {
        return;
    }
    g_ep_snapshot.armed = 3; /* once */

    FILE *f = fopen(g_ep_snapshot.path, "w");
    if (!f) {
        warn_report("MCPX_EP_SNAPSHOT: cannot write %s", g_ep_snapshot.path);
        return;
    }
    uint32_t pc, sp, ssh[16], regs[64];
    dsp_get_pc_sp(d->ep.dsp, &pc, &sp, ssh);
    dsp_get_registers(d->ep.dsp, regs);
    dsp_sync_to_vm(d->ep.dsp);
    fprintf(f, "; MCPX EP snapshot at ep_frame_div=%u\n", d->ep_frame_div);
    fprintf(f, "PC %06X\n", pc);
    for (int i = 0; i < 16; i++) {
        fprintf(f, "SSH %02X %06X\n", i, d->ep.dsp->core.stack[0][i]);
        fprintf(f, "SSL %02X %06X\n", i, d->ep.dsp->core.stack[1][i]);
    }
    for (int i = 0; i < 64; i++) {
        fprintf(f, "R %02X %06X\n", i, regs[i]);
    }
    for (uint32_t a = 0; a < 0x1000; a++) {
        fprintf(f, "P %04X %06X\n", a, dsp_read_memory(d->ep.dsp, 'P', a));
    }
    for (uint32_t a = 0; a < 0x1000; a++) {
        fprintf(f, "X %04X %06X\n", a, dsp_read_memory(d->ep.dsp, 'X', a));
    }
    /* Y including the on-chip data ROM at $0800, so a replay's table reads
     * resolve. */
    for (uint32_t a = 0; a < 0x1000; a++) {
        fprintf(f, "Y %04X %06X\n", a, dsp_read_memory(d->ep.dsp, 'Y', a));
    }
    fclose(f);
    trace_mcpx_apu_ep_snapshot(g_ep_snapshot.path, d->ep_frame_div, pc, sp);
}

static bool sched_stats_enabled(void);

static void ep_stall_note(uint32_t pc)
{
    g_ep_stall_samples++;
    for (int i = 0; i < EP_STALL_PCS; i++) {
        if (g_ep_stall[i].n == 0 || g_ep_stall[i].pc == pc) {
            g_ep_stall[i].pc = pc;
            g_ep_stall[i].n++;
            return;
        }
    }
    /* Table full: a core spread over its code rather than parked on a few
     * PCs. Counted so the distribution is not silently truncated. */
    g_ep_stall_dropped++;
}

/* MCPX_DSP_BLOCK_PROFILE=<prefix>[:<at>[:<span>]]: the JIT's per-start-PC
 * block-entry histogram for both cores, written at the <at>-th and the
 * (<at>+<span>)-th one-second report. Two dumps because the library's
 * counters are cumulative and never reset - the steady-state distribution is
 * the difference between them.
 *
 * It says which start PCs the dispatches are at, and whether a core is
 * waiting rather than working: a spin on a peer's flag is a one- or two-word
 * block with an enormous dispatch count and a couple of cycles each. */
static void dsp_block_profile_report(MCPXAPUState *d)
{
    static const char *prefix;
    static unsigned at = 20, span = 30, reports;
    static char pbuf[480];
    static bool init;
    char path[512];

    if (!init) {
        const char *v = getenv("MCPX_DSP_BLOCK_PROFILE");
        init = true;
        if (v) {
            const char *colon = strchr(v, ':');
            unsigned a, s2;
            if (colon) {
                snprintf(pbuf, sizeof(pbuf), "%.*s", (int)(colon - v), v);
                if (sscanf(colon + 1, "%u:%u", &a, &s2) == 2) {
                    at = a;
                    span = s2;
                } else if (sscanf(colon + 1, "%u", &a) == 1) {
                    at = a;
                }
            } else {
                snprintf(pbuf, sizeof(pbuf), "%s", v);
            }
            prefix = pbuf;
        }
    }
    if (!prefix) {
        return;
    }
    reports++;
    for (int i = 0; i < 2; i++) {
        DSPState *dsp = i ? d->ep.dsp : d->gp.dsp;
        const char *core = i ? "ep" : "gp";

        if (reports == 1) {
            /* Turns profiling on in the library; the file itself is empty. */
            snprintf(path, sizeof(path), "%s-%s-blocks-on.txt", prefix, core);
            dsp_jit_dump_block_profile(dsp, path);
        } else if (reports == at || reports == at + span) {
            snprintf(path, sizeof(path), "%s-%s-blocks-%u.txt", prefix, core,
                     reports);
            dsp_jit_dump_block_profile(dsp, path);
            trace_mcpx_apu_dsp_block_profile(path);
        }
    }
}

/* Diagnostic counters. The fields the core-run path touches - gp_/ep_ timings
 * and waiter_breaks - are incremented atomically because the two worker
 * threads run that code at once. The rest are frame-thread only, and the
 * report reads everything after both workers have been joined. */
static struct {
    uint64_t frames, kicks, run_us, gp_cycles, ep_cycles;
    uint64_t gp_calls, gp_us, ep_calls, ep_us;
    uint64_t ep_cpu_ns;
    uint64_t exit_idle, exit_budget, exit_wall;
    uint64_t gp_unhalted, ep_unhalted, gp_overbudget, ep_overbudget;
    uint64_t waiter_breaks;
    /* Frames a core resumed after halting with a start pending, and
     * frames on which the frame thread finished a late GP frame before
     * handing it the next mixbuf (mcpx_apu_dsp_frame_gp). */
    uint64_t gp_catchup, ep_catchup, gp_late_finish;
    /* Cold-compile burst shape within the window: the worst single frame's
     * translation total and count, and how many frames compiled at all.
     * A steady-state window is all zeros; a scene change is a handful of
     * frames carrying hundreds of microseconds each. */
    uint64_t jit_frame_ns_worst, jit_frame_compiles_worst, jit_compile_frames;
    uint64_t mixbuf_us;
    uint64_t fifo1_writes, fifo1_bytes;
    int64_t last_report_us;
} g_sched;

static bool sched_stats_enabled(void)
{
    return trace_event_get_state_backends(TRACE_MCPX_APU_DSP_SCHED);
}

static void sched_stats_report(MCPXAPUState *d)
{
    if (!sched_stats_enabled()) {
        return;
    }
    int64_t now = qemu_clock_get_us(QEMU_CLOCK_REALTIME);
    if (!g_sched.last_report_us) {
        g_sched.last_report_us = now;
        return;
    }
    static uint64_t dma_p_prev, gp_halts_prev, ep_halts_prev, guest_locks_prev;
    static uint64_t gpc_prev, gpn_prev, gpi_prev, epc_prev, epn_prev, epi_prev;
    static uint64_t spdif_frames_prev, spdif_bad_prev, spdif_under_prev,
        spdif_over_prev;
    uint64_t spdif_frames, spdif_bad, spdif_under, spdif_over;
    unsigned spdif_lvl_min, spdif_lvl_max;
    mcpx_apu_spdif_stats(&spdif_frames, &spdif_bad, &spdif_under,
                         &spdif_over, &spdif_lvl_min, &spdif_lvl_max, false);
    uint64_t gpc = 0, gpn = 0, gpi = 0, epc = 0, epn = 0, epi = 0;
    uint64_t gph = 0, eph = 0, gpt = 0, ept = 0, gpb = 0, epb = 0;
    uint64_t gpz = 0, epz = 0;
    uint64_t gpw = 0, epw = 0;
    static uint64_t gpb_prev, epb_prev;
    static uint64_t eph_prev;
    int64_t span = now - g_sched.last_report_us;
    if (span < 1000000) {
        return;
    }
    dsp_jit_get_stats(d->gp.dsp, &gpc, &gpn, &gpw, &gpi, &gph, &gpt, &gpb,
                      &gpz);
    dsp_jit_get_stats(d->ep.dsp, &epc, &epn, &epw, &epi, &eph, &ept, &epb,
                      &epz);
    char report[2048];
    snprintf(report, sizeof(report),
            "%.0f frames/s (%.1f%% of 1500) kicks/s=%.0f "
            "dsp=%.1f%% of wall | gp=%.0f Mcyc/s ep=%.0f Mcyc/s | "
            "exit idle=%llu budget=%llu wall=%llu | "
            "unhalted gp=%llu ep=%llu, over-budget gp=%llu ep=%llu | "
            "halts/s gp=%.0f ep=%.0f | "
            "guest locks/s=%.0f, frames cut for a waiter=%llu | "
            "mixbuf %.1f%% of wall (%.1f us/frame) | "
            "kick: halts at %.0fk cyc (min %.0fk max %.0fk of 800k) "
            "after %.1f of 8 frames, %llu never | "
            "gp late-finish %llu catchup %llu dropped %u, "
            "ep catchup %llu dropped %u | "
            "fifo1 %llu wr %llu B, spdif %llu frames %llu bad "
            "%llu under %llu over lvl %u..%u | "
            "gp %llu calls %.0f cyc/call %.0f ns/cyc | "
            "ep %llu calls %.0f cyc/call %.0f ns/cyc | dma P wr/s=%.0f | "
            "gp jit %.0f comp/s %.1f%% wall (%.0f us/comp, %.0f inval/s) | "
            "ep jit %.0f comp/s %.1f%% wall (%.0f us/comp, %.0f inval/s, "
            "%.0f hits/s, %llu retained) | "
            "blocks/s gp=%.0fk ep=%.0fk, cyc/block gp=%.1f ep=%.1f, "
            "ns/block gp=%.1f ep=%.1f | code gp=%.0f KiB ep=%.0f KiB",
            g_sched.frames * 1e6 / span, g_sched.frames * 1e6 / span / 15.0,
            g_sched.kicks * 1e6 / span,
            g_sched.run_us * 100.0 / span,
            g_sched.gp_cycles / (double)span,
            g_sched.ep_cycles / (double)span,
            (unsigned long long)g_sched.exit_idle,
            (unsigned long long)g_sched.exit_budget,
            (unsigned long long)g_sched.exit_wall,
            (unsigned long long)g_sched.gp_unhalted,
            (unsigned long long)g_sched.ep_unhalted,
            (unsigned long long)g_sched.gp_overbudget,
            (unsigned long long)g_sched.ep_overbudget,
            (g_dsp_gp_halts - gp_halts_prev) * 1e6 / span,
            (g_dsp_ep_halts - ep_halts_prev) * 1e6 / span,
            (g_apu_guest_locks - guest_locks_prev) * 1e6 / span,
            (unsigned long long)g_sched.waiter_breaks,
            g_sched.mixbuf_us * 100.0 / span,
            g_sched.frames ? g_sched.mixbuf_us / (double)g_sched.frames : 0,
            g_ep_kick.n ? g_ep_kick.cyc_sum / (double)g_ep_kick.n / 1000 : 0,
            g_ep_kick.cyc_min / 1000.0, g_ep_kick.cyc_max / 1000.0,
            g_ep_kick.n ? g_ep_kick.frm_sum / (double)g_ep_kick.n : 0,
            (unsigned long long)g_ep_kick.never,
            (unsigned long long)g_sched.gp_late_finish,
            (unsigned long long)g_sched.gp_catchup,
            dsp_frame_starts_dropped(d->gp.dsp),
            (unsigned long long)g_sched.ep_catchup,
            dsp_frame_starts_dropped(d->ep.dsp),
            (unsigned long long)g_sched.fifo1_writes,
            (unsigned long long)g_sched.fifo1_bytes,
            (unsigned long long)(spdif_frames - spdif_frames_prev),
            (unsigned long long)(spdif_bad - spdif_bad_prev),
            (unsigned long long)(spdif_under - spdif_under_prev),
            (unsigned long long)(spdif_over - spdif_over_prev),
            spdif_lvl_min, spdif_lvl_max,
            (unsigned long long)g_sched.gp_calls,
            g_sched.gp_calls ? g_sched.gp_cycles / (double)g_sched.gp_calls : 0,
            g_sched.gp_cycles ? g_sched.gp_us * 1000.0 / g_sched.gp_cycles : 0,
            (unsigned long long)g_sched.ep_calls,
            g_sched.ep_calls ? g_sched.ep_cycles / (double)g_sched.ep_calls : 0,
            g_sched.ep_cycles ? g_sched.ep_us * 1000.0 / g_sched.ep_cycles : 0,
            (g_dsp_dma_p_writes - dma_p_prev) * 1e6 / span,
            (gpc - gpc_prev) * 1e6 / span,
            (gpn - gpn_prev) / 10.0 / span,
            gpc > gpc_prev ? (gpn - gpn_prev) / 1000.0 / (gpc - gpc_prev) : 0,
            (gpi - gpi_prev) * 1e6 / span,
            (epc - epc_prev) * 1e6 / span,
            (epn - epn_prev) / 10.0 / span,
            epc > epc_prev ? (epn - epn_prev) / 1000.0 / (epc - epc_prev) : 0,
            (epi - epi_prev) * 1e6 / span,
            (eph - eph_prev) * 1e6 / span, (unsigned long long)ept,
            (gpb - gpb_prev) / (double)span,
            (epb - epb_prev) / (double)span,
            gpb > gpb_prev ? g_sched.gp_cycles / (double)(gpb - gpb_prev) : 0,
            epb > epb_prev ? g_sched.ep_cycles / (double)(epb - epb_prev) : 0,
            gpb > gpb_prev ? g_sched.gp_us * 1000.0 / (gpb - gpb_prev) : 0,
            epb > epb_prev ? g_sched.ep_us * 1000.0 / (epb - epb_prev) : 0,
            gpz / 1024.0, epz / 1024.0);
    trace_mcpx_apu_dsp_sched(report);
    /* The EP worker's CPU time against its wall time: the shortfall is time
     * the core was runnable and not running. */
    trace_mcpx_apu_dsp_sched_ep_worker(g_sched.ep_us, g_sched.ep_cpu_ns / 1000);
    /* How translation bunched inside frames this window. */
    trace_mcpx_apu_dsp_sched_jit_burst(g_sched.jit_frame_ns_worst / 1000,
                                       g_sched.jit_frame_compiles_worst,
                                       g_sched.jit_compile_frames,
                                       g_sched.frames, gpw / 1000, epw / 1000);
    dsp_block_profile_report(d);
    dma_p_prev = g_dsp_dma_p_writes;
    gp_halts_prev = g_dsp_gp_halts;
    ep_halts_prev = g_dsp_ep_halts;
    guest_locks_prev = g_apu_guest_locks;
    g_ep_kick.n = g_ep_kick.cyc_sum = g_ep_kick.frm_sum = 0;
    g_ep_kick.cyc_min = g_ep_kick.cyc_max = g_ep_kick.never = 0;
    {
        int top = -1;
        for (int i = 0; i < EP_STALL_PCS && g_ep_stall[i].n; i++) {
            if (top < 0 || g_ep_stall[i].n > g_ep_stall[top].n) {
                top = i;
            }
        }
        unsigned distinct = 0;
        for (int i = 0; i < EP_STALL_PCS && g_ep_stall[i].n; i++) {
            distinct++;
        }
        if (top >= 0) {
            char tops[128];
            int pos = 0;
            for (int k = 0; k < 5; k++) {
                int best = -1;
                for (int i = 0; i < EP_STALL_PCS && g_ep_stall[i].n; i++) {
                    if (best < 0 || g_ep_stall[i].n > g_ep_stall[best].n) {
                        best = i;
                    }
                }
                if (best < 0) {
                    break;
                }
                pos += snprintf(tops + pos, sizeof(tops) - pos, " $%04x x%" PRIu64,
                                g_ep_stall[best].pc, g_ep_stall[best].n);
                g_ep_stall[best].n = 0;
            }
            trace_mcpx_apu_dsp_sched_ep_stall(distinct, g_ep_stall_samples,
                                              g_ep_stall_dropped, tops);
        }
        memset(g_ep_stall, 0, sizeof(g_ep_stall));
        g_ep_stall_samples = 0;
        g_ep_stall_dropped = 0;
    }
    spdif_frames_prev = spdif_frames; spdif_bad_prev = spdif_bad;
    spdif_under_prev = spdif_under; spdif_over_prev = spdif_over;
    mcpx_apu_spdif_stats(&spdif_frames, &spdif_bad, &spdif_under,
                         &spdif_over, &spdif_lvl_min, &spdif_lvl_max, true);
    gpc_prev = gpc; gpn_prev = gpn; gpi_prev = gpi;
    epc_prev = epc; epn_prev = epn; epi_prev = epi;
    eph_prev = eph; gpb_prev = gpb; epb_prev = epb;
    memset(&g_sched, 0, sizeof(g_sched));
    g_sched.last_report_us = now;
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
    } else if (d->monitor.point == MCPX_APU_DEBUG_MON_EP) {
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
                    warn_report("MCPX_EP_FIFO1_DUMP: cannot open %s", path);
                }
            }
            fifo1_dump_checked = true;
        }
        if (fifo1_dump) {
            fwrite(ptr, 1, len, fifo1_dump);
            fflush(fifo1_dump);
        }
        g_sched.fifo1_writes++;
        g_sched.fifo1_bytes += len;
        if (d->monitor.point == MCPX_APU_DEBUG_MON_EP_SPDIF) {
            mcpx_apu_spdif_feed(d, ptr, len);
        }
    }
    ep_snapshot_node_done(d, dir && index == 1);

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

/* One execution slice of a core, timed for the SCHED report when that is
 * on. ns per DSP cycle comes from the wall figure; whatever the thread's
 * CPU time falls short of it is time the core was runnable and not
 * running. */
static void run_core_slice(DSPState *dsp, bool is_gp)
{
    if (!sched_stats_enabled()) {
        dsp_run(dsp, DSP_SLICE_CYCLES);
        return;
    }
    int64_t cpu = 0;
#ifdef CLOCK_THREAD_CPUTIME_ID
    struct timespec t0, t1;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t0);
#endif
    int64_t c0 = qemu_clock_get_us(QEMU_CLOCK_REALTIME);
    dsp_run(dsp, DSP_SLICE_CYCLES);
    int64_t wall = qemu_clock_get_us(QEMU_CLOCK_REALTIME) - c0;
#ifdef CLOCK_THREAD_CPUTIME_ID
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t1);
    cpu = (t1.tv_sec - t0.tv_sec) * 1000000000LL + (t1.tv_nsec - t0.tv_nsec);
#endif
    if (is_gp) {
        qatomic_add(&g_sched.gp_us, wall);
        qatomic_add(&g_sched.gp_calls, 1);
    } else {
        qatomic_add(&g_sched.ep_us, wall);
        qatomic_add(&g_sched.ep_cpu_ns, cpu);
        qatomic_add(&g_sched.ep_calls, 1);
    }
}

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
            !core_resume_owed_frame(dsp, w->is_gp ? &g_sched.gp_catchup
                                                  : &g_sched.ep_catchup)) {
            break;
        }
        run_core_slice(dsp, w->is_gp);
        if (qemu_clock_get_us(QEMU_CLOCK_REALTIME) - t0 >= slice_us) {
            w->wall_bound = true;
            break;
        }
        /* A guest-side accessor is blocked on the APU lock the frame thread
         * is holding while it waits for us. Stop between slices so it can be
         * handed over; the core keeps its state and resumes next frame. */
        if (qatomic_read(&d->lock_waiters) > 0) {
            qatomic_add(&g_sched.waiter_breaks, 1);
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
    int64_t t0;
    /* The GP did not reach frame-complete by the end of the previous frame:
     * its output slice for that frame is still unwritten. */
    bool gp_late;
} g_frame;

/* A core that halted while its next frame start is already pending owes
 * that frame: let it continue within its budget instead of parking until
 * the next frame, which would leave it a frame behind for good. */
static bool core_resume_owed_frame(DSPState *dsp, uint64_t *counter)
{
    if (dsp_get_halt_requested(dsp) && dsp_frame_start_pending(dsp)) {
        dsp_set_halt_requested(dsp, false);
        qatomic_add(counter, 1);
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

/* Per-frame accounting once both cores have stopped: cycle totals, why the
 * frame ended, and the diagnostics that sample a core which did not reach
 * its frame-complete flag. */
static void dsp_frame_account(MCPXAPUState *d, bool gp_active, bool ep_active,
                              uint32_t gp_budget, uint32_t ep_budget,
                              int64_t t0, bool wall_bound)
{
    int64_t t1 = qemu_clock_get_us(QEMU_CLOCK_REALTIME);
    g_sched.run_us += t1 - t0;
    g_sched.gp_cycles += dsp_get_cycle_count(d->gp.dsp);
    g_sched.ep_cycles += dsp_get_cycle_count(d->ep.dsp);
    g_frame.gp_late = gp_active && !dsp_get_halt_requested(d->gp.dsp);

    if (sched_stats_enabled()) {
        /* Both cores' translation time landed on this frame, against a
         * 666 us frame wall. Diffed here, after the workers are joined. */
        static uint64_t c_prev[2], ns_prev[2];
        uint64_t fc = 0, fns = 0, x;
        for (int i = 0; i < 2; i++) {
            uint64_t c, ns;
            DSPState *dsp = i ? d->ep.dsp : d->gp.dsp;
            dsp_jit_get_stats(dsp, &c, &ns, &x, &x, &x, &x, &x, &x);
            fc += c - c_prev[i];
            fns += ns - ns_prev[i];
            c_prev[i] = c;
            ns_prev[i] = ns;
        }
        if (fns > g_sched.jit_frame_ns_worst) {
            g_sched.jit_frame_ns_worst = fns;
            g_sched.jit_frame_compiles_worst = fc;
        }
        g_sched.jit_compile_frames += fc > 0;
    }
    if (wall_bound) {
        g_sched.exit_wall++;
    } else if ((gp_active && dsp_get_cycle_count(d->gp.dsp) >= gp_budget) ||
               (ep_active && dsp_get_cycle_count(d->ep.dsp) >= ep_budget)) {
        g_sched.exit_budget++;
    } else {
        g_sched.exit_idle++;
    }

    if (gp_active && !dsp_get_halt_requested(d->gp.dsp)) {
        g_sched.gp_unhalted++;
        if (dsp_get_cycle_count(d->gp.dsp) >= gp_budget) {
            g_sched.gp_overbudget++;
        }
    }
    ep_snapshot(d);
    if (ep_active && !dsp_get_halt_requested(d->ep.dsp)) {
        g_sched.ep_unhalted++;
        if (dsp_get_cycle_count(d->ep.dsp) >= ep_budget) {
            g_sched.ep_overbudget++;
        }
        if (sched_stats_enabled()) {
            uint32_t pc, sp, ssh[16];
            dsp_get_pc_sp(d->ep.dsp, &pc, &sp, ssh);
            ep_stall_note(pc);
        }
    }
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
            if (g_ep_kick.frames && !g_ep_kick.halted) {
                g_ep_kick.never++;
            }
            g_ep_kick.cycles = 0;
            g_ep_kick.frames = 0;
            g_ep_kick.halted = false;
            dsp_start_frame(d->ep.dsp);
            dsp_set_halt_requested(d->ep.dsp, false);
            g_sched.kicks++;
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
        g_frame.t0 = qemu_clock_get_us(QEMU_CLOCK_REALTIME);

        g_sched.frames++;
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
        g_sched.gp_late_finish++;
        int64_t c0 = qemu_clock_get_us(QEMU_CLOCK_REALTIME);
        while (!dsp_get_halt_requested(d->gp.dsp) &&
               dsp_get_cycle_count(d->gp.dsp) < g_frame.gp_budget &&
               qemu_clock_get_us(QEMU_CLOCK_REALTIME) - c0 < 2000) {
            dsp_run(d->gp.dsp, DSP_SLICE_CYCLES);
            qatomic_add(&g_sched.gp_calls, 1);
        }
        qatomic_add(&g_sched.gp_us,
                    qemu_clock_get_us(QEMU_CLOCK_REALTIME) - c0);
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
    int64_t t_mix = qemu_clock_get_us(QEMU_CLOCK_REALTIME);
    for (int mixbin = 0; mixbin < NUM_MIXBINS; mixbin++) {
        uint32_t base = GP_DSP_MIXBUF_BASE + mixbin * NUM_SAMPLES_PER_FRAME;
        for (int sample = 0; sample < NUM_SAMPLES_PER_FRAME; sample++) {
            dsp_write_memory(d->gp.dsp, 'X', base + sample,
                             float_to_24b(mixbins[mixbin][sample]));
        }
    }

    g_sched.mixbuf_us += qemu_clock_get_us(QEMU_CLOCK_REALTIME) - t_mix;

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
        bool wall_bound = (gp_enabled && g_gp_worker.wall_bound) ||
                          (ep_enabled && g_ep_worker.wall_bound);
        dsp_frame_account(d, gp_enabled, ep_enabled, g_frame.gp_budget,
                          g_frame.ep_budget, g_frame.t0, wall_bound);
        sched_stats_report(d);
    }

    if (gp_enabled) {
        g_dbg.gp.cycles = dsp_get_cycle_count(d->gp.dsp);

        if (d->monitor.point == MCPX_APU_DEBUG_MON_GP) {
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

        g_ep_kick.cycles += dsp_get_cycle_count(d->ep.dsp);
        g_ep_kick.frames++;
        if (!g_ep_kick.halted && dsp_get_halt_requested(d->ep.dsp)) {
            g_ep_kick.halted = true;
            g_ep_kick.n++;
            g_ep_kick.cyc_sum += g_ep_kick.cycles;
            g_ep_kick.frm_sum += g_ep_kick.frames;
            if (g_ep_kick.cyc_min == 0 ||
                g_ep_kick.cycles < g_ep_kick.cyc_min) {
                g_ep_kick.cyc_min = g_ep_kick.cycles;
            }
            if (g_ep_kick.cycles > g_ep_kick.cyc_max) {
                g_ep_kick.cyc_max = g_ep_kick.cycles;
            }
        }
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
