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

/* PBUS - bus control */
uint64_t pbus_read(void *opaque, hwaddr addr, unsigned int size)
{
    NV2AState *s = opaque;
    PCIDevice *d = PCI_DEVICE(s);

    uint64_t r = 0;
    switch (addr) {
    case NV_PBUS_PCI_NV_0:
        r = pci_get_long(d->config + PCI_VENDOR_ID);
        break;
    case NV_PBUS_PCI_NV_1:
        r = pci_get_long(d->config + PCI_COMMAND);
        break;
    case NV_PBUS_PCI_NV_2:
        r = pci_get_long(d->config + PCI_CLASS_REVISION);
        break;
    default:
        break;
    }

    nv2a_reg_log_read(NV_PBUS, addr, size, r);
    return r;
}

void pbus_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size)
{
    NV2AState *s = opaque;
    PCIDevice *d = PCI_DEVICE(s);

    nv2a_reg_log_write(NV_PBUS, addr, size, val);

    switch (addr) {
    case NV_PBUS_PCI_NV_1:
        pci_set_long(d->config + PCI_COMMAND, val);
        break;
    default:
        break;
    }
}
