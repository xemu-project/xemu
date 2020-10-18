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

#define DEFINE_STUB(name, region_id) \
    uint64_t name ## _read(void *opaque, \
                           hwaddr addr, \
                           unsigned int size) \
    { \
        nv2a_reg_log_read(region_id, addr, 0); \
        return 0; \
    } \
    void name ## _write(void *opaque, \
                        hwaddr addr, \
                        uint64_t val, \
                        unsigned int size) \
    { \
        nv2a_reg_log_write(region_id, addr, val); \
    } \

DEFINE_STUB(prma, NV_PRMA)
DEFINE_STUB(pcounter, NV_PCOUNTER)
DEFINE_STUB(pvpe, NV_PVPE)
DEFINE_STUB(ptv, NV_PTV)
DEFINE_STUB(prmfb, NV_PRMFB)
DEFINE_STUB(prmdio, NV_PRMDIO)
DEFINE_STUB(pstraps, NV_PSTRAPS)
// DEFINE_STUB(pramin, NV_PRAMIN)

#undef DEFINE_STUB
