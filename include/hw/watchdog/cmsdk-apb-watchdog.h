/*
 * ARM CMSDK APB watchdog emulation
 *
 * Copyright (c) 2018 Linaro Limited
 * Written by Peter Maydell
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 or
 *  (at your option) any later version.
 */

/*
 * This is a model of the "APB watchdog" which is part of the Cortex-M
 * System Design Kit (CMSDK) and documented in the Cortex-M System
 * Design Kit Technical Reference Manual (ARM DDI0479C):
 * https://developer.arm.com/products/system-design/system-design-kits/cortex-m-system-design-kit
 *
 * QEMU interface:
 *  + Clock input "WDOGCLK": clock for the watchdog's timer
 *  + sysbus MMIO region 0: the register bank
 *  + sysbus IRQ 0: watchdog interrupt
 *
 * In real hardware the watchdog's reset output is just a GPIO line
 * which can then be masked by the board or treated as a simple interrupt.
 * (For instance the IoTKit does this with the non-secure watchdog, so that
 * secure code can control whether non-secure code can perform a system
 * reset via its watchdog.) In QEMU, we just wire up the watchdog reset
 * to watchdog_perform_action(), at least for the moment.
 */

#ifndef CMSDK_APB_WATCHDOG_H
#define CMSDK_APB_WATCHDOG_H

#include "hw/sysbus.h"
#include "hw/ptimer.h"
#include "hw/clock.h"
#include "qom/object.h"

#define TYPE_CMSDK_APB_WATCHDOG "cmsdk-apb-watchdog"
OBJECT_DECLARE_SIMPLE_TYPE(CMSDKAPBWatchdog, CMSDK_APB_WATCHDOG)

/*
 * This shares the same struct (and cast macro) as the base
 * cmsdk-apb-watchdog device.
 */
#define TYPE_LUMINARY_WATCHDOG "luminary-watchdog"

struct CMSDKAPBWatchdog {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    qemu_irq wdogint;
    bool is_luminary;
    struct ptimer_state *timer;
    Clock *wdogclk;

    uint32_t control;
    uint32_t intstatus;
    uint32_t lock;
    uint32_t itcr;
    uint32_t itop;
    uint32_t resetstatus;
    const uint32_t *id;
};

#endif
