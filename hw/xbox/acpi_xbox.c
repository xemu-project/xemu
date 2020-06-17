/*
 * Xbox ACPI implementation
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2020 Matt Borgerson
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
#include "sysemu/reset.h"
#include "hw/acpi/acpi.h"
#include "hw/xbox/xbox_pci.h"
#include "hw/xbox/acpi_xbox.h"
#include "migration/vmstate.h"

// #define DEBUG
#ifdef DEBUG
# define XBOX_DPRINTF(format, ...)     printf(format, ## __VA_ARGS__)
#else
# define XBOX_DPRINTF(format, ...)     do { } while (0)
#endif

#define XBOX_PM_BASE_BAR  0
#define XBOX_PM_GPE_BASE  0x20
#define XBOX_PM_GPE_LEN   4
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

static uint64_t xbox_pm_gpe_readb(void *opaque, hwaddr addr, unsigned width)
{
    XBOX_PMRegs *pm = opaque;
    return acpi_gpe_ioport_readb(&pm->acpi_regs, addr);
}

static void xbox_pm_gpe_writeb(void *opaque, hwaddr addr, uint64_t val,
                            unsigned width)
{
    XBOX_PMRegs *pm = opaque;
    acpi_gpe_ioport_writeb(&pm->acpi_regs, addr, val);
    acpi_update_sci(&pm->acpi_regs, pm->irq);
}

#define VMSTATE_GPE_ARRAY(_field, _state)                            \
 {                                                                   \
     .name       = (stringify(_field)),                              \
     .version_id = 0,                                                \
     .num        = XBOX_PM_GPE_LEN,                                  \
     .info       = &vmstate_info_uint8,                              \
     .size       = sizeof(uint8_t),                                  \
     .flags      = VMS_ARRAY | VMS_POINTER,                          \
     .offset     = vmstate_offset_pointer(_state, _field, uint8_t),  \
 }

const VMStateDescription vmstate_xbox_pm = {
    .name = "xbox-pm",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT16(acpi_regs.pm1.evt.sts, XBOX_PMRegs),
        VMSTATE_UINT16(acpi_regs.pm1.evt.en, XBOX_PMRegs),
        VMSTATE_UINT16(acpi_regs.pm1.cnt.cnt, XBOX_PMRegs),
        VMSTATE_TIMER_PTR(acpi_regs.tmr.timer, XBOX_PMRegs),
        VMSTATE_INT64(acpi_regs.tmr.overflow_time, XBOX_PMRegs),
        VMSTATE_GPE_ARRAY(acpi_regs.gpe.sts, XBOX_PMRegs),
        VMSTATE_GPE_ARRAY(acpi_regs.gpe.en, XBOX_PMRegs),
        VMSTATE_END_OF_LIST()
    },
};

static const MemoryRegionOps xbox_pm_gpe_ops = {
    .read = xbox_pm_gpe_readb,
    .write = xbox_pm_gpe_writeb,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
    .impl.min_access_size = 1,
    .impl.max_access_size = 1,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void pm_reset(void *opaque)
{
    XBOX_PMRegs *pm = opaque;

    acpi_pm1_evt_reset(&pm->acpi_regs);
    acpi_pm1_cnt_reset(&pm->acpi_regs);
    acpi_pm_tmr_reset(&pm->acpi_regs);
    acpi_gpe_reset(&pm->acpi_regs);

    acpi_update_sci(&pm->acpi_regs, pm->irq);
}

void xbox_pm_init(PCIDevice *dev, XBOX_PMRegs *pm, qemu_irq sci_irq)
{
    memory_region_init(&pm->io, OBJECT(dev), "xbox-pm", 256);

    pci_register_bar(dev, XBOX_PM_BASE_BAR, PCI_BASE_ADDRESS_SPACE_IO, &pm->io);

    acpi_pm_tmr_init(&pm->acpi_regs, xbox_pm_update_sci_fn, &pm->io);
    acpi_pm1_evt_init(&pm->acpi_regs, xbox_pm_update_sci_fn, &pm->io);
    acpi_pm1_cnt_init(&pm->acpi_regs, &pm->io, true, true, 2);
    acpi_gpe_init(&pm->acpi_regs, XBOX_PM_GPE_LEN);

    memory_region_init_io(&pm->io_gpe, OBJECT(dev), &xbox_pm_gpe_ops, pm,
                          "xbox-pm-gpe0", XBOX_PM_GPE_LEN);
    memory_region_add_subregion(&pm->io, XBOX_PM_GPE_BASE, &pm->io_gpe);

    memory_region_init_io(&pm->io_gpio, OBJECT(dev), &xbox_pm_gpio_ops, pm,
                          "xbox-pm-gpio", XBOX_PM_GPIO_LEN);
    memory_region_add_subregion(&pm->io, XBOX_PM_GPIO_BASE, &pm->io_gpio);

    pm->irq = sci_irq;
    qemu_register_reset(pm_reset, pm);
}
