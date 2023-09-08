/*
 * libqos PCI bindings
 *
 * Copyright IBM, Corp. 2012-2013
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "pci.h"

#include "hw/pci/pci.h"
#include "hw/pci/pci_bridge.h"
#include "hw/pci/pci_regs.h"
#include "qemu/host-utils.h"
#include "qgraph.h"

void qpci_device_foreach(QPCIBus *bus, int vendor_id, int device_id,
                         void (*func)(QPCIDevice *dev, int devfn, void *data),
                         void *data)
{
    int slot;

    for (slot = 0; slot < 32; slot++) {
        int fn;

        for (fn = 0; fn < 8; fn++) {
            QPCIDevice *dev;

            dev = qpci_device_find(bus, QPCI_DEVFN(slot, fn));
            if (!dev) {
                continue;
            }

            if (vendor_id != -1 &&
                qpci_config_readw(dev, PCI_VENDOR_ID) != vendor_id) {
                g_free(dev);
                continue;
            }

            if (device_id != -1 &&
                qpci_config_readw(dev, PCI_DEVICE_ID) != device_id) {
                g_free(dev);
                continue;
            }

            func(dev, QPCI_DEVFN(slot, fn), data);
        }
    }
}

bool qpci_has_buggy_msi(QPCIDevice *dev)
{
    return dev->bus->has_buggy_msi;
}

bool qpci_check_buggy_msi(QPCIDevice *dev)
{
    if (qpci_has_buggy_msi(dev)) {
        g_test_skip("Skipping due to incomplete support for MSI");
        return true;
    }
    return false;
}

static void qpci_device_set(QPCIDevice *dev, QPCIBus *bus, int devfn)
{
    g_assert(dev);

    dev->bus = bus;
    dev->devfn = devfn;
}

QPCIDevice *qpci_device_find(QPCIBus *bus, int devfn)
{
    QPCIDevice *dev;

    dev = g_malloc0(sizeof(*dev));
    qpci_device_set(dev, bus, devfn);

    if (qpci_config_readw(dev, PCI_VENDOR_ID) == 0xFFFF) {
        g_free(dev);
        return NULL;
    }

    return dev;
}

void qpci_device_init(QPCIDevice *dev, QPCIBus *bus, QPCIAddress *addr)
{
    uint16_t vendor_id, device_id;

    qpci_device_set(dev, bus, addr->devfn);
    vendor_id = qpci_config_readw(dev, PCI_VENDOR_ID);
    device_id = qpci_config_readw(dev, PCI_DEVICE_ID);
    g_assert(!addr->vendor_id || vendor_id == addr->vendor_id);
    g_assert(!addr->device_id || device_id == addr->device_id);
}

static uint8_t qpci_find_resource_reserve_capability(QPCIDevice *dev)
{
    uint16_t device_id;
    uint8_t cap = 0;

    if (qpci_config_readw(dev, PCI_VENDOR_ID) != PCI_VENDOR_ID_REDHAT) {
        return 0;
    }

    device_id = qpci_config_readw(dev, PCI_DEVICE_ID);

    if (device_id != PCI_DEVICE_ID_REDHAT_PCIE_RP &&
        device_id != PCI_DEVICE_ID_REDHAT_BRIDGE) {
        return 0;
    }

    do {
        cap = qpci_find_capability(dev, PCI_CAP_ID_VNDR, cap);
    } while (cap &&
             qpci_config_readb(dev, cap + REDHAT_PCI_CAP_TYPE_OFFSET) !=
             REDHAT_PCI_CAP_RESOURCE_RESERVE);
    if (cap) {
        uint8_t cap_len = qpci_config_readb(dev, cap + PCI_CAP_FLAGS);
        if (cap_len < REDHAT_PCI_CAP_RES_RESERVE_CAP_SIZE) {
            return 0;
        }
    }
    return cap;
}

static void qpci_secondary_buses_rec(QPCIBus *qbus, int bus, int *pci_bus)
{
    QPCIDevice *dev;
    uint16_t class;
    uint8_t pribus, secbus, subbus;
    int index;

    for (index = 0; index < 32; index++) {
        dev = qpci_device_find(qbus, QPCI_DEVFN(bus + index, 0));
        if (dev == NULL) {
            continue;
        }
        class = qpci_config_readw(dev, PCI_CLASS_DEVICE);
        if (class == PCI_CLASS_BRIDGE_PCI) {
            qpci_config_writeb(dev, PCI_SECONDARY_BUS, 255);
            qpci_config_writeb(dev, PCI_SUBORDINATE_BUS, 0);
        }
        g_free(dev);
    }

    for (index = 0; index < 32; index++) {
        dev = qpci_device_find(qbus, QPCI_DEVFN(bus + index, 0));
        if (dev == NULL) {
            continue;
        }
        class = qpci_config_readw(dev, PCI_CLASS_DEVICE);
        if (class != PCI_CLASS_BRIDGE_PCI) {
            g_free(dev);
            continue;
        }

        pribus = qpci_config_readb(dev, PCI_PRIMARY_BUS);
        if (pribus != bus) {
            qpci_config_writeb(dev, PCI_PRIMARY_BUS, bus);
        }

        secbus = qpci_config_readb(dev, PCI_SECONDARY_BUS);
        (*pci_bus)++;
        if (*pci_bus != secbus) {
            secbus = *pci_bus;
            qpci_config_writeb(dev, PCI_SECONDARY_BUS, secbus);
        }

        subbus = qpci_config_readb(dev, PCI_SUBORDINATE_BUS);
        qpci_config_writeb(dev, PCI_SUBORDINATE_BUS, 255);

        qpci_secondary_buses_rec(qbus, secbus << 5, pci_bus);

        if (subbus != *pci_bus) {
            uint8_t res_bus = *pci_bus;
            uint8_t cap = qpci_find_resource_reserve_capability(dev);

            if (cap) {
                uint32_t tmp_res_bus;

                tmp_res_bus = qpci_config_readl(dev, cap +
                                            REDHAT_PCI_CAP_RES_RESERVE_BUS_RES);
                if (tmp_res_bus != (uint32_t)-1) {
                    res_bus = tmp_res_bus & 0xFF;
                    if ((uint8_t)(res_bus + secbus) < secbus ||
                        (uint8_t)(res_bus + secbus) < res_bus) {
                        res_bus = 0;
                    }
                    if (secbus + res_bus > *pci_bus) {
                        res_bus = secbus + res_bus;
                    }
                }
            }
            subbus = res_bus;
            *pci_bus = res_bus;
        }

        qpci_config_writeb(dev, PCI_SUBORDINATE_BUS, subbus);
        g_free(dev);
    }
}

int qpci_secondary_buses_init(QPCIBus *bus)
{
    int last_bus = 0;

    qpci_secondary_buses_rec(bus, 0, &last_bus);

    return last_bus;
}


void qpci_device_enable(QPCIDevice *dev)
{
    uint16_t cmd;

    /* FIXME -- does this need to be a bus callout? */
    cmd = qpci_config_readw(dev, PCI_COMMAND);
    cmd |= PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;
    qpci_config_writew(dev, PCI_COMMAND, cmd);

    /* Verify the bits are now set. */
    cmd = qpci_config_readw(dev, PCI_COMMAND);
    g_assert_cmphex(cmd & PCI_COMMAND_IO, ==, PCI_COMMAND_IO);
    g_assert_cmphex(cmd & PCI_COMMAND_MEMORY, ==, PCI_COMMAND_MEMORY);
    g_assert_cmphex(cmd & PCI_COMMAND_MASTER, ==, PCI_COMMAND_MASTER);
}

/**
 * qpci_find_capability:
 * @dev: the PCI device
 * @id: the PCI Capability ID (PCI_CAP_ID_*)
 * @start_addr: 0 to begin iteration or the last return value to continue
 *              iteration
 *
 * Iterate over the PCI Capabilities List.
 *
 * Returns: PCI Configuration Space offset of the capabililty structure or
 *          0 if no further matching capability is found
 */
uint8_t qpci_find_capability(QPCIDevice *dev, uint8_t id, uint8_t start_addr)
{
    uint8_t cap;
    uint8_t addr;

    if (start_addr) {
        addr = qpci_config_readb(dev, start_addr + PCI_CAP_LIST_NEXT);
    } else {
        addr = qpci_config_readb(dev, PCI_CAPABILITY_LIST);
    }

    do {
        cap = qpci_config_readb(dev, addr);
        if (cap != id) {
            addr = qpci_config_readb(dev, addr + PCI_CAP_LIST_NEXT);
        }
    } while (cap != id && addr != 0);

    return addr;
}

void qpci_msix_enable(QPCIDevice *dev)
{
    uint8_t addr;
    uint16_t val;
    uint32_t table;
    uint8_t bir_table;
    uint8_t bir_pba;

    addr = qpci_find_capability(dev, PCI_CAP_ID_MSIX, 0);
    g_assert_cmphex(addr, !=, 0);

    val = qpci_config_readw(dev, addr + PCI_MSIX_FLAGS);
    qpci_config_writew(dev, addr + PCI_MSIX_FLAGS, val | PCI_MSIX_FLAGS_ENABLE);

    table = qpci_config_readl(dev, addr + PCI_MSIX_TABLE);
    bir_table = table & PCI_MSIX_FLAGS_BIRMASK;
    dev->msix_table_bar = qpci_iomap(dev, bir_table, NULL);
    dev->msix_table_off = table & ~PCI_MSIX_FLAGS_BIRMASK;

    table = qpci_config_readl(dev, addr + PCI_MSIX_PBA);
    bir_pba = table & PCI_MSIX_FLAGS_BIRMASK;
    if (bir_pba != bir_table) {
        dev->msix_pba_bar = qpci_iomap(dev, bir_pba, NULL);
    } else {
        dev->msix_pba_bar = dev->msix_table_bar;
    }
    dev->msix_pba_off = table & ~PCI_MSIX_FLAGS_BIRMASK;

    dev->msix_enabled = true;
}

void qpci_msix_disable(QPCIDevice *dev)
{
    uint8_t addr;
    uint16_t val;

    g_assert(dev->msix_enabled);
    addr = qpci_find_capability(dev, PCI_CAP_ID_MSIX, 0);
    g_assert_cmphex(addr, !=, 0);
    val = qpci_config_readw(dev, addr + PCI_MSIX_FLAGS);
    qpci_config_writew(dev, addr + PCI_MSIX_FLAGS,
                                                val & ~PCI_MSIX_FLAGS_ENABLE);

    if (dev->msix_pba_bar.addr != dev->msix_table_bar.addr) {
        qpci_iounmap(dev, dev->msix_pba_bar);
    }
    qpci_iounmap(dev, dev->msix_table_bar);

    dev->msix_enabled = 0;
    dev->msix_table_off = 0;
    dev->msix_pba_off = 0;
}

bool qpci_msix_pending(QPCIDevice *dev, uint16_t entry)
{
    uint32_t pba_entry;
    uint8_t bit_n = entry % 32;
    uint64_t  off = (entry / 32) * PCI_MSIX_ENTRY_SIZE / 4;

    g_assert(dev->msix_enabled);
    pba_entry = qpci_io_readl(dev, dev->msix_pba_bar, dev->msix_pba_off + off);
    qpci_io_writel(dev, dev->msix_pba_bar, dev->msix_pba_off + off,
                   pba_entry & ~(1 << bit_n));
    return (pba_entry & (1 << bit_n)) != 0;
}

bool qpci_msix_masked(QPCIDevice *dev, uint16_t entry)
{
    uint8_t addr;
    uint16_t val;
    uint64_t vector_off = dev->msix_table_off + entry * PCI_MSIX_ENTRY_SIZE;

    g_assert(dev->msix_enabled);
    addr = qpci_find_capability(dev, PCI_CAP_ID_MSIX, 0);
    g_assert_cmphex(addr, !=, 0);
    val = qpci_config_readw(dev, addr + PCI_MSIX_FLAGS);

    if (val & PCI_MSIX_FLAGS_MASKALL) {
        return true;
    } else {
        return (qpci_io_readl(dev, dev->msix_table_bar,
                              vector_off + PCI_MSIX_ENTRY_VECTOR_CTRL)
                & PCI_MSIX_ENTRY_CTRL_MASKBIT) != 0;
    }
}

uint16_t qpci_msix_table_size(QPCIDevice *dev)
{
    uint8_t addr;
    uint16_t control;

    addr = qpci_find_capability(dev, PCI_CAP_ID_MSIX, 0);
    g_assert_cmphex(addr, !=, 0);

    control = qpci_config_readw(dev, addr + PCI_MSIX_FLAGS);
    return (control & PCI_MSIX_FLAGS_QSIZE) + 1;
}

uint8_t qpci_config_readb(QPCIDevice *dev, uint8_t offset)
{
    return dev->bus->config_readb(dev->bus, dev->devfn, offset);
}

uint16_t qpci_config_readw(QPCIDevice *dev, uint8_t offset)
{
    return dev->bus->config_readw(dev->bus, dev->devfn, offset);
}

uint32_t qpci_config_readl(QPCIDevice *dev, uint8_t offset)
{
    return dev->bus->config_readl(dev->bus, dev->devfn, offset);
}


void qpci_config_writeb(QPCIDevice *dev, uint8_t offset, uint8_t value)
{
    dev->bus->config_writeb(dev->bus, dev->devfn, offset, value);
}

void qpci_config_writew(QPCIDevice *dev, uint8_t offset, uint16_t value)
{
    dev->bus->config_writew(dev->bus, dev->devfn, offset, value);
}

void qpci_config_writel(QPCIDevice *dev, uint8_t offset, uint32_t value)
{
    dev->bus->config_writel(dev->bus, dev->devfn, offset, value);
}

uint8_t qpci_io_readb(QPCIDevice *dev, QPCIBar token, uint64_t off)
{
    QPCIBus *bus = dev->bus;

    if (token.is_io) {
        return bus->pio_readb(bus, token.addr + off);
    } else {
        uint8_t val;

        bus->memread(dev->bus, token.addr + off, &val, sizeof(val));
        return val;
    }
}

uint16_t qpci_io_readw(QPCIDevice *dev, QPCIBar token, uint64_t off)
{
    QPCIBus *bus = dev->bus;

    if (token.is_io) {
        return bus->pio_readw(bus, token.addr + off);
    } else {
        uint16_t val;

        bus->memread(bus, token.addr + off, &val, sizeof(val));
        return le16_to_cpu(val);
    }
}

uint32_t qpci_io_readl(QPCIDevice *dev, QPCIBar token, uint64_t off)
{
    QPCIBus *bus = dev->bus;

    if (token.is_io) {
        return bus->pio_readl(bus, token.addr + off);
    } else {
        uint32_t val;

        bus->memread(dev->bus, token.addr + off, &val, sizeof(val));
        return le32_to_cpu(val);
    }
}

uint64_t qpci_io_readq(QPCIDevice *dev, QPCIBar token, uint64_t off)
{
    QPCIBus *bus = dev->bus;

    if (token.is_io) {
        return bus->pio_readq(bus, token.addr + off);
    } else {
        uint64_t val;

        bus->memread(bus, token.addr + off, &val, sizeof(val));
        return le64_to_cpu(val);
    }
}

void qpci_io_writeb(QPCIDevice *dev, QPCIBar token, uint64_t off,
                    uint8_t value)
{
    QPCIBus *bus = dev->bus;

    if (token.is_io) {
        bus->pio_writeb(bus, token.addr + off, value);
    } else {
        bus->memwrite(bus, token.addr + off, &value, sizeof(value));
    }
}

void qpci_io_writew(QPCIDevice *dev, QPCIBar token, uint64_t off,
                    uint16_t value)
{
    QPCIBus *bus = dev->bus;

    if (token.is_io) {
        bus->pio_writew(bus, token.addr + off, value);
    } else {
        value = cpu_to_le16(value);
        bus->memwrite(bus, token.addr + off, &value, sizeof(value));
    }
}

void qpci_io_writel(QPCIDevice *dev, QPCIBar token, uint64_t off,
                    uint32_t value)
{
    QPCIBus *bus = dev->bus;

    if (token.is_io) {
        bus->pio_writel(bus, token.addr + off, value);
    } else {
        value = cpu_to_le32(value);
        bus->memwrite(bus, token.addr + off, &value, sizeof(value));
    }
}

void qpci_io_writeq(QPCIDevice *dev, QPCIBar token, uint64_t off,
                    uint64_t value)
{
    QPCIBus *bus = dev->bus;

    if (token.is_io) {
        bus->pio_writeq(bus, token.addr + off, value);
    } else {
        value = cpu_to_le64(value);
        bus->memwrite(bus, token.addr + off, &value, sizeof(value));
    }
}

void qpci_memread(QPCIDevice *dev, QPCIBar token, uint64_t off,
                  void *buf, size_t len)
{
    g_assert(!token.is_io);
    dev->bus->memread(dev->bus, token.addr + off, buf, len);
}

void qpci_memwrite(QPCIDevice *dev, QPCIBar token, uint64_t off,
                   const void *buf, size_t len)
{
    g_assert(!token.is_io);
    dev->bus->memwrite(dev->bus, token.addr + off, buf, len);
}

QPCIBar qpci_iomap(QPCIDevice *dev, int barno, uint64_t *sizeptr)
{
    QPCIBus *bus = dev->bus;
    static const int bar_reg_map[] = {
        PCI_BASE_ADDRESS_0, PCI_BASE_ADDRESS_1, PCI_BASE_ADDRESS_2,
        PCI_BASE_ADDRESS_3, PCI_BASE_ADDRESS_4, PCI_BASE_ADDRESS_5,
    };
    QPCIBar bar;
    int bar_reg;
    uint32_t addr, size;
    uint32_t io_type;
    uint64_t loc;

    g_assert(barno >= 0 && barno <= 5);
    bar_reg = bar_reg_map[barno];

    qpci_config_writel(dev, bar_reg, 0xFFFFFFFF);
    addr = qpci_config_readl(dev, bar_reg);

    io_type = addr & PCI_BASE_ADDRESS_SPACE;
    if (io_type == PCI_BASE_ADDRESS_SPACE_IO) {
        addr &= PCI_BASE_ADDRESS_IO_MASK;
    } else {
        addr &= PCI_BASE_ADDRESS_MEM_MASK;
    }

    g_assert(addr); /* Must have *some* size bits */

    size = 1U << ctz32(addr);
    if (sizeptr) {
        *sizeptr = size;
    }

    if (io_type == PCI_BASE_ADDRESS_SPACE_IO) {
        loc = QEMU_ALIGN_UP(bus->pio_alloc_ptr, size);

        g_assert(loc >= bus->pio_alloc_ptr);
        g_assert(loc + size <= bus->pio_limit);

        bus->pio_alloc_ptr = loc + size;
        bar.is_io = true;

        qpci_config_writel(dev, bar_reg, loc | PCI_BASE_ADDRESS_SPACE_IO);
    } else {
        loc = QEMU_ALIGN_UP(bus->mmio_alloc_ptr, size);

        /* Check for space */
        g_assert(loc >= bus->mmio_alloc_ptr);
        g_assert(loc + size <= bus->mmio_limit);

        bus->mmio_alloc_ptr = loc + size;
        bar.is_io = false;

        qpci_config_writel(dev, bar_reg, loc);
    }

    bar.addr = loc;
    return bar;
}

void qpci_iounmap(QPCIDevice *dev, QPCIBar bar)
{
    /* FIXME */
}

QPCIBar qpci_legacy_iomap(QPCIDevice *dev, uint16_t addr)
{
    QPCIBar bar = { .addr = addr, .is_io = true };
    return bar;
}

void add_qpci_address(QOSGraphEdgeOptions *opts, QPCIAddress *addr)
{
    g_assert(addr);
    g_assert(opts);

    opts->arg = addr;
    opts->size_arg = sizeof(QPCIAddress);
}
