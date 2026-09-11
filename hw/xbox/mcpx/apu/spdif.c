/*
 * QEMU MCPX Audio Processing Unit implementation
 *
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

/*
 * EP S/PDIF monitor point.
 *
 * With Dolby Digital enabled the EP writes two things: FIFO #0 carries the
 * analog stereo output (a surround-encoded downmix - not the front L/R the
 * GP monitor taps, so it is not a reference for the 5.1 program), and FIFO
 * #1 carries the S/PDIF stream: IEC 61937 bursts, each an AC-3 frame in
 * 16-bit little-endian words, at the 192 KB/s rate of a 48 kHz stereo
 * carrier. Neither is something the debug monitor could play as PCM.
 *
 * This taps FIFO #1 as the receiver on the other end of the cable would:
 * a byte-oriented parser finds the Pa/Pb preamble, reads the burst type and
 * length, byte-swaps the payload into a big-endian AC-3 frame, checks the
 * frame's CRC, decodes it with liba52 to discrete 5.1 (SDL channel order)
 * and queues the 1536 frames for the monitor's 256-sample pushes. Frames
 * failing the CRC are counted and dropped rather than decoded to noise.
 *
 * The CRC check is a guard, not a filter: with bit-14 DMA offsets reading
 * zero as on silicon, the stream is one valid burst per 6144-byte period
 * and the SCHED line's spdif counters show 0 bad.
 *
 * Threading: `feed` runs on the thread executing the EP core (its DSP
 * worker), `fill_frame` on the APU thread after the cores are joined - the
 * same ordering the FIFO #0 tap relies on, so the ring needs no lock.
 */

#include "apu_int.h"

#include <a52.h>

#define SPDIF_MAX_BURST 4096 /* AC-3 frames are at most 3840 bytes */
#define SPDIF_PCM_FRAMES 16384 /* ~341 ms of decoded 5.1 */

enum {
    IEC_PA0,
    IEC_PA1,
    IEC_PB0,
    IEC_PB1,
    IEC_PC0,
    IEC_PC1,
    IEC_PD0,
    IEC_PD1,
    IEC_PAYLOAD,
};

static struct {
    int state;
    unsigned type;
    unsigned length; /* payload bytes */
    unsigned pos;
    uint8_t burst[SPDIF_MAX_BURST];

    int16_t pcm[SPDIF_PCM_FRAMES][6]; /* SDL order: FL FR FC LFE BL BR */
    unsigned head, tail; /* frames; head == tail is empty */

    a52_state_t *a52;
    bool decoding;
    /* Monitor pulls so far, and the pull at which the last valid burst
     * was seen: the stream is present while the gap is short. */
    unsigned pulls, last_valid_pull;

    uint64_t frames, rejected, underruns, overruns;
    /* Ring level (frames) at each monitor pull, min/max since last read. */
    unsigned level_min, level_max;
} s = { .level_min = UINT_MAX };

/* `reset` starts a new min/max window (the reporter passes it when it
 * prints, not on every poll). */
void mcpx_apu_spdif_stats(uint64_t *frames, uint64_t *rejected,
                          uint64_t *underruns, uint64_t *overruns,
                          unsigned *level_min, unsigned *level_max, bool reset)
{
    *frames = s.frames;
    *rejected = s.rejected;
    *underruns = s.underruns;
    *overruns = s.overruns;
    *level_min = s.level_min == UINT_MAX ? 0 : s.level_min;
    *level_max = s.level_max;
    if (reset) {
        s.level_min = UINT_MAX;
        s.level_max = 0;
    }
}

/* AC-3 CRC-16 (x^16 + x^15 + x^2 + 1). crc2 at the end of a frame is
 * chosen so the CRC of everything after the syncword is zero. */
static uint16_t ac3_crc16(const uint8_t *buf, size_t len)
{
    uint16_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)buf[i] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x8005 : crc << 1;
        }
    }
    return crc;
}

static void pcm_push(const int16_t v[6])
{
    unsigned next = (s.head + 1) % SPDIF_PCM_FRAMES;
    if (next == s.tail) {
        /* Full: drop the oldest so the monitor stays near real time. */
        s.tail = (s.tail + 1) % SPDIF_PCM_FRAMES;
        s.overruns++;
    }
    memcpy(s.pcm[s.head], v, sizeof(s.pcm[0]));
    s.head = next;
}

static inline int16_t to_s16(float v)
{
    v *= 32767.0f;
    if (v > 32767.0f) {
        return 32767;
    }
    if (v < -32768.0f) {
        return -32768;
    }
    return (int16_t)lrintf(v);
}

static void decode_burst(void)
{
    /* Burst type 1 is AC-3 (IEC 61937-3); the payload is big-endian bytes
     * packed into little-endian 16-bit words. */
    if (s.type != 1 || s.length < 8 || (s.length & 1)) {
        s.rejected++;
        return;
    }
    uint8_t *f = s.burst;
    for (unsigned i = 0; i + 1 < s.length; i += 2) {
        uint8_t t = f[i];
        f[i] = f[i + 1];
        f[i + 1] = t;
    }

    if (!s.a52) {
        s.a52 = a52_init(0);
        if (!s.a52) {
            s.rejected++;
            return;
        }
        /* Decode what the encoder produced, without the receiver-side
         * dynamic range compression liba52 applies by default. */
        a52_dynrng(s.a52, NULL, NULL);
    }
    int flags, sample_rate, bit_rate;
    int frame_len = a52_syncinfo(f, &flags, &sample_rate, &bit_rate);
    if (frame_len <= 0 || (unsigned)frame_len > s.length ||
        ac3_crc16(f + 2, frame_len - 2) != 0) {
        s.rejected++;
        return;
    }
    s.last_valid_pull = s.pulls;
    if (!s.decoding) {
        return;
    }
    /* Ask for discrete 3/2 + LFE; liba52 hands back the layout it will
     * actually produce (it never upmixes), and lays the blocks out with
     * LFE first when present, then the full-bandwidth channels in bitstream
     * order. Map each block to its SDL 5.1 slot (FL FR FC LFE BL BR). */
    flags = A52_3F2R | A52_LFE | A52_ADJUST_LEVEL;
    level_t level = 1;
    if (a52_frame(s.a52, f, &flags, &level, 0)) {
        s.rejected++;
        return;
    }
    enum { FL, FR, FC, LFE, BL, BR, SURR_MONO = 8 };
    static const int8_t layout[16][5] = {
        [A52_CHANNEL] = { FL, FR, -1, -1, -1 },
        [A52_MONO] = { FC, -1, -1, -1, -1 },
        [A52_STEREO] = { FL, FR, -1, -1, -1 },
        [A52_3F] = { FL, FC, FR, -1, -1 },
        [A52_2F1R] = { FL, FR, SURR_MONO, -1, -1 },
        [A52_3F1R] = { FL, FC, FR, SURR_MONO, -1 },
        [A52_2F2R] = { FL, FR, BL, BR, -1 },
        [A52_3F2R] = { FL, FC, FR, BL, BR },
        [A52_CHANNEL1] = { FC, -1, -1, -1, -1 },
        [A52_CHANNEL2] = { FC, -1, -1, -1, -1 },
        [A52_DOLBY] = { FL, FR, -1, -1, -1 },
    };
    const int8_t *map = layout[flags & A52_CHANNEL_MASK];
    bool has_lfe = flags & A52_LFE;
    for (int blk = 0; blk < 6; blk++) {
        if (a52_block(s.a52)) {
            s.rejected++;
            return;
        }
        const sample_t *out = a52_samples(s.a52);
        for (int i = 0; i < 256; i++) {
            int16_t v[6] = { 0 };
            const sample_t *ch = out;
            if (has_lfe) {
                v[LFE] = to_s16(ch[i]);
                ch += 256;
            }
            for (int k = 0; k < 5 && map[k] >= 0; k++, ch += 256) {
                int16_t x = to_s16(ch[i]);
                if (map[k] == SURR_MONO) {
                    v[BL] = x;
                    v[BR] = x;
                } else {
                    v[map[k]] = x;
                }
            }
            pcm_push(v);
        }
    }
    s.frames++;
}

void mcpx_apu_spdif_feed(MCPXAPUState *d, const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uint8_t b = buf[i];
        switch (s.state) {
        case IEC_PA0:
            s.state = (b == 0x72) ? IEC_PA1 : IEC_PA0;
            break;
        case IEC_PA1:
            s.state = (b == 0xF8) ? IEC_PB0 : (b == 0x72) ? IEC_PA1 : IEC_PA0;
            break;
        case IEC_PB0:
            s.state = (b == 0x1F) ? IEC_PB1 : (b == 0x72) ? IEC_PA1 : IEC_PA0;
            break;
        case IEC_PB1:
            s.state = (b == 0x4E) ? IEC_PC0 : (b == 0x72) ? IEC_PA1 : IEC_PA0;
            break;
        case IEC_PC0:
            s.type = b & 0x1F;
            s.state = IEC_PC1;
            break;
        case IEC_PC1:
            s.state = IEC_PD0;
            break;
        case IEC_PD0:
            s.length = b;
            s.state = IEC_PD1;
            break;
        case IEC_PD1:
            s.length = (s.length | ((unsigned)b << 8)) / 8;
            s.pos = 0;
            if (s.length == 0 || s.length > SPDIF_MAX_BURST) {
                s.rejected++;
                s.state = IEC_PA0;
            } else {
                s.state = IEC_PAYLOAD;
            }
            break;
        case IEC_PAYLOAD:
            s.burst[s.pos++] = b;
            if (s.pos == s.length) {
                decode_burst();
                s.state = IEC_PA0;
            }
            break;
        }
    }
}

/* Bursts arrive at 31.25/s, one per six pulls; a gap of 32 pulls (~170 ms)
 * means the encoder stopped. */
#define SPDIF_PRESENT_PULLS 32

void mcpx_apu_spdif_pull(void)
{
    s.pulls++;
}

bool mcpx_apu_spdif_stream_present(void)
{
    return s.last_valid_pull && s.pulls - s.last_valid_pull < SPDIF_PRESENT_PULLS;
}

void mcpx_apu_spdif_set_decoding(bool on)
{
    if (on && !s.decoding) {
        /* Start from an empty ring: what it holds is from the last time. */
        s.head = s.tail = 0;
    }
    s.decoding = on;
}

void mcpx_apu_spdif_fill_frame(MCPXAPUState *d)
{
    unsigned n = ARRAY_SIZE(d->monitor.surround_buf);
    unsigned avail = (s.head + SPDIF_PCM_FRAMES - s.tail) % SPDIF_PCM_FRAMES;
    if (avail < s.level_min) {
        s.level_min = avail;
    }
    if (avail > s.level_max) {
        s.level_max = avail;
    }
    if (avail < n) {
        s.underruns++;
    }
    for (unsigned i = 0; i < n; i++) {
        if (s.tail == s.head) {
            memset(d->monitor.surround_buf[i], 0,
                   sizeof(d->monitor.surround_buf[i]));
        } else {
            memcpy(d->monitor.surround_buf[i], s.pcm[s.tail],
                   sizeof(d->monitor.surround_buf[i]));
            s.tail = (s.tail + 1) % SPDIF_PCM_FRAMES;
        }
    }
}
