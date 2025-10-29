/*
 * QEMU Uninorth PCI host (for all Mac99 and newer machines)
 *
 * Copyright (c) 2006 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef UNINORTH_H
#define UNINORTH_H

#include "hw/pci/pci_host.h"
#include "qom/object.h"

/* UniNorth version */
#define UNINORTH_VERSION_10A    0x7

#define TYPE_UNI_NORTH_PCI_HOST_BRIDGE "uni-north-pci-pcihost"
#define TYPE_UNI_NORTH_AGP_HOST_BRIDGE "uni-north-agp-pcihost"
#define TYPE_UNI_NORTH_INTERNAL_PCI_HOST_BRIDGE "uni-north-internal-pci-pcihost"
#define TYPE_U3_AGP_HOST_BRIDGE "u3-agp-pcihost"

typedef struct UNINHostState UNINHostState;
DECLARE_INSTANCE_CHECKER(UNINHostState, UNI_NORTH_PCI_HOST_BRIDGE,
                         TYPE_UNI_NORTH_PCI_HOST_BRIDGE)
DECLARE_INSTANCE_CHECKER(UNINHostState, UNI_NORTH_AGP_HOST_BRIDGE,
                         TYPE_UNI_NORTH_AGP_HOST_BRIDGE)
DECLARE_INSTANCE_CHECKER(UNINHostState, UNI_NORTH_INTERNAL_PCI_HOST_BRIDGE,
                         TYPE_UNI_NORTH_INTERNAL_PCI_HOST_BRIDGE)
DECLARE_INSTANCE_CHECKER(UNINHostState, U3_AGP_HOST_BRIDGE,
                         TYPE_U3_AGP_HOST_BRIDGE)

struct UNINHostState {
    PCIHostState parent_obj;

    uint32_t ofw_addr;
    qemu_irq irqs[4];
    MemoryRegion pci_mmio;
    MemoryRegion pci_hole;
    MemoryRegion pci_io;
};

struct UNINState {
    SysBusDevice parent_obj;

    MemoryRegion mem;
};

#define TYPE_UNI_NORTH "uni-north"
OBJECT_DECLARE_SIMPLE_TYPE(UNINState, UNI_NORTH)

#endif /* UNINORTH_H */
