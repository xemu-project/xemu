/*
 * QEMU Geforce NV2A implementation
 * PTIMER - time measurement and time-based alarms
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2018-2025 Matt Borgerson
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

#define INVALID_TIMER_VALUE 0xffffffff
#define CLOCK_HIGH_MASK 0x1fffffff
#define CLOCK_LOW_MASK 0x7ffffff

static void ptimer_alarm_fired(void *opaque);

void ptimer_reset(NV2AState *d)
{
    d->ptimer.alarm_time = INVALID_TIMER_VALUE;
    d->ptimer.time_offset = 0;
    timer_del(&d->ptimer.timer);
}

void ptimer_init(NV2AState *d)
{
    timer_init_ns(&d->ptimer.timer, QEMU_CLOCK_VIRTUAL, ptimer_alarm_fired, d);
    ptimer_reset(d);
}

static uint64_t ptimer_get_absolute_clock(NV2AState *d)
{
    return muldiv64(muldiv64(qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL),
                             d->pramdac.core_clock_freq,
                             NANOSECONDS_PER_SECOND),
                    d->ptimer.denominator, d->ptimer.numerator);
}

//! Returns the NV2A time relative to the last time register modification.
static uint64_t ptimer_get_relative_clock(NV2AState *d, uint64_t absolute_clock)
{
    return absolute_clock + d->ptimer.time_offset;
}

static uint64_t ptimer_ticks_to_ns(NV2AState *d, uint64_t ticks)
{
    uint64_t gpu_ticks =
        muldiv64(ticks, d->ptimer.numerator, d->ptimer.denominator);
    return muldiv64(gpu_ticks, NANOSECONDS_PER_SECOND,
                    d->pramdac.core_clock_freq);
}

static void schedule_qemu_timer(NV2AState *d)
{
    if (d->ptimer.alarm_time == INVALID_TIMER_VALUE) {
        timer_del(&d->ptimer.timer);
        return;
    }

    uint64_t now_abs = ptimer_get_absolute_clock(d);
    uint64_t now_guest = ptimer_get_relative_clock(d, now_abs);

    uint64_t alarm_val_shifted = d->ptimer.alarm_time;
    uint64_t alarm_ticks_low = (alarm_val_shifted >> 5) & CLOCK_LOW_MASK;
    uint64_t alarm_ticks_high = d->ptimer.alarm_time_high & CLOCK_HIGH_MASK;

    uint64_t target_guest = (alarm_ticks_high << 27) | alarm_ticks_low;

    if (target_guest <= now_guest) {
        timer_mod(&d->ptimer.timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL));
    } else {
        uint64_t diff_ticks = target_guest - now_guest;
        uint64_t diff_ns = ptimer_ticks_to_ns(d, diff_ticks);

        timer_mod(&d->ptimer.timer,
                  qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + diff_ns);
    }
}

static void ptimer_alarm_fired(void *opaque)
{
    NV2AState *d = (NV2AState *)opaque;

    if (d->ptimer.alarm_time == INVALID_TIMER_VALUE) {
        return;
    }

    uint64_t now = ptimer_get_absolute_clock(d);
    uint64_t guest_clock = ptimer_get_relative_clock(d, now);

    uint32_t current_time_low = (guest_clock & CLOCK_LOW_MASK) << 5;
    uint32_t current_time_high = (guest_clock >> 27) & CLOCK_HIGH_MASK;

    if (d->ptimer.alarm_time <= current_time_low ||
        d->ptimer.alarm_time_high < current_time_high) {
        d->ptimer.pending_interrupts |= NV_PTIMER_INTR_0_ALARM;
        d->ptimer.alarm_time = INVALID_TIMER_VALUE;

        nv2a_update_irq(d);
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
        uint64_t now =
            ptimer_get_relative_clock(d, ptimer_get_absolute_clock(d));
        r = (now & CLOCK_LOW_MASK) << 5;
    } break;
    case NV_PTIMER_TIME_1: {
        uint64_t now =
            ptimer_get_relative_clock(d, ptimer_get_absolute_clock(d));
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
        assert(d->ptimer.alarm_time == INVALID_TIMER_VALUE);
        break;
    case NV_PTIMER_NUMERATOR:
        d->ptimer.numerator = val;
        assert(d->ptimer.alarm_time == INVALID_TIMER_VALUE);
        break;
    case NV_PTIMER_ALARM_0: {
        uint64_t now =
            ptimer_get_relative_clock(d, ptimer_get_absolute_clock(d));
        d->ptimer.alarm_time = val;
        d->ptimer.alarm_time_high = (now >> 27) & CLOCK_HIGH_MASK;

        if (d->ptimer.alarm_time == INVALID_TIMER_VALUE) {
            timer_del(&d->ptimer.timer);
        } else {
            schedule_qemu_timer(d);
        }
    } break;
    case NV_PTIMER_TIME_0: {
        uint64_t absolute_clock = ptimer_get_absolute_clock(d);
        uint64_t guest_clock = ptimer_get_relative_clock(d, absolute_clock);
        uint64_t target_guest =
            (guest_clock & ~CLOCK_LOW_MASK) | ((val >> 5) & CLOCK_LOW_MASK);
        d->ptimer.time_offset = target_guest - absolute_clock;
        if (d->ptimer.alarm_time != INVALID_TIMER_VALUE) {
            schedule_qemu_timer(d);
        }
    } break;
    case NV_PTIMER_TIME_1: {
        uint64_t absolute_clock = ptimer_get_absolute_clock(d);
        uint64_t guest_clock = ptimer_get_relative_clock(d, absolute_clock);
        uint64_t target_guest =
            (guest_clock & CLOCK_LOW_MASK) | ((val & CLOCK_HIGH_MASK) << 27);
        d->ptimer.time_offset = target_guest - absolute_clock;
        if (d->ptimer.alarm_time != INVALID_TIMER_VALUE) {
            schedule_qemu_timer(d);
        }
    } break;
    default:
        break;
    }
}
