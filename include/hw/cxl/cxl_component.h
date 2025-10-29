/*
 * QEMU CXL Component
 *
 * Copyright (c) 2020 Intel
 *
 * This work is licensed under the terms of the GNU GPL, version 2. See the
 * COPYING file in the top-level directory.
 */

#ifndef CXL_COMPONENT_H
#define CXL_COMPONENT_H

/* CXL r3.1 Section 8.2.4: CXL.cache and CXL.mem Registers  */
#define CXL2_COMPONENT_IO_REGION_SIZE 0x1000
#define CXL2_COMPONENT_CM_REGION_SIZE 0x1000
#define CXL2_COMPONENT_BLOCK_SIZE 0x10000

#include "qemu/range.h"
#include "hw/cxl/cxl_cdat.h"
#include "hw/register.h"
#include "qapi/error.h"

enum reg_type {
    CXL2_DEVICE,
    CXL2_TYPE3_DEVICE,
    CXL2_LOGICAL_DEVICE,
    CXL2_ROOT_PORT,
    CXL2_RC,
    CXL2_UPSTREAM_PORT,
    CXL2_DOWNSTREAM_PORT,
    CXL3_SWITCH_MAILBOX_CCI,
};

/*
 * Capability registers are defined at the top of the CXL.cache/mem region and
 * are packed. For our purposes we will always define the caps in the same
 * order.
 * CXL r3.1 Table 8-22: CXL_CAPABILITY_ID Assignment for details.
 */

/* CXL r3.1 Section 8.2.4.1: CXL Capability Header Register */
#define CXL_CAPABILITY_VERSION 1
REG32(CXL_CAPABILITY_HEADER, 0)
    FIELD(CXL_CAPABILITY_HEADER, ID, 0, 16)
    FIELD(CXL_CAPABILITY_HEADER, VERSION, 16, 4)
    FIELD(CXL_CAPABILITY_HEADER, CACHE_MEM_VERSION, 20, 4)
    FIELD(CXL_CAPABILITY_HEADER, ARRAY_SIZE, 24, 8)

#define CXLx_CAPABILITY_HEADER(type, offset)                  \
    REG32(CXL_##type##_CAPABILITY_HEADER, offset)             \
        FIELD(CXL_##type##_CAPABILITY_HEADER, ID, 0, 16)      \
        FIELD(CXL_##type##_CAPABILITY_HEADER, VERSION, 16, 4) \
        FIELD(CXL_##type##_CAPABILITY_HEADER, PTR, 20, 12)
CXLx_CAPABILITY_HEADER(RAS, 0x4)
CXLx_CAPABILITY_HEADER(LINK, 0x8)
CXLx_CAPABILITY_HEADER(HDM, 0xc)
CXLx_CAPABILITY_HEADER(EXTSEC, 0x10)
CXLx_CAPABILITY_HEADER(SNOOP, 0x14)

/*
 * Capability structures contain the actual registers that the CXL component
 * implements. Some of these are specific to certain types of components, but
 * this implementation leaves enough space regardless.
 */

/* CXL r3.1 Section 8.2.4.17: CXL RAS Capability Structure */
#define CXL_RAS_CAPABILITY_VERSION 3
/* Give ample space for caps before this */
#define CXL_RAS_REGISTERS_OFFSET 0x80
#define CXL_RAS_REGISTERS_SIZE   0x58
REG32(CXL_RAS_UNC_ERR_STATUS, CXL_RAS_REGISTERS_OFFSET)
#define CXL_RAS_UNC_ERR_CACHE_DATA_PARITY 0
#define CXL_RAS_UNC_ERR_CACHE_ADDRESS_PARITY 1
#define CXL_RAS_UNC_ERR_CACHE_BE_PARITY 2
#define CXL_RAS_UNC_ERR_CACHE_DATA_ECC 3
#define CXL_RAS_UNC_ERR_MEM_DATA_PARITY 4
#define CXL_RAS_UNC_ERR_MEM_ADDRESS_PARITY 5
#define CXL_RAS_UNC_ERR_MEM_BE_PARITY 6
#define CXL_RAS_UNC_ERR_MEM_DATA_ECC 7
#define CXL_RAS_UNC_ERR_REINIT_THRESHOLD 8
#define CXL_RAS_UNC_ERR_RSVD_ENCODING 9
#define CXL_RAS_UNC_ERR_POISON_RECEIVED 10
#define CXL_RAS_UNC_ERR_RECEIVER_OVERFLOW 11
#define CXL_RAS_UNC_ERR_INTERNAL 14
#define CXL_RAS_UNC_ERR_CXL_IDE_TX 15
#define CXL_RAS_UNC_ERR_CXL_IDE_RX 16
#define CXL_RAS_UNC_ERR_CXL_UNUSED 63 /* Magic value */
REG32(CXL_RAS_UNC_ERR_MASK, CXL_RAS_REGISTERS_OFFSET + 0x4)
REG32(CXL_RAS_UNC_ERR_SEVERITY, CXL_RAS_REGISTERS_OFFSET + 0x8)
REG32(CXL_RAS_COR_ERR_STATUS, CXL_RAS_REGISTERS_OFFSET + 0xc)
#define CXL_RAS_COR_ERR_CACHE_DATA_ECC 0
#define CXL_RAS_COR_ERR_MEM_DATA_ECC 1
#define CXL_RAS_COR_ERR_CRC_THRESHOLD 2
#define CXL_RAS_COR_ERR_RETRY_THRESHOLD 3
#define CXL_RAS_COR_ERR_CACHE_POISON_RECEIVED 4
#define CXL_RAS_COR_ERR_MEM_POISON_RECEIVED 5
#define CXL_RAS_COR_ERR_PHYSICAL 6
REG32(CXL_RAS_COR_ERR_MASK, CXL_RAS_REGISTERS_OFFSET + 0x10)
REG32(CXL_RAS_ERR_CAP_CTRL, CXL_RAS_REGISTERS_OFFSET + 0x14)
    FIELD(CXL_RAS_ERR_CAP_CTRL, FIRST_ERROR_POINTER, 0, 6)
    FIELD(CXL_RAS_ERR_CAP_CTRL, MULTIPLE_HEADER_RECORDING_CAP, 9, 1)
    FIELD(CXL_RAS_ERR_POISON_ENABLED, POISON_ENABLED, 13, 1)
REG32(CXL_RAS_ERR_HEADER0, CXL_RAS_REGISTERS_OFFSET + 0x18)
#define CXL_RAS_ERR_HEADER_NUM 32
/* Offset 0x18 - 0x58 reserved for RAS logs */

/* CXL r3.1 Section 8.2.4.18: CXL Security Capability Structure */
#define CXL_SEC_REGISTERS_OFFSET \
    (CXL_RAS_REGISTERS_OFFSET + CXL_RAS_REGISTERS_SIZE)
#define CXL_SEC_REGISTERS_SIZE   0 /* We don't implement 1.1 downstream ports */

/* CXL r3.1 Section 8.2.4.19: CXL Link Capability Structure */
#define CXL_LINK_CAPABILITY_VERSION 2
#define CXL_LINK_REGISTERS_OFFSET \
    (CXL_SEC_REGISTERS_OFFSET + CXL_SEC_REGISTERS_SIZE)
#define CXL_LINK_REGISTERS_SIZE   0x50

/* CXL r3.1 Section 8.2.4.20: CXL HDM Decoder Capability Structure */
#define HDM_DECODE_MAX 10 /* Maximum decoders for Devices */
#define CXL_HDM_CAPABILITY_VERSION 3
#define CXL_HDM_REGISTERS_OFFSET \
    (CXL_LINK_REGISTERS_OFFSET + CXL_LINK_REGISTERS_SIZE)
#define CXL_HDM_REGISTERS_SIZE (0x10 + 0x20 * HDM_DECODE_MAX)
#define HDM_DECODER_INIT(n)                                                    \
  REG32(CXL_HDM_DECODER##n##_BASE_LO,                                          \
        CXL_HDM_REGISTERS_OFFSET + (0x20 * n) + 0x10)                          \
            FIELD(CXL_HDM_DECODER##n##_BASE_LO, L, 28, 4)                      \
  REG32(CXL_HDM_DECODER##n##_BASE_HI,                                          \
        CXL_HDM_REGISTERS_OFFSET + (0x20 * n) + 0x14)                          \
  REG32(CXL_HDM_DECODER##n##_SIZE_LO,                                          \
        CXL_HDM_REGISTERS_OFFSET + (0x20 * n) + 0x18)                          \
  REG32(CXL_HDM_DECODER##n##_SIZE_HI,                                          \
        CXL_HDM_REGISTERS_OFFSET + (0x20 * n) + 0x1C)                          \
  REG32(CXL_HDM_DECODER##n##_CTRL,                                             \
        CXL_HDM_REGISTERS_OFFSET + (0x20 * n) + 0x20)                          \
            FIELD(CXL_HDM_DECODER##n##_CTRL, IG, 0, 4)                         \
            FIELD(CXL_HDM_DECODER##n##_CTRL, IW, 4, 4)                         \
            FIELD(CXL_HDM_DECODER##n##_CTRL, LOCK_ON_COMMIT, 8, 1)             \
            FIELD(CXL_HDM_DECODER##n##_CTRL, COMMIT, 9, 1)                     \
            FIELD(CXL_HDM_DECODER##n##_CTRL, COMMITTED, 10, 1)                 \
            FIELD(CXL_HDM_DECODER##n##_CTRL, ERR, 11, 1)                       \
            FIELD(CXL_HDM_DECODER##n##_CTRL, TYPE, 12, 1)                      \
            FIELD(CXL_HDM_DECODER##n##_CTRL, BI, 13, 1)                        \
            FIELD(CXL_HDM_DECODER##n##_CTRL, UIO, 14, 1)                       \
            FIELD(CXL_HDM_DECODER##n##_CTRL, UIG, 16, 4)                       \
            FIELD(CXL_HDM_DECODER##n##_CTRL, UIW, 20, 4)                       \
            FIELD(CXL_HDM_DECODER##n##_CTRL, ISP, 24, 4)                       \
  REG32(CXL_HDM_DECODER##n##_TARGET_LIST_LO,                                   \
        CXL_HDM_REGISTERS_OFFSET + (0x20 * n) + 0x24)                          \
  REG32(CXL_HDM_DECODER##n##_TARGET_LIST_HI,                                   \
        CXL_HDM_REGISTERS_OFFSET + (0x20 * n) + 0x28)                          \
  REG32(CXL_HDM_DECODER##n##_DPA_SKIP_LO,                                      \
        CXL_HDM_REGISTERS_OFFSET + (0x20 * n) + 0x24)                          \
  REG32(CXL_HDM_DECODER##n##_DPA_SKIP_HI,                                      \
        CXL_HDM_REGISTERS_OFFSET + (0x20 * n) + 0x28)

REG32(CXL_HDM_DECODER_CAPABILITY, CXL_HDM_REGISTERS_OFFSET)
    FIELD(CXL_HDM_DECODER_CAPABILITY, DECODER_COUNT, 0, 4)
    FIELD(CXL_HDM_DECODER_CAPABILITY, TARGET_COUNT, 4, 4)
    FIELD(CXL_HDM_DECODER_CAPABILITY, INTERLEAVE_256B, 8, 1)
    FIELD(CXL_HDM_DECODER_CAPABILITY, INTERLEAVE_4K, 9, 1)
    FIELD(CXL_HDM_DECODER_CAPABILITY, POISON_ON_ERR_CAP, 10, 1)
    FIELD(CXL_HDM_DECODER_CAPABILITY, 3_6_12_WAY, 11, 1)
    FIELD(CXL_HDM_DECODER_CAPABILITY, 16_WAY, 12, 1)
    FIELD(CXL_HDM_DECODER_CAPABILITY, UIO, 13, 1)
    FIELD(CXL_HDM_DECODER_CAPABILITY, UIO_DECODER_COUNT, 16, 4)
    FIELD(CXL_HDM_DECODER_CAPABILITY, MEMDATA_NXM_CAP, 20, 1)
    FIELD(CXL_HDM_DECODER_CAPABILITY, SUPPORTED_COHERENCY_MODEL, 21, 2)
REG32(CXL_HDM_DECODER_GLOBAL_CONTROL, CXL_HDM_REGISTERS_OFFSET + 4)
    FIELD(CXL_HDM_DECODER_GLOBAL_CONTROL, POISON_ON_ERR_EN, 0, 1)
    FIELD(CXL_HDM_DECODER_GLOBAL_CONTROL, HDM_DECODER_ENABLE, 1, 1)

/* Support 4 decoders at all levels of topology */
#define CXL_HDM_DECODER_COUNT 4

HDM_DECODER_INIT(0);
HDM_DECODER_INIT(1);
HDM_DECODER_INIT(2);
HDM_DECODER_INIT(3);

/*
 * CXL r3.1 Section 8.2.4.21: CXL Extended Security Capability Structure
 * (Root complex only)
 */
#define EXTSEC_ENTRY_MAX        256
#define CXL_EXTSEC_CAP_VERSION 2
#define CXL_EXTSEC_REGISTERS_OFFSET \
    (CXL_HDM_REGISTERS_OFFSET + CXL_HDM_REGISTERS_SIZE)
#define CXL_EXTSEC_REGISTERS_SIZE   (8 * EXTSEC_ENTRY_MAX + 4)

/* CXL r3.1 Section 8.2.4.22: CXL IDE Capability Structure */
#define CXL_IDE_CAP_VERSION 2
#define CXL_IDE_REGISTERS_OFFSET \
    (CXL_EXTSEC_REGISTERS_OFFSET + CXL_EXTSEC_REGISTERS_SIZE)
#define CXL_IDE_REGISTERS_SIZE   0x24

/* CXL r3.1 Section 8.2.4.23 - CXL Snoop Filter Capability Structure */
#define CXL_SNOOP_CAP_VERSION 1
#define CXL_SNOOP_REGISTERS_OFFSET \
    (CXL_IDE_REGISTERS_OFFSET + CXL_IDE_REGISTERS_SIZE)
#define CXL_SNOOP_REGISTERS_SIZE   0x8

QEMU_BUILD_BUG_MSG((CXL_SNOOP_REGISTERS_OFFSET +
                    CXL_SNOOP_REGISTERS_SIZE) >= 0x1000,
                   "No space for registers");

typedef struct component_registers {
    /*
     * Main memory region to be registered with QEMU core.
     */
    MemoryRegion component_registers;

    /*
     * CXL r3.1 Table 8-21: CXL Subsystem Component Register Ranges
     *   0x0000 - 0x0fff CXL.io registers
     *   0x1000 - 0x1fff CXL.cache and CXL.mem
     *   0x2000 - 0xdfff Implementation specific
     *   0xe000 - 0xe3ff CXL ARB/MUX registers
     *   0xe400 - 0xffff RSVD
     */
    uint32_t io_registers[CXL2_COMPONENT_IO_REGION_SIZE >> 2];
    MemoryRegion io;

    uint32_t cache_mem_registers[CXL2_COMPONENT_CM_REGION_SIZE >> 2];
    uint32_t cache_mem_regs_write_mask[CXL2_COMPONENT_CM_REGION_SIZE >> 2];
    MemoryRegion cache_mem;

    MemoryRegion impl_specific;
    MemoryRegion arb_mux;
    MemoryRegion rsvd;

    /* special_ops is used for any component that needs any specific handling */
    MemoryRegionOps *special_ops;
} ComponentRegisters;

/*
 * A CXL component represents all entities in a CXL hierarchy. This includes,
 * host bridges, root ports, upstream/downstream switch ports, and devices
 */
typedef struct cxl_component {
    ComponentRegisters crb;
    union {
        struct {
            Range dvsecs[CXL20_MAX_DVSEC];
            uint16_t dvsec_offset;
            struct PCIDevice *pdev;
        };
    };

    CDATObject cdat;
} CXLComponentState;

void cxl_component_register_block_init(Object *obj,
                                       CXLComponentState *cxl_cstate,
                                       const char *type);
void cxl_component_register_init_common(uint32_t *reg_state,
                                        uint32_t *write_msk,
                                        enum reg_type type);

void cxl_component_create_dvsec(CXLComponentState *cxl_cstate,
                                enum reg_type cxl_dev_type, uint16_t length,
                                uint16_t type, uint8_t rev, uint8_t *body);

int cxl_decoder_count_enc(int count);
int cxl_decoder_count_dec(int enc_cnt);

uint8_t cxl_interleave_ways_enc(int iw, Error **errp);
int cxl_interleave_ways_dec(uint8_t iw_enc, Error **errp);
uint8_t cxl_interleave_granularity_enc(uint64_t gran, Error **errp);

hwaddr cxl_decode_ig(int ig);

CXLComponentState *cxl_get_hb_cstate(PCIHostState *hb);
bool cxl_get_hb_passthrough(PCIHostState *hb);

bool cxl_doe_cdat_init(CXLComponentState *cxl_cstate, Error **errp);
void cxl_doe_cdat_release(CXLComponentState *cxl_cstate);
void cxl_doe_cdat_update(CXLComponentState *cxl_cstate, Error **errp);

#endif
