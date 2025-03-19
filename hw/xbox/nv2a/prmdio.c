/*
 * QEMU Geforce NV2A implementation
 *
 * Copyright (c) 2021 Matt Borgerson
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

uint64_t prmdio_read(void *opaque, hwaddr addr, unsigned int size)
{
    NV2AState *d = (NV2AState *)opaque;

    uint64_t r = 0;
    switch (addr) {
    case NV_USER_DAC_WRITE_MODE_ADDRESS:
        r = d->puserdac.write_mode_address / 3;
        break;
    default:
        break;
    }

    nv2a_reg_log_read(NV_PRMDIO, addr, size, r);
    return r;
}

void prmdio_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size)
{
    NV2AState *d = (NV2AState *)opaque;

    nv2a_reg_log_write(NV_PRMDIO, addr, size, val);

    switch (addr) {
    case NV_USER_DAC_WRITE_MODE_ADDRESS:
        d->puserdac.write_mode_address = (val & 0xff) * 3;
        break;
    case NV_USER_DAC_PALETTE_DATA:
        /* FIXME: Confirm wrap-around */
        d->puserdac.palette[d->puserdac.write_mode_address++ % (256*3)] = val;
        break;
    default:
        break;
    }
}
