/*
 * QEMU MCPX Audio Codec Interface implementation
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2020-2021 Matt Borgerson
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
#include "hw/hw.h"
#include "hw/i386/pc.h"
#include "hw/pci/pci.h"
#include "hw/audio/ac97_int.h"
#include "migration/vmstate.h"
#include "qemu/main-loop.h"
#include "qemu/rcu.h"
#include "qemu/timer.h"
#include "aci.h"

typedef struct MCPXACIState {
    PCIDevice dev;

    AC97LinkState ac97;

    MemoryRegion io_nam, io_nabm;

    MemoryRegion mmio;
    MemoryRegion nam_mmio, nabm_mmio;

    /* The bus masters run on the APU's frame thread (mcpx_aci_frame) while
     * the guest programs them under the BQL: one lock over both. */
    QemuMutex lock;
    /* The interrupt line is set from the BQL; a change from the frame
     * thread is applied by this bottom half. */
    QEMUBH *irq_bh;
    int irq_level;
    /* The bus masters are clocked by the sound engine's frames. While it
     * is not running, this timer keeps them consuming at 48 kHz so a guest
     * driving the ACI alone still sees its descriptors advance (nothing is
     * heard on that path). */
    QEMUTimer *fallback;
    int64_t last_frame_ns, fallback_ns;
} MCPXACIState;

#define MCPX_ACI_DEVICE(obj) \
    OBJECT_CHECK(MCPXACIState, (obj), "mcpx-aci")

static MCPXACIState *g_aci;

/* 20 ms without a frame from the sound engine and the fallback takes over. */
#define ACI_FALLBACK_PERIOD_NS (10 * SCALE_MS)
#define ACI_FALLBACK_AFTER_NS (20 * SCALE_MS)

static uint64_t aci_nam_read(void *opaque, hwaddr addr, unsigned size)
{
    MCPXACIState *d = opaque;
    QEMU_LOCK_GUARD(&d->lock);
    return ac97_io_nam_ops.read(&d->ac97, addr, size);
}

static void aci_nam_write(void *opaque, hwaddr addr, uint64_t val,
                          unsigned size)
{
    MCPXACIState *d = opaque;
    QEMU_LOCK_GUARD(&d->lock);
    ac97_io_nam_ops.write(&d->ac97, addr, val, size);
}

static uint64_t aci_nabm_read(void *opaque, hwaddr addr, unsigned size)
{
    MCPXACIState *d = opaque;
    QEMU_LOCK_GUARD(&d->lock);
    return ac97_io_nabm_ops.read(&d->ac97, addr, size);
}

static void aci_nabm_write(void *opaque, hwaddr addr, uint64_t val,
                           unsigned size)
{
    MCPXACIState *d = opaque;
    QEMU_LOCK_GUARD(&d->lock);
    ac97_io_nabm_ops.write(&d->ac97, addr, val, size);
}

static const MemoryRegionOps aci_nam_ops = {
    .read = aci_nam_read,
    .write = aci_nam_write,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static const MemoryRegionOps aci_nabm_ops = {
    .read = aci_nabm_read,
    .write = aci_nabm_write,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void aci_irq_bh(void *opaque)
{
    MCPXACIState *d = opaque;
    pci_set_irq(&d->dev, qatomic_read(&d->irq_level));
}

static void aci_irq_update(void *opaque, int level)
{
    MCPXACIState *d = opaque;
    qatomic_set(&d->irq_level, level);
    if (bql_locked()) {
        pci_set_irq(&d->dev, level);
    } else {
        qemu_bh_schedule(d->irq_bh);
    }
}

/* Run both bus masters for `frames` stereo frames: PCM out into `pcm`
 * (silence where the engine is not running), S/PDIF out into `spdif`. */
static void aci_run(MCPXACIState *d, int frames, int16_t *pcm,
                    uint8_t *spdif)
{
    RCU_READ_LOCK_GUARD();
    QEMU_LOCK_GUARD(&d->lock);
    ac97_xbox_run(&d->ac97, PO_INDEX, (uint8_t *)pcm, frames * 4);
    ac97_xbox_run(&d->ac97, SO_INDEX, spdif, frames * 4);
}

bool mcpx_aci_frame(int frames, int16_t *pcm, uint8_t *spdif)
{
    MCPXACIState *d = g_aci;
    if (!d) {
        return false;
    }
    aci_run(d, frames, pcm, spdif);
    qatomic_set(&d->last_frame_ns, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL));
    return true;
}

static void aci_fallback(void *opaque)
{
    MCPXACIState *d = opaque;
    int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    if (now - qatomic_read(&d->last_frame_ns) > ACI_FALLBACK_AFTER_NS) {
        int64_t since = d->fallback_ns ? now - d->fallback_ns : 0;
        int frames = MIN(since * 48 / SCALE_MS, 4800);
        while (frames > 0) {
            int16_t pcm[32][2];
            uint8_t spdif[32 * 4];
            int n = MIN(frames, 32);
            aci_run(d, n, &pcm[0][0], spdif);
            frames -= n;
        }
    }
    d->fallback_ns = now;
    timer_mod(d->fallback, now + ACI_FALLBACK_PERIOD_NS);
}

static void mcpx_aci_realize(PCIDevice *dev, Error **errp)
{
    MCPXACIState *d = MCPX_ACI_DEVICE(dev);

    dev->config[PCI_INTERRUPT_PIN] = 0x01;

    memory_region_init(&d->mmio, OBJECT(dev), "mcpx-aci-mmio", 0x1000);

    qemu_mutex_init(&d->lock);
    d->irq_bh = qemu_bh_new(aci_irq_bh, d);
    d->ac97.irq_update = aci_irq_update;
    d->ac97.irq_opaque = d;

    memory_region_init_io(&d->io_nam, OBJECT(dev), &aci_nam_ops, d,
                          "mcpx-aci-nam", 0x100);
    memory_region_init_io(&d->io_nabm, OBJECT(dev), &aci_nabm_ops, d,
                          "mcpx-aci-nabm", 0x80);

    /*pci_register_bar(&d->dev, 0, PCI_BASE_ADDRESS_SPACE_IO, &d->io_nam);
    pci_register_bar(&d->dev, 1, PCI_BASE_ADDRESS_SPACE_IO, &d->io_nabm);

    memory_region_init_alias(&d->nam_mmio, NULL, &d->io_nam, 0, 0x100);
    memory_region_add_subregion(&d->mmio, 0x0, &d->nam_mmio);

    memory_region_init_alias(&d->nabm_mmio, NULL, &d->io_nabm, 0, 0x80);
    memory_region_add_subregion(&d->mmio, 0x100, &d->nabm_mmio);*/

    memory_region_add_subregion(&d->mmio, 0x0, &d->io_nam);
    memory_region_add_subregion(&d->mmio, 0x100, &d->io_nabm);

    pci_register_bar(&d->dev, 2, PCI_BASE_ADDRESS_SPACE_MEMORY, &d->mmio);
    ac97_common_init(&d->ac97, &d->dev, pci_get_address_space(&d->dev));

    d->fallback = timer_new_ns(QEMU_CLOCK_VIRTUAL, aci_fallback, d);
    timer_mod(d->fallback,
              qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + ACI_FALLBACK_PERIOD_NS);
    g_aci = d;
}

static const VMStateDescription vmstate_mcpx_aci = {
    .name = "mcpx-aci",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_PCI_DEVICE(dev, MCPXACIState),
        // FIXME
        VMSTATE_END_OF_LIST()
    },
};

static void mcpx_aci_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->vendor_id = PCI_VENDOR_ID_NVIDIA;
    k->device_id = PCI_DEVICE_ID_NVIDIA_MCPX_ACI;
    k->revision = 177;
    k->class_id = PCI_CLASS_MULTIMEDIA_AUDIO;
    k->realize = mcpx_aci_realize;

    dc->desc = "MCPX Audio Codec Interface";
    dc->vmsd = &vmstate_mcpx_aci;
}

static const TypeInfo mcpx_aci_info = {
    .name          = "mcpx-aci",
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(MCPXACIState),
    .class_init    = mcpx_aci_class_init,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

static void mcpx_aci_register(void)
{
    type_register_static(&mcpx_aci_info);
}

type_init(mcpx_aci_register);
