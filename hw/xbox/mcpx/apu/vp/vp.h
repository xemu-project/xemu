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
#ifndef HW_XBOX_MCPX_VP_H
#define HW_XBOX_MCPX_VP_H

#include <samplerate.h>

#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "hw/hw.h"
#include "hw/pci/pci.h"
#include "hw/xbox/mcpx/apu/apu_regs.h"
#include "svf.h"
#include "hrtf.h"

typedef struct MCPXAPUState MCPXAPUState;

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
    int num_workers;
    VoiceWorker *workers;
    bool workers_should_exit;
    QemuCond work_pending;
    uint64_t workers_pending;
    QemuCond work_finished;
    float mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME];
    VoiceWorkItem queue[MCPX_HW_MAX_VOICES];
    int queue_len;
} VoiceWorkDispatch;

typedef struct {
    MemoryRegion mmio;
    VoiceWorkDispatch voice_work_dispatch;
    MCPXAPUVoiceFilter filters[MCPX_HW_MAX_VOICES];

    // FIXME: Where are these stored?
    int ssl_base_page;
    MCPXAPUVPSSLData ssl[MCPX_HW_MAX_VOICES];
    uint8_t hrtf_headroom;
    uint8_t hrtf_submix[4];
    uint8_t submix_headroom[NUM_MIXBINS];
    float sample_buf[NUM_SAMPLES_PER_FRAME][2];
    uint64_t voice_locked[4];

    struct {
        int current_entry;
        // FIXME: Stored in RAM
        struct {
            float hrir[2][HRTF_NUM_TAPS];
            float itd;
        } entries[HRTF_ENTRY_COUNT];
    } hrtf;

    uint32_t inbuf_sge_handle; //FIXME: Where is this stored?
    uint32_t outbuf_sge_handle; //FIXME: Where is this stored?
} MCPXAPUVPState;

extern const MemoryRegionOps vp_ops;

void mcpx_apu_vp_init(MCPXAPUState *d);
void mcpx_apu_vp_finalize(MCPXAPUState *d);
void mcpx_apu_vp_frame(MCPXAPUState *d, float mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME]);
void mcpx_apu_vp_reset(MCPXAPUState *d);

#endif
