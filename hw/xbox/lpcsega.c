/*
 * QEMU Chihiro emulation
 *
 * Copyright (c) 2013 espes
 * Copyright (c) 2018-2021 Matt Borgerson
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
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "migration/vmstate.h"
#include "sysemu/sysemu.h"
#include "hw/char/serial.h"
#include "hw/isa/isa.h"
#include "qapi/error.h"

#define SEGA_CHIP_REVISION                  0xF0
#   define SEGA_CHIP_REVISION_CHIP_ID            0xFF00
#       define SEGA_CHIP_REVISION_FPGA_CHIP_ID      0x0000
#       define SEGA_CHIP_REVISION_ASIC_CHIP_ID      0x0100
#   define SEGA_CHIP_REVISION_REVISION_ID_MASK   0x00FF
#define SEGA_DIMM_SIZE                      0xF4
#   define SEGA_DIMM_SIZE_128M                  0
#   define SEGA_DIMM_SIZE_256M                  1
#   define SEGA_DIMM_SIZE_512M                  2
#   define SEGA_DIMM_SIZE_1024M                 3

#define TYPE_ISA_LPCSEGA_DEVICE "lpcsega"
#define ISA_LPCSEGA_DEVICE(obj) \
    OBJECT_CHECK(ISALPCSEGAState, (obj), TYPE_ISA_LPCSEGA_DEVICE)

// #define DEBUG
#ifdef DEBUG
# define DPRINTF(format, ...) printf(format, ## __VA_ARGS__)
#else
# define DPRINTF(format, ...) do { } while (0)
#endif

typedef struct LPCSEGAState {
    MemoryRegion io;
} LPCSEGAState;

typedef struct ISALPCSEGAState {
    ISADevice parent_obj;

    bool sysopt;
    uint16_t iobase;
    LPCSEGAState state;
} ISALPCSEGAState;

static void lpcsega_io_write(void *opaque, hwaddr addr, uint64_t val,
                               unsigned int size)
{
    DPRINTF("lpcsega io write 0x%02" HWADDR_PRIx " = 0x%02" PRIx64 "\n", addr, val);
}

static uint64_t lpcsega_io_read(void *opaque, hwaddr addr, unsigned int size)
{
    uint32_t val = 0;

    switch (addr) {
    case SEGA_CHIP_REVISION:
        val = SEGA_CHIP_REVISION_ASIC_CHIP_ID;
        break;
    case SEGA_DIMM_SIZE:
        val = SEGA_DIMM_SIZE_128M;
        break;
    }

    DPRINTF("lpcsega io read 0x%02" HWADDR_PRIx " -> 0x%02x\n", addr, val);

    return val;
}

static const MemoryRegionOps lpcsega_io_ops = {
    .read = lpcsega_io_read,
    .write = lpcsega_io_write,
    .valid = {
        .min_access_size = 2,
        .max_access_size = 2,
    },
};

static void lpcsega_realize(DeviceState *dev, Error **errp)
{
    ISADevice *isadev = ISA_DEVICE(dev);
    ISALPCSEGAState *isa = ISA_LPCSEGA_DEVICE(isadev);
    LPCSEGAState *s = &isa->state;

    memory_region_init_io(&s->io, OBJECT(dev), &lpcsega_io_ops, s,
                          "lpcsega-io", 0x100);
    isa_register_ioport(isadev, &s->io, 0x4000);
}

static Property lpcsega_properties[] = {
    DEFINE_PROP_BOOL("sysopt", ISALPCSEGAState, sysopt, false),
    DEFINE_PROP_END_OF_LIST(),
};

static void lpcsega_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = lpcsega_realize;
    device_class_set_props(dc, lpcsega_properties);
}

static void lpcsega_initfn(Object *o)
{
}

static const TypeInfo lpcsega_type_info = {
    .name          = TYPE_ISA_LPCSEGA_DEVICE,
    .parent        = TYPE_ISA_DEVICE,
    .instance_init = lpcsega_initfn,
    .instance_size = sizeof(ISALPCSEGAState),
    .class_init    = lpcsega_class_init,
};

static void lpcsega_register_types(void)
{
    type_register_static(&lpcsega_type_info);
}

type_init(lpcsega_register_types)
