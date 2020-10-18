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

/* PRMCIO - aliases VGA CRTC and attribute controller registers */
uint64_t prmcio_read(void *opaque, hwaddr addr, unsigned int size)
{
    NV2AState *d = opaque;
    uint64_t r = vga_ioport_read(&d->vga, addr);

    nv2a_reg_log_read(NV_PRMCIO, addr, r);
    return r;
}

void prmcio_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size)
{
    NV2AState *d = opaque;

    nv2a_reg_log_write(NV_PRMCIO, addr, val);

    switch (addr) {
    case VGA_ATT_W:
        /* Cromwell sets attrs without enabling VGA_AR_ENABLE_DISPLAY
         * (which should result in a blank screen).
         * Either nvidia's hardware is lenient or it is set through
         * something else. The former seems more likely.
         */
        if (d->vga.ar_flip_flop == 0) {
            val |= VGA_AR_ENABLE_DISPLAY;
        }
        break;
    default:
        break;
    }

    vga_ioport_write(&d->vga, addr, val);
}
