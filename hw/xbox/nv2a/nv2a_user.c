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

/* USER - PFIFO MMIO and DMA submission area */
uint64_t user_read(void *opaque, hwaddr addr, unsigned int size)
{
    NV2AState *d = (NV2AState *)opaque;

    unsigned int channel_id = addr >> 16;
    assert(channel_id < NV2A_NUM_CHANNELS);

    qemu_mutex_lock(&d->pfifo.lock);

    uint32_t channel_modes = d->pfifo.regs[NV_PFIFO_MODE];

    uint64_t r = 0;
    if (channel_modes & (1 << channel_id)) {
        /* DMA Mode */

        unsigned int cur_channel_id =
            GET_MASK(d->pfifo.regs[NV_PFIFO_CACHE1_PUSH1],
                     NV_PFIFO_CACHE1_PUSH1_CHID);

        if (channel_id == cur_channel_id) {
            switch (addr & 0xFFFF) {
            case NV_USER_DMA_PUT:
                r = d->pfifo.regs[NV_PFIFO_CACHE1_DMA_PUT];
                break;
            case NV_USER_DMA_GET:
                r = d->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET];
                break;
            case NV_USER_REF:
                r = d->pfifo.regs[NV_PFIFO_CACHE1_REF];
                break;
            default:
                break;
            }
        } else {
            /* ramfc */
            assert(false);
        }
    } else {
        /* PIO Mode */
        assert(false);
    }

    qemu_mutex_unlock(&d->pfifo.lock);

    nv2a_reg_log_read(NV_USER, addr, r);
    return r;
}

void user_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size)
{
    NV2AState *d = (NV2AState *)opaque;

    nv2a_reg_log_write(NV_USER, addr, val);

    unsigned int channel_id = addr >> 16;
    assert(channel_id < NV2A_NUM_CHANNELS);

    qemu_mutex_lock(&d->pfifo.lock);

    uint32_t channel_modes = d->pfifo.regs[NV_PFIFO_MODE];
    if (channel_modes & (1 << channel_id)) {
        /* DMA Mode */
        unsigned int cur_channel_id =
            GET_MASK(d->pfifo.regs[NV_PFIFO_CACHE1_PUSH1],
                     NV_PFIFO_CACHE1_PUSH1_CHID);

        if (channel_id == cur_channel_id) {
            switch (addr & 0xFFFF) {
            case NV_USER_DMA_PUT:
                d->pfifo.regs[NV_PFIFO_CACHE1_DMA_PUT] = val;
                break;
            case NV_USER_DMA_GET:
                d->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET] = val;
                break;
            case NV_USER_REF:
                d->pfifo.regs[NV_PFIFO_CACHE1_REF] = val;
                break;
            default:
                assert(false);
                break;
            }

            pfifo_kick(d);

        } else {
            /* ramfc */
            assert(false);
        }
    } else {
        /* PIO Mode */
        assert(false);
    }

    qemu_mutex_unlock(&d->pfifo.lock);

}
