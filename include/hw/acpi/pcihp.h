/*
 * QEMU<->ACPI BIOS PCI hotplug interface
 *
 * QEMU supports PCI hotplug via ACPI. This module
 * implements the interface between QEMU and the ACPI BIOS.
 * Interface specification - see docs/specs/acpi_pci_hotplug.txt
 *
 * Copyright (c) 2013, Red Hat Inc, Michael S. Tsirkin (mst@redhat.com)
 * Copyright (c) 2006 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>
 *
 * Contributions after 2012-01-13 are licensed under the terms of the
 * GNU GPL, version 2 or (at your option) any later version.
 */

#ifndef HW_ACPI_PCIHP_H
#define HW_ACPI_PCIHP_H

#include "hw/acpi/acpi.h"
#include "hw/hotplug.h"

#define ACPI_PCIHP_IO_BASE_PROP "acpi-pcihp-io-base"
#define ACPI_PCIHP_IO_LEN_PROP "acpi-pcihp-io-len"

typedef struct AcpiPciHpPciStatus {
    uint32_t up;
    uint32_t down;
    uint32_t hotplug_enable;
} AcpiPciHpPciStatus;

#define ACPI_PCIHP_PROP_BSEL "acpi-pcihp-bsel"
#define ACPI_PCIHP_MAX_HOTPLUG_BUS 256
#define ACPI_PCIHP_BSEL_DEFAULT 0x0

typedef struct AcpiPciHpState {
    AcpiPciHpPciStatus acpi_pcihp_pci_status[ACPI_PCIHP_MAX_HOTPLUG_BUS];
    uint32_t hotplug_select;
    uint32_t acpi_index;
    PCIBus *root;
    MemoryRegion io;
    bool legacy_piix;
    uint16_t io_base;
    uint16_t io_len;
} AcpiPciHpState;

void acpi_pcihp_init(Object *owner, AcpiPciHpState *, PCIBus *root,
                     MemoryRegion *address_space_io, bool bridges_enabled,
                     uint16_t io_base);

void acpi_pcihp_device_pre_plug_cb(HotplugHandler *hotplug_dev,
                                   DeviceState *dev, Error **errp);
void acpi_pcihp_device_plug_cb(HotplugHandler *hotplug_dev, AcpiPciHpState *s,
                               DeviceState *dev, Error **errp);
void acpi_pcihp_device_unplug_cb(HotplugHandler *hotplug_dev, AcpiPciHpState *s,
                                 DeviceState *dev, Error **errp);
void acpi_pcihp_device_unplug_request_cb(HotplugHandler *hotplug_dev,
                                         AcpiPciHpState *s, DeviceState *dev,
                                         Error **errp);

/* Called on reset */
void acpi_pcihp_reset(AcpiPciHpState *s, bool acpihp_root_off);

extern const VMStateDescription vmstate_acpi_pcihp_pci_status;

#define VMSTATE_PCI_HOTPLUG(pcihp, state, test_pcihp, test_acpi_index) \
        VMSTATE_UINT32_TEST(pcihp.hotplug_select, state, \
                            test_pcihp), \
        VMSTATE_STRUCT_ARRAY_TEST(pcihp.acpi_pcihp_pci_status, state, \
                                  ACPI_PCIHP_MAX_HOTPLUG_BUS, \
                                  test_pcihp, 1, \
                                  vmstate_acpi_pcihp_pci_status, \
                                  AcpiPciHpPciStatus), \
        VMSTATE_UINT32_TEST(pcihp.acpi_index, state, \
                            test_acpi_index)

#endif
