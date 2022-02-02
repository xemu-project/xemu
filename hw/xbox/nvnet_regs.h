/*
 * QEMU nForce Ethernet Controller register definitions
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
 *
 * --
 *
 * Most definitions are based on forcedeth.c, taken from cromwell project.
 * Original forcedeth.c license follows:
 *
 * -- 
 *    forcedeth.c -- Etherboot device driver for the NVIDIA nForce 
 *           media access controllers.
 *
 * Note: This driver is based on the Linux driver that was based on
 *      a cleanroom reimplementation which was based on reverse
 *      engineered documentation written by Carl-Daniel Hailfinger
 *      and Andrew de Quincey. It's neither supported nor endorsed
 *      by NVIDIA Corp. Use at your own risk.
 *
 *    Written 2004 by Timothy Legge <tlegge@rogers.com>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *    Portions of this code based on:
 *       forcedeth: Ethernet driver for NVIDIA nForce media access controllers:
 *
 *   (C) 2003 Manfred Spraul
 *       See Linux Driver for full information
 *   
 *   Linux Driver Version 0.22, 19 Jan 2004
 * 
 * 
 *    REVISION HISTORY:
 *    ================
 *    v1.0   01-31-2004  timlegge    Initial port of Linux driver
 *    v1.1   02-03-2004  timlegge    Large Clean up, first release 
 *    
 *    Indent Options: indent -kr -i8
 ***************************************************************************/

#ifndef HW_NVNET_REGS_H
#define HW_NVNET_REGS_H

#define DEV_NEED_LASTPACKET1 0x0001
#define DEV_IRQMASK_1        0x0002
#define DEV_IRQMASK_2        0x0004
#define DEV_NEED_TIMERIRQ    0x0008

enum {
    NvRegIrqStatus = 0x000,
#       define NVREG_IRQSTAT_BIT1     0x002
#       define NVREG_IRQSTAT_BIT4     0x010
#       define NVREG_IRQSTAT_MIIEVENT 0x040
#       define NVREG_IRQSTAT_MASK     0x1ff
    NvRegIrqMask = 0x004,
#       define NVREG_IRQ_RX           0x0002
#       define NVREG_IRQ_RX_NOBUF     0x0004
#       define NVREG_IRQ_TX_ERR       0x0008
#       define NVREG_IRQ_TX2          0x0010
#       define NVREG_IRQ_TIMER        0x0020
#       define NVREG_IRQ_LINK         0x0040
#       define NVREG_IRQ_TX1          0x0100
#       define NVREG_IRQMASK_WANTED_1 0x005f
#       define NVREG_IRQMASK_WANTED_2 0x0147
#       define NVREG_IRQ_UNKNOWN      (~(NVREG_IRQ_RX | NVREG_IRQ_RX_NOBUF | \
    NVREG_IRQ_TX_ERR | NVREG_IRQ_TX2 | NVREG_IRQ_TIMER | NVREG_IRQ_LINK | \
    NVREG_IRQ_TX1))
    NvRegUnknownSetupReg6 = 0x008,
#       define NVREG_UNKSETUP6_VAL 3
/*
 * NVREG_POLL_DEFAULT is the interval length of the timer source on the nic
 * NVREG_POLL_DEFAULT=97 would result in an interval length of 1 ms
 */
    NvRegPollingInterval = 0x00c,
#       define NVREG_POLL_DEFAULT 970
    NvRegMisc1 = 0x080,
#       define NVREG_MISC1_HD    0x02
#       define NVREG_MISC1_FORCE 0x3b0f3c
    NvRegTransmitterControl = 0x084,
#       define NVREG_XMITCTL_START 0x01
    NvRegTransmitterStatus = 0x088,
#       define NVREG_XMITSTAT_BUSY 0x01
    NvRegPacketFilterFlags = 0x8c,
#       define NVREG_PFF_ALWAYS  0x7F0008
#       define NVREG_PFF_PROMISC 0x80
#       define NVREG_PFF_MYADDR  0x20
    NvRegOffloadConfig = 0x90,
#       define NVREG_OFFLOAD_HOMEPHY 0x601
#       define NVREG_OFFLOAD_NORMAL  0x5ee
    NvRegReceiverControl = 0x094,
#       define NVREG_RCVCTL_START 0x01
    NvRegReceiverStatus = 0x98,
#       define NVREG_RCVSTAT_BUSY  0x01
    NvRegRandomSeed = 0x9c,
#       define NVREG_RNDSEED_MASK  0x00ff
#       define NVREG_RNDSEED_FORCE 0x7f00
    NvRegUnknownSetupReg1 = 0xA0,
#       define NVREG_UNKSETUP1_VAL 0x16070f
    NvRegUnknownSetupReg2 = 0xA4,
#       define NVREG_UNKSETUP2_VAL 0x16
    NvRegMacAddrA = 0xA8,
    NvRegMacAddrB = 0xAC,
    NvRegMulticastAddrA = 0xB0,
#       define NVREG_MCASTADDRA_FORCE  0x01
    NvRegMulticastAddrB = 0xB4,
    NvRegMulticastMaskA = 0xB8,
    NvRegMulticastMaskB = 0xBC,
    NvRegTxRingPhysAddr = 0x100,
    NvRegRxRingPhysAddr = 0x104,
    NvRegRingSizes = 0x108,
#       define NVREG_RINGSZ_TXSHIFT 0
#       define NVREG_RINGSZ_RXSHIFT 16
    NvRegUnknownTransmitterReg = 0x10c,
    NvRegLinkSpeed = 0x110,
#       define NVREG_LINKSPEED_FORCE 0x10000
#       define NVREG_LINKSPEED_10    10
#       define NVREG_LINKSPEED_100   100
#       define NVREG_LINKSPEED_1000  1000
    NvRegUnknownSetupReg5 = 0x130,
#       define NVREG_UNKSETUP5_BIT31 (1 << 31)
    NvRegUnknownSetupReg3 = 0x134,
#       define NVREG_UNKSETUP3_VAL1 0x200010
    NvRegUnknownSetupReg8 = 0x13C,
#       define NVREG_UNKSETUP8_VAL1 0x300010
    NvRegUnknownSetupReg7 = 0x140,
#       define NVREG_UNKSETUP7_VAL 0x300010
    NvRegTxRxControl = 0x144,
#       define NVREG_TXRXCTL_KICK  0x0001
#       define NVREG_TXRXCTL_BIT1  0x0002
#       define NVREG_TXRXCTL_BIT2  0x0004
#       define NVREG_TXRXCTL_IDLE  0x0008
#       define NVREG_TXRXCTL_RESET 0x0010
    NvRegMIIStatus = 0x180,
#       define NVREG_MIISTAT_ERROR      0x0001
#       define NVREG_MIISTAT_LINKCHANGE 0x0008
#       define NVREG_MIISTAT_MASK       0x000f
#       define NVREG_MIISTAT_MASK2      0x000f
    NvRegUnknownSetupReg4 = 0x184,
#       define NVREG_UNKSETUP4_VAL 8
    NvRegAdapterControl = 0x188,
#       define NVREG_ADAPTCTL_START    0x02
#       define NVREG_ADAPTCTL_LINKUP   0x04
#       define NVREG_ADAPTCTL_PHYVALID 0x4000
#       define NVREG_ADAPTCTL_RUNNING  0x100000
#       define NVREG_ADAPTCTL_PHYSHIFT 24
    NvRegMIISpeed = 0x18c,
#       define NVREG_MIISPEED_BIT8 (1 << 8)
#       define NVREG_MIIDELAY  5
    NvRegMIIControl = 0x190,
#       define NVREG_MIICTL_INUSE 0x10000
#       define NVREG_MIICTL_WRITE 0x08000
#       define NVREG_MIICTL_ADDRSHIFT  5
    NvRegMIIData = 0x194,
    NvRegWakeUpFlags = 0x200,
#       define NVREG_WAKEUPFLAGS_VAL               0x7770
#       define NVREG_WAKEUPFLAGS_BUSYSHIFT         24
#       define NVREG_WAKEUPFLAGS_ENABLESHIFT       16
#       define NVREG_WAKEUPFLAGS_D3SHIFT           12
#       define NVREG_WAKEUPFLAGS_D2SHIFT           8
#       define NVREG_WAKEUPFLAGS_D1SHIFT           4
#       define NVREG_WAKEUPFLAGS_D0SHIFT           0
#       define NVREG_WAKEUPFLAGS_ACCEPT_MAGPAT     0x01
#       define NVREG_WAKEUPFLAGS_ACCEPT_WAKEUPPAT  0x02
#       define NVREG_WAKEUPFLAGS_ACCEPT_LINKCHANGE 0x04
    NvRegPatternCRC = 0x204,
    NvRegPatternMask = 0x208,
    NvRegPowerCap = 0x268,
#       define NVREG_POWERCAP_D3SUPP (1 << 30)
#       define NVREG_POWERCAP_D2SUPP (1 << 26)
#       define NVREG_POWERCAP_D1SUPP (1 << 25)
    NvRegPowerState = 0x26c,
#       define NVREG_POWERSTATE_POWEREDUP 0x8000
#       define NVREG_POWERSTATE_VALID     0x0100
#       define NVREG_POWERSTATE_MASK      0x0003
#       define NVREG_POWERSTATE_D0        0x0000
#       define NVREG_POWERSTATE_D1        0x0001
#       define NVREG_POWERSTATE_D2        0x0002
#       define NVREG_POWERSTATE_D3        0x0003
};

#define NV_TX_LASTPACKET      (1 << 0)
#define NV_TX_RETRYERROR      (1 << 3)
#define NV_TX_LASTPACKET1     (1 << 8)
#define NV_TX_DEFERRED        (1 << 10)
#define NV_TX_CARRIERLOST     (1 << 11)
#define NV_TX_LATECOLLISION   (1 << 12)
#define NV_TX_UNDERFLOW       (1 << 13)
#define NV_TX_ERROR           (1 << 14)
#define NV_TX_VALID           (1 << 15)
#define NV_RX_DESCRIPTORVALID (1 << 0)
#define NV_RX_MISSEDFRAME     (1 << 1)
#define NV_RX_SUBSTRACT1      (1 << 3)
#define NV_RX_BIT4            (1 << 4)
#define NV_RX_ERROR1          (1 << 7)
#define NV_RX_ERROR2          (1 << 8)
#define NV_RX_ERROR3          (1 << 9)
#define NV_RX_ERROR4          (1 << 10)
#define NV_RX_CRCERR          (1 << 11)
#define NV_RX_OVERFLOW        (1 << 12)
#define NV_RX_FRAMINGERR      (1 << 13)
#define NV_RX_ERROR           (1 << 14)
#define NV_RX_AVAIL           (1 << 15)

/* Miscelaneous hardware related defines: */
#define NV_PCI_REGSZ          0x270

/* various timeout delays: all in usec */
#define NV_TXRX_RESET_DELAY   4
#define NV_TXSTOP_DELAY1      10
#define NV_TXSTOP_DELAY1MAX   500000
#define NV_TXSTOP_DELAY2      100
#define NV_RXSTOP_DELAY1      10
#define NV_RXSTOP_DELAY1MAX   500000
#define NV_RXSTOP_DELAY2      100
#define NV_SETUP5_DELAY       5
#define NV_SETUP5_DELAYMAX    50000
#define NV_POWERUP_DELAY      5
#define NV_POWERUP_DELAYMAX   5000
#define NV_MIIBUSY_DELAY      50
#define NV_MIIPHY_DELAY       10
#define NV_MIIPHY_DELAYMAX    10000
#define NV_WAKEUPPATTERNS     5
#define NV_WAKEUPMASKENTRIES  4

/* General driver defaults */
#define NV_WATCHDOG_TIMEO     (2 * HZ)
#define DEFAULT_MTU           1500

#define RX_RING               4
#define TX_RING               2
/* limited to 1 packet until we understand NV_TX_LASTPACKET */
#define TX_LIMIT_STOP         10
#define TX_LIMIT_START        5

/* rx / tx mac addr + type + vlan + align + slack*/
#define RX_NIC_BUFSIZE        (DEFAULT_MTU + 64)
/* even more slack */
#define RX_ALLOC_BUFSIZE      (DEFAULT_MTU + 128)
#define TX_ALLOC_BUFSIZE      (DEFAULT_MTU + 128)

#define OOM_REFILL            (1 + HZ / 20)
#define POLL_WAIT             (1 + HZ / 100)

#define MII_READ      (-1)
#define MII_PHYSID1   0x02    /* PHYS ID 1                   */
#define MII_PHYSID2   0x03    /* PHYS ID 2                   */
#define MII_BMCR      0x00    /* Basic mode control register */
#define MII_BMSR      0x01    /* Basic mode status register  */
#define MII_ADVERTISE 0x04    /* Advertisement control reg   */
#define MII_LPA       0x05    /* Link partner ability reg    */

#define BMSR_ANEGCOMPLETE 0x0020 /* Auto-negotiation complete   */
#define BMSR_BIT2         0x0004 /* Unknown... */

/* Link partner ability register. */
#define LPA_SLCT     0x001f  /* Same as advertise selector  */
#define LPA_10HALF   0x0020  /* Can do 10mbps half-duplex   */
#define LPA_10FULL   0x0040  /* Can do 10mbps full-duplex   */
#define LPA_100HALF  0x0080  /* Can do 100mbps half-duplex  */
#define LPA_100FULL  0x0100  /* Can do 100mbps full-duplex  */
#define LPA_100BASE4 0x0200  /* Can do 100mbps 4k packets   */
#define LPA_RESV     0x1c00  /* Unused...                   */
#define LPA_RFAULT   0x2000  /* Link partner faulted        */
#define LPA_LPACK    0x4000  /* Link partner acked us       */
#define LPA_NPAGE    0x8000  /* Next page bit               */

#endif /* HW_NVNET_REGS_H */
