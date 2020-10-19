/*
 * QEMU Geforce NV2A implementation
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2018-2020 Matt Borgerson
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

typedef struct RAMHTEntry {
    uint32_t handle;
    hwaddr instance;
    enum FIFOEngine engine;
    unsigned int channel_id : 5;
    bool valid;
} RAMHTEntry;

static void pfifo_run_pusher(NV2AState *d);
static uint32_t ramht_hash(NV2AState *d, uint32_t handle);
static RAMHTEntry ramht_lookup(NV2AState *d, uint32_t handle);

/* PFIFO - MMIO and DMA FIFO submission to PGRAPH and VPE */
uint64_t pfifo_read(void *opaque, hwaddr addr, unsigned int size)
{
    NV2AState *d = (NV2AState *)opaque;

    qemu_mutex_lock(&d->pfifo.lock);

    uint64_t r = 0;
    switch (addr) {
    case NV_PFIFO_INTR_0:
        r = d->pfifo.pending_interrupts;
        break;
    case NV_PFIFO_INTR_EN_0:
        r = d->pfifo.enabled_interrupts;
        break;
    case NV_PFIFO_RUNOUT_STATUS:
        r = NV_PFIFO_RUNOUT_STATUS_LOW_MARK; /* low mark empty */
        break;
    default:
        r = d->pfifo.regs[addr];
        break;
    }

    qemu_mutex_unlock(&d->pfifo.lock);

    nv2a_reg_log_read(NV_PFIFO, addr, r);
    return r;
}

void pfifo_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size)
{
    NV2AState *d = (NV2AState *)opaque;

    nv2a_reg_log_write(NV_PFIFO, addr, val);

    qemu_mutex_lock(&d->pfifo.lock);

    switch (addr) {
    case NV_PFIFO_INTR_0:
        d->pfifo.pending_interrupts &= ~val;
        nv2a_update_irq(d);
        break;
    case NV_PFIFO_INTR_EN_0:
        d->pfifo.enabled_interrupts = val;
        nv2a_update_irq(d);
        break;
    default:
        d->pfifo.regs[addr] = val;
        break;
    }

    pfifo_kick(d);

    qemu_mutex_unlock(&d->pfifo.lock);
}

void pfifo_kick(NV2AState *d)
{
    d->pfifo.fifo_kick = true;
    qemu_cond_broadcast(&d->pfifo.fifo_cond);
}

static int pgraph_can_fifo_access(NV2AState *d) {
    return !!(d->pgraph.regs[NV_PGRAPH_FIFO] & NV_PGRAPH_FIFO_ACCESS);
}

/* If NV097_FLIP_STALL was executed, check if the flip has completed.
 * This will usually happen in the VSYNC interrupt handler.
 */
static int pgraph_is_flip_stall_complete(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;

    NV2A_DPRINTF("flip stall read: %d, write: %d, modulo: %d\n",
        GET_MASK(pg->regs[NV_PGRAPH_SURFACE], NV_PGRAPH_SURFACE_READ_3D),
        GET_MASK(pg->regs[NV_PGRAPH_SURFACE], NV_PGRAPH_SURFACE_WRITE_3D),
        GET_MASK(pg->regs[NV_PGRAPH_SURFACE], NV_PGRAPH_SURFACE_MODULO_3D));

    uint32_t s = pg->regs[NV_PGRAPH_SURFACE];
    if (GET_MASK(s, NV_PGRAPH_SURFACE_READ_3D)
        != GET_MASK(s, NV_PGRAPH_SURFACE_WRITE_3D)) {
        return 1;
    }

    return 0;
}

static bool pfifo_stall_for_flip(NV2AState *d)
{
    bool should_stall = false;

    if (atomic_read(&d->pgraph.waiting_for_flip)) {
        qemu_mutex_lock(&d->pgraph.lock);
        if (!pgraph_is_flip_stall_complete(d)) {
            should_stall = true;
        } else {
            d->pgraph.waiting_for_flip = false;
        }
        qemu_mutex_unlock(&d->pgraph.lock);
    }

    return should_stall;
}

static void pfifo_run_puller(NV2AState *d)
{
    if (pfifo_stall_for_flip(d)) return;

    // Conditions cleared by pgraph_write
    if (atomic_read(&d->pgraph.waiting_for_nop) ||
        atomic_read(&d->pgraph.waiting_for_context_switch)) {
        // Wait for events
        return;
    }

    uint32_t *pull0 = &d->pfifo.regs[NV_PFIFO_CACHE1_PULL0];
    uint32_t *pull1 = &d->pfifo.regs[NV_PFIFO_CACHE1_PULL1];
    uint32_t *engine_reg = &d->pfifo.regs[NV_PFIFO_CACHE1_ENGINE];

    uint32_t *status = &d->pfifo.regs[NV_PFIFO_CACHE1_STATUS];
    uint32_t *get_reg = &d->pfifo.regs[NV_PFIFO_CACHE1_GET];
    uint32_t *put_reg = &d->pfifo.regs[NV_PFIFO_CACHE1_PUT];

    // TODO
    // CacheEntry working_cache[NV2A_CACHE1_SIZE];
    // int working_cache_size = 0;
    // pull everything into our own queue

    // TODO think more about locking

    while (true) {
        if (!GET_MASK(*pull0, NV_PFIFO_CACHE1_PULL0_ACCESS)) return;

        /* empty cache1 */
        if (*status & NV_PFIFO_CACHE1_STATUS_LOW_MARK) break;

        uint32_t get = *get_reg;
        uint32_t put = *put_reg;

        assert(get < 128*4 && (get % 4) == 0);
        uint32_t method_entry = d->pfifo.regs[NV_PFIFO_CACHE1_METHOD + get*2];
        uint32_t parameter = d->pfifo.regs[NV_PFIFO_CACHE1_DATA + get*2];
        uint32_t method = method_entry & 0x1FFC;
        uint32_t subchannel = GET_MASK(method_entry, NV_PFIFO_CACHE1_METHOD_SUBCHANNEL);

        // NV2A_DPRINTF("pull %d 0x%x 0x%x - subch %d\n", get/4, method_entry, parameter, subchannel);

        if (method == 0) {
            RAMHTEntry entry = ramht_lookup(d, parameter);
            assert(entry.valid);

            // assert(entry.channel_id == state->channel_id);

            assert(entry.engine == ENGINE_GRAPHICS);


            /* the engine is bound to the subchannel */
            assert(subchannel < 8);
            SET_MASK(*engine_reg, 3 << (4*subchannel), entry.engine);
            SET_MASK(*pull1, NV_PFIFO_CACHE1_PULL1_ENGINE, entry.engine);
            // NV2A_DPRINTF("engine_reg1 %d 0x%x\n", subchannel, *engine_reg);

            // TODO: this is fucked
            qemu_mutex_unlock(&d->pfifo.lock);
            qemu_mutex_lock(&d->pgraph.lock);
            //make pgraph busy

            if (!pgraph_can_fifo_access(d)) {
                qemu_mutex_unlock(&d->pgraph.lock);
                qemu_mutex_lock(&d->pfifo.lock);
                // Wait for event
                return;
            }

            // Switch contexts if necessary
            pgraph_context_switch(d, entry.channel_id);
            if (d->pgraph.waiting_for_context_switch) {
                qemu_mutex_unlock(&d->pgraph.lock);
                qemu_mutex_lock(&d->pfifo.lock);
                // Wait for event
                return;
            }
            pgraph_method(d, subchannel, 0, entry.instance);

            // make pgraph not busy
            qemu_mutex_unlock(&d->pgraph.lock);
            qemu_mutex_lock(&d->pfifo.lock);

        } else if (method >= 0x100) {
            // method passed to engine

            /* methods that take objects.
             * TODO: Check this range is correct for the nv2a */
            if (method >= 0x180 && method < 0x200) {
                //qemu_mutex_lock_iothread();
                RAMHTEntry entry = ramht_lookup(d, parameter);
                assert(entry.valid);
                // assert(entry.channel_id == state->channel_id);
                parameter = entry.instance;
                //qemu_mutex_unlock_iothread();
            }

            enum FIFOEngine engine = GET_MASK(*engine_reg, 3 << (4*subchannel));
            // NV2A_DPRINTF("engine_reg2 %d 0x%x\n", subchannel, *engine_reg);
            assert(engine == ENGINE_GRAPHICS);
            SET_MASK(*pull1, NV_PFIFO_CACHE1_PULL1_ENGINE, engine);

            // TODO: this is fucked
            qemu_mutex_unlock(&d->pfifo.lock);
            qemu_mutex_lock(&d->pgraph.lock);
            //make pgraph busy

            if (!pgraph_can_fifo_access(d)) {
                qemu_mutex_unlock(&d->pgraph.lock);
                qemu_mutex_lock(&d->pfifo.lock);
                // Wait for event
                return;
            }
            d->pgraph.waiting_for_fifo_access = false;
            pgraph_method(d, subchannel, method, parameter);

            // make pgraph not busy
            qemu_mutex_unlock(&d->pgraph.lock);
            qemu_mutex_lock(&d->pfifo.lock);
        } else {
            assert(false);
        }

        // Advance now that the method has executed
        uint32_t new_get = (get+4) & 0x1fc;
        *get_reg = new_get;

        if (new_get == put) {
            // set low mark
            *status |= NV_PFIFO_CACHE1_STATUS_LOW_MARK;
            d->pfifo.fifo_kick = true;
        }
        if (*status & NV_PFIFO_CACHE1_STATUS_HIGH_MARK) {
            // unset high mark
            *status &= ~NV_PFIFO_CACHE1_STATUS_HIGH_MARK;
            d->pfifo.fifo_kick = true;
        }

        // If the condition is already satisfied, in which case do not attempt to stall
        if (pfifo_stall_for_flip(d)) return;

        if (atomic_read(&d->pgraph.waiting_for_nop)) {
            // Wait for event
            return;
        }
    }
}

static void pfifo_run_pusher(NV2AState *d)
{
    uint32_t *push0 = &d->pfifo.regs[NV_PFIFO_CACHE1_PUSH0];
    uint32_t *push1 = &d->pfifo.regs[NV_PFIFO_CACHE1_PUSH1];
    uint32_t *dma_subroutine = &d->pfifo.regs[NV_PFIFO_CACHE1_DMA_SUBROUTINE];
    uint32_t *dma_state = &d->pfifo.regs[NV_PFIFO_CACHE1_DMA_STATE];
    uint32_t *dma_push = &d->pfifo.regs[NV_PFIFO_CACHE1_DMA_PUSH];
    uint32_t *dma_get = &d->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET];
    uint32_t *dma_put = &d->pfifo.regs[NV_PFIFO_CACHE1_DMA_PUT];
    uint32_t *dma_dcount = &d->pfifo.regs[NV_PFIFO_CACHE1_DMA_DCOUNT];

    uint32_t *status = &d->pfifo.regs[NV_PFIFO_CACHE1_STATUS];
    uint32_t *get_reg = &d->pfifo.regs[NV_PFIFO_CACHE1_GET];
    uint32_t *put_reg = &d->pfifo.regs[NV_PFIFO_CACHE1_PUT];

    if (!GET_MASK(*push0, NV_PFIFO_CACHE1_PUSH0_ACCESS)) {
        return;
    }
    if (!GET_MASK(*dma_push, NV_PFIFO_CACHE1_DMA_PUSH_ACCESS)) {
        return;
    }

    /* suspended */
    if (GET_MASK(*dma_push, NV_PFIFO_CACHE1_DMA_PUSH_STATUS)) {
        return;
    }

    // TODO: should we become busy here??
    // NV_PFIFO_CACHE1_DMA_PUSH_STATE _BUSY

    unsigned int channel_id = GET_MASK(*push1,
                                       NV_PFIFO_CACHE1_PUSH1_CHID);


    /* Channel running DMA mode */
    uint32_t channel_modes = d->pfifo.regs[NV_PFIFO_MODE];
    assert(channel_modes & (1 << channel_id));

    assert(GET_MASK(*push1, NV_PFIFO_CACHE1_PUSH1_MODE)
            == NV_PFIFO_CACHE1_PUSH1_MODE_DMA);

    /* We're running so there should be no pending errors... */
    assert(GET_MASK(*dma_state, NV_PFIFO_CACHE1_DMA_STATE_ERROR)
            == NV_PFIFO_CACHE1_DMA_STATE_ERROR_NONE);

    hwaddr dma_instance =
        GET_MASK(d->pfifo.regs[NV_PFIFO_CACHE1_DMA_INSTANCE],
                 NV_PFIFO_CACHE1_DMA_INSTANCE_ADDRESS) << 4;

    hwaddr dma_len;
    uint8_t *dma = nv_dma_map(d, dma_instance, &dma_len);

    while (true) {
        uint32_t dma_get_v = *dma_get;
        uint32_t dma_put_v = *dma_put;
        if (dma_get_v == dma_put_v) break;
        if (dma_get_v >= dma_len) {
            assert(false);
            SET_MASK(*dma_state, NV_PFIFO_CACHE1_DMA_STATE_ERROR,
                     NV_PFIFO_CACHE1_DMA_STATE_ERROR_PROTECTION);
            break;
        }

        uint32_t word = ldl_le_p((uint32_t*)(dma + dma_get_v));
        dma_get_v += 4;

        uint32_t method_type =
            GET_MASK(*dma_state, NV_PFIFO_CACHE1_DMA_STATE_METHOD_TYPE);
        uint32_t method_subchannel =
            GET_MASK(*dma_state, NV_PFIFO_CACHE1_DMA_STATE_SUBCHANNEL);
        uint32_t method =
            GET_MASK(*dma_state, NV_PFIFO_CACHE1_DMA_STATE_METHOD) << 2;
        uint32_t method_count =
            GET_MASK(*dma_state, NV_PFIFO_CACHE1_DMA_STATE_METHOD_COUNT);

        uint32_t subroutine_state =
            GET_MASK(*dma_subroutine, NV_PFIFO_CACHE1_DMA_SUBROUTINE_STATE);

        if (method_count) {
            /* full */
            if (*status & NV_PFIFO_CACHE1_STATUS_HIGH_MARK) return;


            /* data word of methods command */
            d->pfifo.regs[NV_PFIFO_CACHE1_DMA_DATA_SHADOW] = word;

            uint32_t put = *put_reg;
            uint32_t get = *get_reg;

            assert((method & 3) == 0);
            uint32_t method_entry = 0;
            SET_MASK(method_entry, NV_PFIFO_CACHE1_METHOD_ADDRESS, method >> 2);
            SET_MASK(method_entry, NV_PFIFO_CACHE1_METHOD_TYPE, method_type);
            SET_MASK(method_entry, NV_PFIFO_CACHE1_METHOD_SUBCHANNEL, method_subchannel);

            // NV2A_DPRINTF("push %d 0x%x 0x%x - subch %d\n", put/4, method_entry, word, method_subchannel);

            assert(put < 128*4 && (put%4) == 0);
            d->pfifo.regs[NV_PFIFO_CACHE1_METHOD + put*2] = method_entry;
            d->pfifo.regs[NV_PFIFO_CACHE1_DATA + put*2] = word;

            uint32_t new_put = (put+4) & 0x1fc;
            *put_reg = new_put;
            if (new_put == get) {
                // set high mark
                *status |= NV_PFIFO_CACHE1_STATUS_HIGH_MARK;
            }
            if (*status & NV_PFIFO_CACHE1_STATUS_LOW_MARK) {
                // unset low mark
                *status &= ~NV_PFIFO_CACHE1_STATUS_LOW_MARK;
            }

            if (method_type == NV_PFIFO_CACHE1_DMA_STATE_METHOD_TYPE_INC) {
                SET_MASK(*dma_state, NV_PFIFO_CACHE1_DMA_STATE_METHOD,
                         (method + 4) >> 2);
            }
            SET_MASK(*dma_state, NV_PFIFO_CACHE1_DMA_STATE_METHOD_COUNT,
                     method_count - 1);
            (*dma_dcount)++;
        } else {
            /* no command active - this is the first word of a new one */
            d->pfifo.regs[NV_PFIFO_CACHE1_DMA_RSVD_SHADOW] = word;

            /* match all forms */
            if ((word & 0xe0000003) == 0x20000000) {
                /* old jump */
                d->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET_JMP_SHADOW] =
                    dma_get_v;
                dma_get_v = word & 0x1fffffff;
                NV2A_DPRINTF("pb OLD_JMP 0x%x\n", dma_get_v);
            } else if ((word & 3) == 1) {
                /* jump */
                d->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET_JMP_SHADOW] =
                    dma_get_v;
                dma_get_v = word & 0xfffffffc;
                NV2A_DPRINTF("pb JMP 0x%x\n", dma_get_v);
            } else if ((word & 3) == 2) {
                /* call */
                if (subroutine_state) {
                    SET_MASK(*dma_state, NV_PFIFO_CACHE1_DMA_STATE_ERROR,
                             NV_PFIFO_CACHE1_DMA_STATE_ERROR_CALL);
                    break;
                } else {
                    *dma_subroutine = dma_get_v;
                    SET_MASK(*dma_subroutine,
                             NV_PFIFO_CACHE1_DMA_SUBROUTINE_STATE, 1);
                    dma_get_v = word & 0xfffffffc;
                    NV2A_DPRINTF("pb CALL 0x%x\n", dma_get_v);
                }
            } else if (word == 0x00020000) {
                /* return */
                if (!subroutine_state) {
                    SET_MASK(*dma_state, NV_PFIFO_CACHE1_DMA_STATE_ERROR,
                             NV_PFIFO_CACHE1_DMA_STATE_ERROR_RETURN);
                    // break;
                } else {
                    dma_get_v = *dma_subroutine & 0xfffffffc;
                    SET_MASK(*dma_subroutine,
                             NV_PFIFO_CACHE1_DMA_SUBROUTINE_STATE, 0);
                    NV2A_DPRINTF("pb RET 0x%x\n", dma_get_v);
                }
            } else if ((word & 0xe0030003) == 0) {
                /* increasing methods */
                SET_MASK(*dma_state, NV_PFIFO_CACHE1_DMA_STATE_METHOD,
                         (word & 0x1fff) >> 2 );
                SET_MASK(*dma_state, NV_PFIFO_CACHE1_DMA_STATE_SUBCHANNEL,
                         (word >> 13) & 7);
                SET_MASK(*dma_state, NV_PFIFO_CACHE1_DMA_STATE_METHOD_COUNT,
                         (word >> 18) & 0x7ff);
                SET_MASK(*dma_state, NV_PFIFO_CACHE1_DMA_STATE_METHOD_TYPE,
                         NV_PFIFO_CACHE1_DMA_STATE_METHOD_TYPE_INC);
                *dma_dcount = 0;
            } else if ((word & 0xe0030003) == 0x40000000) {
                /* non-increasing methods */
                SET_MASK(*dma_state, NV_PFIFO_CACHE1_DMA_STATE_METHOD,
                         (word & 0x1fff) >> 2 );
                SET_MASK(*dma_state, NV_PFIFO_CACHE1_DMA_STATE_SUBCHANNEL,
                         (word >> 13) & 7);
                SET_MASK(*dma_state, NV_PFIFO_CACHE1_DMA_STATE_METHOD_COUNT,
                         (word >> 18) & 0x7ff);
                SET_MASK(*dma_state, NV_PFIFO_CACHE1_DMA_STATE_METHOD_TYPE,
                         NV_PFIFO_CACHE1_DMA_STATE_METHOD_TYPE_NON_INC);
                *dma_dcount = 0;
            } else {
                NV2A_DPRINTF("pb reserved cmd 0x%x - 0x%x\n",
                             dma_get_v, word);
                SET_MASK(*dma_state, NV_PFIFO_CACHE1_DMA_STATE_ERROR,
                         NV_PFIFO_CACHE1_DMA_STATE_ERROR_RESERVED_CMD);
                // break;
                assert(false);
            }
        }

        *dma_get = dma_get_v;

        if (GET_MASK(*dma_state, NV_PFIFO_CACHE1_DMA_STATE_ERROR)) {
            break;
        }
    }

    // NV2A_DPRINTF("DMA pusher done: max 0x%" HWADDR_PRIx ", 0x%" HWADDR_PRIx " - 0x%" HWADDR_PRIx "\n",
    //      dma_len, control->dma_get, control->dma_put);

    uint32_t error = GET_MASK(*dma_state, NV_PFIFO_CACHE1_DMA_STATE_ERROR);
    if (error) {
        NV2A_DPRINTF("pb error: %d\n", error);
        assert(false);

        SET_MASK(*dma_push, NV_PFIFO_CACHE1_DMA_PUSH_STATUS, 1); /* suspended */

        // d->pfifo.pending_interrupts |= NV_PFIFO_INTR_0_DMA_PUSHER;
        // nv2a_update_irq(d);
    }
}

void *pfifo_thread(void *arg)
{
    NV2AState *d = (NV2AState *)arg;
    glo_set_current(g_nv2a_context_render);

    rcu_register_thread();

    qemu_mutex_lock(&d->pfifo.lock);
    while (true) {
        d->pfifo.fifo_kick = false;

        if (atomic_read(&d->pgraph.downloads_pending)) {
            pgraph_process_pending_downloads(d);
            atomic_set(&d->pgraph.downloads_pending, false);
        }

        if (atomic_read(&d->pgraph.gl_sync_pending)) {
            pgraph_gl_sync(d);
            atomic_set(&d->pgraph.gl_sync_pending, false);
        }

        pfifo_run_pusher(d);
        pfifo_run_puller(d);

        if (!d->pfifo.fifo_kick) {
            qemu_cond_broadcast(&d->pfifo.fifo_idle_cond);

            // Both the pusher and puller are waiting for some action
            qemu_cond_wait(&d->pfifo.fifo_cond, &d->pfifo.lock);
        }

        if (d->exiting) {
            break;
        }
    }
    qemu_mutex_unlock(&d->pfifo.lock);

    rcu_unregister_thread();

    return NULL;
}

static uint32_t ramht_hash(NV2AState *d, uint32_t handle)
{
    unsigned int ramht_size =
        1 << (GET_MASK(d->pfifo.regs[NV_PFIFO_RAMHT], NV_PFIFO_RAMHT_SIZE)+12);

    /* XXX: Think this is different to what nouveau calculates... */
    unsigned int bits = ctz32(ramht_size)-1;

    uint32_t hash = 0;
    while (handle) {
        hash ^= (handle & ((1 << bits) - 1));
        handle >>= bits;
    }

    unsigned int channel_id = GET_MASK(d->pfifo.regs[NV_PFIFO_CACHE1_PUSH1],
                                       NV_PFIFO_CACHE1_PUSH1_CHID);
    hash ^= channel_id << (bits - 4);

    return hash;
}


static RAMHTEntry ramht_lookup(NV2AState *d, uint32_t handle)
{
    hwaddr ramht_size =
        1 << (GET_MASK(d->pfifo.regs[NV_PFIFO_RAMHT], NV_PFIFO_RAMHT_SIZE)+12);

    uint32_t hash = ramht_hash(d, handle);
    assert(hash * 8 < ramht_size);

    hwaddr ramht_address =
        GET_MASK(d->pfifo.regs[NV_PFIFO_RAMHT],
                 NV_PFIFO_RAMHT_BASE_ADDRESS) << 12;

    assert(ramht_address + hash * 8 < memory_region_size(&d->ramin));

    uint8_t *entry_ptr = d->ramin_ptr + ramht_address + hash * 8;

    uint32_t entry_handle = ldl_le_p((uint32_t*)entry_ptr);
    uint32_t entry_context = ldl_le_p((uint32_t*)(entry_ptr + 4));

    return (RAMHTEntry){
        .handle = entry_handle,
        .instance = (entry_context & NV_RAMHT_INSTANCE) << 4,
        .engine = (entry_context & NV_RAMHT_ENGINE) >> 16,
        .channel_id = (entry_context & NV_RAMHT_CHID) >> 24,
        .valid = entry_context & NV_RAMHT_STATUS,
    };
}
