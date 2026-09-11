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
#ifndef HW_XBOX_MCPX_APU_INT_H
#define HW_XBOX_MCPX_APU_INT_H

#include "qemu/osdep.h"
#include <math.h>
#include <SDL3/SDL.h>
#include "hw/hw.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_device.h"
#include "cpu.h"
#include "exec/target_page.h"
#include "migration/vmstate.h"
#include "qemu/main-loop.h"
#include "qemu/thread.h"
#include "system/physmem.h"
#include "system/runstate.h"
#include "ui/xemu-settings.h"

#include "trace.h"
#include "apu.h"
#include "apu_regs.h"
#include "apu_debug.h"
#include "fpconv.h"
#include "vp/vp.h"
#include "dsp/gp_ep.h"

#define GET_MASK(v, mask) (((v) & (mask)) >> ctz32(mask))

#define SET_MASK(v, mask, val)                                       \
    do {                                                             \
        (v) &= ~(mask);                                              \
        (v) |= ((val) << ctz32(mask)) & (mask);                      \
    } while (0)

#define CASE_4(v, step)                                              \
    case (v):                                                        \
    case (v)+(step):                                                 \
    case (v)+(step)*2:                                               \
    case (v)+(step)*3

// #define DEBUG_MCPX

#ifdef DEBUG_MCPX
#define DPRINTF(fmt, ...) \
    do { fprintf(stderr, fmt, ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
    do { } while (0)
#endif

#define MCPX_APU_DEVICE(obj) \
    OBJECT_CHECK(MCPXAPUState, (obj), "mcpx-apu")

typedef struct MCPXAPUState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/

    bool exiting;
    bool set_irq;

    QemuThread apu_thread;
    QemuMutex lock;
    QemuCond cond;
    QemuCond idle_cond;
    bool pause_requested;
    bool is_idle;

    MemoryRegion *ram;
    uint8_t *ram_ptr;
    /* Cached: the RAM helpers below bound-check every access. */
    uint64_t ram_size;
    MemoryRegion mmio;

    MCPXAPUVPState vp;
    MCPXAPUGPState gp;
    MCPXAPUEPState ep;

    uint32_t regs[0x20000];

    int ep_frame_div;
    /* Guest-side accessors blocked on, or about to block on, `lock`. The
     * frame thread holds the lock while the DSP cores run and cannot see a
     * waiter from the mutex itself; it hands the lock over while this is
     * non-zero (see mcpx_apu_guest_lock). */
    int lock_waiters;
    int frame_work_acc_us;
    int frame_count;
    int64_t frame_count_time_us;
    int64_t next_frame_time_us;

    struct {
        struct {
            int backoff, ok, speedup;
        } pacing;
        struct {
            int64_t last_us;
            int64_t min_us, max_us, sum_us;
            int count;
        } deviation;
        int queued_bytes_min, queued_bytes_max;
        int64_t queued_bytes_sum;
        int queued_bytes_count;
    } throttle;

    struct {
        McpxApuDebugMonitorPoint point;
        int16_t frame_buf[256][2]; // 1 EP frame (0x400 bytes), stereo taps
        /* 6-channel frames (FL FR FC LFE BL BR) for the EP S/PDIF tap.
         * `channels` is what the open stream was created with; the monitor
         * reopens it when the selected tap wants another count. */
        int16_t surround_buf[256][6];
        int channels;
        SDL_AudioStream *stream;
        int queued_bytes_low, queued_bytes_high;
    } monitor;
} MCPXAPUState;

/* Take the APU lock from a guest-side accessor (MMIO, voice lock). Announce
 * the wait first: the frame thread polls this to decide when to let go. */
static inline void mcpx_apu_guest_lock(MCPXAPUState *d)
{
    qatomic_inc(&d->lock_waiters);
    qemu_mutex_lock(&d->lock);
    qatomic_dec(&d->lock_waiters);
}

/* Release it, waking a frame thread that handed the lock over. */
static inline void mcpx_apu_guest_unlock(MCPXAPUState *d)
{
    qemu_cond_signal(&d->cond);
    qemu_mutex_unlock(&d->lock);
}

/* Guest words the APU chases per sample or per DMA node - voice params, SGE
 * entries - live in RAM, and resolving each through the address space walks
 * the flat view per call, millions of times a second. These read and write
 * through the RAM pointer and keep the walk for an address outside RAM. */

/* Marking dirty through memory_region_set_dirty costs an RCU section and a
 * locked RMW per client bitmap even when every bit is already set, which
 * for the pages the DSP and VP stream into is every call. The lazy variant
 * scans first. The CODE bit is set without a TB invalidate, exactly like
 * memory_region_set_dirty. */
static inline void mcpx_apu_ram_set_dirty(MCPXAPUState *d, hwaddr addr,
                                          hwaddr len)
{
    physical_memory_set_dirty_range_lazy(
        memory_region_get_ram_addr(d->ram) + addr, len,
        memory_region_get_dirty_log_mask(d->ram));
}

static inline uint32_t mcpx_apu_ram_ldl(MCPXAPUState *d, hwaddr addr)
{
    if (addr + 4 <= d->ram_size) {
        return ldl_le_p(&d->ram_ptr[addr]);
    }
    return ldl_le_phys(&address_space_memory, addr);
}

static inline uint32_t mcpx_apu_ram_ldub(MCPXAPUState *d, hwaddr addr)
{
    if (addr < d->ram_size) {
        return d->ram_ptr[addr];
    }
    return ldub_phys(&address_space_memory, addr);
}

static inline uint32_t mcpx_apu_ram_lduw(MCPXAPUState *d, hwaddr addr)
{
    if (addr + 2 <= d->ram_size) {
        return lduw_le_p(&d->ram_ptr[addr]);
    }
    return lduw_le_phys(&address_space_memory, addr);
}

static inline void mcpx_apu_ram_stl(MCPXAPUState *d, hwaddr addr, uint32_t val)
{
    if (addr + 4 <= d->ram_size) {
        stl_le_p(&d->ram_ptr[addr], val);
        mcpx_apu_ram_set_dirty(d, addr, 4);
    } else {
        stl_le_phys(&address_space_memory, addr, val);
    }
}

extern MCPXAPUState *g_state; // Used via debug handlers
extern struct McpxApuDebug g_dbg, g_dbg_cache;
extern int g_dbg_voice_monitor;
extern uint64_t g_dbg_muted_voices[4];

void mcpx_debug_begin_frame(void);
void mcpx_debug_end_frame(void);

void mcpx_apu_monitor_init(MCPXAPUState *d, Error **errp);
void mcpx_apu_monitor_finalize(MCPXAPUState *d);
void mcpx_apu_monitor_frame(MCPXAPUState *d);

/* EP S/PDIF (IEC 61937 AC-3) monitor point, spdif.c */
void mcpx_apu_spdif_feed(MCPXAPUState *d, const uint8_t *buf, size_t len);
void mcpx_apu_spdif_fill_frame(MCPXAPUState *d);
void mcpx_apu_spdif_stats(uint64_t *frames, uint64_t *rejected,
                          uint64_t *underruns, uint64_t *overruns,
                          unsigned *level_min, unsigned *level_max,
                          bool reset);

#endif
