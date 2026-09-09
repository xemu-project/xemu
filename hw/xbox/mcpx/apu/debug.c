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

#include "apu_int.h"

struct McpxApuDebug g_dbg, g_dbg_cache;
int g_dbg_voice_monitor = -1;
uint64_t g_dbg_muted_voices[4];

const struct McpxApuDebug *mcpx_apu_get_debug_info(void)
{
    return &g_dbg_cache;
}

void mcpx_debug_begin_frame(void)
{
    for (int i = 0; i < MCPX_HW_MAX_VOICES; i++) {
        g_dbg.vp.v[i].active = false;
        g_dbg.vp.v[i].multipass_dst_voice = 0xFFFF;
    }
}

void mcpx_debug_end_frame(void)
{
    g_dbg_cache = g_dbg;
}

void mcpx_apu_debug_set_gp_realtime_enabled(bool run)
{
    g_state->gp.realtime = run;
}

void mcpx_apu_debug_set_ep_realtime_enabled(bool run)
{
    g_state->ep.realtime = run;
}

McpxApuDebugMonitorPoint mcpx_apu_debug_get_monitor(void)
{
    return g_state->monitor.point;
}

void mcpx_apu_debug_set_monitor(McpxApuDebugMonitorPoint monitor)
{
    g_state->monitor.point = monitor;
}

int mcpx_apu_debug_get_monitor_channels(void)
{
    /* The open stream's count, or - before it has been (re)opened, and when
     * there is no audio device at all - what the selected tap will ask for,
     * so the UI always matches the layout the mask is applied to. */
    if (g_state->monitor.channels) {
        return g_state->monitor.channels;
    }
    return g_state->monitor.point == MCPX_APU_DEBUG_MON_EP_SPDIF ? 6 : 2;
}

uint32_t mcpx_apu_debug_get_monitor_channel_mask(void)
{
    return g_state->monitor.channel_mask;
}

void mcpx_apu_debug_set_monitor_channel_mask(uint32_t mask)
{
    g_state->monitor.channel_mask = mask;
}

float mcpx_apu_debug_get_monitor_channel_level(int channel)
{
    if (channel < 0 || channel >= 6) {
        return 0.0f;
    }
    return g_state->monitor.channel_level[channel];
}

void mcpx_apu_debug_isolate_voice(uint16_t v)
{
    g_dbg_voice_monitor = v;
}

void mcpx_apu_debug_clear_isolations(void)
{
    g_dbg_voice_monitor = -1;
}

bool mcpx_apu_debug_is_muted(uint16_t v)
{
    assert(v < MCPX_HW_MAX_VOICES);
    return g_dbg_muted_voices[v / 64] & (1LL << (v % 64));
}

void mcpx_apu_debug_toggle_mute(uint16_t v)
{
    assert(v < MCPX_HW_MAX_VOICES);
    g_dbg_muted_voices[v / 64] ^= (1LL << (v % 64));
}
