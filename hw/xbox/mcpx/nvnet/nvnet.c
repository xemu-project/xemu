/*
 * QEMU nForce Ethernet Controller implementation
 *
 * Copyright (c) 2013 espes
 * Copyright (c) 2015-2025 Matt Borgerson
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
#include "hw/net/mii.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_device.h"
#include "hw/qdev-properties.h"
#include "net/net.h"
#include "net/eth.h"
#include "qemu/bswap.h"
#include "qemu/iov.h"
#include "migration/vmstate.h"
#include "nvnet_regs.h"

#define IOPORT_SIZE 0x8
#define MMIO_SIZE 0x400
#define PHY_ADDR 1

#define GET_MASK(v, mask) (((v) & (mask)) >> ctz32(mask))

#ifndef DEBUG_NVNET
#define DEBUG_NVNET 0
#endif

#define NVNET_DPRINTF(fmt, ...)                  \
    do {                                         \
        if (DEBUG_NVNET) {                       \
            fprintf(stderr, fmt, ##__VA_ARGS__); \
        }                                        \
    } while (0);

#define TYPE_NVNET "nvnet"
OBJECT_DECLARE_SIMPLE_TYPE(NvNetState, NVNET)

typedef struct NvNetState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/

    NICState *nic;
    NICConf conf;
    MemoryRegion mmio, io;

    uint8_t regs[MMIO_SIZE];
    uint32_t phy_regs[6];
    uint8_t tx_ring_index;
    uint8_t tx_ring_size;
    uint8_t rx_ring_index;
    uint8_t rx_ring_size;
    uint8_t tx_dma_buf[TX_ALLOC_BUFSIZE];
    uint32_t tx_dma_buf_offset;
    uint8_t rx_dma_buf[RX_ALLOC_BUFSIZE];
} NvNetState;

struct RingDesc {
    uint32_t packet_buffer;
    uint16_t length;
    uint16_t flags;
} QEMU_PACKED;

#define R(r) \
    case r:  \
        return stringify(r);

static const char *nvnet_get_reg_name(hwaddr addr)
{
    switch (addr) {
        R(NVNET_IRQ_STATUS)
        R(NVNET_IRQ_MASK)
        R(NVNET_UNKNOWN_SETUP_REG6)
        R(NVNET_POLLING_INTERVAL)
        R(NVNET_MISC1)
        R(NVNET_TRANSMITTER_CONTROL)
        R(NVNET_TRANSMITTER_STATUS)
        R(NVNET_PACKET_FILTER)
        R(NVNET_OFFLOAD)
        R(NVNET_RECEIVER_CONTROL)
        R(NVNET_RECEIVER_STATUS)
        R(NVNET_RANDOM_SEED)
        R(NVNET_UNKNOWN_SETUP_REG1)
        R(NVNET_UNKNOWN_SETUP_REG2)
        R(NVNET_MAC_ADDR_A)
        R(NVNET_MAC_ADDR_B)
        R(NVNET_MULTICAST_ADDR_A)
        R(NVNET_MULTICAST_ADDR_B)
        R(NVNET_MULTICAST_MASK_A)
        R(NVNET_MULTICAST_MASK_B)
        R(NVNET_TX_RING_PHYS_ADDR)
        R(NVNET_RX_RING_PHYS_ADDR)
        R(NVNET_RING_SIZE)
        R(NVNET_UNKNOWN_TRANSMITTER_REG)
        R(NVNET_LINKSPEED)
        R(NVNET_UNKNOWN_SETUP_REG5)
        R(NVNET_UNKNOWN_SETUP_REG3)
        R(NVNET_UNKNOWN_SETUP_REG8)
        R(NVNET_UNKNOWN_SETUP_REG7)
        R(NVNET_TX_RX_CONTROL)
        R(NVNET_MII_STATUS)
        R(NVNET_UNKNOWN_SETUP_REG4)
        R(NVNET_ADAPTER_CONTROL)
        R(NVNET_MII_SPEED)
        R(NVNET_MDIO_ADDR)
        R(NVNET_MDIO_DATA)
        R(NVNET_WAKEUPFLAGS)
        R(NVNET_PATTERN_CRC)
        R(NVNET_PATTERN_MASK)
        R(NVNET_POWERCAP)
        R(NVNET_POWERSTATE)
    default:
        return "Unknown";
    }
}

static const char *nvnet_get_mii_reg_name(uint8_t reg)
{
    switch (reg) {
        R(MII_PHYID1)
        R(MII_PHYID2)
        R(MII_BMCR)
        R(MII_BMSR)
        R(MII_ANAR)
        R(MII_ANLPAR)
    default:
        return "Unknown";
    }
}

#undef R

static void nvnet_dump_ring_descriptors(NvNetState *s)
{
#if NVNET_DEBUG
    struct RingDesc desc;
    PCIDevice *d = PCI_DEVICE(s);

    NVNET_DPRINTF("------------------------------------------------\n");
    for (int i = 0; i < s->tx_ring_size; i++) {
        dma_addr_t tx_ring_addr = nvnet_get_reg(s, NVNET_TX_RING_PHYS_ADDR, 4);
        tx_ring_addr += i * sizeof(desc);
        pci_dma_read(d, tx_ring_addr, &desc, sizeof(desc));
        NVNET_DPRINTF("TX: Dumping ring desc %d (%" HWADDR_PRIx "): ", i,
                      tx_ring_addr);
        NVNET_DPRINTF("Buffer: 0x%x, ", desc.packet_buffer);
        NVNET_DPRINTF("Length: 0x%x, ", desc.length);
        NVNET_DPRINTF("Flags: 0x%x\n", desc.flags);
    }
    NVNET_DPRINTF("------------------------------------------------\n");

    for (int i = 0; i < s->rx_ring_size; i++) {
        dma_addr_t rx_ring_addr = nvnet_get_reg(s, NVNET_RX_RING_PHYS_ADDR, 4);
        rx_ring_addr += i * sizeof(desc);
        pci_dma_read(d, rx_ring_addr, &desc, sizeof(desc));
        NVNET_DPRINTF("RX: Dumping ring desc %d (%" HWADDR_PRIx "): ", i,
                      rx_ring_addr);
        NVNET_DPRINTF("Buffer: 0x%x, ", desc.packet_buffer);
        NVNET_DPRINTF("Length: 0x%x, ", desc.length);
        NVNET_DPRINTF("Flags: 0x%x\n", desc.flags);
    }
    NVNET_DPRINTF("------------------------------------------------\n");
#endif
}

static uint32_t nvnet_get_reg(NvNetState *s, hwaddr addr, unsigned int size)
{
    assert(addr < MMIO_SIZE);

    switch (size) {
    case 4:
        assert((addr & 3) == 0);
        return ((uint32_t *)s->regs)[addr >> 2];

    case 2:
        assert((addr & 1) == 0);
        return ((uint16_t *)s->regs)[addr >> 1];

    case 1:
        return s->regs[addr];

    default:
        assert(!"Unsupported register access");
        return 0;
    }
}

static void nvnet_set_reg(NvNetState *s, hwaddr addr, uint32_t val,
                          unsigned int size)
{
    assert(addr < MMIO_SIZE);

    switch (size) {
    case 4:
        assert((addr & 3) == 0);
        ((uint32_t *)s->regs)[addr >> 2] = val;
        break;

    case 2:
        assert((addr & 1) == 0);
        ((uint16_t *)s->regs)[addr >> 1] = (uint16_t)val;
        break;

    case 1:
        s->regs[addr] = (uint8_t)val;
        break;

    default:
        assert(!"Unsupported register access");
    }
}

static void nvnet_update_irq(NvNetState *s)
{
    PCIDevice *d = PCI_DEVICE(s);

    uint32_t irq_mask = nvnet_get_reg(s, NVNET_IRQ_MASK, 4);
    uint32_t irq_status = nvnet_get_reg(s, NVNET_IRQ_STATUS, 4);

    if (irq_mask & irq_status) {
        NVNET_DPRINTF("Asserting IRQ\n");
        pci_irq_assert(d);
    } else {
        pci_irq_deassert(d);
    }
}

static void nvnet_send_packet(NvNetState *s, const uint8_t *buf, int size)
{
    NetClientState *nc = qemu_get_queue(s->nic);

    NVNET_DPRINTF("nvnet: Sending packet!\n");
    qemu_send_packet(nc, buf, size);
}

static ssize_t nvnet_dma_packet_to_guest(NvNetState *s, const uint8_t *buf,
                                         size_t size)
{
    PCIDevice *d = PCI_DEVICE(s);
    bool did_receive = false;

    nvnet_set_reg(s, NVNET_TX_RX_CONTROL,
                  nvnet_get_reg(s, NVNET_TX_RX_CONTROL, 4) &
                      ~NVNET_TX_RX_CONTROL_IDLE,
                  4);

    for (int i = 0; i < s->rx_ring_size; i++) {
        struct RingDesc desc;
        s->rx_ring_index %= s->rx_ring_size;
        dma_addr_t rx_ring_addr = nvnet_get_reg(s, NVNET_RX_RING_PHYS_ADDR, 4);
        rx_ring_addr += s->rx_ring_index * sizeof(desc);
        pci_dma_read(d, rx_ring_addr, &desc, sizeof(desc));

        NVNET_DPRINTF("RX: Looking at ring descriptor %d (0x%" HWADDR_PRIx
                      "): ",
                      s->rx_ring_index, rx_ring_addr);
        NVNET_DPRINTF("Buffer: 0x%x, ", desc.packet_buffer);
        NVNET_DPRINTF("Length: 0x%x, ", desc.length);
        NVNET_DPRINTF("Flags: 0x%x\n", desc.flags);

        if (!(desc.flags & NV_RX_AVAIL)) {
            break;
        }

        assert((desc.length + 1) >= size); // FIXME

        s->rx_ring_index += 1;

        NVNET_DPRINTF("Transferring packet, size 0x%zx, to memory at 0x%x\n",
                      size, desc.packet_buffer);
        pci_dma_write(d, desc.packet_buffer, buf, size);

        desc.length = size;
        desc.flags = NV_RX_BIT4 | NV_RX_DESCRIPTORVALID;
        pci_dma_write(d, rx_ring_addr, &desc, sizeof(desc));

        NVNET_DPRINTF("Updated ring descriptor: ");
        NVNET_DPRINTF("Length: 0x%x, ", desc.length);
        NVNET_DPRINTF("Flags: 0x%x\n", desc.flags);

        /* Trigger interrupt */
        NVNET_DPRINTF("Triggering interrupt\n");
        uint32_t irq_status = nvnet_get_reg(s, NVNET_IRQ_STATUS, 4);
        nvnet_set_reg(s, NVNET_IRQ_STATUS, irq_status | NVNET_IRQ_STATUS_RX, 4);
        nvnet_update_irq(s);
        did_receive = true;
        break;
    }

    nvnet_set_reg(
        s, NVNET_TX_RX_CONTROL,
        nvnet_get_reg(s, NVNET_TX_RX_CONTROL, 4) | NVNET_TX_RX_CONTROL_IDLE, 4);

    if (did_receive) {
        return size;
    } else {
        NVNET_DPRINTF("Could not find free buffer!\n");
        return -1;
    }
}

static ssize_t nvnet_dma_packet_from_guest(NvNetState *s)
{
    PCIDevice *d = PCI_DEVICE(s);
    bool packet_sent = false;

    nvnet_set_reg(s, NVNET_TX_RX_CONTROL,
                  nvnet_get_reg(s, NVNET_TX_RX_CONTROL, 4) &
                      ~NVNET_TX_RX_CONTROL_IDLE,
                  4);

    for (int i = 0; i < s->tx_ring_size; i++) {
        /* Read ring descriptor */
        struct RingDesc desc;
        s->tx_ring_index %= s->tx_ring_size;
        dma_addr_t tx_ring_addr = nvnet_get_reg(s, NVNET_TX_RING_PHYS_ADDR, 4);
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

        assert((s->tx_dma_buf_offset + desc.length + 1) <=
               sizeof(s->tx_dma_buf));
        pci_dma_read(d, desc.packet_buffer,
                     &s->tx_dma_buf[s->tx_dma_buf_offset], desc.length + 1);
        s->tx_dma_buf_offset += desc.length + 1;

        bool is_last_packet = desc.flags & NV_TX_LASTPACKET;
        if (is_last_packet) {
            NVNET_DPRINTF("Sending packet...\n");
            nvnet_send_packet(s, s->tx_dma_buf, s->tx_dma_buf_offset);
            s->tx_dma_buf_offset = 0;
            packet_sent = true;
        }

        desc.flags &= ~(NV_TX_VALID | NV_TX_RETRYERROR | NV_TX_DEFERRED |
                        NV_TX_CARRIERLOST | NV_TX_LATECOLLISION |
                        NV_TX_UNDERFLOW | NV_TX_ERROR);
        desc.length = desc.length + 5;
        pci_dma_write(d, tx_ring_addr, &desc, sizeof(desc));

        if (is_last_packet) {
            // FIXME
            break;
        }
    }

    if (packet_sent) {
        NVNET_DPRINTF("Triggering interrupt\n");
        uint32_t irq_status = nvnet_get_reg(s, NVNET_IRQ_STATUS, 4);
        nvnet_set_reg(s, NVNET_IRQ_STATUS, irq_status | NVNET_IRQ_STATUS_TX, 4);
        nvnet_update_irq(s);
    }

    nvnet_set_reg(
        s, NVNET_TX_RX_CONTROL,
        nvnet_get_reg(s, NVNET_TX_RX_CONTROL, 4) | NVNET_TX_RX_CONTROL_IDLE, 4);

    return 0;
}

static bool nvnet_can_receive(NetClientState *nc)
{
    NVNET_DPRINTF("nvnet_can_receive called\n");
    return true;
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

    uint32_t rctl = nvnet_get_reg(s, NVNET_PACKET_FILTER, 4);

    /* Broadcast */
    if (is_broadcast_ether_addr(buf)) {
        /* FIXME: bcast filtering */
        trace_nvnet_rx_filter_bcast_match();
        return true;
    }

    if (!(rctl & NVNET_PACKET_FILTER_MYADDR)) {
        /* FIXME: Confirm PFF_MYADDR filters mcast */
        return true;
    }

    /* Multicast */
    uint32_t addr[2];
    addr[0] = cpu_to_le32(nvnet_get_reg(s, NVNET_MULTICAST_ADDR_A, 4));
    addr[1] = cpu_to_le32(nvnet_get_reg(s, NVNET_MULTICAST_ADDR_B, 4));
    if (!is_broadcast_ether_addr((uint8_t *)addr)) {
        uint32_t dest_addr[2];
        memcpy(dest_addr, buf, 6);
        dest_addr[0] &=
            cpu_to_le32(nvnet_get_reg(s, NVNET_MULTICAST_MASK_A, 4));
        dest_addr[1] &=
            cpu_to_le32(nvnet_get_reg(s, NVNET_MULTICAST_MASK_B, 4));

        if (!memcmp(dest_addr, addr, 6)) {
            trace_nvnet_rx_filter_mcast_match(MAC_ARG(dest_addr));
            return true;
        } else {
            trace_nvnet_rx_filter_mcast_mismatch(MAC_ARG(dest_addr));
        }
    }

    /* Unicast */
    addr[0] = cpu_to_le32(nvnet_get_reg(s, NVNET_MAC_ADDR_A, 4));
    addr[1] = cpu_to_le32(nvnet_get_reg(s, NVNET_MAC_ADDR_B, 4));
    if (!memcmp(buf, addr, 6)) {
        trace_nvnet_rx_filter_ucast_match(MAC_ARG(buf));
        return true;
    } else {
        trace_nvnet_rx_filter_ucast_mismatch(MAC_ARG(buf));
    }

    return false;
}

static ssize_t nvnet_receive_iov(NetClientState *nc, const struct iovec *iov,
                                 int iovcnt)
{
    NvNetState *s = qemu_get_nic_opaque(nc);
    size_t size = iov_size(iov, iovcnt);

    NVNET_DPRINTF("nvnet: Packet received!\n");

    if (nvnet_is_packet_oversized(size)) {
        NVNET_DPRINTF("%s packet too large!\n", __func__);
        trace_nvnet_rx_oversized(size);
        return size;
    }

    iov_to_buf(iov, iovcnt, 0, s->rx_dma_buf, size);

    if (!receive_filter(s, s->rx_dma_buf, size)) {
        trace_nvnet_rx_filter_dropped();
        return size;
    }

    return nvnet_dma_packet_to_guest(s, s->rx_dma_buf, size);
}

static ssize_t nvnet_receive(NetClientState *nc, const uint8_t *buf,
                             size_t size)
{
    const struct iovec iov = { .iov_base = (uint8_t *)buf, .iov_len = size };

    NVNET_DPRINTF("nvnet_receive called\n");
    return nvnet_receive_iov(nc, &iov, 1);
}

static uint16_t nvnet_phy_reg_read(NvNetState *s, uint8_t reg)
{
    uint16_t value;

    switch (reg) {
    case MII_BMSR:
        value = MII_BMSR_AN_COMP | MII_BMSR_LINK_ST;
        break;

    case MII_ANAR:
        /* Fall through... */

    case MII_ANLPAR:
        value = MII_ANLPAR_10 | MII_ANLPAR_10FD | MII_ANLPAR_TX |
                MII_ANLPAR_TXFD | MII_ANLPAR_T4;
        break;

    default:
        value = 0;
        break;
    }

    trace_nvnet_mii_read(PHY_ADDR, reg, nvnet_get_mii_reg_name(reg), value);
    return value;
}

static void nvnet_phy_reg_write(NvNetState *s, uint8_t reg, uint16_t value)
{
    trace_nvnet_mii_write(PHY_ADDR, reg, nvnet_get_mii_reg_name(reg), value);
}

static void nvnet_mdio_read(NvNetState *s)
{
    uint32_t mdio_addr = nvnet_get_reg(s, NVNET_MDIO_ADDR, 4);
    uint32_t mdio_data = -1;
    uint32_t phy_addr = GET_MASK(mdio_addr, NVNET_MDIO_ADDR_PHYADDR);
    uint32_t phy_reg = GET_MASK(mdio_addr, NVNET_MDIO_ADDR_PHYREG);

    if (phy_addr == PHY_ADDR) {
        mdio_data = nvnet_phy_reg_read(s, phy_reg);
    }

    mdio_addr &= ~NVNET_MDIO_ADDR_INUSE;
    nvnet_set_reg(s, NVNET_MDIO_ADDR, mdio_addr, 4);
    nvnet_set_reg(s, NVNET_MDIO_DATA, mdio_data, 4);
}

static void nvnet_mdio_write(NvNetState *s)
{
    uint32_t mdio_addr = nvnet_get_reg(s, NVNET_MDIO_ADDR, 4);
    uint32_t mdio_data = nvnet_get_reg(s, NVNET_MDIO_DATA, 4);
    uint32_t phy_addr = GET_MASK(mdio_addr, NVNET_MDIO_ADDR_PHYADDR);
    uint32_t phy_reg = GET_MASK(mdio_addr, NVNET_MDIO_ADDR_PHYREG);

    if (phy_addr == PHY_ADDR) {
        nvnet_phy_reg_write(s, phy_reg, mdio_data);
    }

    mdio_addr &= ~NVNET_MDIO_ADDR_INUSE;
    nvnet_set_reg(s, NVNET_MDIO_ADDR, mdio_addr, 4);
}

static uint64_t nvnet_mmio_read(void *opaque, hwaddr addr, unsigned int size)
{
    NvNetState *s = NVNET(opaque);
    uint64_t retval;

    switch (addr) {
    case NVNET_MII_STATUS:
        retval = 0;
        break;

    default:
        retval = nvnet_get_reg(s, addr, size);
        break;
    }

    trace_nvnet_reg_read(addr, nvnet_get_reg_name(addr & ~3), size, retval);
    return retval;
}

static void nvnet_mmio_write(void *opaque, hwaddr addr, uint64_t val,
                             unsigned int size)
{
    NvNetState *s = NVNET(opaque);
    uint32_t temp;

    trace_nvnet_reg_write(addr, nvnet_get_reg_name(addr & ~3), size, val);

    switch (addr) {
    case NVNET_RING_SIZE:
        nvnet_set_reg(s, addr, val, size);
        s->rx_ring_size = GET_MASK(val, NVNET_RING_SIZE_RX) + 1;
        s->tx_ring_size = GET_MASK(val, NVNET_RING_SIZE_TX) + 1;
        break;

    case NVNET_MDIO_ADDR:
        assert(size == 4);
        nvnet_set_reg(s, addr, val, size);
        if (val & NVNET_MDIO_ADDR_WRITE) {
            nvnet_mdio_write(s);
        } else {
            nvnet_mdio_read(s);
        }
        break;

    case NVNET_TX_RX_CONTROL:
        if (val == NVNET_TX_RX_CONTROL_KICK) {
            NVNET_DPRINTF("NVNET_TX_RX_CONTROL = NVNET_TX_RX_CONTROL_KICK!\n");
            nvnet_dump_ring_descriptors(s);
            nvnet_dma_packet_from_guest(s);
        }

        if (val & NVNET_TX_RX_CONTROL_BIT2) {
            nvnet_set_reg(s, NVNET_TX_RX_CONTROL, NVNET_TX_RX_CONTROL_IDLE, 4);
            break;
        }

        if (val & NVNET_TX_RX_CONTROL_RESET) {
            s->tx_ring_index = 0;
            s->rx_ring_index = 0;
            s->tx_dma_buf_offset = 0;
        }

        if (val & NVNET_TX_RX_CONTROL_BIT1) {
            // FIXME
            nvnet_set_reg(s, NVNET_IRQ_STATUS, 0, 4);
            break;
        } else if (val == 0) {
            temp = nvnet_get_reg(s, NVNET_UNKNOWN_SETUP_REG3, 4);
            if (temp == NVNET_UNKNOWN_SETUP_REG3_VAL1) {
                /* forcedeth waits for this bit to be set... */
                nvnet_set_reg(s, NVNET_UNKNOWN_SETUP_REG5,
                              NVNET_UNKNOWN_SETUP_REG5_BIT31, 4);
                break;
            }
        }

        nvnet_set_reg(s, NVNET_TX_RX_CONTROL, val, size);
        break;

    case NVNET_IRQ_MASK:
        nvnet_set_reg(s, addr, val, size);
        nvnet_update_irq(s);
        break;

    case NVNET_IRQ_STATUS:
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

static uint64_t nvnet_io_read(void *opaque, hwaddr addr, unsigned int size)
{
    uint64_t r = 0;
    trace_nvnet_io_read(addr, size, r);
    return r;
}

static void nvnet_io_write(void *opaque, hwaddr addr, uint64_t val,
                           unsigned int size)
{
    trace_nvnet_io_write(addr, size, val);
}

static const MemoryRegionOps nvnet_io_ops = {
    .read = nvnet_io_read,
    .write = nvnet_io_write,
};

static NetClientInfo net_nvnet_info = {
    .type = NET_CLIENT_DRIVER_NIC,
    .size = sizeof(NICState),
    .can_receive = nvnet_can_receive,
    .receive = nvnet_receive,
    .receive_iov = nvnet_receive_iov,
    .link_status_changed = nvnet_set_link_status,
};

static void nvnet_realize(PCIDevice *pci_dev, Error **errp)
{
    DeviceState *dev = DEVICE(pci_dev);
    NvNetState *s = NVNET(pci_dev);
    PCIDevice *d = PCI_DEVICE(s);

    pci_dev->config[PCI_INTERRUPT_PIN] = 0x01;

    memset(s->regs, 0, sizeof(s->regs));

    s->rx_ring_index = 0;
    s->rx_ring_size = 0;
    s->tx_ring_index = 0;
    s->tx_ring_size = 0;

    memory_region_init_io(&s->mmio, OBJECT(dev), &nvnet_mmio_ops, s,
                          "nvnet-mmio", MMIO_SIZE);
    pci_register_bar(d, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->mmio);

    memory_region_init_io(&s->io, OBJECT(dev), &nvnet_io_ops, s, "nvnet-io",
                          IOPORT_SIZE);
    pci_register_bar(d, 1, PCI_BASE_ADDRESS_SPACE_IO, &s->io);

    qemu_macaddr_default_if_unset(&s->conf.macaddr);
    s->nic =
        qemu_new_nic(&net_nvnet_info, &s->conf, object_get_typename(OBJECT(s)),
                     dev->id, &dev->mem_reentrancy_guard, s);
    assert(s->nic);
}

static void nvnet_uninit(PCIDevice *dev)
{
    NvNetState *s = NVNET(dev);
    qemu_del_nic(s->nic);
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

static void nvnet_reset_hold(Object *obj, ResetType type)
{
    NvNetState *s = NVNET(obj);
    nvnet_reset(s);
}

static const VMStateDescription vmstate_nvnet = {
    .name = "nvnet",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields =
        (VMStateField[]){ VMSTATE_PCI_DEVICE(parent_obj, NvNetState),
                          VMSTATE_UINT8_ARRAY(regs, NvNetState, MMIO_SIZE),
                          VMSTATE_UINT32_ARRAY(phy_regs, NvNetState, 6),
                          VMSTATE_UINT8(tx_ring_index, NvNetState),
                          VMSTATE_UINT8(tx_ring_size, NvNetState),
                          VMSTATE_UINT8(rx_ring_index, NvNetState),
                          VMSTATE_UINT8(rx_ring_size, NvNetState),
                          VMSTATE_END_OF_LIST() },
};

static Property nvnet_properties[] = {
    DEFINE_NIC_PROPERTIES(NvNetState, conf),
    DEFINE_PROP_END_OF_LIST(),
};

static void nvnet_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->vendor_id = PCI_VENDOR_ID_NVIDIA;
    k->device_id = PCI_DEVICE_ID_NVIDIA_NVENET_1;
    k->revision = 177;
    k->class_id = PCI_CLASS_NETWORK_ETHERNET;
    k->realize = nvnet_realize;
    k->exit = nvnet_uninit;

    rc->phases.hold = nvnet_reset_hold;

    set_bit(DEVICE_CATEGORY_NETWORK, dc->categories);
    dc->desc = "nForce Ethernet Controller";
    dc->vmsd = &vmstate_nvnet;
    device_class_set_props(dc, nvnet_properties);
}

static const TypeInfo nvnet_info = {
    .name = "nvnet",
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(NvNetState),
    .class_init = nvnet_class_init,
    .interfaces =
        (InterfaceInfo[]){
            { INTERFACE_CONVENTIONAL_PCI_DEVICE },
            {},
        },
};

static void nvnet_register(void)
{
    type_register_static(&nvnet_info);
}

type_init(nvnet_register)
