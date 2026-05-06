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
#define AUTONEG_DURATION_MS 250

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

    uint32_t tx_dma_buf_offset;
    uint8_t tx_dma_buf[TX_ALLOC_BUFSIZE];
    uint8_t rx_dma_buf[RX_ALLOC_BUFSIZE];

    QEMUTimer *autoneg_timer;

    /* Deprecated */
    uint8_t tx_ring_index;
    uint8_t rx_ring_index;
} NvNetState;

struct RingDesc {
    uint32_t buffer_addr;
    uint16_t length;
    uint16_t flags;
} QEMU_PACKED;

#define R(r) \
    case r:  \
        return #r;

static const char *get_reg_name(hwaddr addr)
{
    switch (addr & ~3) {
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
        R(NVNET_TX_RING_CURRENT_DESC_PHYS_ADDR)
        R(NVNET_RX_RING_CURRENT_DESC_PHYS_ADDR)
        R(NVNET_TX_CURRENT_BUFFER_PHYS_ADDR)
        R(NVNET_RX_CURRENT_BUFFER_PHYS_ADDR)
        R(NVNET_UNKNOWN_SETUP_REG5)
        R(NVNET_TX_RING_NEXT_DESC_PHYS_ADDR)
        R(NVNET_RX_RING_NEXT_DESC_PHYS_ADDR)
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

static const char *get_phy_reg_name(uint8_t reg)
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

static uint32_t get_reg_ext(NvNetState *s, hwaddr addr, unsigned int size)
{
    assert(addr < MMIO_SIZE);
    assert((addr & (size - 1)) == 0);

    switch (size) {
    case 4:
        return le32_to_cpu(*(uint32_t *)&s->regs[addr]);

    case 2:
        return le16_to_cpu(*(uint16_t *)&s->regs[addr]);

    case 1:
        return s->regs[addr];

    default:
        assert(!"Unsupported register access");
        return 0;
    }
}

static uint32_t get_reg(NvNetState *s, hwaddr addr)
{
    return get_reg_ext(s, addr, 4);
}

static void set_reg_ext(NvNetState *s, hwaddr addr, uint32_t val,
                        unsigned int size)
{
    assert(addr < MMIO_SIZE);
    assert((addr & (size - 1)) == 0);

    switch (size) {
    case 4:
        *(uint32_t *)&s->regs[addr] = cpu_to_le32((uint32_t)val);
        break;

    case 2:
        *(uint16_t *)&s->regs[addr] = cpu_to_le16((uint16_t)val);
        break;

    case 1:
        s->regs[addr] = (uint8_t)val;
        break;

    default:
        assert(!"Unsupported register access");
    }
}

static void set_reg(NvNetState *s, hwaddr addr, uint32_t val)
{
    set_reg_ext(s, addr, val, 4);
}

static void or_reg(NvNetState *s, hwaddr addr, uint32_t val)
{
    set_reg(s, addr, get_reg(s, addr) | val);
}

static void and_reg(NvNetState *s, hwaddr addr, uint32_t val)
{
    set_reg(s, addr, get_reg(s, addr) & val);
}

static void set_reg_with_mask(NvNetState *s, hwaddr addr, uint32_t val, uint32_t w_mask)
{
    set_reg(s, addr, ((get_reg(s, addr) & (val | ~w_mask)) | (val & w_mask)));
}

static void update_irq(NvNetState *s)
{
    PCIDevice *d = PCI_DEVICE(s);

    uint32_t irq_status = get_reg(s, NVNET_IRQ_STATUS);
    uint32_t irq_mask = get_reg(s, NVNET_IRQ_MASK);

    trace_nvnet_update_irq(irq_status, irq_mask);

    if (irq_mask & irq_status) {
        pci_irq_assert(d);
    } else {
        pci_irq_deassert(d);
    }
}

static void set_intr_status(NvNetState *s, uint32_t status)
{
    or_reg(s, NVNET_IRQ_STATUS, status);
    update_irq(s);
}

static void set_mii_intr_status(NvNetState *s, uint32_t status)
{
    or_reg(s, NVNET_MII_STATUS, status);
    set_intr_status(s, NVNET_IRQ_STATUS_MIIEVENT);
    // FIXME: MII status mask?
}

static void send_packet(NvNetState *s, const uint8_t *buf, size_t size)
{
    NetClientState *nc = qemu_get_queue(s->nic);

    trace_nvnet_packet_tx(size);
    qemu_send_packet(nc, buf, size);
}

static uint16_t get_tx_ring_size(NvNetState *s)
{
    uint32_t ring_size = get_reg(s, NVNET_RING_SIZE);
    return GET_MASK(ring_size, NVNET_RING_SIZE_TX) + 1;
}

static uint16_t get_rx_ring_size(NvNetState *s)
{
    uint32_t ring_size = get_reg(s, NVNET_RING_SIZE);
    return GET_MASK(ring_size, NVNET_RING_SIZE_RX) + 1;
}

static void reset_descriptor_ring_pointers(NvNetState *s)
{
    uint32_t base_desc_addr;

    base_desc_addr = get_reg(s, NVNET_TX_RING_PHYS_ADDR);
    set_reg(s, NVNET_TX_RING_CURRENT_DESC_PHYS_ADDR, base_desc_addr);
    set_reg(s, NVNET_TX_RING_NEXT_DESC_PHYS_ADDR, base_desc_addr);

    base_desc_addr = get_reg(s, NVNET_RX_RING_PHYS_ADDR);
    set_reg(s, NVNET_RX_RING_CURRENT_DESC_PHYS_ADDR, base_desc_addr);
    set_reg(s, NVNET_RX_RING_NEXT_DESC_PHYS_ADDR, base_desc_addr);
}

static bool link_up(NvNetState *s)
{
    return s->phy_regs[MII_BMSR] & MII_BMSR_LINK_ST;
}

static bool dma_enabled(NvNetState *s)
{
    return (get_reg(s, NVNET_TX_RX_CONTROL) & NVNET_TX_RX_CONTROL_BIT2) == 0;
}

static void set_dma_idle(NvNetState *s, bool idle)
{
    if (idle) {
        or_reg(s, NVNET_TX_RX_CONTROL, NVNET_TX_RX_CONTROL_IDLE);
    } else {
        and_reg(s, NVNET_TX_RX_CONTROL, ~NVNET_TX_RX_CONTROL_IDLE);
    }
}

static bool rx_enabled(NvNetState *s)
{
    return get_reg(s, NVNET_RECEIVER_CONTROL) & NVNET_RECEIVER_CONTROL_START;
}

static uint32_t update_current_rx_ring_desc_addr(NvNetState *s)
{
    uint32_t base_desc_addr = get_reg(s, NVNET_RX_RING_PHYS_ADDR);
    uint32_t max_desc_addr =
        base_desc_addr + get_rx_ring_size(s) * sizeof(struct RingDesc);
    uint32_t cur_desc_addr = get_reg(s, NVNET_RX_RING_NEXT_DESC_PHYS_ADDR);
    if ((cur_desc_addr < base_desc_addr) ||
        ((cur_desc_addr + sizeof(struct RingDesc)) > max_desc_addr)) {
        cur_desc_addr = base_desc_addr;
    }
    set_reg(s, NVNET_RX_RING_CURRENT_DESC_PHYS_ADDR, cur_desc_addr);
    return cur_desc_addr;
}

static void advance_next_rx_ring_desc_addr(NvNetState *s)
{
    uint32_t base_desc_addr = get_reg(s, NVNET_RX_RING_PHYS_ADDR);
    uint32_t max_desc_addr =
        base_desc_addr + get_rx_ring_size(s) * sizeof(struct RingDesc);
    uint32_t cur_desc_addr = get_reg(s, NVNET_RX_RING_CURRENT_DESC_PHYS_ADDR);

    uint32_t next_desc_addr = cur_desc_addr + sizeof(struct RingDesc);
    if (next_desc_addr >= max_desc_addr) {
        next_desc_addr = base_desc_addr;
    }
    set_reg(s, NVNET_RX_RING_NEXT_DESC_PHYS_ADDR, next_desc_addr);
}

static struct RingDesc load_ring_desc(NvNetState *s, dma_addr_t desc_addr)
{
    PCIDevice *d = PCI_DEVICE(s);

    struct RingDesc raw_desc;
    pci_dma_read(d, desc_addr, &raw_desc, sizeof(raw_desc));

    return (struct RingDesc){
        .buffer_addr = le32_to_cpu(raw_desc.buffer_addr),
        .length = le16_to_cpu(raw_desc.length),
        .flags = le16_to_cpu(raw_desc.flags),
    };
}

static void store_ring_desc(NvNetState *s, dma_addr_t desc_addr,
                            struct RingDesc desc)
{
    PCIDevice *d = PCI_DEVICE(s);

    trace_nvnet_desc_store(desc_addr, desc.buffer_addr, desc.length,
                           desc.flags);

    struct RingDesc raw_desc = {
        .buffer_addr = cpu_to_le32(desc.buffer_addr),
        .length = cpu_to_le16(desc.length),
        .flags = cpu_to_le16(desc.flags),
    };
    pci_dma_write(d, desc_addr, &raw_desc, sizeof(raw_desc));
}

static bool rx_buf_available(NvNetState *s)
{
    uint32_t cur_desc_addr = update_current_rx_ring_desc_addr(s);
    struct RingDesc desc = load_ring_desc(s, cur_desc_addr);
    return desc.flags & NV_RX_AVAIL;
}

static bool nvnet_can_receive(NetClientState *nc)
{
    NvNetState *s = qemu_get_nic_opaque(nc);

    bool rx_en = rx_enabled(s);
    bool dma_en = dma_enabled(s);
    bool link_en = link_up(s);
    bool buf_avail = rx_buf_available(s);
    bool can_rx = rx_en && dma_en && link_en && buf_avail;

    if (!can_rx) {
        trace_nvnet_cant_rx(rx_en, dma_en, link_en, buf_avail);
    }

    return can_rx;
}

static ssize_t dma_packet_to_guest(NvNetState *s, const uint8_t *buf,
                                   size_t size)
{
    PCIDevice *d = PCI_DEVICE(s);
    NetClientState *nc = qemu_get_queue(s->nic);
    ssize_t rval;

    if (!nvnet_can_receive(nc)) {
        return -1;
    }

    set_dma_idle(s, false);

    uint32_t base_desc_addr = get_reg(s, NVNET_RX_RING_PHYS_ADDR);
    uint32_t cur_desc_addr = update_current_rx_ring_desc_addr(s);
    struct RingDesc desc = load_ring_desc(s, cur_desc_addr);

    NVNET_DPRINTF("RX: Looking at ring descriptor %zd (0x%x): "
                  "Buffer: 0x%x, Length: 0x%x, Flags: 0x%x\n",
                  (cur_desc_addr - base_desc_addr) / sizeof(struct RingDesc),
                  cur_desc_addr, desc.buffer_addr, desc.length, desc.flags);

    if (desc.flags & NV_RX_AVAIL) {
        assert((desc.length + 1) >= size); // FIXME

        trace_nvnet_rx_dma(desc.buffer_addr, size);
        pci_dma_write(d, desc.buffer_addr, buf, size);

        desc.length = size;
        desc.flags = NV_RX_BIT4 | NV_RX_DESCRIPTORVALID;
        store_ring_desc(s, cur_desc_addr, desc);

        set_intr_status(s, NVNET_IRQ_STATUS_RX);

        advance_next_rx_ring_desc_addr(s);

        rval = size;
    } else {
        NVNET_DPRINTF("Could not find free buffer!\n");
        rval = -1;
    }

    set_dma_idle(s, true);

    return rval;
}

static bool tx_enabled(NvNetState *s)
{
    return get_reg(s, NVNET_TRANSMITTER_CONTROL) &
           NVNET_TRANSMITTER_CONTROL_START;
}

static bool can_transmit(NvNetState *s)
{
    bool tx_en = tx_enabled(s);
    bool dma_en = dma_enabled(s);
    bool link_en = link_up(s);
    bool can_tx = tx_en && dma_en && link_en;

    if (!can_tx) {
        trace_nvnet_cant_tx(tx_en, dma_en, link_en);
    }

    return can_tx;
}

static uint32_t update_current_tx_ring_desc_addr(NvNetState *s)
{
    uint32_t base_desc_addr = get_reg(s, NVNET_TX_RING_PHYS_ADDR);
    uint32_t max_desc_addr =
        base_desc_addr + get_tx_ring_size(s) * sizeof(struct RingDesc);

    uint32_t cur_desc_addr = get_reg(s, NVNET_TX_RING_NEXT_DESC_PHYS_ADDR);
    if ((cur_desc_addr < base_desc_addr) ||
        ((cur_desc_addr + sizeof(struct RingDesc)) > max_desc_addr)) {
        cur_desc_addr = base_desc_addr;
    }
    set_reg(s, NVNET_TX_RING_CURRENT_DESC_PHYS_ADDR, cur_desc_addr);
    return cur_desc_addr;
}

static void advance_next_tx_ring_desc_addr(NvNetState *s)
{
    uint32_t base_desc_addr = get_reg(s, NVNET_TX_RING_PHYS_ADDR);
    uint32_t max_desc_addr =
        base_desc_addr + get_tx_ring_size(s) * sizeof(struct RingDesc);
    uint32_t cur_desc_addr = get_reg(s, NVNET_TX_RING_CURRENT_DESC_PHYS_ADDR);

    uint32_t next_desc_addr = cur_desc_addr + sizeof(struct RingDesc);
    if (next_desc_addr >= max_desc_addr) {
        next_desc_addr = base_desc_addr;
    }
    set_reg(s, NVNET_TX_RING_NEXT_DESC_PHYS_ADDR, next_desc_addr);
}

static void dma_packet_from_guest(NvNetState *s)
{
    PCIDevice *d = PCI_DEVICE(s);
    bool packet_sent = false;

    if (!can_transmit(s)) {
        return;
    }

    set_dma_idle(s, false);

    uint32_t base_desc_addr = get_reg(s, NVNET_TX_RING_PHYS_ADDR);

    for (int i = 0; i < get_tx_ring_size(s); i++) {
        uint32_t cur_desc_addr = update_current_tx_ring_desc_addr(s);
        struct RingDesc desc = load_ring_desc(s, cur_desc_addr);
        uint16_t length = desc.length + 1;

        NVNET_DPRINTF("TX: Looking at ring desc %zd (%x): "
                      "Buffer: 0x%x, Length: 0x%x, Flags: 0x%x\n",
                      (cur_desc_addr - base_desc_addr) /
                          sizeof(struct RingDesc),
                      cur_desc_addr, desc.buffer_addr, length, desc.flags);

        if (!(desc.flags & NV_TX_VALID)) {
            break;
        }

        assert((s->tx_dma_buf_offset + length) <= sizeof(s->tx_dma_buf));

        trace_nvnet_tx_dma(desc.buffer_addr, length);
        pci_dma_read(d, desc.buffer_addr, &s->tx_dma_buf[s->tx_dma_buf_offset],
                     length);
        s->tx_dma_buf_offset += length;

        bool is_last_packet = desc.flags & NV_TX_LASTPACKET;
        if (is_last_packet) {
            send_packet(s, s->tx_dma_buf, s->tx_dma_buf_offset);
            s->tx_dma_buf_offset = 0;
            packet_sent = true;
        }

        desc.flags &= ~(NV_TX_VALID | NV_TX_RETRYERROR | NV_TX_DEFERRED |
                        NV_TX_CARRIERLOST | NV_TX_LATECOLLISION |
                        NV_TX_UNDERFLOW | NV_TX_ERROR);
        store_ring_desc(s, cur_desc_addr, desc);

        advance_next_tx_ring_desc_addr(s);

        if (is_last_packet) {
            // FIXME
            break;
        }
    }

    set_dma_idle(s, true);

    if (packet_sent) {
        set_intr_status(s, NVNET_IRQ_STATUS_TX);
    }
}

static bool is_packet_oversized(size_t size)
{
    return size > RX_ALLOC_BUFSIZE;
}

static bool receive_filter(NvNetState *s, const uint8_t *buf, int size)
{
    if (size < 6) {
        return false;
    }

    uint32_t rctl = get_reg(s, NVNET_PACKET_FILTER);

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
    addr[0] = cpu_to_le32(get_reg(s, NVNET_MULTICAST_ADDR_A));
    addr[1] = cpu_to_le32(get_reg(s, NVNET_MULTICAST_ADDR_B));
    if (!is_broadcast_ether_addr((uint8_t *)addr)) {
        uint32_t dest_addr[2];
        memcpy(dest_addr, buf, 6);
        dest_addr[0] &= cpu_to_le32(get_reg(s, NVNET_MULTICAST_MASK_A));
        dest_addr[1] &= cpu_to_le32(get_reg(s, NVNET_MULTICAST_MASK_B));

        if (!memcmp(dest_addr, addr, 6)) {
            trace_nvnet_rx_filter_mcast_match(MAC_ARG(dest_addr));
            return true;
        } else {
            trace_nvnet_rx_filter_mcast_mismatch(MAC_ARG(dest_addr));
        }
    }

    /* Unicast */
    addr[0] = cpu_to_le32(get_reg(s, NVNET_MAC_ADDR_A));
    addr[1] = cpu_to_le32(get_reg(s, NVNET_MAC_ADDR_B));
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

    if (is_packet_oversized(size)) {
        trace_nvnet_rx_oversized(size);
        return size;
    }

    iov_to_buf(iov, iovcnt, 0, s->rx_dma_buf, size);

    if (!receive_filter(s, s->rx_dma_buf, size)) {
        trace_nvnet_rx_filter_dropped();
        return size;
    }

    return dma_packet_to_guest(s, s->rx_dma_buf, size);
}

static ssize_t nvnet_receive(NetClientState *nc, const uint8_t *buf,
                             size_t size)
{
    const struct iovec iov = { .iov_base = (uint8_t *)buf, .iov_len = size };
    return nvnet_receive_iov(nc, &iov, 1);
}

static void update_regs_on_link_down(NvNetState *s)
{
    s->phy_regs[MII_BMSR] &= ~MII_BMSR_LINK_ST;
    s->phy_regs[MII_BMSR] &= ~MII_BMSR_AN_COMP;
    s->phy_regs[MII_ANLPAR] &= ~MII_ANLPAR_ACK;
    and_reg(s, NVNET_ADAPTER_CONTROL, ~NVNET_ADAPTER_CONTROL_LINKUP);
}

static void set_link_down(NvNetState *s)
{
    update_regs_on_link_down(s);
    set_mii_intr_status(s, NVNET_MII_STATUS_LINKCHANGE);
}

static void update_regs_on_link_up(NvNetState *s)
{
    s->phy_regs[MII_BMSR] |= MII_BMSR_LINK_ST;
    or_reg(s, NVNET_ADAPTER_CONTROL, NVNET_ADAPTER_CONTROL_LINKUP);
}

static void set_link_up(NvNetState *s)
{
    update_regs_on_link_up(s);
    set_mii_intr_status(s, NVNET_MII_STATUS_LINKCHANGE);
}

static void restart_autoneg(NvNetState *s)
{
    trace_nvnet_link_negotiation_start();
    timer_mod(s->autoneg_timer,
              qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL) + AUTONEG_DURATION_MS);
}

static void autoneg_done(void *opaque)
{
    NvNetState *s = opaque;

    trace_nvnet_link_negotiation_done();

    s->phy_regs[MII_ANLPAR] |= MII_ANLPAR_ACK;
    s->phy_regs[MII_BMSR] |= MII_BMSR_AN_COMP;

    set_link_up(s);
}

static void autoneg_timer(void *opaque)
{
    NvNetState *s = opaque;

    if (!qemu_get_queue(s->nic)->link_down) {
        autoneg_done(s);
    }
}

static bool have_autoneg(NvNetState *s)
{
    return (s->phy_regs[MII_BMCR] & MII_BMCR_AUTOEN);
}

static void nvnet_set_link_status(NetClientState *nc)
{
    NvNetState *s = qemu_get_nic_opaque(nc);

    trace_nvnet_link_status_changed(nc->link_down ? false : true);

    if (nc->link_down) {
        set_link_down(s);
    } else {
        if (have_autoneg(s) && !(s->phy_regs[MII_BMSR] & MII_BMSR_AN_COMP)) {
            restart_autoneg(s);
        } else {
            set_link_up(s);
        }
    }
}

static NetClientInfo nvnet_client_info = {
    .type = NET_CLIENT_DRIVER_NIC,
    .size = sizeof(NICState),
    .can_receive = nvnet_can_receive,
    .receive = nvnet_receive,
    .receive_iov = nvnet_receive_iov,
    .link_status_changed = nvnet_set_link_status,
};

static uint16_t phy_reg_read(NvNetState *s, uint8_t reg)
{
    uint16_t value;

    if (reg < ARRAY_SIZE(s->phy_regs)) {
        value = s->phy_regs[reg];
    } else {
        value = 0;
    }

    trace_nvnet_phy_reg_read(PHY_ADDR, reg, get_phy_reg_name(reg), value);
    return value;
}

static void phy_reg_write(NvNetState *s, uint8_t reg, uint16_t value)
{
    trace_nvnet_phy_reg_write(PHY_ADDR, reg, get_phy_reg_name(reg),
                              value);

    if (reg < ARRAY_SIZE(s->phy_regs)) {
        s->phy_regs[reg] = value;
    }
}

static void mdio_read(NvNetState *s)
{
    uint32_t mdio_addr = get_reg(s, NVNET_MDIO_ADDR);
    uint32_t mdio_data = -1;
    uint8_t phy_addr = GET_MASK(mdio_addr, NVNET_MDIO_ADDR_PHYADDR);
    uint8_t phy_reg = GET_MASK(mdio_addr, NVNET_MDIO_ADDR_PHYREG);

    if (phy_addr == PHY_ADDR) {
        mdio_data = phy_reg_read(s, phy_reg);
    }

    set_reg(s, NVNET_MDIO_DATA, mdio_data);
    and_reg(s, NVNET_MDIO_ADDR, ~NVNET_MDIO_ADDR_INUSE);
}

static void mdio_write(NvNetState *s)
{
    uint32_t mdio_addr = get_reg(s, NVNET_MDIO_ADDR);
    uint32_t mdio_data = get_reg(s, NVNET_MDIO_DATA);
    uint8_t phy_addr = GET_MASK(mdio_addr, NVNET_MDIO_ADDR_PHYADDR);
    uint8_t phy_reg = GET_MASK(mdio_addr, NVNET_MDIO_ADDR_PHYREG);

    if (phy_addr == PHY_ADDR) {
        phy_reg_write(s, phy_reg, mdio_data);
    }

    and_reg(s, NVNET_MDIO_ADDR, ~NVNET_MDIO_ADDR_INUSE);
}

static uint64_t nvnet_mmio_read(void *opaque, hwaddr addr, unsigned int size)
{
    NvNetState *s = NVNET(opaque);
    uint32_t retval = get_reg_ext(s, addr, size);
    trace_nvnet_reg_read(addr, get_reg_name(addr), size, retval);
    return retval;
}

static void dump_ring_descriptors(NvNetState *s)
{
#if DEBUG_NVNET
    NVNET_DPRINTF("------------------------------------------------\n");

    for (int i = 0; i < get_tx_ring_size(s); i++) {
        dma_addr_t desc_addr =
            get_reg(s, NVNET_TX_RING_PHYS_ADDR) + i * sizeof(struct RingDesc);
        struct RingDesc desc = load_ring_desc(s, desc_addr);
        NVNET_DPRINTF("TX desc %d (%" HWADDR_PRIx "): "
                      "Buffer: 0x%x, Length: 0x%x, Flags: 0x%x\n",
                      i, desc_addr, desc.buffer_addr, desc.length,
                      desc.flags);
    }

    NVNET_DPRINTF("------------------------------------------------\n");

    for (int i = 0; i < get_rx_ring_size(s); i++) {
        dma_addr_t desc_addr =
            get_reg(s, NVNET_RX_RING_PHYS_ADDR) + i * sizeof(struct RingDesc);
        struct RingDesc desc = load_ring_desc(s, desc_addr);
        NVNET_DPRINTF("RX desc %d (%" HWADDR_PRIx "): "
                      "Buffer: 0x%x, Length: 0x%x, Flags: 0x%x\n",
                      i, desc_addr, desc.buffer_addr, desc.length,
                      desc.flags);
    }

    NVNET_DPRINTF("------------------------------------------------\n");
#endif
}

static void nvnet_mmio_write(void *opaque, hwaddr addr, uint64_t val,
                             unsigned int size)
{
    NvNetState *s = NVNET(opaque);

    trace_nvnet_reg_write(addr, get_reg_name(addr), size, val);
    assert((addr & 3) == 0 && "Unaligned MMIO write");

    switch (addr) {
    case NVNET_MDIO_ADDR:
        assert(size == 4);
        set_reg_ext(s, addr, val, size);
        if (val & NVNET_MDIO_ADDR_WRITE) {
            mdio_write(s);
        } else {
            mdio_read(s);
        }
        break;

    case NVNET_TX_RX_CONTROL:
        set_reg_with_mask(s, addr, val, ~NVNET_TX_RX_CONTROL_IDLE);

        if (val & NVNET_TX_RX_CONTROL_KICK) {
            dump_ring_descriptors(s);
            dma_packet_from_guest(s);
        }

        if (val & NVNET_TX_RX_CONTROL_RESET) {
            reset_descriptor_ring_pointers(s);
            s->tx_dma_buf_offset = 0;
        }

        if (val & NVNET_TX_RX_CONTROL_BIT1) {
            // FIXME
            set_reg(s, NVNET_IRQ_STATUS, 0);
            break;
        } else if (val == 0) {
            /* forcedeth waits for this bit to be set... */
            set_reg(s, NVNET_UNKNOWN_SETUP_REG5,
                    NVNET_UNKNOWN_SETUP_REG5_BIT31);
        }
        break;

    case NVNET_IRQ_STATUS:
    case NVNET_MII_STATUS:
        set_reg_ext(s, addr, get_reg_ext(s, addr, size) & ~val, size);
        update_irq(s);
        break;

    case NVNET_IRQ_MASK:
        set_reg_ext(s, addr, val, size);
        update_irq(s);
        break;

    default:
        set_reg_ext(s, addr, val, size);
        break;
    }
}

static const MemoryRegionOps nvnet_mmio_ops = {
    .read = nvnet_mmio_read,
    .write = nvnet_mmio_write,
};

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

static void nvnet_realize(PCIDevice *pci_dev, Error **errp)
{
    DeviceState *dev = DEVICE(pci_dev);
    NvNetState *s = NVNET(pci_dev);
    PCIDevice *d = PCI_DEVICE(s);

    pci_dev->config[PCI_INTERRUPT_PIN] = 0x01;

    memset(s->regs, 0, sizeof(s->regs));

    memory_region_init_io(&s->mmio, OBJECT(dev), &nvnet_mmio_ops, s,
                          "nvnet-mmio", MMIO_SIZE);
    pci_register_bar(d, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->mmio);

    memory_region_init_io(&s->io, OBJECT(dev), &nvnet_io_ops, s, "nvnet-io",
                          IOPORT_SIZE);
    pci_register_bar(d, 1, PCI_BASE_ADDRESS_SPACE_IO, &s->io);

    qemu_macaddr_default_if_unset(&s->conf.macaddr);
    s->nic = qemu_new_nic(&nvnet_client_info, &s->conf,
                          object_get_typename(OBJECT(s)), dev->id,
                          &dev->mem_reentrancy_guard, s);

    s->autoneg_timer = timer_new_ms(QEMU_CLOCK_VIRTUAL, autoneg_timer, s);
}

static void nvnet_uninit(PCIDevice *dev)
{
    NvNetState *s = NVNET(dev);
    qemu_del_nic(s->nic);
    timer_free(s->autoneg_timer);
}

// clang-format off

static const uint32_t phy_reg_init[] = {
    [MII_BMCR] =
        MII_BMCR_FD |
        MII_BMCR_AUTOEN,
    [MII_BMSR] =
        MII_BMSR_AUTONEG |
        MII_BMSR_AN_COMP |
        MII_BMSR_LINK_ST,
    [MII_ANAR] =
        MII_ANLPAR_10 |
        MII_ANLPAR_10FD |
        MII_ANLPAR_TX |
        MII_ANLPAR_TXFD |
        MII_ANLPAR_T4,
    [MII_ANLPAR] =
        MII_ANLPAR_10 |
        MII_ANLPAR_10FD |
        MII_ANLPAR_TX |
        MII_ANLPAR_TXFD |
        MII_ANLPAR_T4,
};

// clang-format on

static void reset_phy_regs(NvNetState *s)
{
    assert(sizeof(s->phy_regs) >= sizeof(phy_reg_init));
    memset(s->phy_regs, 0, sizeof(s->phy_regs));
    memcpy(s->phy_regs, phy_reg_init, sizeof(phy_reg_init));
}

static void nvnet_reset(void *opaque)
{
    NvNetState *s = opaque;

    memset(&s->regs, 0, sizeof(s->regs));
    or_reg(s, NVNET_TX_RX_CONTROL, NVNET_TX_RX_CONTROL_IDLE);

    reset_phy_regs(s);
    memset(&s->tx_dma_buf, 0, sizeof(s->tx_dma_buf));
    memset(&s->rx_dma_buf, 0, sizeof(s->rx_dma_buf));
    s->tx_dma_buf_offset = 0;

    timer_del(s->autoneg_timer);

    if (qemu_get_queue(s->nic)->link_down) {
        update_regs_on_link_down(s);
    }

    /* Deprecated */
    s->tx_ring_index = 0;
    s->rx_ring_index = 0;
}

static void nvnet_reset_hold(Object *obj, ResetType type)
{
    NvNetState *s = NVNET(obj);
    nvnet_reset(s);
}

static int nvnet_post_load(void *opaque, int version_id)
{
    NvNetState *s = NVNET(opaque);
    NetClientState *nc = qemu_get_queue(s->nic);

    if (version_id < 2) {
        /* PHY regs were stored but not used until version 2 */
        reset_phy_regs(s);

        /* Migrate old snapshot tx descriptor index */
        uint32_t next_desc_addr =
            get_reg(s, NVNET_TX_RING_PHYS_ADDR) +
            (s->tx_ring_index % get_tx_ring_size(s)) * sizeof(struct RingDesc);
        set_reg(s, NVNET_TX_RING_NEXT_DESC_PHYS_ADDR, next_desc_addr);
        s->tx_ring_index = 0;

        /* Migrate old snapshot rx descriptor index */
        next_desc_addr =
            get_reg(s, NVNET_RX_RING_PHYS_ADDR) +
            (s->rx_ring_index % get_rx_ring_size(s)) * sizeof(struct RingDesc);
        set_reg(s, NVNET_RX_RING_NEXT_DESC_PHYS_ADDR, next_desc_addr);
        s->rx_ring_index = 0;
    }

    /* nc.link_down can't be migrated, so infer link_down according
     * to link status bit in PHY regs.
     * Alternatively, restart link negotiation if it was in progress. */
    nc->link_down = (s->phy_regs[MII_BMSR] & MII_BMSR_LINK_ST) == 0;

    if (have_autoneg(s) && !(s->phy_regs[MII_BMSR] & MII_BMSR_AN_COMP)) {
        nc->link_down = false;
        restart_autoneg(s);
    }

    return 0;
}

static const VMStateDescription vmstate_nvnet = {
    .name = "nvnet",
    .version_id = 2,
    .minimum_version_id = 1,
    .post_load = nvnet_post_load,
    // clang-format off
    .fields = (VMStateField[]){
        VMSTATE_PCI_DEVICE(parent_obj, NvNetState),
        VMSTATE_UINT8_ARRAY(regs, NvNetState, MMIO_SIZE),
        VMSTATE_UINT32_ARRAY(phy_regs, NvNetState, 6),
        VMSTATE_UINT8(tx_ring_index, NvNetState),
        VMSTATE_UNUSED(1),
        VMSTATE_UINT8(rx_ring_index, NvNetState),
        VMSTATE_UNUSED(1),
        VMSTATE_END_OF_LIST()
        },
    // clang-format on
};

static const Property nvnet_properties[] = {
    DEFINE_NIC_PROPERTIES(NvNetState, conf),
};

static void nvnet_class_init(ObjectClass *klass, const void *data)
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
