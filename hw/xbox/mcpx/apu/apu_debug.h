/*
 * QEMU MCPX Audio Processing Unit implementation
 *
 * Copyright (c) 2020-2026 Matt Borgerson
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

#define MAX_VOICE_WORKERS 16

typedef enum McpxApuDebugMonitorPoint {
    MCPX_APU_DEBUG_MON_AC97,
    MCPX_APU_DEBUG_MON_VP,
    MCPX_APU_DEBUG_MON_GP,
    MCPX_APU_DEBUG_MON_EP,
    /* EP output FIFO #1 - the S/PDIF stream - decoded from IEC 61937 AC-3
     * to discrete 5.1 and played through a 6-channel stream (spdif.c,
     * monitor.c). MON_EP is FIFO #0, the analog output. */
    MCPX_APU_DEBUG_MON_EP_SPDIF,
    /* The EP's output as a listener would take it: FIFO #0 until the
     * S/PDIF stream carries valid AC-3, the decoded stream while it does
     * (monitor.c). */
    MCPX_APU_DEBUG_MON_EP_AUTO,
} McpxApuDebugMonitorPoint;

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
    uint8_t multipass_bin;
    uint16_t multipass_dst_voice;
    int container_size, sample_size;
    unsigned int samples_per_block;
    uint32_t ebo, cbo, lbo, ba;
    float rate;
};

struct McpxApuDebugVp
{
    struct McpxApuDebugVoice v[256];
    int num_workers;
    struct {
        int num_voices;
        int time_us;
    } workers[MAX_VOICE_WORKERS];
    int total_worker_time_us;
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
    struct {
        struct {
            float backoff, ok, speedup;
        } pacing;
        struct {
            int64_t min_us, avg_us, max_us;
        } deviation;
        struct {
            float min_ms, avg_ms, max_ms;
            float low_ms, high_ms;
        } latency;
    } throttle;
};

#ifdef __cplusplus
extern "C" {
#endif

const struct McpxApuDebug *mcpx_apu_get_debug_info(void);
McpxApuDebugMonitorPoint mcpx_apu_debug_get_monitor(void);
void mcpx_apu_debug_set_monitor(McpxApuDebugMonitorPoint monitor);
/* Channels the monitor is currently playing (2 or 6) and the mute mask over
 * them; mask bit c enables channel c. */
int mcpx_apu_debug_get_monitor_channels(void);
uint32_t mcpx_apu_debug_get_monitor_channel_mask(void);
void mcpx_apu_debug_set_monitor_channel_mask(uint32_t mask);
/* Held peak of monitor channel c over its last pushes, 0..1 of full scale,
 * measured before the mask. */
float mcpx_apu_debug_get_monitor_channel_level(int channel);
void mcpx_apu_debug_isolate_voice(uint16_t v);
void mcpx_apu_debug_clear_isolations(void);
void mcpx_apu_debug_toggle_mute(uint16_t v);
bool mcpx_apu_debug_is_muted(uint16_t v);

#ifdef __cplusplus
}
#endif

#endif
