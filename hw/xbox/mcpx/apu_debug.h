/*
 * QEMU MCPX Audio Processing Unit implementation
 *
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

#ifndef MCPX_APU_DEBUG_H
#define MCPX_APU_DEBUG_H

#include <stdbool.h>
#include <stdint.h>

enum McpxApuDebugMon {
    MCPX_APU_DEBUG_MON_AC97,
    MCPX_APU_DEBUG_MON_VP,
    MCPX_APU_DEBUG_MON_GP,
    MCPX_APU_DEBUG_MON_EP,
    MCPX_APU_DEBUG_MON_GP_OR_EP
};

struct McpxApuDebugVoice
{
    bool active;
    bool paused;
    bool stereo;
    uint8_t bin[8];
    uint16_t vol[8];

    bool stream;
    bool loop;
    bool persist;
    bool multipass;
    bool linked;
    int container_size, sample_size;
    unsigned int samples_per_block;
    uint32_t ebo, cbo, lbo, ba;
    float rate;
};

struct McpxApuDebugVp
{
    struct McpxApuDebugVoice v[256];
};

struct McpxApuDebugDsp
{
    int cycles;
};

struct McpxApuDebug
{
    struct McpxApuDebugVp vp;
    struct McpxApuDebugDsp gp, ep;
    int frames_processed;
    float utilization;
    bool gp_realtime, ep_realtime;
};

#ifdef __cplusplus
extern "C" {
#endif

const struct McpxApuDebug *mcpx_apu_get_debug_info(void);
int mcpx_apu_debug_get_monitor(void);
void mcpx_apu_debug_set_monitor(int mon);
void mcpx_apu_debug_isolate_voice(uint16_t v);
void mcpx_apu_debug_clear_isolations(void);
void mcpx_apu_debug_toggle_mute(uint16_t v);
bool mcpx_apu_debug_is_muted(uint16_t v);
void mcpx_apu_debug_set_gp_realtime_enabled(bool enable);
void mcpx_apu_debug_set_ep_realtime_enabled(bool enable);

#ifdef __cplusplus
}
#endif

#endif
