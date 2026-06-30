/*
 * QEMU Geforce NV2A profiling helpers
 *
 * Copyright (c) 2020-2024 Matt Borgerson
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

#include "hw/xbox/nv2a/nv2a_int.h"

NV2AStats g_nv2a_stats;

void nv2a_profile_increment(void)
{
    int64_t now = qemu_clock_get_us(QEMU_CLOCK_REALTIME);
    const int64_t fps_update_interval = 250000;
    g_nv2a_stats.last_flip_time = now;

    static int64_t frame_count = 0;
    frame_count++;

    static int64_t ts = 0;
    int64_t delta = now - ts;
    if (delta >= fps_update_interval) {
        g_nv2a_stats.increment_fps = frame_count * 1000000 / delta;
        ts = now;
        frame_count = 0;
    }
}

/* Optional, opt-in profiling dump. Set XEMU_NV2A_PROFILE=1 (or to a frame
 * interval) to periodically print FPS, frame time, and the per-frame NV2A
 * counters (averaged over a recent window) to stderr. Off by default and
 * does nothing unless the env var is present, so it is safe to keep in tree.
 */
static void nv2a_profile_maybe_dump(void)
{
    static int enabled = -1;
    static int interval = 120;
    if (enabled < 0) {
        const char *env = getenv("XEMU_NV2A_PROFILE");
        enabled = (env && env[0] && env[0] != '0') ? 1 : 0;
        if (enabled) {
            int v = atoi(env);
            if (v > 0) {
                interval = v;
            }
        }
    }
    if (!enabled || (g_nv2a_stats.frame_count % interval) != 0) {
        return;
    }

    unsigned int window = g_nv2a_stats.frame_count < NV2A_PROF_NUM_FRAMES ?
                              g_nv2a_stats.frame_count : NV2A_PROF_NUM_FRAMES;
    if (window == 0) {
        return;
    }

    double mspf_sum = 0;
    double counter_sum[NV2A_PROF__COUNT] = { 0 };
    for (unsigned int i = 0; i < window; i++) {
        mspf_sum += g_nv2a_stats.frame_history[i].mspf;
        for (int c = 0; c < NV2A_PROF__COUNT; c++) {
            counter_sum[c] += g_nv2a_stats.frame_history[i].counters[c];
        }
    }

    fprintf(stderr, "[nv2a-prof] fps=%u avg_mspf=%.2f (avg per-frame counters "
                    "over %u frames):\n",
            g_nv2a_stats.increment_fps, mspf_sum / window, window);
    for (int c = 0; c < NV2A_PROF__COUNT; c++) {
        double avg = counter_sum[c] / window;
        if (avg >= 0.5) {
            fprintf(stderr, "[nv2a-prof]   %-32s %10.1f\n",
                    nv2a_profile_get_counter_name(c), avg);
        }
    }
}

void nv2a_profile_flip_stall(void)
{
    int64_t now = qemu_clock_get_us(QEMU_CLOCK_REALTIME);
    int64_t render_time = (now-g_nv2a_stats.last_flip_time)/1000;

    g_nv2a_stats.frame_working.mspf = render_time;
    g_nv2a_stats.frame_history[g_nv2a_stats.frame_ptr] =
        g_nv2a_stats.frame_working;
    g_nv2a_stats.frame_ptr =
        (g_nv2a_stats.frame_ptr + 1) % NV2A_PROF_NUM_FRAMES;
    g_nv2a_stats.frame_count++;
    memset(&g_nv2a_stats.frame_working, 0, sizeof(g_nv2a_stats.frame_working));

    nv2a_profile_maybe_dump();
}

const char *nv2a_profile_get_counter_name(unsigned int cnt)
{
    const char *default_names[NV2A_PROF__COUNT] = {
        #define _X(x) stringify(x),
        NV2A_PROF_COUNTERS_XMAC
        #undef _X
    };

    assert(cnt < NV2A_PROF__COUNT);
    return default_names[cnt] + 10; /* 'NV2A_PROF_' */
}

int nv2a_profile_get_counter_value(unsigned int cnt)
{
    assert(cnt < NV2A_PROF__COUNT);
    unsigned int idx = (g_nv2a_stats.frame_ptr + NV2A_PROF_NUM_FRAMES - 1) %
                       NV2A_PROF_NUM_FRAMES;
    return g_nv2a_stats.frame_history[idx].counters[cnt];
}
