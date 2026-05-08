/*
 * QEMU Geforce NV2A implementation
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2018-2024 Matt Borgerson
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

#include "hw/xbox/nv2a/nv2a_int.h"

uint32_t pgraph_rdi_read(PGRAPHState *pg, unsigned int select,
                         unsigned int address)
{
    uint32_t r = 0;
    switch(select) {
    case RDI_INDEX_VTX_CONSTANTS0:
    case RDI_INDEX_VTX_CONSTANTS1:
        assert((address / 4) < NV2A_VERTEXSHADER_CONSTANTS);
        r = pg->vsh_constants[address / 4][3 - address % 4];
        break;
    default:
        fprintf(stderr, "nv2a: unknown rdi read select 0x%x address 0x%x\n",
                select, address);
        assert(false);
        break;
    }
    return r;
}

void pgraph_rdi_write(PGRAPHState *pg, unsigned int select,
                      unsigned int address, uint32_t val)
{
    switch(select) {
    case RDI_INDEX_VTX_CONSTANTS0:
    case RDI_INDEX_VTX_CONSTANTS1:
        assert(false); /* Untested */
        assert((address / 4) < NV2A_VERTEXSHADER_CONSTANTS);
        pg->vsh_constants_dirty[address / 4] |=
            (val != pg->vsh_constants[address / 4][3 - address % 4]);
        pg->vsh_constants[address / 4][3 - address % 4] = val;
        break;
    default:
        NV2A_DPRINTF("unknown rdi write select 0x%x, address 0x%x, val 0x%08x\n",
                     select, address, val);
        break;
    }
}
