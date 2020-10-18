/*
 * QEMU Geforce NV2A implementation
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2018 Matt Borgerson
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

#include "nv2a_int.h"

uint64_t pramdac_read(void *opaque, hwaddr addr, unsigned int size)
{
    NV2AState *d = (NV2AState *)opaque;

    uint64_t r = 0;
    switch (addr & ~3) {
    case NV_PRAMDAC_NVPLL_COEFF:
        r = d->pramdac.core_clock_coeff;
        break;
    case NV_PRAMDAC_MPLL_COEFF:
        r = d->pramdac.memory_clock_coeff;
        break;
    case NV_PRAMDAC_VPLL_COEFF:
        r = d->pramdac.video_clock_coeff;
        break;
    case NV_PRAMDAC_PLL_TEST_COUNTER:
        /* emulated PLLs locked instantly? */
        r = NV_PRAMDAC_PLL_TEST_COUNTER_VPLL2_LOCK
             | NV_PRAMDAC_PLL_TEST_COUNTER_NVPLL_LOCK
             | NV_PRAMDAC_PLL_TEST_COUNTER_MPLL_LOCK
             | NV_PRAMDAC_PLL_TEST_COUNTER_VPLL_LOCK;
        break;
    case NV_PRAMDAC_GENERAL_CONTROL:
        r = d->pramdac.general_control;
        break;
    case NV_PRAMDAC_FP_VDISPLAY_END:
        r = d->pramdac.fp_vdisplay_end;
        break;
    case NV_PRAMDAC_FP_VCRTC:
        r = d->pramdac.fp_vcrtc;
        break;
    case NV_PRAMDAC_FP_VSYNC_END:
        r = d->pramdac.fp_vsync_end;
        break;
    case NV_PRAMDAC_FP_VVALID_END:
        r = d->pramdac.fp_vvalid_end;
        break;
    case NV_PRAMDAC_FP_HDISPLAY_END:
        r = d->pramdac.fp_hdisplay_end;
        break;
    case NV_PRAMDAC_FP_HCRTC:
        r = d->pramdac.fp_hcrtc;
        break;
    case NV_PRAMDAC_FP_HVALID_END:
        r = d->pramdac.fp_hvalid_end;
        break;
    default:
        break;
    }

    /* Surprisingly, QEMU doesn't handle unaligned access for you properly */
    r >>= 32 - 8 * size - 8 * (addr & 3);

    NV2A_DPRINTF("PRAMDAC: read %d [0x%" HWADDR_PRIx "] -> %llx\n", size, addr, r);
    return r;
}

void pramdac_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size)
{
    NV2AState *d = (NV2AState *)opaque;
    uint32_t m, n, p;

    nv2a_reg_log_write(NV_PRAMDAC, addr, val);

    switch (addr) {
    case NV_PRAMDAC_NVPLL_COEFF:
        d->pramdac.core_clock_coeff = val;

        m = val & NV_PRAMDAC_NVPLL_COEFF_MDIV;
        n = (val & NV_PRAMDAC_NVPLL_COEFF_NDIV) >> 8;
        p = (val & NV_PRAMDAC_NVPLL_COEFF_PDIV) >> 16;

        if (m == 0) {
            d->pramdac.core_clock_freq = 0;
        } else {
            d->pramdac.core_clock_freq = (NV2A_CRYSTAL_FREQ * n)
                                          / (1 << p) / m;
        }

        break;
    case NV_PRAMDAC_MPLL_COEFF:
        d->pramdac.memory_clock_coeff = val;
        break;
    case NV_PRAMDAC_VPLL_COEFF:
        d->pramdac.video_clock_coeff = val;
        break;
    case NV_PRAMDAC_GENERAL_CONTROL:
        d->pramdac.general_control = val;
        break;
    case NV_PRAMDAC_FP_VDISPLAY_END:
        d->pramdac.fp_vdisplay_end = val;
        break;
    case NV_PRAMDAC_FP_VCRTC:
        d->pramdac.fp_vcrtc = val;
        break;
    case NV_PRAMDAC_FP_VSYNC_END:
        d->pramdac.fp_vsync_end = val;
        break;
    case NV_PRAMDAC_FP_VVALID_END:
        d->pramdac.fp_vvalid_end = val;
        break;
    case NV_PRAMDAC_FP_HDISPLAY_END:
        d->pramdac.fp_hdisplay_end = val;
        break;
    case NV_PRAMDAC_FP_HCRTC:
        d->pramdac.fp_hcrtc = val;
        break;
    case NV_PRAMDAC_FP_HVALID_END:
        d->pramdac.fp_hvalid_end = val;
        break;
    default:
        break;
    }
}
