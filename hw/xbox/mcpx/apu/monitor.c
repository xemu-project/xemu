/*
 * QEMU MCPX Audio Processing Unit implementation
 *
 * Copyright (c) 2019-2026 Matt Borgerson
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
#include "qapi/error.h"
#include "qemu/cutils.h"
#include "qemu/error-report.h"

/* Whether the pull plays the decoded S/PDIF stream (six channels) rather
 * than a stereo pair. The S/PDIF tap always does; the EP tap does while a
 * title is encoding. SDL converts the layout to whatever the device has,
 * so a stereo host hears a downmix of the decoded 5.1. */
static bool monitor_surround(MCPXAPUState *d)
{
    switch (d->monitor.point) {
    case MCPX_APU_DEBUG_MON_EP_SPDIF:
        return true;
    case MCPX_APU_DEBUG_MON_EP_AUTO:
        return mcpx_apu_spdif_stream_present();
    default:
        return false;
    }
}

static int monitor_frame_bytes(int channels)
{
    return 256 * channels * (int)sizeof(int16_t);
}

/* (Re)open the playback stream with `channels`. Pacing thresholds are
 * derived from the frame size, so they follow the channel count. */
static bool monitor_open(MCPXAPUState *d, int channels, Error **errp)
{
    SDL_AudioSpec spec = {
        .freq = 48000,
        .format = SDL_AUDIO_S16LE,
        .channels = channels,
    };

    if (d->monitor.stream) {
        SDL_DestroyAudioStream(d->monitor.stream);
        d->monitor.stream = NULL;
    }

    d->monitor.stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (d->monitor.stream == NULL) {
        error_setg(errp, "SDL_OpenAudioDeviceStream failed: %s",
                   SDL_GetError());
        return false;
    }
    d->monitor.channels = channels;

    SDL_AudioDeviceID dev = SDL_GetAudioStreamDevice(d->monitor.stream);

    SDL_AudioSpec dev_spec;
    int dev_buf_frames = 0;
    int dev_drain_bytes = 0;
    if (SDL_GetAudioDeviceFormat(dev, &dev_spec, &dev_buf_frames)) {
        dev_drain_bytes = dev_buf_frames * spec.channels *
                          SDL_AUDIO_BYTESIZE(spec.format) *
                          spec.freq / dev_spec.freq;
    }
    int frame_bytes = monitor_frame_bytes(channels);
    int drain = MAX(dev_drain_bytes, frame_bytes);
    d->monitor.queued_bytes_low = drain;
    d->monitor.queued_bytes_high = 3 * drain;

    SDL_ResumeAudioDevice(dev);
    return true;
}

/* The monitor plays the EP's output. Its analog output, FIFO #0, is the
 * point present in every guest audio configuration (a stereo pair, L == R
 * for a mono guest, the EP's own surround-compatible downmix with Dolby
 * Digital on); the S/PDIF stream on FIFO #1 exists only while a title runs
 * the encoder, and is played decoded while it does (monitor_surround). The
 * VP and GP taps bypass the EP; those and the pinned EP taps are debug
 * views.
 *
 * MCPX_APU_MONITOR=ac97|vp|gp|ep|spdif|auto pins the point from the
 * environment - what the UI combo does, for headless captures. `ep` is
 * FIFO #0 alone; `spdif` decodes the AC-3 stream on FIFO #1 (spdif.c). A
 * capture wants one of those: under `auto` the layout of the dump can
 * change when the title starts or stops encoding. */
static void monitor_select_point(MCPXAPUState *d)
{
    d->monitor.point = MCPX_APU_DEBUG_MON_EP_AUTO;
    const char *mon = getenv("MCPX_APU_MONITOR");
    if (mon) {
        if (!strcmp(mon, "auto")) {
            d->monitor.point = MCPX_APU_DEBUG_MON_EP_AUTO;
        } else if (!strcmp(mon, "ac97")) {
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
}

/* MCPX_APU_MON_CHANNELS=<list>: solo/mute channels of the monitor's
 * layout - comma-separated names (l,r or fl,fr,fc,lfe,bl,br), channel
 * indices, or a hex mask (0x..). Unset = all channels. */
static void monitor_select_channels(MCPXAPUState *d)
{
    const char *chs = getenv("MCPX_APU_MON_CHANNELS");
    d->monitor.channel_mask = 0x3F;
    if (!chs) {
        return;
    }
    static const char *const names[6] = { "fl", "fr", "fc", "lfe", "bl", "br" };
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

void mcpx_apu_monitor_init(MCPXAPUState *d, Error **errp)
{
    d->monitor.stream = NULL;
    d->monitor.channels = 0;
    monitor_select_point(d);
    monitor_select_channels(d);

    if (!SDL_Init(SDL_INIT_AUDIO)) {
        error_setg(errp, "SDL_Init failed: %s", SDL_GetError());
        return;
    }

    if (!monitor_open(d, monitor_surround(d) ? 6 : 2, errp)) {
        return;
    }
}

void mcpx_apu_monitor_finalize(MCPXAPUState *d)
{
    if (d->monitor.stream) {
        SDL_DestroyAudioStream(d->monitor.stream);
    }
}

void mcpx_apu_monitor_frame(MCPXAPUState *d)
{
    if ((d->ep_frame_div + 1) % 8) {
        return;
    }

    mcpx_apu_spdif_pull();
    bool surround = monitor_surround(d);
    mcpx_apu_spdif_set_decoding(surround);
    int want = surround ? 6 : 2;
    if (d->monitor.stream && want != d->monitor.channels) {
        Error *err = NULL;
        if (!monitor_open(d, want, &err)) {
            error_report_err(err);
        }
    } else if (!d->monitor.stream) {
        /* Dump-only: the layout is whatever this pull writes. */
        d->monitor.channels = want;
    }

    if (surround) {
        mcpx_apu_spdif_fill_frame(d);
    }
    const void *buf = surround ? (const void *)d->monitor.surround_buf
                               : (const void *)d->monitor.frame_buf;
    size_t len = surround ? sizeof(d->monitor.surround_buf)
                          : sizeof(d->monitor.frame_buf);

    /* Per-channel peak for the debug meters, pre-mask. */
    {
        const int16_t *pcm = (const int16_t *)buf;
        for (int c = 0; c < want; c++) {
            int peak = 0;
            for (int i = 0; i < 256; i++) {
                int v = abs(pcm[i * want + c]);
                peak = v > peak ? v : peak;
            }
            float level = peak / 32768.0f;
            float held = d->monitor.channel_level[c] * 0.85f;
            d->monitor.channel_level[c] = level > held ? level : held;
        }
        for (int c = want; c < 6; c++) {
            d->monitor.channel_level[c] = 0.0f;
        }
    }

    /* Mute the channels the mask leaves out, on whichever layout is live. */
    uint32_t mask = d->monitor.channel_mask;
    if ((mask & ((1u << want) - 1)) != ((1u << want) - 1)) {
        int16_t *pcm = (int16_t *)buf;
        for (int i = 0; i < 256; i++) {
            for (int c = 0; c < want; c++) {
                if (!(mask & (1u << c))) {
                    pcm[i * want + c] = 0;
                }
            }
        }
    }

    /* MCPX_APU_MON_DUMP=<path>: append the monitor PCM (S16LE, 48 kHz,
     * pre-gain, post-channel-mask; stereo, or 6-channel FL FR FC LFE BL BR
     * on the S/PDIF tap) - the exact signal the debug monitor plays - so an
     * audio artifact can be diffed across arms instead of heard. */
    static int dump_init;
    static FILE *dump_file;
    if (!dump_init) {
        dump_init = 1;
        const char *path = getenv("MCPX_APU_MON_DUMP");
        if (path) {
            dump_file = fopen(path, "wb");
        }
    }
    if (dump_file) {
        fwrite(buf, 1, len, dump_file);
    }

    if (d->monitor.stream && d->monitor.channels == want) {
        float vu = pow(fmax(0.0, fmin(g_config.audio.volume_limit, 1.0)), M_E);
        SDL_SetAudioStreamGain(d->monitor.stream, vu);
        SDL_PutAudioStreamData(d->monitor.stream, buf, len);
    }

    memset(d->monitor.frame_buf, 0, sizeof(d->monitor.frame_buf));
    memset(d->monitor.surround_buf, 0, sizeof(d->monitor.surround_buf));
}
