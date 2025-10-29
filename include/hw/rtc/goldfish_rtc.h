/*
 * Goldfish virtual platform RTC
 *
 * Copyright (C) 2019 Western Digital Corporation or its affiliates.
 *
 * For more details on Google Goldfish virtual platform refer:
 * https://android.googlesource.com/platform/external/qemu/+/master/docs/GOLDFISH-VIRTUAL-HARDWARE.TXT
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_RTC_GOLDFISH_RTC_H
#define HW_RTC_GOLDFISH_RTC_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_GOLDFISH_RTC "goldfish_rtc"
OBJECT_DECLARE_SIMPLE_TYPE(GoldfishRTCState, GOLDFISH_RTC)

struct GoldfishRTCState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    QEMUTimer *timer;
    qemu_irq irq;

    uint64_t tick_offset;
    uint64_t tick_offset_vmstate;
    uint64_t alarm_next;
    uint32_t alarm_running;
    uint32_t irq_pending;
    uint32_t irq_enabled;
    uint32_t time_high;

    bool big_endian;
};

#endif
