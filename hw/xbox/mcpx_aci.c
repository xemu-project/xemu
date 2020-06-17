/*
 * QEMU MCPX Audio Codec Interface implementation
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
#include "hw/i386/pc.h"
#include "hw/pci/pci.h"
#include "hw/audio/ac97_int.h"
#include "migration/vmstate.h"

typedef struct MCPXACIState {
    PCIDevice dev;

    AC97LinkState ac97;

    MemoryRegion io_nam, io_nabm;

    MemoryRegion mmio;
    MemoryRegion nam_mmio, nabm_mmio;
} MCPXACIState;

#define MCPX_ACI_DEVICE(obj) \
    OBJECT_CHECK(MCPXACIState, (obj), "mcpx-aci")

static void mcpx_aci_realize(PCIDevice *dev, Error **errp)
{
    MCPXACIState *d = MCPX_ACI_DEVICE(dev);

    dev->config[PCI_INTERRUPT_PIN] = 0x01;

    memory_region_init(&d->mmio, OBJECT(dev), "mcpx-aci-mmio", 0x1000);

    memory_region_init_io(&d->io_nam, OBJECT(dev), &ac97_io_nam_ops, &d->ac97,
                          "mcpx-aci-nam", 0x100);
    memory_region_init_io(&d->io_nabm, OBJECT(dev), &ac97_io_nabm_ops, &d->ac97,
                          "mcpx-aci-nabm", 0x80);

    /*pci_register_bar(&d->dev, 0, PCI_BASE_ADDRESS_SPACE_IO, &d->io_nam);
    pci_register_bar(&d->dev, 1, PCI_BASE_ADDRESS_SPACE_IO, &d->io_nabm);

    memory_region_init_alias(&d->nam_mmio, NULL, &d->io_nam, 0, 0x100);
    memory_region_add_subregion(&d->mmio, 0x0, &d->nam_mmio);

    memory_region_init_alias(&d->nabm_mmio, NULL, &d->io_nabm, 0, 0x80);
    memory_region_add_subregion(&d->mmio, 0x100, &d->nabm_mmio);*/

    memory_region_add_subregion(&d->mmio, 0x0, &d->io_nam);
    memory_region_add_subregion(&d->mmio, 0x100, &d->io_nabm);

    pci_register_bar(&d->dev, 2, PCI_BASE_ADDRESS_SPACE_MEMORY, &d->mmio);
    ac97_common_init(&d->ac97, &d->dev, pci_get_address_space(&d->dev));
}

static const VMStateDescription vmstate_mcpx_aci = {
    .name = "mcpx-aci",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_PCI_DEVICE(dev, MCPXACIState),
        // FIXME
        VMSTATE_END_OF_LIST()
    },
};

static void mcpx_aci_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->vendor_id = PCI_VENDOR_ID_NVIDIA;
    k->device_id = PCI_DEVICE_ID_NVIDIA_MCPX_ACI;
    k->revision = 210;
    k->class_id = PCI_CLASS_MULTIMEDIA_AUDIO;
    k->realize = mcpx_aci_realize;

    dc->desc = "MCPX Audio Codec Interface";
    dc->vmsd = &vmstate_mcpx_aci;
}

static const TypeInfo mcpx_aci_info = {
    .name          = "mcpx-aci",
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(MCPXACIState),
    .class_init    = mcpx_aci_class_init,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

static void mcpx_aci_register(void)
{
    type_register_static(&mcpx_aci_info);
}

type_init(mcpx_aci_register);
