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
#include "hw/hw.h"
#include "hw/loader.h"
#include "hw/i386/pc.h"
#include "hw/i386/apic.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_ids.h"
#include "hw/usb.h"
#include "net/net.h"
#include "hw/boards.h"
#include "hw/ide.h"
#include "sysemu/kvm.h"
#include "hw/kvm/clock.h"
#include "sysemu/sysemu.h"
#include "hw/sysbus.h"
#include "sysemu/arch_init.h"
#include "hw/xen/xen.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "hw/acpi/acpi.h"
#include "cpu.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#ifdef CONFIG_XEN
#include <xen/hvm/hvm_info_table.h>
#include "hw/xen/xen_pt.h"
#endif
#include "migration/global_state.h"
#include "migration/misc.h"
#include "kvm_i386.h"
#include "sysemu/numa.h"

#include "hw/rtc/mc146818rtc.h"
#include "xbox_pci.h"
#include "smbus.h"

#include "qemu/option.h"
#include "xbox.h"

#include "block/block.h"

#define TYPE_CHIHIRO_MACHINE MACHINE_TYPE_NAME("chihiro")

#define CHIHIRO_MACHINE(obj) \
    OBJECT_CHECK(ChihiroMachineState, (obj), TYPE_CHIHIRO_MACHINE)

#define CHIHIRO_MACHINE_CLASS(klass) \
    OBJECT_CLASS_CHECK(ChihiroMachineClass, (klass), TYPE_CHIHIRO_MACHINE)

typedef struct ChihiroMachineState {
    /*< private >*/
    PCMachineState parent_obj;

    /*< public >*/
    char *mediaboard_rom;
    char *mediaboard_filesystem;
} ChihiroMachineState;

typedef struct ChihiroMachineClass {
    /*< private >*/
    PCMachineClass parent_class;

    /*< public >*/
} ChihiroMachineClass;



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

//#define DEBUG_CHIHIRO

typedef struct ChihiroLPCState {
    ISADevice dev;
    MemoryRegion ioport;
} ChihiroLPCState;

#define CHIHIRO_LPC_DEVICE(obj) \
    OBJECT_CHECK(ChihiroLPCState, (obj), "chihiro-lpc")


static uint64_t chhiro_lpc_io_read(void *opaque, hwaddr addr,
                                   unsigned size)
{
    uint64_t r = 0;
    switch (addr) {
    case SEGA_CHIP_REVISION:
        r = SEGA_CHIP_REVISION_ASIC_CHIP_ID;
        break;
    case SEGA_DIMM_SIZE:
        r = SEGA_DIMM_SIZE_128M;
        break;
    }
#ifdef DEBUG_CHIHIRO
    printf("chihiro lpc read [0x%llx] -> 0x%llx\n", addr, r);
#endif
    return r;
}

static void chhiro_lpc_io_write(void *opaque, hwaddr addr, uint64_t val,
                                unsigned size)
{
#ifdef DEBUG_CHIHIRO
    printf("chihiro lpc write [0x%llx] = 0x%llx\n", addr, val);
#endif
}

static const MemoryRegionOps chihiro_lpc_io_ops = {
    .read = chhiro_lpc_io_read,
    .write = chhiro_lpc_io_write,
    .impl = {
        .min_access_size = 2,
        .max_access_size = 2,
    },
};

static void chihiro_lpc_realize(DeviceState *dev, Error **errp)
{
    ChihiroLPCState *s = CHIHIRO_LPC_DEVICE(dev);
    ISADevice *isa = ISA_DEVICE(dev);

    memory_region_init_io(&s->ioport, OBJECT(dev), &chihiro_lpc_io_ops, s,
                          "chihiro-lpc-io", 0x100);
    isa_register_ioport(isa, &s->ioport, 0x4000);
}

static void chihiro_lpc_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = chihiro_lpc_realize;
    dc->desc = "Chihiro LPC";
}

static const TypeInfo chihiro_lpc_info = {
    .name          = "chihiro-lpc",
    .parent        = TYPE_ISA_DEVICE,
    .instance_size = sizeof(ChihiroLPCState),
    .class_init    = chihiro_lpc_class_init,
};

static void chihiro_register_types(void)
{
    type_register_static(&chihiro_lpc_info);
}

type_init(chihiro_register_types)


/* The chihiro baseboard communicates with the xbox by acting as an IDE
 * device. The device maps the boot rom from the mediaboard, a communication
 * area for interfacing with the network board, and the ram on the baseboard.
 * The baseboard ram is populated at boot from the gd-rom drive on the
 * mediaboard containing something like a combined disc+hdd image.
 */

#define FILESYSTEM_START         0
#define ROM_START                0x8000000
#define ROM_SECTORS              0x2000
#define COMMUNICATION_START      0x9000000
#define COMMUNICATION_SECTORS    0x10000
#define SECTOR_SIZE              512

static void chihiro_ide_interface_init(const char *rom_file,
                                       const char *filesystem_file)
{
    if (drive_get(IF_IDE, 0, 1)) {
        fprintf(stderr, "chihiro ide interface needs to be attached "
                        "to IDE device 1 but it's already in use.");
        exit(1);
    }

    MemoryRegion *interface, *rom, *filesystem;
    interface = g_malloc(sizeof(*interface));
    memory_region_init(interface, NULL, "chihiro.interface",
                       (uint64_t)0x10000000 * SECTOR_SIZE);

    rom = g_malloc(sizeof(*rom));
    memory_region_init_ram(rom, NULL, "chihiro.interface.rom",
                           ROM_SECTORS * SECTOR_SIZE, &error_fatal);
    memory_region_add_subregion(interface,
                                (uint64_t)ROM_START * SECTOR_SIZE, rom);


    /* limited by the size of the board ram, which we emulate as 128M for now */
    filesystem = g_malloc(sizeof(*filesystem));
    memory_region_init_ram(filesystem, NULL, "chihiro.interface.filesystem",
                           128 * 1024 * 1024, &error_fatal);
    memory_region_add_subregion(interface,
                                (uint64_t)FILESYSTEM_START * SECTOR_SIZE,
                                filesystem);


    AddressSpace *interface_space;
    interface_space = g_malloc(sizeof(*interface_space));
    address_space_init(interface_space, interface, "chihiro-interface");

    /* read files */
    int rc, fd = -1;

    if (!rom_file || (*rom_file == '\x00')) {
        rom_file = "fpr21042_m29w160et.bin";
    }
    char *rom_filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, rom_file);
    if (rom_filename) {
        int rom_size = get_image_size(rom_filename);
        assert(rom_size < memory_region_size(rom));

        fd = open(rom_filename, O_RDONLY | O_BINARY);
        assert(fd != -1);
        rc = read(fd, memory_region_get_ram_ptr(rom), rom_size);
        assert(rc == rom_size);
        close(fd);
    }

    if (filesystem_file && (*filesystem_file != '\x00')) {
        assert(access(filesystem_file, R_OK) == 0);

        int filesystem_size = get_image_size(filesystem_file);
        assert(filesystem_size < memory_region_size(filesystem));

        fd = open(filesystem_file, O_RDONLY | O_BINARY);
        assert(fd != -1);
        rc = read(fd, memory_region_get_ram_ptr(rom), filesystem_size);
        assert(rc == filesystem_size);
        close(fd);
    }

#if 0 // FIXME
    /* create the device */
    DriveInfo *dinfo;
    dinfo = g_malloc0(sizeof(*dinfo));
    dinfo->id = g_strdup("chihiro-interface");
    dinfo->bdrv = bdrv_new(dinfo->id);
    dinfo->type = IF_IDE;
    dinfo->bus = 0;
    dinfo->unit = 1;
    dinfo->refcount = 1;

    assert(!bdrv_memory_open(dinfo->bdrv, interface_space,
                             memory_region_size(interface)));

    drive_append(dinfo);
#else
    printf("Chihiro IDE not yet implemented (please fix it)\n");
    assert(0);
#endif
}

static void chihiro_init(MachineState *machine)
{
    const char *mediaboard_rom_file = object_property_get_str(
        qdev_get_machine(), "mediaboard-rom", NULL);
    const char *mediaboard_filesystem_file = object_property_get_str(
        qdev_get_machine(), "mediaboard-filesystem", NULL);
    chihiro_ide_interface_init(mediaboard_rom_file,
                               mediaboard_filesystem_file);

    ISABus *isa_bus;
    xbox_init_common(machine, NULL, &isa_bus);
    isa_create_simple(isa_bus, "chihiro-lpc");
}

static void chihiro_machine_options(MachineClass *m)
{
    PCMachineClass *pcmc = PC_MACHINE_CLASS(m);

    m->desc = "Sega Chihiro";
    m->max_cpus = 1;
    m->option_rom_has_mr = true;
    m->rom_file_has_mr = false;
    m->no_floppy = 1,
    m->no_cdrom = 1,
    m->no_sdcard = 1,
    m->default_cpu_type = X86_CPU_TYPE_NAME("486");

    pcmc->pci_enabled = true;
    pcmc->has_acpi_build = false;
    pcmc->smbios_defaults = false;
    pcmc->gigabyte_align = false;
    pcmc->smbios_legacy_mode = true;
    pcmc->has_reserved_memory = false;
    pcmc->default_nic_model = "nvnet";
}

static char *machine_get_mediaboard_rom(Object *obj, Error **errp)
{
    ChihiroMachineState *ms = CHIHIRO_MACHINE(obj);

    return g_strdup(ms->mediaboard_rom);
}

static void machine_set_mediaboard_rom(Object *obj, const char *value,
                                       Error **errp)
{
    ChihiroMachineState *ms = CHIHIRO_MACHINE(obj);

    g_free(ms->mediaboard_rom);
    ms->mediaboard_rom = g_strdup(value);
}

static char *machine_get_mediaboard_filesystem(Object *obj, Error **errp)
{
    ChihiroMachineState *ms = CHIHIRO_MACHINE(obj);

    return g_strdup(ms->mediaboard_filesystem);
}

static void machine_set_mediaboard_filesystem(Object *obj, const char *value,
                                              Error **errp)
{
    ChihiroMachineState *ms = CHIHIRO_MACHINE(obj);

    g_free(ms->mediaboard_filesystem);
    ms->mediaboard_filesystem = g_strdup(value);
}

static inline void chihiro_machine_initfn(Object *obj)
{
    object_property_add_str(obj, "mediaboard-rom",
                            machine_get_mediaboard_rom,
                            machine_set_mediaboard_rom);
    object_property_set_description(obj, "mediaboard-rom",
                                    "Chihiro mediaboard ROM");

    object_property_add_str(obj, "mediaboard-filesystem",
                            machine_get_mediaboard_filesystem,
                            machine_set_mediaboard_filesystem);
    object_property_set_description(obj, "mediaboard-filesystem",
                                    "Chihiro mediaboard filesystem");
}

static void chihiro_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    chihiro_machine_options(mc);
    mc->init = chihiro_init;
}

static const TypeInfo pc_machine_type_chihiro = {
    .name = TYPE_CHIHIRO_MACHINE,
    .parent = TYPE_PC_MACHINE,
    .abstract = false,
    .instance_size = sizeof(ChihiroMachineState),
    .instance_init = chihiro_machine_initfn,
    .class_size = sizeof(ChihiroMachineClass),
    .class_init = chihiro_machine_class_init,
    .interfaces = (InterfaceInfo[]) {
         // { TYPE_HOTPLUG_HANDLER },
         // { TYPE_NMI },
         { }
    },
};

static void pc_machine_init_chihiro(void)
{
    type_register_static(&pc_machine_type_chihiro);
}

type_init(pc_machine_init_chihiro)
