/*
 * Xbox ACPI implementation
 *
 * Copyright (c) 2012 espes
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

#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/irq.h"
#include "hw/i386/pc.h"
#include "hw/pci/pci.h"
#include "qemu/timer.h"
#include "sysemu/sysemu.h"
#include "hw/acpi/acpi.h"
#include "hw/xbox/xbox_pci.h"
#include "hw/xbox/acpi_xbox.h"

// #define DEBUG
#ifdef DEBUG
# define XBOX_DPRINTF(format, ...)     printf(format, ## __VA_ARGS__)
#else
# define XBOX_DPRINTF(format, ...)     do { } while (0)
#endif

#define XBOX_PM_BASE_BAR  0
#define XBOX_PM_GPIO_BASE 0xC0
#define XBOX_PM_GPIO_LEN  26

static int field_pin;

static uint64_t xbox_pm_gpio_read(void *opaque, hwaddr addr, unsigned width)
{
    uint64_t r = 0;
    switch (addr) {
    case 0:
        /* field pin from tv encoder? */
        field_pin = (field_pin + 1) & 1;
        r = field_pin << 5;
        break;
    default:
        break;
    }
    XBOX_DPRINTF("pm gpio read [0x%llx] -> 0x%llx\n", addr, r);
    return r;
}

static void xbox_pm_gpio_write(void *opaque, hwaddr addr, uint64_t val,
                               unsigned width)
{
    XBOX_DPRINTF("pm gpio write [0x%llx] = 0x%llx\n", addr, val);
}

static const MemoryRegionOps xbox_pm_gpio_ops = {
    .read = xbox_pm_gpio_read,
    .write = xbox_pm_gpio_write,
};

static void pm_update_sci(XBOX_PMRegs *pm)
{
    int sci_level, pm1a_sts;

    pm1a_sts = acpi_pm1_evt_get_sts(&pm->acpi_regs);

    sci_level = (((pm1a_sts & pm->acpi_regs.pm1.evt.en) &
                  (ACPI_BITMASK_RT_CLOCK_ENABLE |
                   ACPI_BITMASK_POWER_BUTTON_ENABLE |
                   ACPI_BITMASK_GLOBAL_LOCK_ENABLE |
                   ACPI_BITMASK_TIMER_ENABLE)) != 0);
    qemu_set_irq(pm->irq, sci_level);

    /* schedule a timer interruption if needed */
    acpi_pm_tmr_update(&pm->acpi_regs,
                       (pm->acpi_regs.pm1.evt.en & ACPI_BITMASK_TIMER_ENABLE) &&
                       !(pm1a_sts & ACPI_BITMASK_TIMER_STATUS));
}

static void xbox_pm_update_sci_fn(ACPIREGS *regs)
{
    XBOX_PMRegs *pm = container_of(regs, XBOX_PMRegs, acpi_regs);
    pm_update_sci(pm);
}

void xbox_pm_init(PCIDevice *dev, XBOX_PMRegs *pm, qemu_irq sci_irq)
{
    memory_region_init(&pm->io, OBJECT(dev), "xbox-pm", 256);

    pci_register_bar(dev, XBOX_PM_BASE_BAR, PCI_BASE_ADDRESS_SPACE_IO, &pm->io);

    acpi_pm_tmr_init(&pm->acpi_regs, xbox_pm_update_sci_fn, &pm->io);
    acpi_pm1_evt_init(&pm->acpi_regs, xbox_pm_update_sci_fn, &pm->io);
    acpi_pm1_cnt_init(&pm->acpi_regs, &pm->io, true, true, 2);

    memory_region_init_io(&pm->io_gpio, OBJECT(dev), &xbox_pm_gpio_ops, pm,
                          "xbox-pm-gpio", XBOX_PM_GPIO_LEN);
    memory_region_add_subregion(&pm->io, XBOX_PM_GPIO_BASE, &pm->io_gpio);

    pm->irq = sci_irq;
}
