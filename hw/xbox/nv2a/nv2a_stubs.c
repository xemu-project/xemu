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

uint64_t prma_read(void *opaque,
                                  hwaddr addr, unsigned int size)
{
    reg_log_read(NV_PRMA, addr, 0);
    return 0;
}
void prma_write(void *opaque, hwaddr addr,
                               uint64_t val, unsigned int size)
{
    reg_log_write(NV_PRMA, addr, val);
}

uint64_t pcounter_read(void *opaque,
                                  hwaddr addr, unsigned int size)
{
    reg_log_read(NV_PCOUNTER, addr, 0);
    return 0;
}
void pcounter_write(void *opaque, hwaddr addr,
                               uint64_t val, unsigned int size)
{
    reg_log_write(NV_PCOUNTER, addr, val);
}

uint64_t pvpe_read(void *opaque,
                                  hwaddr addr, unsigned int size)
{
    reg_log_read(NV_PVPE, addr, 0);
    return 0;
}
void pvpe_write(void *opaque, hwaddr addr,
                               uint64_t val, unsigned int size)
{
    reg_log_write(NV_PVPE, addr, val);
}

uint64_t ptv_read(void *opaque,
                                  hwaddr addr, unsigned int size)
{
    reg_log_read(NV_PTV, addr, 0);
    return 0;
}
void ptv_write(void *opaque, hwaddr addr,
                               uint64_t val, unsigned int size)
{
    reg_log_write(NV_PTV, addr, val);
}

uint64_t prmfb_read(void *opaque,
                                  hwaddr addr, unsigned int size)
{
    reg_log_read(NV_PRMFB, addr, 0);
    return 0;
}
void prmfb_write(void *opaque, hwaddr addr,
                               uint64_t val, unsigned int size)
{
    reg_log_write(NV_PRMFB, addr, val);
}

uint64_t prmdio_read(void *opaque,
                                  hwaddr addr, unsigned int size)
{
    reg_log_read(NV_PRMDIO, addr, 0);
    return 0;
}
void prmdio_write(void *opaque, hwaddr addr,
                               uint64_t val, unsigned int size)
{
    reg_log_write(NV_PRMDIO, addr, val);
}

uint64_t pstraps_read(void *opaque,
                                  hwaddr addr, unsigned int size)
{
    reg_log_read(NV_PSTRAPS, addr, 0);
    return 0;
}
void pstraps_write(void *opaque, hwaddr addr,
                               uint64_t val, unsigned int size)
{
    reg_log_write(NV_PSTRAPS, addr, val);
}

/* PRAMIN - RAMIN access */
/*
uint64_t pramin_read(void *opaque,
                                 hwaddr addr, unsigned int size)
{
    NV2A_DPRINTF("nv2a PRAMIN: read [0x%" HWADDR_PRIx "] -> 0x%" HWADDR_PRIx "\n", addr, r);
    return 0;
}
void pramin_write(void *opaque, hwaddr addr,
                              uint64_t val, unsigned int size)
{
    NV2A_DPRINTF("nv2a PRAMIN: [0x%" HWADDR_PRIx "] = 0x%02llx\n", addr, val);
}*/
