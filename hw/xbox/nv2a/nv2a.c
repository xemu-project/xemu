/*
 * QEMU Geforce NV2A implementation
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2018 Matt Borgerson
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>

#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "qemu/main-loop.h"
#include "qapi/error.h"
#include "qemu/error-report.h"

#include "hw/hw.h"
#include "hw/display/vga.h"
#include "hw/display/vga_int.h"
#include "hw/display/vga_regs.h"
#include "hw/pci/pci.h"
#include "cpu.h"

#include "swizzle.h"

#include "hw/xbox/nv2a/nv2a_int.h"

#include "hw/xbox/nv2a/nv2a.h"


#define DEFINE_PROTO(n)                                              \
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

static void update_irq(NV2AState *d)
{
    /* PFIFO */
    if (d->pfifo.pending_interrupts & d->pfifo.enabled_interrupts) {
        d->pmc.pending_interrupts |= NV_PMC_INTR_0_PFIFO;
    } else {
        d->pmc.pending_interrupts &= ~NV_PMC_INTR_0_PFIFO;
    }

    /* PCRTC */
    if (d->pcrtc.pending_interrupts & d->pcrtc.enabled_interrupts) {
        d->pmc.pending_interrupts |= NV_PMC_INTR_0_PCRTC;
    } else {
        d->pmc.pending_interrupts &= ~NV_PMC_INTR_0_PCRTC;
    }

    /* PGRAPH */
    if (d->pgraph.pending_interrupts & d->pgraph.enabled_interrupts) {
        d->pmc.pending_interrupts |= NV_PMC_INTR_0_PGRAPH;
    } else {
        d->pmc.pending_interrupts &= ~NV_PMC_INTR_0_PGRAPH;
    }

    if (d->pmc.pending_interrupts && d->pmc.enabled_interrupts) {
        NV2A_DPRINTF("raise irq\n");
        pci_irq_assert(&d->dev);
    } else {
        pci_irq_deassert(&d->dev);
    }
}

static DMAObject nv_dma_load(NV2AState *d, hwaddr dma_obj_address)
{
    assert(dma_obj_address < memory_region_size(&d->ramin));

    uint32_t *dma_obj = (uint32_t *)(d->ramin_ptr + dma_obj_address);
    uint32_t flags = ldl_le_p(dma_obj);
    uint32_t limit = ldl_le_p(dma_obj + 1);
    uint32_t frame = ldl_le_p(dma_obj + 2);

    return (DMAObject){
        .dma_class  = GET_MASK(flags, NV_DMA_CLASS),
        .dma_target = GET_MASK(flags, NV_DMA_TARGET),
        .address    = (frame & NV_DMA_ADDRESS) | GET_MASK(flags, NV_DMA_ADJUST),
        .limit      = limit,
    };
}

static void *nv_dma_map(NV2AState *d, hwaddr dma_obj_address, hwaddr *len)
{
    DMAObject dma = nv_dma_load(d, dma_obj_address);

    /* TODO: Handle targets and classes properly */
    NV2A_DPRINTF("dma_map %" HWADDR_PRIx " - %x, %x, %" HWADDR_PRIx " %" HWADDR_PRIx "\n",
                 dma_obj_address,
                 dma.dma_class, dma.dma_target, dma.address, dma.limit);

    dma.address &= 0x07FFFFFF;

    assert(dma.address < memory_region_size(d->vram));
    // assert(dma.address + dma.limit < memory_region_size(d->vram));
    *len = dma.limit;
    return d->vram_ptr + dma.address;
}

#include "nv2a_pbus.c"
#include "nv2a_pcrtc.c"
#include "nv2a_pfb.c"
#include "nv2a_pgraph.c"
#include "nv2a_pfifo.c"
#include "nv2a_pmc.c"
#include "nv2a_pramdac.c"
#include "nv2a_prmcio.c"
#include "nv2a_prmvio.c"
#include "nv2a_ptimer.c"
#include "nv2a_pvideo.c"
#include "nv2a_stubs.c"
#include "nv2a_user.c"

#define ENTRY(NAME, OFFSET, SIZE, RDFUNC, WRFUNC)      \
    [NV_##NAME] = {                                    \
        .name   = #NAME,                               \
        .offset = OFFSET,                              \
        .size   = SIZE,                                \
        .ops    = { .read = RDFUNC, .write = WRFUNC }, \
    }

const struct NV2ABlockInfo blocktable[] = {
    ENTRY(PMC,      0x000000, 0x001000, pmc_read,      pmc_write),
    ENTRY(PBUS,     0x001000, 0x001000, pbus_read,     pbus_write),
    ENTRY(PFIFO,    0x002000, 0x002000, pfifo_read,    pfifo_write),
    ENTRY(PRMA,     0x007000, 0x001000, prma_read,     prma_write),
    ENTRY(PVIDEO,   0x008000, 0x001000, pvideo_read,   pvideo_write),
    ENTRY(PTIMER,   0x009000, 0x001000, ptimer_read,   ptimer_write),
    ENTRY(PCOUNTER, 0x00a000, 0x001000, pcounter_read, pcounter_write),
    ENTRY(PVPE,     0x00b000, 0x001000, pvpe_read,     pvpe_write),
    ENTRY(PTV,      0x00d000, 0x001000, ptv_read,      ptv_write),
    ENTRY(PRMFB,    0x0a0000, 0x020000, prmfb_read,    prmfb_write),
    ENTRY(PRMVIO,   0x0c0000, 0x001000, prmvio_read,   prmvio_write),
    ENTRY(PFB,      0x100000, 0x001000, pfb_read,      pfb_write),
    ENTRY(PSTRAPS,  0x101000, 0x001000, pstraps_read,  pstraps_write),
    ENTRY(PGRAPH,   0x400000, 0x002000, pgraph_read,   pgraph_write),
    ENTRY(PCRTC,    0x600000, 0x001000, pcrtc_read,    pcrtc_write),
    ENTRY(PRMCIO,   0x601000, 0x001000, prmcio_read,   prmcio_write),
    ENTRY(PRAMDAC,  0x680000, 0x001000, pramdac_read,  pramdac_write),
    ENTRY(PRMDIO,   0x681000, 0x001000, prmdio_read,   prmdio_write),
    // ENTRY(PRAMIN,   0x700000, 0x100000, pramin_read,   pramin_write),
    ENTRY(USER,     0x800000, 0x800000, user_read,     user_write),
};

#undef ENTRY

static const char* nv2a_reg_names[] = {};

static void reg_log_read(int block, hwaddr addr, uint64_t val)
{
    if (blocktable[block].name) {
        hwaddr naddr = blocktable[block].offset + addr;
        if (naddr < ARRAY_SIZE(nv2a_reg_names) && nv2a_reg_names[naddr]) {
            NV2A_DPRINTF("%s: read [%s] -> 0x%" PRIx64 "\n",
                    blocktable[block].name, nv2a_reg_names[naddr], val);
        } else {
            NV2A_DPRINTF("%s: read [%" HWADDR_PRIx "] -> 0x%" PRIx64 "\n",
                         blocktable[block].name, addr, val);
        }
    } else {
        NV2A_DPRINTF("(%d?): read [%" HWADDR_PRIx "] -> 0x%" PRIx64 "\n",
                     block, addr, val);
    }
}

static void reg_log_write(int block, hwaddr addr, uint64_t val)
{
    if (blocktable[block].name) {
        hwaddr naddr = blocktable[block].offset + addr;
        if (naddr < ARRAY_SIZE(nv2a_reg_names) && nv2a_reg_names[naddr]) {
            NV2A_DPRINTF("%s: [%s] = 0x%" PRIx64 "\n",
                    blocktable[block].name, nv2a_reg_names[naddr], val);
        } else {
            NV2A_DPRINTF("%s: [%" HWADDR_PRIx "] = 0x%" PRIx64 "\n",
                         blocktable[block].name, addr, val);
        }
    } else {
        NV2A_DPRINTF("(%d?): [%" HWADDR_PRIx "] = 0x%" PRIx64 "\n",
                     block, addr, val);
    }
}

#if 0
/* FIXME: Probably totally wrong */
static inline unsigned int rgb_to_pixel8(unsigned int r, unsigned int g,
                                         unsigned int b)
{
    return ((r >> 5) << 5) | ((g >> 5) << 2) | (b >> 6);
}
static inline unsigned int rgb_to_pixel16(unsigned int r, unsigned int g,
                                          unsigned int b)
{
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}
static inline unsigned int rgb_to_pixel32(unsigned int r, unsigned int g,
                                          unsigned int b)
{
    return (r << 16) | (g << 8) | b;
}

static void nv2a_overlay_draw_line(VGACommonState *vga, uint8_t *line, int y)
{
    NV2A_DPRINTF("nv2a_overlay_draw_line\n");

    NV2AState *d = container_of(vga, NV2AState, vga);
    DisplaySurface *surface = qemu_console_surface(d->vga.con);

    int surf_bpp = surface_bytes_per_pixel(surface);
    int surf_width = surface_width(surface);

    if (!(d->pvideo.regs[NV_PVIDEO_BUFFER] & NV_PVIDEO_BUFFER_0_USE)) return;

    hwaddr base = d->pvideo.regs[NV_PVIDEO_BASE];
    hwaddr limit = d->pvideo.regs[NV_PVIDEO_LIMIT];
    hwaddr offset = d->pvideo.regs[NV_PVIDEO_OFFSET];

    int in_width = GET_MASK(d->pvideo.regs[NV_PVIDEO_SIZE_IN],
                            NV_PVIDEO_SIZE_IN_WIDTH);
    int in_height = GET_MASK(d->pvideo.regs[NV_PVIDEO_SIZE_IN],
                             NV_PVIDEO_SIZE_IN_HEIGHT);
    int in_s = GET_MASK(d->pvideo.regs[NV_PVIDEO_POINT_IN],
                        NV_PVIDEO_POINT_IN_S);
    int in_t = GET_MASK(d->pvideo.regs[NV_PVIDEO_POINT_IN],
                        NV_PVIDEO_POINT_IN_T);
    int in_pitch = GET_MASK(d->pvideo.regs[NV_PVIDEO_FORMAT],
                            NV_PVIDEO_FORMAT_PITCH);
    int in_color = GET_MASK(d->pvideo.regs[NV_PVIDEO_FORMAT],
                            NV_PVIDEO_FORMAT_COLOR);

    // TODO: support other color formats
    assert(in_color == NV_PVIDEO_FORMAT_COLOR_LE_CR8YB8CB8YA8);

    int out_width = GET_MASK(d->pvideo.regs[NV_PVIDEO_SIZE_OUT],
                             NV_PVIDEO_SIZE_OUT_WIDTH);
    int out_height = GET_MASK(d->pvideo.regs[NV_PVIDEO_SIZE_OUT],
                             NV_PVIDEO_SIZE_OUT_HEIGHT);
    int out_x = GET_MASK(d->pvideo.regs[NV_PVIDEO_POINT_OUT],
                         NV_PVIDEO_POINT_OUT_X);
    int out_y = GET_MASK(d->pvideo.regs[NV_PVIDEO_POINT_OUT],
                         NV_PVIDEO_POINT_OUT_Y);


    if (y < out_y || y >= out_y + out_height) return;

    // TODO: scaling, color keys

    int in_y = y - out_y;
    if (in_y >= in_height) return;

    assert(offset + in_pitch * (in_y + 1) <= limit);
    uint8_t *in_line = d->vram_ptr + base + offset + in_pitch * in_y;

    int x;
    for (x=0; x<out_width; x++) {
        int ox = out_x + x;
        if (ox >= surf_width) break;
        int ix = in_s + x;
        if (ix >= in_width) break;

        uint8_t r,g,b;
        convert_yuy2_to_rgb(in_line, ix, &r, &g, &b);

        // unsigned int pixel = vga->rgb_to_pixel(r, g, b);
        switch (surf_bpp) {
        case 1:
            ((uint8_t*)line)[ox] = (uint8_t)rgb_to_pixel8(r,g,b);
            break;
        case 2:
            ((uint16_t*)line)[ox] = (uint16_t)rgb_to_pixel16(r,g,b);
            break;
        case 4:
            ((uint32_t*)line)[ox] = (uint32_t)rgb_to_pixel32(r,g,b);
            break;
        default:
            assert(false);
            break;
        }
    }
}
#endif

static int nv2a_get_bpp(VGACommonState *s)
{
    if ((s->cr[0x28] & 3) == 3) {
        return 32;
    }
    return (s->cr[0x28] & 3) * 8;
}

static void nv2a_get_offsets(VGACommonState *s,
                             uint32_t *pline_offset,
                             uint32_t *pstart_addr,
                             uint32_t *pline_compare)
{
    NV2AState *d = container_of(s, NV2AState, vga);
    uint32_t start_addr, line_offset, line_compare;

    line_offset = s->cr[0x13]
        | ((s->cr[0x19] & 0xe0) << 3)
        | ((s->cr[0x25] & 0x20) << 6);
    line_offset <<= 3;
    *pline_offset = line_offset;

    start_addr = d->pcrtc.start / 4;
    *pstart_addr = start_addr;

    line_compare = s->cr[VGA_CRTC_LINE_COMPARE] |
                   ((s->cr[VGA_CRTC_OVERFLOW] & 0x10) << 4) |
                   ((s->cr[VGA_CRTC_MAX_SCAN] & 0x40) << 3);
    *pline_compare = line_compare;
}

static void nv2a_vga_gfx_update(void *opaque)
{
    VGACommonState *vga = opaque;
    vga->hw_ops->gfx_update(vga);

    NV2AState *d = container_of(vga, NV2AState, vga);
    d->pcrtc.pending_interrupts |= NV_PCRTC_INTR_0_VBLANK;
    update_irq(d);
}

static void nv2a_init_memory(NV2AState *d, MemoryRegion *ram)
{
    /* xbox is UMA - vram *is* ram */
    d->vram = ram;

     /* PCI exposed vram */
    memory_region_init_alias(&d->vram_pci, OBJECT(d), "nv2a-vram-pci", d->vram,
                             0, memory_region_size(d->vram));
    pci_register_bar(&d->dev, 1, PCI_BASE_ADDRESS_MEM_PREFETCH, &d->vram_pci);


    /* RAMIN - should be in vram somewhere, but not quite sure where atm */
    memory_region_init_ram(&d->ramin, OBJECT(d), "nv2a-ramin", 0x100000, &error_fatal);
    /* memory_region_init_alias(&d->ramin, "nv2a-ramin", &d->vram,
                         memory_region_size(d->vram) - 0x100000,
                         0x100000); */

    memory_region_add_subregion(&d->mmio, 0x700000, &d->ramin);


    d->vram_ptr = memory_region_get_ram_ptr(d->vram);
    d->ramin_ptr = memory_region_get_ram_ptr(&d->ramin);

    memory_region_set_log(d->vram, true, DIRTY_MEMORY_NV2A);
    memory_region_set_dirty(d->vram, 0, memory_region_size(d->vram));

    /* hacky. swap out vga's vram */
    memory_region_destroy(&d->vga.vram);
    // memory_region_unref(&d->vga.vram); // FIXME: Is ths right?
    memory_region_init_alias(&d->vga.vram, OBJECT(d), "vga.vram",
                             d->vram, 0, memory_region_size(d->vram));
    d->vga.vram_ptr = memory_region_get_ram_ptr(&d->vga.vram);
    vga_dirty_log_start(&d->vga);

    pgraph_init(d);

    /* fire up puller */
    qemu_thread_create(&d->pfifo.puller_thread, "nv2a.puller_thread",
                       pfifo_puller_thread,
                       d, QEMU_THREAD_JOINABLE);

    /* fire up pusher */
    qemu_thread_create(&d->pfifo.pusher_thread, "nv2a.pusher_thread",
                       pfifo_pusher_thread,
                       d, QEMU_THREAD_JOINABLE);
}

static void nv2a_realize(PCIDevice *dev, Error **errp)
{
    int i;
    NV2AState *d;

    d = NV2A_DEVICE(dev);

    dev->config[PCI_INTERRUPT_PIN] = 0x01;

    d->pcrtc.start = 0;

    d->pramdac.core_clock_coeff = 0x00011c01; /* 189MHz...? */
    d->pramdac.core_clock_freq = 189000000;
    d->pramdac.memory_clock_coeff = 0;
    d->pramdac.video_clock_coeff = 0x0003C20D; /* 25182Khz...? */

    /* legacy VGA shit */
    VGACommonState *vga = &d->vga;
    vga_common_reset(vga);

    vga->vram_size_mb = 64;
    /* seems to start in color mode */
    vga->msr = VGA_MIS_COLOR;

    vga_common_init(vga, OBJECT(dev));
    vga->get_bpp = nv2a_get_bpp;
    vga->get_offsets = nv2a_get_offsets;
    // vga->overlay_draw_line = nv2a_overlay_draw_line;

    d->hw_ops = *vga->hw_ops;
    d->hw_ops.gfx_update = nv2a_vga_gfx_update;
    vga->con = graphic_console_init(DEVICE(dev), 0, &d->hw_ops, vga);

    /* mmio */
    memory_region_init(&d->mmio, OBJECT(dev), "nv2a-mmio", 0x1000000);
    pci_register_bar(&d->dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &d->mmio);

    for (i=0; i<ARRAY_SIZE(blocktable); i++) {
        if (!blocktable[i].name) continue;
        memory_region_init_io(&d->block_mmio[i], OBJECT(dev),
                              &blocktable[i].ops, d,
                              blocktable[i].name, blocktable[i].size);
        memory_region_add_subregion(&d->mmio, blocktable[i].offset,
                                    &d->block_mmio[i]);
    }

    qemu_mutex_init(&d->pfifo.lock);
    qemu_cond_init(&d->pfifo.puller_cond);
    qemu_cond_init(&d->pfifo.pusher_cond);

    d->pfifo.regs[NV_PFIFO_CACHE1_STATUS] |= NV_PFIFO_CACHE1_STATUS_LOW_MARK;
}

static void nv2a_exitfn(PCIDevice *dev)
{
    NV2AState *d;
    d = NV2A_DEVICE(dev);

    d->exiting = true;

    qemu_cond_broadcast(&d->pfifo.puller_cond);
    qemu_cond_broadcast(&d->pfifo.pusher_cond);
    qemu_thread_join(&d->pfifo.puller_thread);
    qemu_thread_join(&d->pfifo.pusher_thread);

    pgraph_destroy(&d->pgraph);
}

static void nv2a_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->vendor_id = PCI_VENDOR_ID_NVIDIA;
    k->device_id = PCI_DEVICE_ID_NVIDIA_GEFORCE_NV2A;
    k->revision  = 161;
    k->class_id  = PCI_CLASS_DISPLAY_3D;
    k->realize   = nv2a_realize;
    k->exit      = nv2a_exitfn;

    dc->desc = "GeForce NV2A Integrated Graphics";
}

static const TypeInfo nv2a_info = {
    .name          = "nv2a",
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(NV2AState),
    .class_init    = nv2a_class_init,
    .interfaces          = (InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

static void nv2a_register(void)
{
    type_register_static(&nv2a_info);
}
type_init(nv2a_register);

void nv2a_init(PCIBus *bus, int devfn, MemoryRegion *ram)
{
    PCIDevice *dev = pci_create_simple(bus, devfn, "nv2a");
    NV2AState *d = NV2A_DEVICE(dev);
    nv2a_init_memory(d, ram);
}
