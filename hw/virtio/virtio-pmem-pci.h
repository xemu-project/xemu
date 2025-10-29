/*
 * Virtio PMEM PCI device
 *
 * Copyright (C) 2018-2019 Red Hat, Inc.
 *
 * Authors:
 *  Pankaj Gupta <pagupta@redhat.com>
 *  David Hildenbrand <david@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.
 * See the COPYING file in the top-level directory.
 */

#ifndef QEMU_VIRTIO_PMEM_PCI_H
#define QEMU_VIRTIO_PMEM_PCI_H

#include "hw/virtio/virtio-md-pci.h"
#include "hw/virtio/virtio-pmem.h"
#include "qom/object.h"

typedef struct VirtIOPMEMPCI VirtIOPMEMPCI;

/*
 * virtio-pmem-pci: This extends VirtIOMDPCI.
 */
#define TYPE_VIRTIO_PMEM_PCI "virtio-pmem-pci-base"
DECLARE_INSTANCE_CHECKER(VirtIOPMEMPCI, VIRTIO_PMEM_PCI,
                         TYPE_VIRTIO_PMEM_PCI)

struct VirtIOPMEMPCI {
    VirtIOMDPCI parent_obj;
    VirtIOPMEM vdev;
};

#endif /* QEMU_VIRTIO_PMEM_PCI_H */
