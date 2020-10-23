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

extern SurfaceBinding *pgraph_surface_get(NV2AState *d, hwaddr addr);

uint64_t pcrtc_read(void *opaque, hwaddr addr, unsigned int size)
{
    NV2AState *d = (NV2AState *)opaque;

    uint64_t r = 0;
    switch (addr) {
        case NV_PCRTC_INTR_0:
            r = d->pcrtc.pending_interrupts;
            break;
        case NV_PCRTC_INTR_EN_0:
            r = d->pcrtc.enabled_interrupts;
            break;
        case NV_PCRTC_START:
            r = d->pcrtc.start;
            break;
        case NV_PCRTC_RASTER: {
            static int stage = 0;
            SurfaceBinding *surface = pgraph_surface_get(d, d->pcrtc.start);
            if (surface != NULL) {
                // HACK: Alternates between 0, mid-frame, and end-of-frame, enough to keep Alter Echo happy
                // FIXME: This should return the scanline currently being scanned out to the display, including blanking lines
                switch (stage++) {
                    case 0: r = 0; break; // Start of frame
                    case 1: r = surface->height / 2; break; // Mid-frame
                    case 2: r = surface->height + 1; break; // In VBlank period
                }

                if (stage > 2) {
                    stage = 0;
                }
            }
            break;
        }
        default:
            break;
    }

    nv2a_reg_log_read(NV_PCRTC, addr, r);
    return r;
}

void pcrtc_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size)
{
    NV2AState *d = (NV2AState *)opaque;

    nv2a_reg_log_write(NV_PCRTC, addr, val);

    switch (addr) {
    case NV_PCRTC_INTR_0:
        d->pcrtc.pending_interrupts &= ~val;
        nv2a_update_irq(d);
        break;
    case NV_PCRTC_INTR_EN_0:
        d->pcrtc.enabled_interrupts = val;
        nv2a_update_irq(d);
        break;
    case NV_PCRTC_START:
        val &= 0x07FFFFFF;
        // assert(val < memory_region_size(d->vram));
        d->pcrtc.start = val;

        NV2A_DPRINTF("PCRTC_START - %x %x %x %x\n",
                d->vram_ptr[val+64], d->vram_ptr[val+64+1],
                d->vram_ptr[val+64+2], d->vram_ptr[val+64+3]);
        break;
    default:
        break;
    }
}
