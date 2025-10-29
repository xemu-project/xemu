/*
 * Raspberry Pi emulation (c) 2012 Gregory Estrade
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef BCM2835_IC_H
#define BCM2835_IC_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_BCM2835_IC "bcm2835-ic"
OBJECT_DECLARE_SIMPLE_TYPE(BCM2835ICState, BCM2835_IC)

#define BCM2835_IC_GPU_IRQ "gpu-irq"
#define BCM2835_IC_ARM_IRQ "arm-irq"

struct BCM2835ICState {
    /*< private >*/
    SysBusDevice busdev;
    /*< public >*/

    MemoryRegion iomem;
    qemu_irq irq;
    qemu_irq fiq;

    /* 64 GPU IRQs + 8 ARM IRQs = 72 total (GPU first) */
    uint64_t gpu_irq_level, gpu_irq_enable;
    uint8_t arm_irq_level, arm_irq_enable;
    bool fiq_enable;
    uint8_t fiq_select;
};

#endif
