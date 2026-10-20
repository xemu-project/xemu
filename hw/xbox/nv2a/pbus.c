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
#include "hw/pci/pci_host.h"

/* PCI config space mirror; hardware decodes it twice (address bit 8 ignored). */
#define NV_PBUS_PCI_NV_MIRROR_SIZE (2 * PCI_CONFIG_SPACE_SIZE)

static bool pbus_pci_mirror_offset(hwaddr addr, uint32_t *cfg_addr)
{
    if (addr < NV_PBUS_PCI_NV_0 ||
        addr >= NV_PBUS_PCI_NV_0 + NV_PBUS_PCI_NV_MIRROR_SIZE) {
        return false;
    }

    *cfg_addr = (addr - NV_PBUS_PCI_NV_0) & (PCI_CONFIG_SPACE_SIZE - 1);
    return true;
}

/* PBUS - bus control */
uint64_t pbus_read(void *opaque, hwaddr addr, unsigned int size)
{
    NV2AState *s = opaque;
    PCIDevice *d = PCI_DEVICE(s);
    uint32_t cfg_addr;

    uint64_t r = 0;
    if (pbus_pci_mirror_offset(addr, &cfg_addr)) {
        r = pci_host_config_read_common(d, cfg_addr, PCI_CONFIG_SPACE_SIZE,
                                        size);
    }

    nv2a_reg_log_read(NV_PBUS, addr, size, r);
    return r;
}

void pbus_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size)
{
    NV2AState *s = opaque;
    PCIDevice *d = PCI_DEVICE(s);
    uint32_t cfg_addr;

    nv2a_reg_log_write(NV_PBUS, addr, size, val);

    if (pbus_pci_mirror_offset(addr, &cfg_addr)) {
        pci_host_config_write_common(d, cfg_addr, PCI_CONFIG_SPACE_SIZE,
                                     (uint32_t)val, size);
    }
}
