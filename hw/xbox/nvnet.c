/*
 * QEMU nForce Ethernet Controller implementation
 *
 * Copyright (c) 2013 espes
 * Copyright (c) 2015-2021 Matt Borgerson
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
#include "trace.h"
#include "hw/hw.h"
#include "hw/i386/pc.h"
#include "hw/pci/pci.h"
#include "hw/qdev-properties.h"
#include "net/net.h"
#include "qemu/bswap.h"
#include "qemu/iov.h"
#include "migration/vmstate.h"
#include "nvnet_regs.h"

#define IOPORT_SIZE 0x8
#define MMIO_SIZE   0x400

static const uint8_t bcast[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

// #define DEBUG
#ifdef DEBUG
#   define NVNET_DPRINTF(format, ...) printf(format, ## __VA_ARGS__)
#   define NVNET_DUMP_PACKETS_TO_SCREEN
#else
#   define NVNET_DPRINTF(format, ...) do { } while (0)
#endif

static NetClientInfo net_nvnet_info;
static Property nvnet_properties[];

/*******************************************************************************
 * Primary State Structure
 ******************************************************************************/

typedef struct NvNetState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/

    NICState     *nic;
    NICConf      conf;
    MemoryRegion mmio, io;

    uint8_t      regs[MMIO_SIZE];
    uint32_t     phy_regs[6];
    uint8_t      tx_ring_index;
    uint8_t      tx_ring_size;
    uint8_t      rx_ring_index;
    uint8_t      rx_ring_size;
    uint8_t      tx_dma_buf[TX_ALLOC_BUFSIZE];
    uint32_t     tx_dma_buf_offset;
    uint8_t      rx_dma_buf[RX_ALLOC_BUFSIZE];

    FILE         *packet_dump_file;
    char         *packet_dump_path;
} NvNetState;

#pragma pack(1)
struct RingDesc {
    uint32_t packet_buffer;
    uint16_t length;
    uint16_t flags;
};
#pragma pack()

/*******************************************************************************
 * Helper Macros
 ******************************************************************************/

#define NVNET_DEVICE(obj) \
    OBJECT_CHECK(NvNetState, (obj), "nvnet")

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/* Init */
static void nvnet_realize(PCIDevice *dev, Error **errp);
static void nvnet_uninit(PCIDevice *dev);
static void nvnet_class_init(ObjectClass *klass, void *data);
static void nvnet_cleanup(NetClientState *nc);
static void nvnet_reset(void *opaque);
static void qdev_nvnet_reset(DeviceState *dev);
static void nvnet_register(void);

/* MMIO / IO / Phy / Device Register Access */
static uint64_t nvnet_mmio_read(void *opaque,
    hwaddr addr, unsigned int size);
static void nvnet_mmio_write(void *opaque,
    hwaddr addr, uint64_t val, unsigned int size);
static uint32_t nvnet_get_reg(NvNetState *s,
    hwaddr addr, unsigned int size);
static void nvnet_set_reg(NvNetState *s,
    hwaddr addr, uint32_t val, unsigned int size);
static uint64_t nvnet_io_read(void *opaque,
    hwaddr addr, unsigned int size);
static void nvnet_io_write(void *opaque,
    hwaddr addr, uint64_t val, unsigned int size);
static int nvnet_mii_rw(NvNetState *s,
    uint64_t val);

/* Link State */
static void nvnet_link_down(NvNetState *s);
static void nvnet_link_up(NvNetState *s);
static void nvnet_set_link_status(NetClientState *nc);

/* Interrupts */
static void nvnet_update_irq(NvNetState *s);

/* Packet Tx / Rx */
static void nvnet_send_packet(NvNetState *s,
    const uint8_t *buf, int size);
static ssize_t nvnet_dma_packet_to_guest(NvNetState *s,
    const uint8_t *buf, size_t size);
static ssize_t nvnet_dma_packet_from_guest(NvNetState *s);
static bool nvnet_can_receive(NetClientState *nc);
static ssize_t nvnet_receive(NetClientState *nc,
    const uint8_t *buf, size_t size);
static ssize_t nvnet_receive_iov(NetClientState *nc,
    const struct iovec *iov, int iovcnt);

/* Utility Functions */
static void nvnet_hex_dump(NvNetState *s, const uint8_t *buf, int size);

static const char *nvnet_get_reg_name(hwaddr addr);
static const char *nvnet_get_mii_reg_name(uint8_t reg);
#ifdef DEBUG
static void nvnet_dump_ring_descriptors(NvNetState *s);
#endif

/*******************************************************************************
 * IRQ
 ******************************************************************************/

/*
 * Update IRQ status
 */
static void nvnet_update_irq(NvNetState *s)
{
    PCIDevice *d = PCI_DEVICE(s);

    uint32_t irq_mask = nvnet_get_reg(s, NvRegIrqMask, 4);
    uint32_t irq_status = nvnet_get_reg(s, NvRegIrqStatus, 4);

    if (irq_mask & irq_status) {
        NVNET_DPRINTF("Asserting IRQ\n");
        pci_irq_assert(d);
    } else {
        pci_irq_deassert(d);
    }
}

/*******************************************************************************
 * Register Control
 ******************************************************************************/

/*
 * Read backing store for a device register.
 */
static uint32_t nvnet_get_reg(NvNetState *s, hwaddr addr, unsigned int size)
{
    assert(addr < MMIO_SIZE);

    switch (size) {
    case 4:
        assert((addr & 3) == 0); /* Unaligned register access. */
        return ((uint32_t *)s->regs)[addr >> 2];

    case 2:
        assert((addr & 1) == 0); /* Unaligned register access. */
        return ((uint16_t *)s->regs)[addr >> 1];

    case 1:
        return s->regs[addr];

    default:
        assert(0); /* Unsupported register access. */
        return 0;
    }
}

/*
 * Write backing store for a device register.
 */
static void nvnet_set_reg(NvNetState *s,
                          hwaddr addr, uint32_t val, unsigned int size)
{
    assert(addr < MMIO_SIZE);

    switch (size) {
    case 4:
        assert((addr & 3) == 0); /* Unaligned register access. */
        ((uint32_t *)s->regs)[addr >> 2] = val;
        break;

    case 2:
        assert((addr & 1) == 0); /* Unaligned register access. */
        ((uint16_t *)s->regs)[addr >> 1] = (uint16_t)val;
        break;

    case 1:
        s->regs[addr] = (uint8_t)val;
        break;

    default:
        assert(0); /* Unsupported register access. */
    }
}

/*******************************************************************************
 * PHY Control
 ******************************************************************************/

/*
 * Read from PHY.
 */
static int nvnet_mii_rw(NvNetState *s, uint64_t val)
{
    uint32_t mii_ctl;
    int write, retval, phy_addr, reg;

    retval   = 0;
    mii_ctl  = nvnet_get_reg(s, NvRegMIIControl, 4);
    phy_addr = (mii_ctl >> NVREG_MIICTL_ADDRSHIFT) & 0x1f;
    reg      = mii_ctl & ((1 << NVREG_MIICTL_ADDRSHIFT) - 1);
    write    = mii_ctl & NVREG_MIICTL_WRITE;

    if (phy_addr != 1) {
        retval = -1;
        goto out;
    }

    if (write) {
        goto out;
    }

    switch (reg) {
    case MII_BMSR:
        /* Phy initialization code waits for BIT2 to be set.. If not set,
         * software may report controller as not running */
        retval = BMSR_ANEGCOMPLETE | BMSR_BIT2;
        break;

    case MII_ADVERTISE:
        /* Fall through... */

    case MII_LPA:
        retval = LPA_10HALF | LPA_10FULL;
        retval |= LPA_100HALF | LPA_100FULL | LPA_100BASE4;
        break;

    default:
        break;
    }

out:
    if (write) {
        trace_nvnet_mii_write(phy_addr, reg, nvnet_get_mii_reg_name(reg), val);
    } else {
        trace_nvnet_mii_read(phy_addr, reg, nvnet_get_mii_reg_name(reg),
                             retval);
    }
    return retval;
}

/*******************************************************************************
 * MMIO Read / Write
 ******************************************************************************/

/*
 * Handler for guest reads from MMIO ranges owned by this device.
 */
static uint64_t nvnet_mmio_read(void *opaque, hwaddr addr, unsigned int size)
{
    NvNetState *s;
    uint64_t retval;

    s = NVNET_DEVICE(opaque);

    switch (addr) {
    case NvRegMIIData:
        assert(size == 4);
        retval = nvnet_mii_rw(s, MII_READ);
        break;

    case NvRegMIIControl:
        retval = nvnet_get_reg(s, addr, size);
        retval &= ~NVREG_MIICTL_INUSE;
        break;

    case NvRegMIIStatus:
        retval = 0;
        break;

    default:
        retval = nvnet_get_reg(s, addr, size);
        break;
    }

    trace_nvnet_reg_read(addr, nvnet_get_reg_name(addr & ~3), size, retval);
    return retval;
}

/*
 * Handler for guest writes to MMIO ranges owned by this device.
 */
static void nvnet_mmio_write(void *opaque, hwaddr addr,
                             uint64_t val, unsigned int size)
{
    NvNetState *s = NVNET_DEVICE(opaque);
    uint32_t temp;

    trace_nvnet_reg_write(addr, nvnet_get_reg_name(addr & ~3), size, val);

    switch (addr) {
    case NvRegRingSizes:
        nvnet_set_reg(s, addr, val, size);
        s->rx_ring_size = ((val >> NVREG_RINGSZ_RXSHIFT) & 0xffff) + 1;
        s->tx_ring_size = ((val >> NVREG_RINGSZ_TXSHIFT) & 0xffff) + 1;
        break;

    case NvRegMIIData:
        nvnet_mii_rw(s, val);
        break;

    case NvRegTxRxControl:
        if (val == NVREG_TXRXCTL_KICK) {
            NVNET_DPRINTF("NvRegTxRxControl = NVREG_TXRXCTL_KICK!\n");
#ifdef DEBUG
            nvnet_dump_ring_descriptors(s);
#endif
            nvnet_dma_packet_from_guest(s);
        }

        if (val & NVREG_TXRXCTL_BIT2) {
            nvnet_set_reg(s, NvRegTxRxControl, NVREG_TXRXCTL_IDLE, 4);
            break;
        }

        if (val & NVREG_TXRXCTL_RESET) {
            s->tx_ring_index = 0;
            s->rx_ring_index = 0;
            s->tx_dma_buf_offset = 0;
        }

        if (val & NVREG_TXRXCTL_BIT1) {
            // FIXME
            nvnet_set_reg(s, NvRegIrqStatus, 0, 4);
            break;
        } else if (val == 0) {
            temp = nvnet_get_reg(s, NvRegUnknownSetupReg3, 4);
            if (temp == NVREG_UNKSETUP3_VAL1) {
                /* forcedeth waits for this bit to be set... */
                nvnet_set_reg(s, NvRegUnknownSetupReg5,
                                 NVREG_UNKSETUP5_BIT31, 4);
                break;
            }
        }

        nvnet_set_reg(s, NvRegTxRxControl, val, size);
        break;

    case NvRegIrqMask:
        nvnet_set_reg(s, addr, val, size);
        nvnet_update_irq(s);
        break;

    case NvRegIrqStatus:
        nvnet_set_reg(s, addr, nvnet_get_reg(s, addr, size) & ~val, size);
        nvnet_update_irq(s);
        break;

    default:
        nvnet_set_reg(s, addr, val, size);
        break;
    }
}

static const MemoryRegionOps nvnet_mmio_ops = {
    .read = nvnet_mmio_read,
    .write = nvnet_mmio_write,
};

/*******************************************************************************
 * Packet TX / RX
 ******************************************************************************/

static void nvnet_send_packet(NvNetState *s, const uint8_t *buf, int size)
{
    NetClientState *nc = qemu_get_queue(s->nic);

    NVNET_DPRINTF("nvnet: Sending packet!\n");
    nvnet_hex_dump(s, buf, size);
    qemu_send_packet(nc, buf, size);
}

static bool nvnet_can_receive(NetClientState *nc)
{
    NVNET_DPRINTF("nvnet_can_receive called\n");
    return 1;
}

static ssize_t nvnet_receive(NetClientState *nc,
                             const uint8_t *buf, size_t size)
{
    const struct iovec iov = {
        .iov_base = (uint8_t *)buf,
        .iov_len = size
    };

    NVNET_DPRINTF("nvnet_receive called\n");
    return nvnet_receive_iov(nc, &iov, 1);
}

static bool nvnet_is_packet_oversized(size_t size)
{
    return size > RX_ALLOC_BUFSIZE;
}

static bool receive_filter(NvNetState *s, const uint8_t *buf, int size)
{
    if (size < 6) {
        return false;
    }

    uint32_t rctl = nvnet_get_reg(s, NvRegPacketFilterFlags, 4);
    int isbcast = !memcmp(buf, bcast, sizeof bcast);

    /* Broadcast */
    if (isbcast) {
        /* FIXME: bcast filtering */
        trace_nvnet_rx_filter_bcast_match();
        return true;
    }

    if (!(rctl & NVREG_PFF_MYADDR)) {
        /* FIXME: Confirm PFF_MYADDR filters mcast */
        return true;
    }

    /* Multicast */
    uint32_t addr[2];
    addr[0] = cpu_to_le32(nvnet_get_reg(s, NvRegMulticastAddrA, 4));
    addr[1] = cpu_to_le32(nvnet_get_reg(s, NvRegMulticastAddrB, 4));
    if (memcmp(addr, bcast, sizeof bcast)) {
        uint32_t dest_addr[2];
        memcpy(dest_addr, buf, 6);
        dest_addr[0] &= cpu_to_le32(nvnet_get_reg(s, NvRegMulticastMaskA, 4));
        dest_addr[1] &= cpu_to_le32(nvnet_get_reg(s, NvRegMulticastMaskB, 4));

        if (!memcmp(dest_addr, addr, 6)) {
            trace_nvnet_rx_filter_mcast_match(MAC_ARG(dest_addr));
            return true;
        } else {
            trace_nvnet_rx_filter_mcast_mismatch(MAC_ARG(dest_addr));
        }
    }

    /* Unicast */
    addr[0] = cpu_to_le32(nvnet_get_reg(s, NvRegMacAddrA, 4));
    addr[1] = cpu_to_le32(nvnet_get_reg(s, NvRegMacAddrB, 4));
    if (!memcmp(buf, addr, 6)) {
        trace_nvnet_rx_filter_ucast_match(MAC_ARG(buf));
        return true;
    } else {
        trace_nvnet_rx_filter_ucast_mismatch(MAC_ARG(buf));
    }

    return false;
}

static ssize_t nvnet_receive_iov(NetClientState *nc,
                                 const struct iovec *iov, int iovcnt)
{
    NvNetState *s = qemu_get_nic_opaque(nc);
    size_t size = iov_size(iov, iovcnt);

    NVNET_DPRINTF("nvnet: Packet received!\n");

    if (nvnet_is_packet_oversized(size)) {
        /* Drop */
        NVNET_DPRINTF("%s packet too large!\n", __func__);
        trace_nvnet_rx_oversized(size);
        return size;
    }

    iov_to_buf(iov, iovcnt, 0, s->rx_dma_buf, size);

    if (!receive_filter(s, s->rx_dma_buf, size)) {
        trace_nvnet_rx_filter_dropped();
        return size;
    }

#ifdef DEBUG
    nvnet_hex_dump(s, s->rx_dma_buf, size);
#endif
    return nvnet_dma_packet_to_guest(s, s->rx_dma_buf, size);
}


static ssize_t nvnet_dma_packet_to_guest(NvNetState *s,
                                         const uint8_t *buf, size_t size)
{
    PCIDevice *d = PCI_DEVICE(s);
    struct RingDesc desc;
    int i;
    bool did_receive = false;

    nvnet_set_reg(s, NvRegTxRxControl,
        nvnet_get_reg(s, NvRegTxRxControl, 4) & ~NVREG_TXRXCTL_IDLE,
        4);

    for (i = 0; i < s->rx_ring_size; i++) {
        /* Read current ring descriptor */
        s->rx_ring_index %= s->rx_ring_size;
        dma_addr_t rx_ring_addr = nvnet_get_reg(s, NvRegRxRingPhysAddr, 4);
        rx_ring_addr += s->rx_ring_index * sizeof(desc);
        pci_dma_read(d, rx_ring_addr, &desc, sizeof(desc));
        NVNET_DPRINTF("RX: Looking at ring descriptor %d (0x%" HWADDR_PRIx "): ",
                      s->rx_ring_index, rx_ring_addr);
        NVNET_DPRINTF("Buffer: 0x%x, ", desc.packet_buffer);
        NVNET_DPRINTF("Length: 0x%x, ", desc.length);
        NVNET_DPRINTF("Flags: 0x%x\n", desc.flags);

        if (!(desc.flags & NV_RX_AVAIL)) {
            break;
        }

        assert((desc.length+1) >= size); // FIXME

        s->rx_ring_index += 1;

        /* Transfer packet from device to memory */
        NVNET_DPRINTF("Transferring packet, size 0x%zx, to memory at 0x%x\n",
                      size, desc.packet_buffer);
        pci_dma_write(d, desc.packet_buffer, buf, size);

        /* Update descriptor indicating the packet is waiting */
        desc.length = size;
        desc.flags  = NV_RX_BIT4 | NV_RX_DESCRIPTORVALID;
        pci_dma_write(d, rx_ring_addr, &desc, sizeof(desc));
        NVNET_DPRINTF("Updated ring descriptor: ");
        NVNET_DPRINTF("Length: 0x%x, ", desc.length);
        NVNET_DPRINTF("Flags: 0x%x\n", desc.flags);

        /* Trigger interrupt */
        NVNET_DPRINTF("Triggering interrupt\n");
        uint32_t irq_status = nvnet_get_reg(s, NvRegIrqStatus, 4);
        nvnet_set_reg(s, NvRegIrqStatus, irq_status | NVREG_IRQSTAT_BIT1, 4);
        nvnet_update_irq(s);
        did_receive = true;
        break;
    }

    nvnet_set_reg(s, NvRegTxRxControl,
        nvnet_get_reg(s, NvRegTxRxControl, 4) | NVREG_TXRXCTL_IDLE,
        4);

    if (did_receive) {
        return size;
    } else {
        /* Could not find free buffer, or packet too large. */
        NVNET_DPRINTF("Could not find free buffer!\n");
        return -1;
    }
}

static ssize_t nvnet_dma_packet_from_guest(NvNetState *s)
{
    PCIDevice *d = PCI_DEVICE(s);
    struct RingDesc desc;
    bool is_last_packet;
    bool packet_sent = false;
    int i;

    nvnet_set_reg(s, NvRegTxRxControl,
        nvnet_get_reg(s, NvRegTxRxControl, 4) & ~NVREG_TXRXCTL_IDLE,
        4);

    for (i = 0; i < s->tx_ring_size; i++) {
        /* Read ring descriptor */
        s->tx_ring_index %= s->tx_ring_size;
        dma_addr_t tx_ring_addr = nvnet_get_reg(s, NvRegTxRingPhysAddr, 4);
        tx_ring_addr += s->tx_ring_index * sizeof(desc);
        pci_dma_read(d, tx_ring_addr, &desc, sizeof(desc));
        NVNET_DPRINTF("TX: Looking at ring desc %d (%" HWADDR_PRIx "): ",
                      s->tx_ring_index, tx_ring_addr);
        NVNET_DPRINTF("Buffer: 0x%x, ", desc.packet_buffer);
        NVNET_DPRINTF("Length: 0x%x, ", desc.length);
        NVNET_DPRINTF("Flags: 0x%x\n", desc.flags);

        if (!(desc.flags & NV_TX_VALID)) {
            break;
        }

        s->tx_ring_index += 1;

        /* Transfer packet from guest memory */
        assert((s->tx_dma_buf_offset + desc.length + 1) <= sizeof(s->tx_dma_buf));
        pci_dma_read(d, desc.packet_buffer,
                     &s->tx_dma_buf[s->tx_dma_buf_offset],
                     desc.length + 1);
        s->tx_dma_buf_offset += desc.length + 1;

        /* Update descriptor */
        is_last_packet = desc.flags & NV_TX_LASTPACKET;
        if (is_last_packet) {
            NVNET_DPRINTF("Sending packet...\n");
            nvnet_send_packet(s, s->tx_dma_buf, s->tx_dma_buf_offset);
            s->tx_dma_buf_offset = 0;
            packet_sent = true;
        }

        desc.flags &= ~(NV_TX_VALID | NV_TX_RETRYERROR | NV_TX_DEFERRED |
            NV_TX_CARRIERLOST | NV_TX_LATECOLLISION | NV_TX_UNDERFLOW |
            NV_TX_ERROR);
        desc.length = desc.length + 5;
        pci_dma_write(d, tx_ring_addr, &desc, sizeof(desc));

        if (is_last_packet) {
            // FIXME
            break;
        }
    }

    /* Trigger interrupt */
    if (packet_sent) {
        NVNET_DPRINTF("Triggering interrupt\n");
        uint32_t irq_status = nvnet_get_reg(s, NvRegIrqStatus, 4);
        nvnet_set_reg(s, NvRegIrqStatus, irq_status | NVREG_IRQSTAT_BIT4, 4);
        nvnet_update_irq(s);
    }

    nvnet_set_reg(s, NvRegTxRxControl,
        nvnet_get_reg(s, NvRegTxRxControl, 4) | NVREG_TXRXCTL_IDLE,
        4);

    return 0;
}

/*******************************************************************************
 * Link Status Control
 ******************************************************************************/

static void nvnet_link_down(NvNetState *s)
{
    NVNET_DPRINTF("nvnet_link_down called\n");
}

static void nvnet_link_up(NvNetState *s)
{
    NVNET_DPRINTF("nvnet_link_up called\n");
}

static void nvnet_set_link_status(NetClientState *nc)
{
    NvNetState *s = qemu_get_nic_opaque(nc);
    if (nc->link_down) {
        nvnet_link_down(s);
    } else {
        nvnet_link_up(s);
    }
}

/*******************************************************************************
 * IO Read / Write
 ******************************************************************************/

static uint64_t nvnet_io_read(void *opaque, hwaddr addr, unsigned int size)
{
    uint64_t r = 0;
    trace_nvnet_io_read(addr, size, r);
    return r;
}

static void nvnet_io_write(void *opaque,
                           hwaddr addr, uint64_t val, unsigned int size)
{
    trace_nvnet_io_write(addr, size, val);
}

static const MemoryRegionOps nvnet_io_ops = {
    .read  = nvnet_io_read,
    .write = nvnet_io_write,
};

/*******************************************************************************
 * Init
 ******************************************************************************/

static void nvnet_realize(PCIDevice *pci_dev, Error **errp)
{
    DeviceState *dev = DEVICE(pci_dev);
    NvNetState *s = NVNET_DEVICE(pci_dev);
    PCIDevice *d = PCI_DEVICE(s);

    pci_dev->config[PCI_INTERRUPT_PIN] = 0x01;

    s->packet_dump_file = NULL;
    if (s->packet_dump_path && *s->packet_dump_path != '\x00') {
        s->packet_dump_file = fopen(s->packet_dump_path, "wb");
        if (!s->packet_dump_file) {
            fprintf(stderr, "Failed to open %s for writing!\n",
                            s->packet_dump_path);
            exit(1);
        }
    }

    memset(s->regs, 0, sizeof(s->regs));

    s->rx_ring_index = 0;
    s->rx_ring_size  = 0;
    s->tx_ring_index = 0;
    s->tx_ring_size  = 0;

    memory_region_init_io(&s->mmio, OBJECT(dev), &nvnet_mmio_ops, s,
        "nvnet-mmio", MMIO_SIZE);
    pci_register_bar(d, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->mmio);

    memory_region_init_io(&s->io, OBJECT(dev), &nvnet_io_ops, s,
        "nvnet-io", IOPORT_SIZE);
    pci_register_bar(d, 1, PCI_BASE_ADDRESS_SPACE_IO, &s->io);

    qemu_macaddr_default_if_unset(&s->conf.macaddr);
    s->nic = qemu_new_nic(&net_nvnet_info, &s->conf,
        object_get_typename(OBJECT(s)), dev->id, s);
    assert(s->nic);
}

static void nvnet_uninit(PCIDevice *dev)
{
    NvNetState *s = NVNET_DEVICE(dev);

    if (s->packet_dump_file) {
        fclose(s->packet_dump_file);
    }

    // memory_region_destroy(&s->mmio);
    // memory_region_destroy(&s->io);
    qemu_del_nic(s->nic);
}

void nvnet_cleanup(NetClientState *nc)
{
}

static void nvnet_reset(void *opaque)
{
    NvNetState *s = opaque;

    if (qemu_get_queue(s->nic)->link_down) {
        nvnet_link_down(s);
    }

    memset(&s->regs, 0, sizeof(s->regs));
    memset(&s->phy_regs, 0, sizeof(s->phy_regs));
    s->tx_ring_index = 0;
    s->tx_ring_size = 0;
    s->rx_ring_index = 0;
    s->rx_ring_size = 0;
    memset(&s->tx_dma_buf, 0, sizeof(s->tx_dma_buf));
    s->tx_dma_buf_offset = 0;
    memset(&s->rx_dma_buf, 0, sizeof(s->rx_dma_buf));
}

static void qdev_nvnet_reset(DeviceState *dev)
{
    NvNetState *s = NVNET_DEVICE(dev);
    nvnet_reset(s);
}

/*******************************************************************************
 * Utility Functions
 ******************************************************************************/

static void hex_dump(FILE *f, const uint8_t *buf, int size)
{
    int len, i, j, c;

    for (i = 0; i < size; i += 16) {
        len = size - i;
        if (len > 16) {
            len = 16;
        }
        fprintf(f, "%08x ", i);
        for (j = 0; j < 16; j++) {
            if (j < len) {
                fprintf(f, " %02x", buf[i + j]);
            } else {
                fprintf(f, "   ");
            }
        }
        fprintf(f, " ");
        for (j = 0; j < len; j++) {
            c = buf[i + j];
            if (c < ' ' || c > '~') {
                c = '.';
            }
            fprintf(f, "%c", c);
        }
        fprintf(f, "\n");
    }
}

static void nvnet_hex_dump(NvNetState *s, const uint8_t *buf, int size)
{
#ifdef NVNET_DUMP_PACKETS_TO_SCREEN
    hex_dump(stdout, buf, size);
#endif
    if (s->packet_dump_file) {
        hex_dump(s->packet_dump_file, buf, size);
    }
}

/*
 * Return register name given the offset of the device register.
 */
static const char *nvnet_get_reg_name(hwaddr addr)
{
    switch (addr) {
    case NvRegIrqStatus:             return "NvRegIrqStatus";
    case NvRegIrqMask:               return "NvRegIrqMask";
    case NvRegUnknownSetupReg6:      return "NvRegUnknownSetupReg6";
    case NvRegPollingInterval:       return "NvRegPollingInterval";
    case NvRegMisc1:                 return "NvRegMisc1";
    case NvRegTransmitterControl:    return "NvRegTransmitterControl";
    case NvRegTransmitterStatus:     return "NvRegTransmitterStatus";
    case NvRegPacketFilterFlags:     return "NvRegPacketFilterFlags";
    case NvRegOffloadConfig:         return "NvRegOffloadConfig";
    case NvRegReceiverControl:       return "NvRegReceiverControl";
    case NvRegReceiverStatus:        return "NvRegReceiverStatus";
    case NvRegRandomSeed:            return "NvRegRandomSeed";
    case NvRegUnknownSetupReg1:      return "NvRegUnknownSetupReg1";
    case NvRegUnknownSetupReg2:      return "NvRegUnknownSetupReg2";
    case NvRegMacAddrA:              return "NvRegMacAddrA";
    case NvRegMacAddrB:              return "NvRegMacAddrB";
    case NvRegMulticastAddrA:        return "NvRegMulticastAddrA";
    case NvRegMulticastAddrB:        return "NvRegMulticastAddrB";
    case NvRegMulticastMaskA:        return "NvRegMulticastMaskA";
    case NvRegMulticastMaskB:        return "NvRegMulticastMaskB";
    case NvRegTxRingPhysAddr:        return "NvRegTxRingPhysAddr";
    case NvRegRxRingPhysAddr:        return "NvRegRxRingPhysAddr";
    case NvRegRingSizes:             return "NvRegRingSizes";
    case NvRegUnknownTransmitterReg: return "NvRegUnknownTransmitterReg";
    case NvRegLinkSpeed:             return "NvRegLinkSpeed";
    case NvRegUnknownSetupReg5:      return "NvRegUnknownSetupReg5";
    case NvRegUnknownSetupReg3:      return "NvRegUnknownSetupReg3";
    case NvRegUnknownSetupReg8:      return "NvRegUnknownSetupReg8";
    case NvRegUnknownSetupReg7:      return "NvRegUnknownSetupReg7";
    case NvRegTxRxControl:           return "NvRegTxRxControl";
    case NvRegMIIStatus:             return "NvRegMIIStatus";
    case NvRegUnknownSetupReg4:      return "NvRegUnknownSetupReg4";
    case NvRegAdapterControl:        return "NvRegAdapterControl";
    case NvRegMIISpeed:              return "NvRegMIISpeed";
    case NvRegMIIControl:            return "NvRegMIIControl";
    case NvRegMIIData:               return "NvRegMIIData";
    case NvRegWakeUpFlags:           return "NvRegWakeUpFlags";
    case NvRegPatternCRC:            return "NvRegPatternCRC";
    case NvRegPatternMask:           return "NvRegPatternMask";
    case NvRegPowerCap:              return "NvRegPowerCap";
    case NvRegPowerState:            return "NvRegPowerState";
    default:                         return "Unknown";
    }
}

/*
 * Get PHY register name.
 */
static const char *nvnet_get_mii_reg_name(uint8_t reg)
{
    switch (reg) {
    case MII_PHYSID1:   return "MII_PHYSID1";
    case MII_PHYSID2:   return "MII_PHYSID2";
    case MII_BMCR:      return "MII_BMCR";
    case MII_BMSR:      return "MII_BMSR";
    case MII_ADVERTISE: return "MII_ADVERTISE";
    case MII_LPA:       return "MII_LPA";
    default:            return "Unknown";
    }
}

#ifdef DEBUG
static void nvnet_dump_ring_descriptors(NvNetState *s)
{
    struct RingDesc desc;
    PCIDevice *d = PCI_DEVICE(s);

    NVNET_DPRINTF("------------------------------------------------\n");
    for (int i = 0; i < s->tx_ring_size; i++) {
        /* Read ring descriptor */
        dma_addr_t tx_ring_addr = nvnet_get_reg(s, NvRegTxRingPhysAddr, 4);
        tx_ring_addr += i * sizeof(desc);
        pci_dma_read(d, tx_ring_addr, &desc, sizeof(desc));
        NVNET_DPRINTF("TX: Dumping ring desc %d (%" HWADDR_PRIx "): ",
                      i, tx_ring_addr);
        NVNET_DPRINTF("Buffer: 0x%x, ", desc.packet_buffer);
        NVNET_DPRINTF("Length: 0x%x, ", desc.length);
        NVNET_DPRINTF("Flags: 0x%x\n", desc.flags);
    }
    NVNET_DPRINTF("------------------------------------------------\n");

    for (int i = 0; i < s->rx_ring_size; i++) {
        /* Read ring descriptor */
        dma_addr_t rx_ring_addr = nvnet_get_reg(s, NvRegRxRingPhysAddr, 4);
        rx_ring_addr += i * sizeof(desc);
        pci_dma_read(d, rx_ring_addr, &desc, sizeof(desc));
        NVNET_DPRINTF("RX: Dumping ring desc %d (%" HWADDR_PRIx "): ",
                      i, rx_ring_addr);
        NVNET_DPRINTF("Buffer: 0x%x, ", desc.packet_buffer);
        NVNET_DPRINTF("Length: 0x%x, ", desc.length);
        NVNET_DPRINTF("Flags: 0x%x\n", desc.flags);
    }
    NVNET_DPRINTF("------------------------------------------------\n");
}
#endif

/*******************************************************************************
 * Properties
 ******************************************************************************/

static const VMStateDescription vmstate_nvnet = {
    .name = "nvnet",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_PCI_DEVICE(parent_obj, NvNetState),
        VMSTATE_UINT8_ARRAY(regs, NvNetState, MMIO_SIZE),
        VMSTATE_UINT32_ARRAY(phy_regs, NvNetState, 6),
        VMSTATE_UINT8(tx_ring_index, NvNetState),
        VMSTATE_UINT8(tx_ring_size, NvNetState),
        VMSTATE_UINT8(rx_ring_index, NvNetState),
        VMSTATE_UINT8(rx_ring_size, NvNetState),
        VMSTATE_END_OF_LIST()
    },
};

static void nvnet_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->vendor_id = PCI_VENDOR_ID_NVIDIA;
    k->device_id = PCI_DEVICE_ID_NVIDIA_NVENET_1;
    k->revision = 177;
    k->class_id = PCI_CLASS_NETWORK_ETHERNET;
    k->realize = nvnet_realize;
    k->exit = nvnet_uninit;

    set_bit(DEVICE_CATEGORY_NETWORK, dc->categories);
    dc->desc = "nForce Ethernet Controller";
    dc->reset = qdev_nvnet_reset;
    dc->vmsd = &vmstate_nvnet;
    device_class_set_props(dc, nvnet_properties);
}

static Property nvnet_properties[] = {
    DEFINE_NIC_PROPERTIES(NvNetState, conf),
    DEFINE_PROP_STRING("dump", NvNetState, packet_dump_path),
    DEFINE_PROP_END_OF_LIST(),
};

static NetClientInfo net_nvnet_info = {
    .type                = NET_CLIENT_DRIVER_NIC,
    .size                = sizeof(NICState),
    .can_receive         = nvnet_can_receive,
    .receive             = nvnet_receive,
    .receive_iov         = nvnet_receive_iov,
    .cleanup             = nvnet_cleanup,
    .link_status_changed = nvnet_set_link_status,
};

static const TypeInfo nvnet_info = {
    .name                = "nvnet",
    .parent              = TYPE_PCI_DEVICE,
    .instance_size       = sizeof(NvNetState),
    .class_init          = nvnet_class_init,
    .interfaces          = (InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

static void nvnet_register(void)
{
    type_register_static(&nvnet_info);
}
type_init(nvnet_register);
