/*
 * Faraday FTGMAC100 Gigabit Ethernet
 *
 * Copyright (C) 2016-2017, IBM Corporation.
 *
 * This code is licensed under the GPL version 2 or later. See the
 * COPYING file in the top-level directory.
 */

#ifndef FTGMAC100_H
#define FTGMAC100_H
#include "qom/object.h"

#define TYPE_FTGMAC100 "ftgmac100"
OBJECT_DECLARE_SIMPLE_TYPE(FTGMAC100State, FTGMAC100)

#define FTGMAC100_MEM_SIZE 0x1000
#define FTGMAC100_REG_MEM_SIZE 0x100
#define FTGMAC100_REG_HIGH_MEM_SIZE 0x100
#define FTGMAC100_REG_HIGH_OFFSET 0x100

#include "hw/sysbus.h"
#include "net/net.h"

/*
 * Max frame size for the receiving buffer
 */
#define FTGMAC100_MAX_FRAME_SIZE    9220

struct FTGMAC100State {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    NICState *nic;
    NICConf conf;
    qemu_irq irq;
    MemoryRegion iomem_container;
    MemoryRegion iomem;
    MemoryRegion iomem_high;

    uint8_t frame[FTGMAC100_MAX_FRAME_SIZE];

    uint32_t irq_state;
    uint32_t isr;
    uint32_t ier;
    uint32_t rx_enabled;
    uint32_t math[2];
    uint32_t rbsr;
    uint32_t itc;
    uint32_t aptcr;
    uint32_t dblac;
    uint32_t revr;
    uint32_t fear1;
    uint32_t tpafcr;
    uint32_t maccr;
    uint32_t phycr;
    uint32_t phydata;
    uint32_t fcr;
    uint64_t rx_ring;
    uint64_t rx_descriptor;
    uint64_t tx_ring;
    uint64_t tx_descriptor;

    uint32_t phy_status;
    uint32_t phy_control;
    uint32_t phy_advertise;
    uint32_t phy_int;
    uint32_t phy_int_mask;

    bool aspeed;
    uint32_t txdes0_edotr;
    uint32_t rxdes0_edorr;
    bool dma64;
};

#define TYPE_ASPEED_MII "aspeed-mmi"
OBJECT_DECLARE_SIMPLE_TYPE(AspeedMiiState, ASPEED_MII)

/*
 * AST2600 MII controller
 */
struct AspeedMiiState {
    /*< private >*/
    SysBusDevice parent_obj;

    FTGMAC100State *nic;

    MemoryRegion iomem;
    uint32_t phycr;
    uint32_t phydata;
};

#endif
