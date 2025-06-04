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
#include <samplerate.h>
#include <SDL.h>
#include "hw/hw.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_device.h"
#include "cpu.h"
#include "migration/vmstate.h"
#include "qemu/main-loop.h"
#include "qemu/thread.h"
#include "sysemu/runstate.h"
#include "audio/audio.h"
#include "qemu/fifo8.h"
#include "ui/xemu-settings.h"

#include "trace.h"
#include "dsp/dsp.h"
#include "dsp/dsp_dma.h"
#include "dsp/dsp_cpu.h"
#include "dsp/dsp_state.h"
#include "apu.h"
#include "apu_regs.h"
#include "apu_debug.h"
#include "adpcm.h"
#include "svf.h"
#include "fpconv.h"
#include "hrtf.h"

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

#define NUM_VOICE_WORKERS 16

typedef struct MCPXAPUVPSSLData {
    uint32_t base[MCPX_HW_SSLS_PER_VOICE];
    uint8_t count[MCPX_HW_SSLS_PER_VOICE];
    int ssl_index;
    int ssl_seg;
} MCPXAPUVPSSLData;

typedef struct MCPXAPUVoiceFilter {
    uint16_t voice;
    float resample_buf[NUM_SAMPLES_PER_FRAME * 2];
    SRC_STATE *resampler;
    sv_filter svf[2];
    HrtfFilter hrtf;
} MCPXAPUVoiceFilter;

typedef struct VoiceWorkItem {
    int voice;
    int list;
} VoiceWorkItem;

typedef struct VoiceWorker {
    QemuThread thread;
    float mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME];
    float sample_buf[NUM_SAMPLES_PER_FRAME][2];
    VoiceWorkItem queue[MCPX_HW_MAX_VOICES];
    int queue_len;
} VoiceWorker;

typedef struct VoiceWorkDispatch {
    QemuMutex lock;
    VoiceWorker workers[NUM_VOICE_WORKERS];
    bool workers_should_exit;
    QemuCond work_pending;
    uint64_t workers_pending;
    QemuCond work_finished;
    float mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME];
    VoiceWorkItem queue[MCPX_HW_MAX_VOICES];
    int queue_len;
} VoiceWorkDispatch;

typedef struct MCPXAPUState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/

    bool exiting;
    bool set_irq;

    QemuThread apu_thread;
    QemuMutex lock;
    QemuCond cond;

    MemoryRegion *ram;
    uint8_t *ram_ptr;
    MemoryRegion mmio;

    /* Setup Engine */
    struct {
    } se;

    /* Voice Processor */
    struct {
        MemoryRegion mmio;
        VoiceWorkDispatch voice_work_dispatch;
        MCPXAPUVoiceFilter filters[MCPX_HW_MAX_VOICES];
        QemuSpin out_buf_lock;
        Fifo8 out_buf;

        // FIXME: Where are these stored?
        int ssl_base_page;
        MCPXAPUVPSSLData ssl[MCPX_HW_MAX_VOICES];
        uint8_t hrtf_headroom;
        uint8_t hrtf_submix[4];
        uint8_t submix_headroom[NUM_MIXBINS];
        float sample_buf[NUM_SAMPLES_PER_FRAME][2];
        uint64_t voice_locked[4];
        QemuSpin voice_spinlocks[MCPX_HW_MAX_VOICES];

        struct {
            int current_entry;
            // FIXME: Stored in RAM
            struct {
                float hrir[2][HRTF_NUM_TAPS];
                float itd;
            } entries[HRTF_ENTRY_COUNT];
        } hrtf;
    } vp;

    /* Global Processor */
    struct {
        bool realtime;
        MemoryRegion mmio;
        DSPState *dsp;
        uint32_t regs[0x10000];
    } gp;

    /* Encode Processor */
    struct {
        bool realtime;
        MemoryRegion mmio;
        DSPState *dsp;
        uint32_t regs[0x10000];
    } ep;

    uint32_t regs[0x20000];

    uint32_t inbuf_sge_handle; //FIXME: Where is this stored?
    uint32_t outbuf_sge_handle; //FIXME: Where is this stored?

    int mon;
    int ep_frame_div;
    int sleep_acc;
    int frame_count;
    int64_t frame_count_time;
    int16_t apu_fifo_output[256][2]; // 1 EP frame (0x400 bytes), 8 buffered
} MCPXAPUState;

static const struct {
    hwaddr top, current, next;
} voice_list_regs[] = {
    { NV_PAPU_TVL2D, NV_PAPU_CVL2D, NV_PAPU_NVL2D }, // 2D
    { NV_PAPU_TVL3D, NV_PAPU_CVL3D, NV_PAPU_NVL3D }, // 3D
    { NV_PAPU_TVLMP, NV_PAPU_CVLMP, NV_PAPU_NVLMP }, // MP
};

extern MCPXAPUState *g_state; // Used via debug handlers
extern struct McpxApuDebug g_dbg, g_dbg_cache;
extern int g_dbg_voice_monitor;
extern uint64_t g_dbg_muted_voices[4];

void mcpx_debug_begin_frame(void);
void mcpx_debug_end_frame(void);

#endif
