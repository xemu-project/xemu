/*
 * QEMU nForce Ethernet Controller register definitions
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

// clang-format off

#define NVNET_IRQ_STATUS                         0x000
#  define NVNET_IRQ_STATUS_RX                      0x00000002
#  define NVNET_IRQ_STATUS_RX_NOBUF                0x00000004
#  define NVNET_IRQ_STATUS_TX_ERR                  0x00000008
#  define NVNET_IRQ_STATUS_TX                      0x00000010
#  define NVNET_IRQ_STATUS_TIMER                   0x00000020
#  define NVNET_IRQ_STATUS_MIIEVENT                0x00000040
#define NVNET_IRQ_MASK                           0x004
#define NVNET_UNKNOWN_SETUP_REG6                 0x008
#  define NVNET_UNKNOWN_SETUP_REG6_VAL             3
/*
 * NVNET_POLLING_INTERVAL_DEFAULT is the interval length of the timer source on the nic
 * NVNET_POLLING_INTERVAL_DEFAULT=97 would result in an interval length of 1 ms
 */
#define NVNET_POLLING_INTERVAL                   0x00C
#  define NVNET_POLLING_INTERVAL_DEFAULT           970
#define NVNET_MISC1                              0x080
#  define NVNET_MISC1_HD                           0x00000002
#  define NVNET_MISC1_FORCE                        0x003B0F3C
#define NVNET_TRANSMITTER_CONTROL                0x084
#  define NVNET_TRANSMITTER_CONTROL_START          0x00000001
#define NVNET_TRANSMITTER_STATUS                 0x088
#  define NVNET_TRANSMITTER_STATUS_BUSY            0x00000001
#define NVNET_PACKET_FILTER                      0x08C
#  define NVNET_PACKET_FILTER_ALWAYS               0x007F0008
#  define NVNET_PACKET_FILTER_PROMISC              0x00000080
#  define NVNET_PACKET_FILTER_MYADDR               0x00000020
#define NVNET_OFFLOAD                            0x090
#  define NVNET_OFFLOAD_HOMEPHY                    0x00000601
#  define NVNET_OFFLOAD_NORMAL                     0x000005EE
#define NVNET_RECEIVER_CONTROL                   0x094
#  define NVNET_RECEIVER_CONTROL_START             0x00000001
#define NVNET_RECEIVER_STATUS                    0x098
#  define NVNET_RECEIVER_STATUS_BUSY               0x00000001
#define NVNET_RANDOM_SEED                        0x09C
#  define NVNET_RANDOM_SEED_MASK                   0x000000FF
#  define NVNET_RANDOM_SEED_FORCE                  0x00007F00
#define NVNET_UNKNOWN_SETUP_REG1                 0x0A0
#  define NVNET_UNKNOWN_SETUP_REG1_VAL             0x0016070F
#define NVNET_UNKNOWN_SETUP_REG2                 0x0A4
#  define NVNET_UNKNOWN_SETUP_REG2_VAL             0x00000016
#define NVNET_MAC_ADDR_A                         0x0A8
#define NVNET_MAC_ADDR_B                         0x0AC
#define NVNET_MULTICAST_ADDR_A                   0x0B0
#  define NVNET_MULTICAST_ADDR_A_FORCE             0x00000001
#define NVNET_MULTICAST_ADDR_B                   0x0B4
#define NVNET_MULTICAST_MASK_A                   0x0B8
#define NVNET_MULTICAST_MASK_B                   0x0BC
#define NVNET_TX_RING_PHYS_ADDR                  0x100
#define NVNET_RX_RING_PHYS_ADDR                  0x104
#define NVNET_RING_SIZE                          0x108
#  define NVNET_RING_SIZE_TX                       0x0000FFFF
#  define NVNET_RING_SIZE_RX                       0xFFFF0000
#define NVNET_UNKNOWN_TRANSMITTER_REG            0x10C
#define NVNET_LINKSPEED                          0x110
#  define NVNET_LINKSPEED_FORCE                    0x00010000
#  define NVNET_LINKSPEED_10                       10
#  define NVNET_LINKSPEED_100                      100
#  define NVNET_LINKSPEED_1000                     1000
#define NVNET_TX_RING_CURRENT_DESC_PHYS_ADDR     0x11C
#define NVNET_RX_RING_CURRENT_DESC_PHYS_ADDR     0x120
#define NVNET_TX_CURRENT_BUFFER_PHYS_ADDR        0x124
#define NVNET_RX_CURRENT_BUFFER_PHYS_ADDR        0x12C
#define NVNET_UNKNOWN_SETUP_REG5                 0x130
#  define NVNET_UNKNOWN_SETUP_REG5_BIT31           (1 << 31)
#define NVNET_TX_RING_NEXT_DESC_PHYS_ADDR        0x134
#define NVNET_RX_RING_NEXT_DESC_PHYS_ADDR        0x138
#define NVNET_UNKNOWN_SETUP_REG8                 0x13C
#  define NVNET_UNKNOWN_SETUP_REG8_VAL1            0x00300010
#define NVNET_UNKNOWN_SETUP_REG7                 0x140
#  define NVNET_UNKNOWN_SETUP_REG7_VAL             0x00300010
#define NVNET_TX_RX_CONTROL                      0x144
#  define NVNET_TX_RX_CONTROL_KICK                 0x00000001
#  define NVNET_TX_RX_CONTROL_BIT1                 0x00000002
#  define NVNET_TX_RX_CONTROL_BIT2                 0x00000004
#  define NVNET_TX_RX_CONTROL_IDLE                 0x00000008
#  define NVNET_TX_RX_CONTROL_RESET                0x00000010
#define NVNET_MII_STATUS                         0x180
#  define NVNET_MII_STATUS_ERROR                   0x00000001
#  define NVNET_MII_STATUS_LINKCHANGE              0x00000008
#define NVNET_UNKNOWN_SETUP_REG4                 0x184
#  define NVNET_UNKNOWN_SETUP_REG4_VAL             8
#define NVNET_ADAPTER_CONTROL                    0x188
#  define NVNET_ADAPTER_CONTROL_START              0x00000002
#  define NVNET_ADAPTER_CONTROL_LINKUP             0x00000004
#  define NVNET_ADAPTER_CONTROL_PHYVALID           0x00004000
#  define NVNET_ADAPTER_CONTROL_RUNNING            0x00100000
#  define NVNET_ADAPTER_CONTROL_PHYSHIFT           24
#define NVNET_MII_SPEED                          0x18C
#  define NVNET_MII_SPEED_BIT8                     (1 << 8)
#  define NVNET_MII_SPEED_DELAY                    5
#define NVNET_MDIO_ADDR                          0x190
#  define NVNET_MDIO_ADDR_INUSE                    0x00008000
#  define NVNET_MDIO_ADDR_WRITE                    0x00000400
#  define NVNET_MDIO_ADDR_PHYADDR                  0x000003E0
#  define NVNET_MDIO_ADDR_PHYREG                   0x0000001F
#define NVNET_MDIO_DATA                          0x194
#define NVNET_WAKEUPFLAGS                        0x200
#  define NVNET_WAKEUPFLAGS_VAL                    0x00007770
#  define NVNET_WAKEUPFLAGS_BUSYSHIFT              24
#  define NVNET_WAKEUPFLAGS_ENABLESHIFT            16
#  define NVNET_WAKEUPFLAGS_D3SHIFT                12
#  define NVNET_WAKEUPFLAGS_D2SHIFT                8
#  define NVNET_WAKEUPFLAGS_D1SHIFT                4
#  define NVNET_WAKEUPFLAGS_D0SHIFT                0
#  define NVNET_WAKEUPFLAGS_ACCEPT_MAGPAT          0x00000001
#  define NVNET_WAKEUPFLAGS_ACCEPT_WAKEUPPAT       0x00000002
#  define NVNET_WAKEUPFLAGS_ACCEPT_LINKCHANGE      0x00000004
#define NVNET_PATTERN_CRC                        0x204
#define NVNET_PATTERN_MASK                       0x208
#define NVNET_POWERCAP                           0x268
#  define NVNET_POWERCAP_D3SUPP                    (1 << 30)
#  define NVNET_POWERCAP_D2SUPP                    (1 << 26)
#  define NVNET_POWERCAP_D1SUPP                    (1 << 25)
#define NVNET_POWERSTATE                         0x26C
#  define NVNET_POWERSTATE_POWEREDUP               0x00008000
#  define NVNET_POWERSTATE_VALID                   0x00000100
#  define NVNET_POWERSTATE_MASK                    0x00000003
#  define NVNET_POWERSTATE_D0                      0x00000000
#  define NVNET_POWERSTATE_D1                      0x00000001
#  define NVNET_POWERSTATE_D2                      0x00000002
#  define NVNET_POWERSTATE_D3                      0x00000003

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

/* Link partner ability register. */
#define LPA_SLCT     0x001F  /* Same as advertise selector  */
#define LPA_RESV     0x1C00  /* Unused...                   */
#define LPA_RFAULT   0x2000  /* Link partner faulted        */
#define LPA_NPAGE    0x8000  /* Next page bit               */

// clang-format on

#endif /* HW_NVNET_REGS_H */
