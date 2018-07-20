/*
 * QEMU Geforce NV2A implementation
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2018 Matt Borgerson
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/* PRMVIO - aliases VGA sequencer and graphics controller registers */
uint64_t prmvio_read(void *opaque, hwaddr addr, unsigned int size)
{
    NV2AState *d = opaque;
    uint64_t r = vga_ioport_read(&d->vga, addr);

    reg_log_read(NV_PRMVIO, addr, r);
    return r;
}

void prmvio_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size)
{
    NV2AState *d = opaque;

    reg_log_write(NV_PRMVIO, addr, val);
    vga_ioport_write(&d->vga, addr, val);
}
