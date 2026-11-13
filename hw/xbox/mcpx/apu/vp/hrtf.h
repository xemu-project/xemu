/*
 * HRTF Filter
 *
 * Copyright (c) 2025 Matt Borgerson
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

#ifndef HW_XBOX_MCPX_HRTF_H
#define HW_XBOX_MCPX_HRTF_H

#include <string.h>
#include <stddef.h>
#include <math.h>

#include "hw/xbox/mcpx/apu/apu_regs.h"

#define HRTF_SAMPLES_PER_FRAME  NUM_SAMPLES_PER_FRAME
#define HRTF_NUM_TAPS           31
#define HRTF_MAX_DELAY_SAMPLES  42
#define HRTF_BUFLEN             (HRTF_NUM_TAPS + HRTF_MAX_DELAY_SAMPLES)

typedef struct {
    int buf_pos;
    struct {
        float buf[HRTF_BUFLEN];
        float hrir_coeff_cur[HRTF_NUM_TAPS];
        float hrir_coeff_tar[HRTF_NUM_TAPS];
    } ch[2];
    float itd_cur;
    float itd_tar;
    bool params_set;
} HrtfFilter;

static inline void hrtf_filter_init(HrtfFilter *f)
{
    memset(f, 0, sizeof(*f));
}

static inline void hrtf_filter_clear_history(HrtfFilter *f)
{
    f->buf_pos = 0;
    memset(f->ch[0].buf, 0, sizeof(f->ch[0].buf));
    memset(f->ch[1].buf, 0, sizeof(f->ch[1].buf));
}

static inline void
hrtf_filter_set_target_params(HrtfFilter *f, float hrir_coeff[2][HRTF_NUM_TAPS],
                              float itd)
{
    f->itd_tar =
        fmaxf(-HRTF_MAX_DELAY_SAMPLES, fminf(itd, HRTF_MAX_DELAY_SAMPLES));

    /* Taps apply as written: hardware does not normalize them. */
    for (int ch = 0; ch < 2; ch++) {
        memcpy(f->ch[ch].hrir_coeff_tar, hrir_coeff[ch],
               sizeof(f->ch[ch].hrir_coeff_tar));
    }
}

static inline void hrtf_filter_settle_parameters(HrtfFilter *f)
{
    for (int ch = 0; ch < 2; ch++) {
        memcpy(f->ch[ch].hrir_coeff_cur, f->ch[ch].hrir_coeff_tar,
               sizeof(f->ch[ch].hrir_coeff_cur));
    }
    f->itd_cur = f->itd_tar;
}

static inline void hrtf_filter_process(HrtfFilter *f,
                                       float in[HRTF_SAMPLES_PER_FRAME][2],
                                       float out[HRTF_SAMPLES_PER_FRAME][2])
{
    /* Taps and delay move from the last frame's to the target across one
     * frame (measured); a voice's first frame starts at the target. */
    if (!f->params_set) {
        hrtf_filter_settle_parameters(f);
        f->params_set = true;
    }

    for (int n = 0; n < HRTF_SAMPLES_PER_FRAME; n++) {
        float w = n / (float)HRTF_SAMPLES_PER_FRAME;

        for (int ch = 0; ch < 2; ch++) {
            float *buf = f->ch[ch].buf;
            float *coeff_cur = f->ch[ch].hrir_coeff_cur;
            float *coeff_tar = f->ch[ch].hrir_coeff_tar;

            // Push new sample
            buf[f->buf_pos] = in[n][ch];

            // Interaural time difference (channel delay)
            float itd = f->itd_cur + (f->itd_tar - f->itd_cur) * w;
            float d = itd * (ch == 0 ? +1.0f : -1.0f);
            if (d < 0.0f) {
                d = 0.0f;
            }
            int di = d;
            float dfrac = d - di;

            // HRIR Convolution
            float acc = 0.0f;
            for (int k = 0; k < HRTF_NUM_TAPS; k++) {
                int idx1 = (f->buf_pos - di - k + HRTF_BUFLEN) % HRTF_BUFLEN;
                float s = buf[idx1];

                // Linear interpolation for fractional part
                if (dfrac > 0.0f) {
                    int idx2 = (idx1 - 1 + HRTF_BUFLEN) % HRTF_BUFLEN;
                    s = s * (1 - dfrac) + buf[idx2] * dfrac;
                }
                acc += (coeff_cur[k] + (coeff_tar[k] - coeff_cur[k]) * w) * s;
            }

            out[n][ch] = acc;
        }

        f->buf_pos = (f->buf_pos + 1) % HRTF_BUFLEN;
    }

    hrtf_filter_settle_parameters(f);
}

#endif
