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

#define CLOCK_HIGH_MASK 0x1fffffffULL
#define ALARM_MASK 0xffffffe0ULL

#define PTIMER_REG_TIME_HIGH_MASK ((uint64_t)CLOCK_HIGH_MASK << 32)
#define PTIMER_REG_TIME_LOW_MASK 0xffffffffULL
#define PTIMER_REG_TIME_MASK (PTIMER_REG_TIME_HIGH_MASK | ALARM_MASK)

#define PTIMER_INTERNAL_TIME_MASK (PTIMER_REG_TIME_MASK >> 5)

#define PTIMER_INTERNAL_TO_REG_TIME(internal_ticks) \
    (((uint64_t)(internal_ticks) << 5) & PTIMER_REG_TIME_MASK)

#define PTIMER_REG_TO_INTERNAL_TIME(reg_time) \
    (((uint64_t)(reg_time) >> 5) & PTIMER_INTERNAL_TIME_MASK)

#define PTIMER_MAKE_REG_TIME(time_1, time_0)          \
    ((((uint64_t)(time_1) & CLOCK_HIGH_MASK) << 32) | \
     ((uint64_t)(time_0) & PTIMER_REG_TIME_LOW_MASK))

#define PTIMER_REG_TIME_GET_TIME_0(reg_time) \
    ((uint32_t)((reg_time) & PTIMER_REG_TIME_LOW_MASK))

#define PTIMER_REG_TIME_GET_TIME_1(reg_time) \
    ((uint32_t)(((reg_time) >> 32) & CLOCK_HIGH_MASK))

static void ptimer_alarm_fired(void *opaque);

void ptimer_reset(NV2AState *d)
{
    d->ptimer.alarm_time = 0;
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

static inline uint64_t get_internal_clock(NV2AState *d, uint64_t absolute_clock)
{
    return (absolute_clock + d->ptimer.time_offset) & PTIMER_INTERNAL_TIME_MASK;
}

static inline uint64_t get_reg_time(NV2AState *d)
{
    uint64_t internal_clock =
        get_internal_clock(d, ptimer_get_absolute_clock(d));
    return PTIMER_INTERNAL_TO_REG_TIME(internal_clock);
}

static uint64_t ptimer_ticks_to_ns(NV2AState *d, uint64_t internal_ticks)
{
    uint64_t gpu_ticks =
        muldiv64(internal_ticks, d->ptimer.numerator, d->ptimer.denominator);
    return muldiv64(gpu_ticks, NANOSECONDS_PER_SECOND,
                    d->pramdac.core_clock_freq);
}

static inline uint64_t ptimer_alarm_distance(uint64_t reg_now,
                                             uint64_t alarm_time)
{
    uint64_t diff = (alarm_time - reg_now) & PTIMER_REG_TIME_MASK;
    if (diff > (PTIMER_REG_TIME_MASK >> 1)) {
        return 0;
    }
    return diff;
}

static inline bool is_alarm_reached(uint64_t reg_now, uint64_t alarm_time)
{
    return !ptimer_alarm_distance(reg_now, alarm_time);
}

static inline uint64_t advance_alarm_epoch(uint64_t reg_time)
{
    return (reg_time + (1ULL << 32)) & PTIMER_REG_TIME_MASK;
}

static void schedule_qemu_timer(NV2AState *d)
{
    uint64_t reg_now = get_reg_time(d);
    uint64_t diff_reg_time =
        ptimer_alarm_distance(reg_now, d->ptimer.alarm_time);
    uint64_t diff_ns = 0;

    if (diff_reg_time > 0) {
        uint64_t internal_diff_ticks =
            PTIMER_REG_TO_INTERNAL_TIME(diff_reg_time);
        diff_ns = ptimer_ticks_to_ns(d, internal_diff_ticks);
    }

    timer_mod(&d->ptimer.timer,
              qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + diff_ns);
}

static void ptimer_alarm_fired(void *opaque)
{
    NV2AState *d = (NV2AState *)opaque;
    uint64_t reg_now = get_reg_time(d);

    if (is_alarm_reached(reg_now, d->ptimer.alarm_time)) {
        d->ptimer.pending_interrupts |= NV_PTIMER_INTR_0_ALARM;
        d->ptimer.alarm_time = advance_alarm_epoch(d->ptimer.alarm_time);
        nv2a_update_irq(d);
    }

    schedule_qemu_timer(d);
}

uint64_t ptimer_read(void *opaque, hwaddr addr, unsigned int size)
{
    NV2AState *d = opaque;

    uint64_t r = 0;
    switch (addr) {
    case NV_PTIMER_INTR_0:
        if (timer_pending(&d->ptimer.timer)) {
            uint64_t reg_now = get_reg_time(d);
            if (is_alarm_reached(reg_now, d->ptimer.alarm_time)) {
                d->ptimer.pending_interrupts |= NV_PTIMER_INTR_0_ALARM;
                d->ptimer.alarm_time =
                    advance_alarm_epoch(d->ptimer.alarm_time);
                nv2a_update_irq(d);
                schedule_qemu_timer(d);
            }
        }
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
        uint64_t reg_now = get_reg_time(d);
        r = PTIMER_REG_TIME_GET_TIME_0(reg_now);
    } break;
    case NV_PTIMER_TIME_1: {
        uint64_t reg_now = get_reg_time(d);
        r = PTIMER_REG_TIME_GET_TIME_1(reg_now);
    } break;
    case NV_PTIMER_ALARM_0:
        r = PTIMER_REG_TIME_GET_TIME_0(d->ptimer.alarm_time);
        break;
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
        if (val && timer_pending(&d->ptimer.timer)) {
            uint64_t reg_now = get_reg_time(d);
            if (is_alarm_reached(reg_now, d->ptimer.alarm_time)) {
                d->ptimer.pending_interrupts |= NV_PTIMER_INTR_0_ALARM;
                d->ptimer.alarm_time =
                    advance_alarm_epoch(d->ptimer.alarm_time);
                schedule_qemu_timer(d);
            }
        }
        nv2a_update_irq(d);
        break;
    case NV_PTIMER_DENOMINATOR:
        d->ptimer.denominator = val;
        if (timer_pending(&d->ptimer.timer)) {
            schedule_qemu_timer(d);
        }
        break;
    case NV_PTIMER_NUMERATOR:
        d->ptimer.numerator = val;
        if (timer_pending(&d->ptimer.timer)) {
            schedule_qemu_timer(d);
        }
        break;
    case NV_PTIMER_ALARM_0: {
        uint64_t reg_now = get_reg_time(d);
        uint32_t now_low = PTIMER_REG_TIME_GET_TIME_0(reg_now);
        uint32_t val_low = val & ALARM_MASK;
        uint64_t target = (reg_now & ~PTIMER_REG_TIME_LOW_MASK) | val_low;
        if (val_low <= (now_low & ALARM_MASK)) {
            target = advance_alarm_epoch(target);
        }
        d->ptimer.alarm_time = target & PTIMER_REG_TIME_MASK;
        schedule_qemu_timer(d);
    } break;
    case NV_PTIMER_TIME_0: {
        uint64_t current_reg_time = get_reg_time(d);
        uint64_t target_reg_time = PTIMER_MAKE_REG_TIME(
            PTIMER_REG_TIME_GET_TIME_1(current_reg_time), val & ALARM_MASK);
        uint64_t target_internal = PTIMER_REG_TO_INTERNAL_TIME(target_reg_time);
        d->ptimer.time_offset = target_internal - ptimer_get_absolute_clock(d);
        if (timer_pending(&d->ptimer.timer)) {
            schedule_qemu_timer(d);
        }
    } break;
    case NV_PTIMER_TIME_1: {
        uint64_t current_reg_time = get_reg_time(d);
        uint64_t target_reg_time = PTIMER_MAKE_REG_TIME(
            val, PTIMER_REG_TIME_GET_TIME_0(current_reg_time));
        uint64_t target_internal = PTIMER_REG_TO_INTERNAL_TIME(target_reg_time);
        d->ptimer.time_offset = target_internal - ptimer_get_absolute_clock(d);
        if (timer_pending(&d->ptimer.timer)) {
            schedule_qemu_timer(d);
        }
    } break;
    default:
        break;
    }
}
