/*
 * QEMU Geforce NV2A implementation
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2018-2021 Matt Borgerson
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

#define CLOCK_HIGH_MASK 0x1fffffff
#define CLOCK_LOW_MASK 0x7ffffff

/* PTIMER - time measurement and time-based alarms */
static uint64_t ptimer_get_host_clock(NV2AState *d)
{
    return muldiv64(muldiv64(qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL),
                             d->pramdac.core_clock_freq,
                             NANOSECONDS_PER_SECOND),
                    d->ptimer.denominator,
                    d->ptimer.numerator);
}

static uint64_t ptimer_get_guest_clock(NV2AState *d, uint64_t host_clock)
{
    return host_clock + d->ptimer.time_offset;
}

void ptimer_process_alarm(NV2AState *d)
{
    if (d->ptimer.alarm_time == 0xFFFFFFFF) {
        return;
    }
    uint64_t now = ptimer_get_host_clock(d);
    uint64_t guest_clock = ptimer_get_guest_clock(d, now);

    uint32_t current_time_low = (guest_clock & CLOCK_LOW_MASK) << 5;
    uint32_t current_time_high = (guest_clock >> 27) & CLOCK_HIGH_MASK;

    if (d->ptimer.alarm_time <= current_time_low ||
        d->ptimer.alarm_time_high < current_time_high) {
        d->ptimer.pending_interrupts |= NV_PTIMER_INTR_0_ALARM;
        d->ptimer.alarm_time = 0xFFFFFFFF;
    }
}

uint64_t ptimer_read(void *opaque, hwaddr addr, unsigned int size)
{
    NV2AState *d = opaque;

    uint64_t r = 0;
    switch (addr) {
    case NV_PTIMER_INTR_0:
        r = d->ptimer.pending_interrupts;
        break;
    case NV_PTIMER_INTR_EN_0:
        r = d->ptimer.enabled_interrupts;
        break;
    case NV_PTIMER_NUMERATOR:
        r = d->ptimer.numerator;
        break;
    case NV_PTIMER_DENOMINATOR:
        r = d->ptimer.denominator;
        break;
    case NV_PTIMER_TIME_0: {
        uint64_t now = ptimer_get_guest_clock(d, ptimer_get_host_clock(d));
        r = (now & CLOCK_LOW_MASK) << 5;
    } break;
    case NV_PTIMER_TIME_1: {
        uint64_t now = ptimer_get_guest_clock(d, ptimer_get_host_clock(d));
        r = (now >> 27) & CLOCK_HIGH_MASK;
    } break;
    default:
        break;
    }

    nv2a_reg_log_read(NV_PTIMER, addr, size, r);
    return r;
}

void ptimer_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size)
{
    NV2AState *d = opaque;

    nv2a_reg_log_write(NV_PTIMER, addr, size, val);

    switch (addr) {
    case NV_PTIMER_INTR_0:
        d->ptimer.pending_interrupts &= ~val;
        nv2a_update_irq(d);
        break;
    case NV_PTIMER_INTR_EN_0:
        d->ptimer.enabled_interrupts = val;
        nv2a_update_irq(d);
        break;
    case NV_PTIMER_DENOMINATOR:
        d->ptimer.denominator = val;
        break;
    case NV_PTIMER_NUMERATOR:
        d->ptimer.numerator = val;
        break;
    case NV_PTIMER_ALARM_0: {
        uint64_t now = ptimer_get_guest_clock(d, ptimer_get_host_clock(d));
        d->ptimer.alarm_time = val;
        d->ptimer.alarm_time_high = (now >> 27) & CLOCK_HIGH_MASK;
    } break;
    case NV_PTIMER_TIME_0: {
        uint64_t host_clock = ptimer_get_host_clock(d);
        uint64_t guest_clock = ptimer_get_guest_clock(d, host_clock);
        uint64_t target_guest =
            (guest_clock & ~CLOCK_LOW_MASK) | ((val >> 5) & CLOCK_LOW_MASK);
        d->ptimer.time_offset = target_guest - host_clock;
    } break;
    case NV_PTIMER_TIME_1: {
        uint64_t host_clock = ptimer_get_host_clock(d);
        uint64_t guest_clock = ptimer_get_guest_clock(d, host_clock);
        uint64_t target_guest =
            (guest_clock & CLOCK_LOW_MASK) | ((val & CLOCK_HIGH_MASK) << 27);
        d->ptimer.time_offset = target_guest - host_clock;
    } break;
    default:
        break;
    }
}
