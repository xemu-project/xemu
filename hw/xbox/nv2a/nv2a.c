/*
 * QEMU Geforce NV2A implementation
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2018-2020 Matt Borgerson
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

#include "hw/xbox/nv2a/nv2a_int.h"

void nv2a_update_irq(NV2AState *d)
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
        pci_irq_assert(PCI_DEVICE(d));
    } else {
        pci_irq_deassert(PCI_DEVICE(d));
    }
}

DMAObject nv_dma_load(NV2AState *d, hwaddr dma_obj_address)
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

void *nv_dma_map(NV2AState *d, hwaddr dma_obj_address, hwaddr *len)
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

const struct NV2ABlockInfo blocktable[] = {
    #define ENTRY(NAME, OFFSET, SIZE, RDFUNC, WRFUNC)  \
    [NV_##NAME] = {                                    \
        .name   = #NAME,                               \
        .offset = OFFSET,                              \
        .size   = SIZE,                                \
        .ops    = { .read = RDFUNC, .write = WRFUNC }, \
    }
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

#ifdef NV2A_DEBUG
static const char *nv2a_reg_names[] = {};

void nv2a_reg_log_read(int block, hwaddr addr, uint64_t val)
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

void nv2a_reg_log_write(int block, hwaddr addr, uint64_t val)
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
#endif

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
    NV2AState *d = container_of(s, NV2AState, vga);

    int depth = s->cr[0x28] & 3;

    int bpp;
    switch (depth) {
    case 0:
        /* FIXME: This case is sometimes hit during early Xbox startup.
         *        Presumably a race-condition where VGA isn't initialized, yet.
         *        `bpp = 0` mimics old code that did `bpp = depth * 8;`.
         *        This works around the issue of this mode being unhandled.
         *        However, QEMU VGA uses a 4bpp mode if `bpp = 0`.
         *        We don't know if Xbox hardware would do the same. */
        bpp = 0;
        break;
    case 2:
        bpp = d->pramdac.general_control &
              NV_PRAMDAC_GENERAL_CONTROL_ALT_MODE_SEL ? 16 : 15;
        break;
    case 3:
        bpp = 32;
        break;
    default:
        /* This is only a fallback path */
        bpp = depth * 8;
        fprintf(stderr, "Unknown VGA depth: %d\n", depth);
        assert(false);
        break;
    }

    return bpp;
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
    nv2a_update_irq(d);
}

static void nv2a_init_memory(NV2AState *d, MemoryRegion *ram)
{
    /* xbox is UMA - vram *is* ram */
    d->vram = ram;

     /* PCI exposed vram */
    memory_region_init_alias(&d->vram_pci, OBJECT(d), "nv2a-vram-pci", d->vram,
                             0, memory_region_size(d->vram));
    pci_register_bar(PCI_DEVICE(d), 1, PCI_BASE_ADDRESS_MEM_PREFETCH, &d->vram_pci);


    /* RAMIN - should be in vram somewhere, but not quite sure where atm */
    memory_region_init_ram(&d->ramin, OBJECT(d), "nv2a-ramin", 0x100000, &error_fatal);
    /* memory_region_init_alias(&d->ramin, "nv2a-ramin", &d->vram,
                         memory_region_size(d->vram) - 0x100000,
                         0x100000); */

    memory_region_add_subregion(&d->mmio, 0x700000, &d->ramin);


    d->vram_ptr = memory_region_get_ram_ptr(d->vram);
    d->ramin_ptr = memory_region_get_ram_ptr(&d->ramin);

    memory_region_set_log(d->vram, true, DIRTY_MEMORY_NV2A);
    memory_region_set_log(d->vram, true, DIRTY_MEMORY_NV2A_TEX);
    memory_region_set_dirty(d->vram, 0, memory_region_size(d->vram));

    /* hacky. swap out vga's vram */
    memory_region_destroy(&d->vga.vram);
    // memory_region_unref(&d->vga.vram); // FIXME: Is ths right?
    memory_region_init_alias(&d->vga.vram, OBJECT(d), "vga.vram",
                             d->vram, 0, memory_region_size(d->vram));
    d->vga.vram_ptr = memory_region_get_ram_ptr(&d->vga.vram);
    vga_dirty_log_start(&d->vga);

    pgraph_init(d);

    /* fire up pfifo */
    qemu_thread_create(&d->pfifo.thread, "nv2a.pfifo_thread",
                       pfifo_thread, d, QEMU_THREAD_JOINABLE);
}

static void nv2a_lock_fifo(NV2AState *d)
{
    qemu_mutex_lock(&d->pfifo.lock);
    qemu_cond_broadcast(&d->pfifo.fifo_cond);
    qemu_mutex_unlock_iothread();
    qemu_cond_wait(&d->pfifo.fifo_idle_cond, &d->pfifo.lock);
    qemu_mutex_lock_iothread();
    qemu_mutex_lock(&d->pgraph.lock);
}

static void nv2a_unlock_fifo(NV2AState *d)
{
    pfifo_kick(d);
    qemu_mutex_unlock(&d->pgraph.lock);
    qemu_mutex_unlock(&d->pfifo.lock);
}

static void nv2a_reset(NV2AState *d)
{
    nv2a_lock_fifo(d);

    memset(d->pfifo.regs, 0, sizeof(d->pfifo.regs));
    memset(d->pgraph.regs, 0, sizeof(d->pgraph.regs));

    d->pcrtc.start = 0;
    d->pramdac.core_clock_coeff = 0x00011c01; /* 189MHz...? */
    d->pramdac.core_clock_freq = 189000000;
    d->pramdac.memory_clock_coeff = 0;
    d->pramdac.video_clock_coeff = 0x0003C20D; /* 25182Khz...? */

    d->pfifo.regs[NV_PFIFO_CACHE1_STATUS] |= NV_PFIFO_CACHE1_STATUS_LOW_MARK;

    vga_common_reset(&d->vga);

    d->pgraph.waiting_for_nop = false;
    d->pgraph.waiting_for_flip = false;
    d->pgraph.waiting_for_fifo_access = false;
    d->pgraph.waiting_for_context_switch = false;
    d->pgraph.flush_pending = true;

    d->pmc.pending_interrupts = 0;
    d->pfifo.pending_interrupts = 0;
    d->ptimer.pending_interrupts = 0;
    d->pcrtc.pending_interrupts = 0;

    nv2a_unlock_fifo(d);
}

static void nv2a_realize(PCIDevice *dev, Error **errp)
{
    NV2AState *d = NV2A_DEVICE(dev);

    /* setting subsystem ids again, see comment in nv2a_class_init() */
    pci_set_word(dev->config + PCI_SUBSYSTEM_VENDOR_ID, 0);
    pci_set_word(dev->config + PCI_SUBSYSTEM_ID, 0);
    dev->config[PCI_INTERRUPT_PIN] = 0x01;

    /* legacy VGA shit */
    VGACommonState *vga = &d->vga;
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
    pci_register_bar(PCI_DEVICE(d), 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &d->mmio);

    for (int i=0; i < ARRAY_SIZE(blocktable); i++) {
        if (!blocktable[i].name) continue;
        memory_region_init_io(&d->block_mmio[i], OBJECT(dev),
                              &blocktable[i].ops, d,
                              blocktable[i].name, blocktable[i].size);
        memory_region_add_subregion(&d->mmio, blocktable[i].offset,
                                    &d->block_mmio[i]);
    }

    qemu_mutex_init(&d->pfifo.lock);
    qemu_cond_init(&d->pfifo.fifo_cond);
    qemu_cond_init(&d->pfifo.fifo_idle_cond);
}

static void nv2a_exitfn(PCIDevice *dev)
{
    NV2AState *d;
    d = NV2A_DEVICE(dev);

    d->exiting = true;

    qemu_cond_broadcast(&d->pfifo.fifo_cond);
    qemu_thread_join(&d->pfifo.thread);

    pgraph_destroy(&d->pgraph);
}

static void qdev_nv2a_reset(DeviceState *dev)
{
    NV2AState *d = NV2A_DEVICE(dev);
    nv2a_reset(d);
}

// Note: This is handled as a VM state change and not as a `pre_save` callback
// because we want to halt the FIFO before any VM state is saved/restored to
// avoid corruption.
static void nv2a_vm_state_change(void *opaque, int running, RunState state)
{
    NV2AState *d = opaque;
    if (state == RUN_STATE_SAVE_VM) {
        // FIXME: writeback all surfaces to RAM before snapshot
        nv2a_lock_fifo(d);
    } else if (state == RUN_STATE_RESTORE_VM) {
        nv2a_reset(d); // Early reset to avoid changing any state during load
    }
}

static int nv2a_post_save(void *opaque)
{
    NV2AState *d = opaque;
    nv2a_unlock_fifo(d);
    return 0;
}

static int nv2a_pre_load(void *opaque)
{
    NV2AState *d = opaque;
    nv2a_lock_fifo(d);
    return 0;
}

static int nv2a_post_load(void *opaque, int version_id)
{
    NV2AState *d = opaque;
    d->pgraph.flush_pending = true;
    nv2a_unlock_fifo(d);
    return 0;
}

const VMStateDescription vmstate_nv2a_pgraph_vertex_attributes = {
    .name = "nv2a/pgraph/vertex-attr",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields = (VMStateField[]) {
        // FIXME
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_nv2a = {
    .name = "nv2a",
    .version_id = 1,
    .minimum_version_id = 1,
    .post_save = nv2a_post_save,
    .post_load = nv2a_post_load,
    .pre_load = nv2a_pre_load,
    .fields = (VMStateField[]) {
        // FIXME: Split this up into subsections
        VMSTATE_PCI_DEVICE(parent_obj, NV2AState),
        VMSTATE_STRUCT(vga, NV2AState, 0, vmstate_vga_common, VGACommonState),
        VMSTATE_UINT32(pgraph.pending_interrupts, NV2AState),
        VMSTATE_UINT32(pgraph.enabled_interrupts, NV2AState),
        VMSTATE_UINT64(pgraph.context_surfaces_2d.object_instance, NV2AState),
        VMSTATE_UINT64(pgraph.context_surfaces_2d.dma_image_source, NV2AState),
        VMSTATE_UINT64(pgraph.context_surfaces_2d.dma_image_dest, NV2AState),
        VMSTATE_UINT32(pgraph.context_surfaces_2d.color_format, NV2AState),
        VMSTATE_UINT32(pgraph.context_surfaces_2d.source_pitch, NV2AState),
        VMSTATE_UINT32(pgraph.context_surfaces_2d.dest_pitch, NV2AState),
        VMSTATE_UINT64(pgraph.context_surfaces_2d.source_offset, NV2AState),
        VMSTATE_UINT64(pgraph.context_surfaces_2d.dest_offset, NV2AState),
        VMSTATE_UINT64(pgraph.image_blit.object_instance, NV2AState),
        VMSTATE_UINT64(pgraph.image_blit.context_surfaces, NV2AState),
        VMSTATE_UINT32(pgraph.image_blit.operation, NV2AState),
        VMSTATE_UINT32(pgraph.image_blit.in_x, NV2AState),
        VMSTATE_UINT32(pgraph.image_blit.in_y, NV2AState),
        VMSTATE_UINT32(pgraph.image_blit.out_x, NV2AState),
        VMSTATE_UINT32(pgraph.image_blit.out_y, NV2AState),
        VMSTATE_UINT32(pgraph.image_blit.width, NV2AState),
        VMSTATE_UINT32(pgraph.image_blit.height, NV2AState),
        VMSTATE_UINT64(pgraph.kelvin.object_instance, NV2AState),
        VMSTATE_UINT64(pgraph.dma_color, NV2AState),
        VMSTATE_UINT64(pgraph.dma_zeta, NV2AState),
        VMSTATE_BOOL(pgraph.surface_color.draw_dirty, NV2AState),
        VMSTATE_BOOL(pgraph.surface_zeta.draw_dirty, NV2AState),
        VMSTATE_BOOL(pgraph.surface_color.buffer_dirty, NV2AState),
        VMSTATE_BOOL(pgraph.surface_zeta.buffer_dirty, NV2AState),
        VMSTATE_BOOL(pgraph.surface_color.write_enabled_cache, NV2AState),
        VMSTATE_BOOL(pgraph.surface_zeta.write_enabled_cache, NV2AState),
        VMSTATE_UINT32(pgraph.surface_color.pitch, NV2AState),
        VMSTATE_UINT32(pgraph.surface_zeta.pitch, NV2AState),
        VMSTATE_UINT64(pgraph.surface_color.offset, NV2AState),
        VMSTATE_UINT64(pgraph.surface_zeta.offset, NV2AState),
        VMSTATE_UINT32(pgraph.surface_type, NV2AState),
        VMSTATE_UINT32(pgraph.surface_shape.z_format, NV2AState),
        VMSTATE_UINT32(pgraph.surface_shape.color_format, NV2AState),
        VMSTATE_UINT32(pgraph.surface_shape.zeta_format, NV2AState),
        VMSTATE_UINT32(pgraph.surface_shape.log_width, NV2AState),
        VMSTATE_UINT32(pgraph.surface_shape.log_height, NV2AState),
        VMSTATE_UINT32(pgraph.surface_shape.clip_x, NV2AState),
        VMSTATE_UINT32(pgraph.surface_shape.clip_width, NV2AState),
        VMSTATE_UINT32(pgraph.surface_shape.clip_height, NV2AState),
        VMSTATE_UINT32(pgraph.surface_shape.anti_aliasing, NV2AState),
        VMSTATE_UINT32(pgraph.last_surface_shape.z_format, NV2AState),
        VMSTATE_UINT32(pgraph.last_surface_shape.color_format, NV2AState),
        VMSTATE_UINT32(pgraph.last_surface_shape.zeta_format, NV2AState),
        VMSTATE_UINT32(pgraph.last_surface_shape.log_width, NV2AState),
        VMSTATE_UINT32(pgraph.last_surface_shape.log_height, NV2AState),
        VMSTATE_UINT32(pgraph.last_surface_shape.clip_x, NV2AState),
        VMSTATE_UINT32(pgraph.last_surface_shape.clip_width, NV2AState),
        VMSTATE_UINT32(pgraph.last_surface_shape.clip_height, NV2AState),
        VMSTATE_UINT32(pgraph.last_surface_shape.anti_aliasing, NV2AState),
        VMSTATE_UINT64(pgraph.dma_a, NV2AState),
        VMSTATE_UINT64(pgraph.dma_b, NV2AState),
        VMSTATE_UINT64(pgraph.dma_state, NV2AState),
        VMSTATE_UINT64(pgraph.dma_notifies, NV2AState),
        VMSTATE_UINT64(pgraph.dma_semaphore, NV2AState),
        VMSTATE_UINT64(pgraph.dma_report, NV2AState),
        VMSTATE_UINT64(pgraph.report_offset, NV2AState),
        VMSTATE_UINT64(pgraph.dma_vertex_a, NV2AState),
        VMSTATE_UINT64(pgraph.dma_vertex_b, NV2AState),
        VMSTATE_UINT32_2DARRAY(pgraph.program_data, NV2AState, NV2A_MAX_TRANSFORM_PROGRAM_LENGTH, VSH_TOKEN_SIZE),
        VMSTATE_UINT32_2DARRAY(pgraph.vsh_constants, NV2AState, NV2A_VERTEXSHADER_CONSTANTS, 4),
        VMSTATE_BOOL_ARRAY(pgraph.vsh_constants_dirty, NV2AState, NV2A_VERTEXSHADER_CONSTANTS),
        VMSTATE_UINT32_2DARRAY(pgraph.ltctxa, NV2AState, NV2A_LTCTXA_COUNT, 4),
        VMSTATE_BOOL_ARRAY(pgraph.ltctxa_dirty, NV2AState, NV2A_LTCTXA_COUNT),
        VMSTATE_UINT32_2DARRAY(pgraph.ltctxb, NV2AState, NV2A_LTCTXB_COUNT, 4),
        VMSTATE_BOOL_ARRAY(pgraph.ltctxb_dirty, NV2AState, NV2A_LTCTXB_COUNT),
        VMSTATE_UINT32_2DARRAY(pgraph.ltc1, NV2AState, NV2A_LTC1_COUNT, 4),
        VMSTATE_BOOL_ARRAY(pgraph.ltc1_dirty, NV2AState, NV2A_LTC1_COUNT),
        VMSTATE_STRUCT_ARRAY(pgraph.vertex_attributes, NV2AState, NV2A_VERTEXSHADER_ATTRIBUTES, 1, vmstate_nv2a_pgraph_vertex_attributes, VertexAttribute),
        VMSTATE_UINT32(pgraph.inline_array_length, NV2AState),
        VMSTATE_UINT32_ARRAY(pgraph.inline_array, NV2AState, NV2A_MAX_BATCH_LENGTH),
        VMSTATE_UINT32(pgraph.inline_elements_length, NV2AState), // fixme
        VMSTATE_UINT32_ARRAY(pgraph.inline_elements, NV2AState, NV2A_MAX_BATCH_LENGTH),
        VMSTATE_UINT32(pgraph.inline_buffer_length, NV2AState), // fixme
        VMSTATE_UINT32(pgraph.draw_arrays_length, NV2AState), // fixme
        VMSTATE_UINT32(pgraph.draw_arrays_max_count, NV2AState), // fixme
        // GLint gl_draw_arrays_start[1000]; // fixme
        // GLsizei gl_draw_arrays_count[1000]; // fixme
        VMSTATE_UINT32_ARRAY(pgraph.regs, NV2AState, 0x2000),
        VMSTATE_UINT32(pmc.pending_interrupts, NV2AState),
        VMSTATE_UINT32(pmc.enabled_interrupts, NV2AState),
        VMSTATE_UINT32(pfifo.pending_interrupts, NV2AState),
        VMSTATE_UINT32(pfifo.enabled_interrupts, NV2AState),
        VMSTATE_UINT32_ARRAY(pfifo.regs, NV2AState, 0x2000),
        VMSTATE_UINT32_ARRAY(pvideo.regs, NV2AState, 0x1000),
        VMSTATE_UINT32(ptimer.pending_interrupts, NV2AState),
        VMSTATE_UINT32(ptimer.enabled_interrupts, NV2AState),
        VMSTATE_UINT32(ptimer.numerator, NV2AState),
        VMSTATE_UINT32(ptimer.denominator, NV2AState),
        VMSTATE_UINT32(ptimer.alarm_time, NV2AState),
        VMSTATE_UINT32_ARRAY(pfb.regs, NV2AState, 0x1000),
        VMSTATE_UINT32(pcrtc.pending_interrupts, NV2AState),
        VMSTATE_UINT32(pcrtc.enabled_interrupts, NV2AState),
        VMSTATE_UINT64(pcrtc.start, NV2AState),
        VMSTATE_UINT32(pramdac.core_clock_coeff, NV2AState),
        VMSTATE_UINT64(pramdac.core_clock_freq, NV2AState),
        VMSTATE_UINT32(pramdac.memory_clock_coeff, NV2AState),
        VMSTATE_UINT32(pramdac.video_clock_coeff, NV2AState),
        VMSTATE_BOOL(pgraph.waiting_for_flip, NV2AState),
        VMSTATE_BOOL(pgraph.waiting_for_nop, NV2AState),
        VMSTATE_BOOL(pgraph.waiting_for_fifo_access, NV2AState),
        VMSTATE_BOOL(pgraph.waiting_for_context_switch, NV2AState),
        VMSTATE_END_OF_LIST()
    },
};

static void nv2a_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->vendor_id = PCI_VENDOR_ID_NVIDIA;
    k->device_id = PCI_DEVICE_ID_NVIDIA_GEFORCE_NV2A;
    k->revision  = 0xA1;
    k->class_id  = PCI_CLASS_DISPLAY_VGA;
    /* When both subsystem ids are set to 0, QEMU sets them to its own
     * default values. However we set them anyway in case upstream decides
     * to change this behavior. */
    k->subsystem_vendor_id = 0;
    k->subsystem_id = 0;
    k->realize   = nv2a_realize;
    k->exit      = nv2a_exitfn;

    dc->desc = "GeForce NV2A Integrated Graphics";
    dc->vmsd = &vmstate_nv2a;
    dc->reset = qdev_nv2a_reset;
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
    qemu_add_vm_change_state_handler(nv2a_vm_state_change, d);
}
