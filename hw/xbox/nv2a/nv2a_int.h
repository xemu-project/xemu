/*
 * QEMU Geforce NV2A internal definitions
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2018-2025 Matt Borgerson
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

#ifndef HW_NV2A_INT_H
#define HW_NV2A_INT_H

#include <assert.h>

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/thread.h"
#include "qemu/queue.h"
#include "qemu/main-loop.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "migration/vmstate.h"
#include "system/runstate.h"
#include "ui/console.h"
#include "hw/display/vga_int.h"
#include "hw/pci/pci_device.h"
#include "exec/target_page.h"

#include "hw/hw.h"
#include "hw/display/vga.h"
#include "hw/display/vga_int.h"
#include "hw/display/vga_regs.h"
#include "hw/pci/pci.h"
#include "cpu.h"

#include "trace.h"

#include "nv2a.h"
#include "pgraph/pgraph.h"
#include "debug.h"
#include "nv2a_regs.h"

#define NV2A_DEVICE(obj) OBJECT_CHECK(NV2AState, (obj), "nv2a")

enum FIFOEngine {
    ENGINE_SOFTWARE = 0,
    ENGINE_GRAPHICS = 1,
    ENGINE_DVD = 2,
};

typedef struct DMAObject {
    unsigned int dma_class;
    unsigned int dma_target;
    hwaddr address;
    hwaddr limit;
} DMAObject;

typedef struct NV2AState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/

    qemu_irq irq;
    bool exiting;

    VGACommonState vga;
    GraphicHwOps hw_ops;
    QEMUTimer *vblank_timer;

    MemoryRegion *vram;
    MemoryRegion vram_pci;
    uint8_t *vram_ptr;
    MemoryRegion ramin;
    uint8_t *ramin_ptr;

    MemoryRegion mmio;
    MemoryRegion block_mmio[NV_NUM_BLOCKS];

    struct {
        uint32_t pending_interrupts;
        uint32_t enabled_interrupts;
    } pmc;

    struct {
        uint32_t pending_interrupts;
        uint32_t enabled_interrupts;
        uint32_t regs[0x2000];
        QemuMutex lock;
        QemuThread thread;
        QemuCond fifo_cond;
        QemuCond fifo_idle_cond;
        bool fifo_kick;
        bool halt;
    } pfifo;

    struct {
        uint32_t regs[0x1000];
    } pvideo;

    struct {
        uint32_t pending_interrupts;
        uint32_t enabled_interrupts;
        uint32_t numerator;
        uint32_t denominator;
        uint32_t alarm_time;
    } ptimer;

    struct {
        uint32_t regs[0x1000];
    } pfb;

    struct PGRAPHState pgraph;

    struct {
        uint32_t pending_interrupts;
        uint32_t enabled_interrupts;
        hwaddr start;
        uint32_t raster;
    } pcrtc;

    struct {
        uint32_t core_clock_coeff;
        uint64_t core_clock_freq;
        uint32_t memory_clock_coeff;
        uint32_t video_clock_coeff;
        uint32_t general_control;
        uint32_t fp_vdisplay_end;
        uint32_t fp_vcrtc;
        uint32_t fp_vsync_end;
        uint32_t fp_vvalid_end;
        uint32_t fp_hdisplay_end;
        uint32_t fp_hcrtc;
        uint32_t fp_hvalid_end;
    } pramdac;

    struct {
        uint16_t write_mode_address;
        uint8_t palette[256*3];
    } puserdac;

} NV2AState;

typedef struct NV2ABlockInfo {
    const char *name;
    hwaddr offset;
    uint64_t size;
    MemoryRegionOps ops;
} NV2ABlockInfo;
extern const NV2ABlockInfo blocktable[NV_NUM_BLOCKS];

void nv2a_update_irq(NV2AState *d);

static inline
void nv2a_reg_log_read(int block, hwaddr addr, unsigned int size, uint64_t val)
{
    const char *block_name = "UNK";
    if (block < ARRAY_SIZE(blocktable) && blocktable[block].name) {
        block_name = blocktable[block].name;
    }
    trace_nv2a_reg_read(block_name, addr, size, val);
}

static inline
void nv2a_reg_log_write(int block, hwaddr addr, unsigned int size, uint64_t val)
{
    const char *block_name = "UNK";
    if (block < ARRAY_SIZE(blocktable) && blocktable[block].name) {
        block_name = blocktable[block].name;
    }
    trace_nv2a_reg_write(block_name, addr, size, val);
}

#define DEFINE_PROTO(n) \
    uint64_t n##_read(void *opaque, hwaddr addr, unsigned int size); \
    void n##_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size);

DEFINE_PROTO(pmc)
DEFINE_PROTO(pbus)
DEFINE_PROTO(pfifo)
DEFINE_PROTO(prma)
DEFINE_PROTO(pvideo)
DEFINE_PROTO(ptimer)
DEFINE_PROTO(pcounter)
DEFINE_PROTO(pvpe)
DEFINE_PROTO(ptv)
DEFINE_PROTO(prmfb)
DEFINE_PROTO(prmvio)
DEFINE_PROTO(pfb)
DEFINE_PROTO(pstraps)
DEFINE_PROTO(pgraph)
DEFINE_PROTO(pcrtc)
DEFINE_PROTO(prmcio)
DEFINE_PROTO(pramdac)
DEFINE_PROTO(prmdio)
// DEFINE_PROTO(pramin)
DEFINE_PROTO(user)
#undef DEFINE_PROTO

DMAObject nv_dma_load(NV2AState *d, hwaddr dma_obj_address);
void *nv_dma_map(NV2AState *d, hwaddr dma_obj_address, hwaddr *len);

/**
 * Clips an image blit to fit into a GPU tile it overlaps.
 * @param blit_base_address Address of the blit target
 * @param len Length of the blit in bytes
 * @return The adjusted length
 */
hwaddr nv_clip_gpu_tile_blit(NV2AState *d, hwaddr blit_base_address,
                             hwaddr len);

#endif
