/*
 * QEMU Xbox System Emulator
 *
 * Copyright (c) 2012 espes
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

#include "qemu/osdep.h"
#include "qemu/option.h"
#include "qemu/datadir.h"
#include "hw/hw.h"
#include "hw/loader.h"
#include "hw/i386/pc.h"
#include "hw/i386/kvm/clock.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_ids.h"
#include "hw/usb.h"
#include "net/net.h"
#include "hw/boards.h"
#include "hw/ide/pci.h"
#include "system/system.h"
#include "system/kvm.h"
#include "kvm/kvm_i386.h"
#include "hw/dma/i8257.h"

#include "hw/sysbus.h"
#include "system/arch_init.h"
#include "system/memory.h"
#include "system/address-spaces.h"
#include "cpu.h"

#include "qapi/error.h"
#include "qemu/error-report.h"

#include "hw/timer/i8254.h"
#include "hw/audio/pcspk.h"
#include "hw/rtc/mc146818rtc.h"

#include "hw/xbox/xbox_pci.h"
#include "hw/i2c/i2c.h"
#include "hw/i2c/smbus_eeprom.h"
#include "hw/xbox/nv2a/nv2a.h"
#include "hw/xbox/mcpx/apu/apu.h"

#include "hw/xbox/xbox.h"
#include "smbus.h"

#define MAX_IDE_BUS 2

/* FIXME: Clean this up and propagate errors to UI */
static void xbox_flash_init(MachineState *ms, MemoryRegion *rom_memory)
{
    const uint32_t rom_start = 0xFF000000;
    const char *bios_name;

    /* Locate BIOS ROM image */
    bios_name = ms->firmware ?: "bios.bin";

    int failed_to_load_bios = 1;
    char *filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, bios_name);
    uint32_t bios_size = 256 * 1024;

    if (filename != NULL) {
        int bios_file_size = get_image_size(filename, NULL);
        if ((bios_file_size > 0) && ((bios_file_size % 65536) == 0)) {
            failed_to_load_bios = 0;
            bios_size = bios_file_size;
        }
    }

    char *bios_data = g_malloc(bios_size);
    assert(bios_data != NULL);

    if (!failed_to_load_bios && (filename != NULL)) {
        /* Read BIOS ROM into memory */
        failed_to_load_bios = 1;
        int fd = qemu_open(filename, O_RDONLY | O_BINARY, NULL);
        if (fd >= 0) {
            int rc = read(fd, bios_data, bios_size);
            if (rc == bios_size) {
                failed_to_load_bios = 0;
            }
            close(fd);
        }
    }

    if (failed_to_load_bios) {
        fprintf(stderr, "Failed to load BIOS '%s'\n", filename ? filename : "(null)");
        memset(bios_data, 0xff, bios_size);
    }
    if (filename != NULL) {
        g_free(filename);
    }

    /* Create BIOS region */
    MemoryRegion *bios;
    bios = g_malloc(sizeof(*bios));
    assert(bios != NULL);
    memory_region_init_rom(bios, NULL, "xbox.bios", bios_size, &error_fatal);
    rom_add_blob_fixed("xbox.bios", bios_data, bios_size, rom_start);

    /* Mirror ROM from 0xff000000 - 0xffffffff */
    uint32_t map_loc;
    for (map_loc = rom_start; map_loc >= rom_start; map_loc += bios_size) {
        MemoryRegion *map_bios = g_malloc(sizeof(*map_bios));
        memory_region_init_alias(map_bios, NULL, "pci-bios", bios, 0,
                                 bios_size);
        memory_region_add_subregion(rom_memory, map_loc, map_bios);
        memory_region_set_readonly(map_bios, true);
    }

    /* Create MCPX Boot ROM memory region
     *
     * For performance reasons, the overlay region should be page-aligned.
     * To do this, we simply make the memory region size equal to the size
     * of the BIOS image, and then overlay the boot ROM contents on top.
     *
     * Additionally, retail 1.1+ kernels have a quirk in very early boot stage
     * that depends on physical CPU WB caching behavior to briefly store a
     * computed value to a location in ROM and read it back in the next
     * instruction. Because we cannot emulate this cache behavior accurately,
     * work around this quirk by making this MCPX ROM region writable. When the
     * ROM is disabled during boot, any apparent writes to the region will be
     * discarded.
     *
     * Offending code which writes to ROM:
     *   sub ds:0FFFFD52Ch, eax
     *   mov eax, ds:0FFFFD52Ch
     */
    const char *bootrom_file =
        object_property_get_str(qdev_get_machine(), "bootrom", NULL);

    if ((bootrom_file != NULL) && *bootrom_file) {
        filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, bootrom_file);
        assert(filename);

        int bootrom_size = get_image_size(filename, NULL);
        if (bootrom_size != 512) {
            fprintf(stderr, "MCPX bootrom should be 512 bytes, got %d\n",
                    bootrom_size);
            exit(1);
            return;
        }

        /* Read in MCPX ROM over last 512 bytes of BIOS data */
        int fd = qemu_open(filename, O_RDONLY | O_BINARY, NULL);
        assert(fd >= 0);
        int rc = read(fd, bios_data + bios_size - bootrom_size, bootrom_size);
        assert(rc == bootrom_size);
        close(fd);
        g_free(filename);
    }

    // Leave last BIOS image overlay writeable to satisfy cache dependency
    MemoryRegion *mcpx = g_malloc(sizeof(MemoryRegion));
    memory_region_init_ram(mcpx, NULL, "xbox.mcpx", bios_size, &error_fatal);
    rom_add_blob_fixed("xbox.mcpx", bios_data, bios_size, -bios_size);
    memory_region_add_subregion_overlap(rom_memory, -bios_size, mcpx, 1);

    g_free(bios_data); /* duplicated by `rom_add_blob_fixed` */
}

static void xbox_memory_init(PCMachineState *pcms,
                             MemoryRegion *system_memory,
                             MemoryRegion *rom_memory,
                             MemoryRegion **ram_memory)
{
    // int linux_boot, i;
    MemoryRegion *ram;//, *option_rom_mr;
    // FWCfgState *fw_cfg;
    MachineState *machine = MACHINE(pcms);
    // PCMachineClass *pcmc = PC_MACHINE_GET_CLASS(pcms);

    // linux_boot = (machine->kernel_filename != NULL);

    /* Allocate RAM.  We allocate it as a single memory region and use
     * aliases to address portions of it, mostly for backwards compatibility
     * with older qemus that used qemu_ram_alloc().
     */
    ram = g_malloc(sizeof(*ram));
    memory_region_init_ram(ram, NULL, "xbox.ram",
                           machine->ram_size, &error_fatal);

    *ram_memory = ram;
    memory_region_add_subregion(system_memory, 0, ram);

    xbox_flash_init(machine, rom_memory);
    pc_system_flash_cleanup_unused(pcms);
}

/* PC hardware initialisation */
static void xbox_init(MachineState *machine)
{
    xbox_init_common(machine, NULL, NULL);
}

void xbox_init_common(MachineState *machine,
                      PCIBus **pci_bus_out,
                      ISABus **isa_bus_out)
{
    PCMachineState *pcms = PC_MACHINE(machine);
    PCMachineClass *pcmc = PC_MACHINE_GET_CLASS(pcms);
    X86MachineState *x86ms = X86_MACHINE(machine);
    MemoryRegion *system_memory = get_system_memory();
    // MemoryRegion *system_io = get_system_io();

    PCIBus *pci_bus;
    ISABus *isa_bus;

    // qemu_irq *i8259;
    // qemu_irq smi_irq; // XBOX_TODO: SMM support?

    GSIState *gsi_state;

    // DriveInfo *hd[MAX_IDE_BUS * MAX_IDE_DEVS];
    // BusState *idebus[MAX_IDE_BUS];
    MC146818RtcState *rtc_state;
    ISADevice *pit = NULL;
    int pit_isa_irq = 0;
    qemu_irq pit_alt_irq = NULL;
    // ISADevice *pit;

    MemoryRegion *ram_memory;
    MemoryRegion *pci_memory;
    MemoryRegion *rom_memory;

    I2CBus *smbus;
    PCIBus *agp_bus;

    x86_cpus_init(x86ms, pcmc->default_cpu_version);

    if (kvm_enabled()) {
        kvmclock_create(pcmc->kvmclock_create_always);
    }

    pci_memory = g_new(MemoryRegion, 1);
    memory_region_init(pci_memory, NULL, "pci", UINT64_MAX);
    rom_memory = pci_memory;

    // pc_guest_info_init(pcms);

    /* allocate ram and load rom/bios */
    xbox_memory_init(pcms, system_memory, rom_memory, &ram_memory);

    gsi_state = pc_gsi_create(&x86ms->gsi, pcmc->pci_enabled);

    xbox_pci_init(x86ms->gsi,
                  get_system_memory(), get_system_io(),
                  pci_memory, ram_memory, rom_memory,
                  &pci_bus,
                  &isa_bus,
                  &smbus,
                  &agp_bus);

    pcms->pcibus = pci_bus;

    isa_bus_register_input_irqs(isa_bus, x86ms->gsi);

    pc_i8259_create(isa_bus, gsi_state->i8259_irq);

    if (tcg_enabled()) {
        x86_register_ferr_irq(x86ms->gsi[13]);
    }

    /* init basic PC hardware */
    rtc_state = mc146818_rtc_init(isa_bus, 2000, NULL);
    x86ms->rtc = ISA_DEVICE(rtc_state);

    if (kvm_pit_in_kernel()) {
        pit = kvm_pit_init(isa_bus, 0x40);
    } else {
        pit = i8254_pit_init(isa_bus, 0x40, pit_isa_irq, pit_alt_irq);
    }

    i8257_dma_init(OBJECT(machine), isa_bus, 0);

    object_property_set_link(OBJECT(pcms->pcspk), "pit",
                             OBJECT(pit), &error_fatal);
    isa_realize_and_unref(pcms->pcspk, isa_bus, &error_fatal);

    PCIDevice *dev = pci_create_simple(pci_bus, PCI_DEVFN(9, 0), "piix3-ide");
    pci_ide_create_devs(dev);
    // idebus[0] = qdev_get_child_bus(&dev->qdev, "ide.0");
    // idebus[1] = qdev_get_child_bus(&dev->qdev, "ide.1");

    /* smbus devices */
    smbus_xbox_smc_init(smbus, 0x10);

    const char *video_encoder =
        object_property_get_str(qdev_get_machine(), "video-encoder", NULL);

    if (strcmp(video_encoder, "xcalibur") != 0) {
        if (!strcmp(video_encoder, "conexant")) {
            smbus_cx25871_init(smbus, 0x45);
        } else if (!strcmp(video_encoder, "focus")) {
            smbus_fs454_init(smbus, 0x6A);
        }
        smbus_adm1032_init(smbus, 0x4C);
    } else {
        smbus_xcalibur_init(smbus, 0x70);
    }

    /* USB */
    PCIDevice *usb1 = pci_new(PCI_DEVFN(3, 0), "pci-ohci");
    qdev_prop_set_uint32(&usb1->qdev, "num-ports", 4);
    pci_realize_and_unref(usb1, pci_bus, &error_fatal);

    PCIDevice *usb0 = pci_new(PCI_DEVFN(2, 0), "pci-ohci");
    qdev_prop_set_uint32(&usb0->qdev, "num-ports", 4);
    pci_realize_and_unref(usb0, pci_bus, &error_fatal);

    /* Ethernet! */
    PCIDevice *nvnet = pci_new(PCI_DEVFN(4, 0), "nvnet");
    qemu_configure_nic_device(DEVICE(nvnet), true, "nvnet");
    pci_realize_and_unref(nvnet, pci_bus, &error_fatal);

    /* APU! */
    mcpx_apu_init(pci_bus, PCI_DEVFN(5, 0), ram_memory);

    /* ACI! */
    pci_create_simple(pci_bus, PCI_DEVFN(6, 0), "mcpx-aci");

    /* GPU! */
    nv2a_init(agp_bus, PCI_DEVFN(0, 0), ram_memory);

    /* FIXME: Stub the memory controller */
    pci_create_simple(pci_bus, PCI_DEVFN(0, 3), "pci-testdev");

    if (pci_bus_out) {
        *pci_bus_out = pci_bus;
    }
    if (isa_bus_out) {
        *isa_bus_out = isa_bus;
    }
}

static char *machine_get_bootrom(Object *obj, Error **errp)
{
    XboxMachineState *ms = XBOX_MACHINE(obj);

    return g_strdup(ms->bootrom);
}

static void machine_set_bootrom(Object *obj, const char *value, Error **errp)
{
    XboxMachineState *ms = XBOX_MACHINE(obj);

    g_free(ms->bootrom);
    ms->bootrom = g_strdup(value);
}

static char *machine_get_avpack(Object *obj, Error **errp)
{
    XboxMachineState *ms = XBOX_MACHINE(obj);

    return g_strdup(ms->avpack);
}

static void machine_set_avpack(Object *obj, const char *value, Error **errp)
{
    XboxMachineState *ms = XBOX_MACHINE(obj);

    if (!xbox_smc_avpack_to_reg(value, NULL)) {
        error_setg(errp, "-machine avpack=%s: unsupported option", value);
        xbox_smc_append_avpack_hint(errp);
        return;
    }

    g_free(ms->avpack);
    ms->avpack = g_strdup(value);
}

static void machine_set_short_animation(Object *obj, bool value, Error **errp)
{
    XboxMachineState *ms = XBOX_MACHINE(obj);

    ms->short_animation = value;
}

static bool machine_get_short_animation(Object *obj, Error **errp)
{
    XboxMachineState *ms = XBOX_MACHINE(obj);
    return ms->short_animation;
}

static char *machine_get_smc_version(Object *obj, Error **errp)
{
    XboxMachineState *ms = XBOX_MACHINE(obj);

    return g_strdup(ms->smc_version);
}

static void machine_set_smc_version(Object *obj, const char *value,
                                    Error **errp)
{
    XboxMachineState *ms = XBOX_MACHINE(obj);

    if (strlen(value) != 3) {
        error_setg(errp, "-machine smc-version=%s: unsupported option", value);
        xbox_smc_append_smc_version_hint(errp);
        return;
    }

    g_free(ms->smc_version);
    ms->smc_version = g_strdup(value);
}

static char *machine_get_video_encoder(Object *obj, Error **errp)
{
    XboxMachineState *ms = XBOX_MACHINE(obj);

    return g_strdup(ms->video_encoder);
}

static void machine_set_video_encoder(Object *obj, const char *value,
                                      Error **errp)
{
    XboxMachineState *ms = XBOX_MACHINE(obj);

    if (strcmp(value, "conexant") != 0 && strcmp(value, "focus") != 0 &&
        strcmp(value, "xcalibur") != 0) {
        error_setg(errp, "-machine video_encoder=%s: unsupported option",
                   value);
        error_append_hint(
            errp, "Valid options are: conexant (default), focus, xcalibur\n");
        return;
    }

    g_free(ms->video_encoder);
    ms->video_encoder = g_strdup(value);
}

static void xbox_machine_options(MachineClass *m)
{
    ObjectClass *oc = OBJECT_CLASS(m);

    PCMachineClass *pcmc = PC_MACHINE_CLASS(m);
    m->desc              = "Microsoft Xbox";
    m->max_cpus          = 1;
    m->option_rom_has_mr = true;
    m->rom_file_has_mr   = false;
    m->no_floppy         = 1,
    m->no_cdrom          = 1,
    m->default_cpu_type  = X86_CPU_TYPE_NAME("pentium3");
    m->is_default        = true;
    m->default_nic       = "nvnet";

    pcmc->pci_enabled         = true;
    pcmc->has_acpi_build      = false;
    pcmc->smbios_defaults     = false;
    pcmc->gigabyte_align      = false;
    pcmc->smbios_legacy_mode  = true;
    pcmc->has_reserved_memory = false;

    object_class_property_add_str(oc, "bootrom", machine_get_bootrom,
                                  machine_set_bootrom);
    object_class_property_set_description(oc, "bootrom", "Xbox bootrom file");

    object_class_property_add_str(oc, "avpack", machine_get_avpack,
                                  machine_set_avpack);
    object_class_property_set_description(
        oc, "avpack",
        "Xbox video connector: composite, scart, svideo, vga, rfu, hdtv "
        "(default), none");

    object_class_property_add_bool(oc, "short-animation",
                                   machine_get_short_animation,
                                   machine_set_short_animation);
    object_class_property_set_description(oc, "short-animation",
                                          "Skip Xbox boot animation");

    object_class_property_add_str(oc, "smc-version", machine_get_smc_version,
                                  machine_set_smc_version);
    object_class_property_set_description(
        oc, "smc-version", "Set the SMC version number, default is P01");

    object_class_property_add_str(oc, "video-encoder",
                                  machine_get_video_encoder,
                                  machine_set_video_encoder);
    object_class_property_set_description(
        oc, "video-encoder",
        "Set the encoder presented to the OS: conexant (default), focus, "
        "xcalibur");
}

static inline void xbox_machine_initfn(Object *obj)
{
    object_property_set_str(obj, "avpack", "hdtv", &error_fatal);
    object_property_set_bool(obj, "short-animation", false, &error_fatal);
    object_property_set_str(obj, "smc-version", "P01", &error_fatal);
    object_property_set_str(obj, "video-encoder", "conexant", &error_fatal);
}

static void xbox_machine_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    xbox_machine_options(mc);
    mc->init = xbox_init;
}

static const TypeInfo pc_machine_type_xbox = {
    .name = TYPE_XBOX_MACHINE,
    .parent = TYPE_PC_MACHINE,
    .abstract = false,
    .instance_size = sizeof(XboxMachineState),
    .instance_init = xbox_machine_initfn,
    .class_size = sizeof(XboxMachineClass),
    .class_init = xbox_machine_class_init,
    .interfaces = (InterfaceInfo[]) {
         // { TYPE_HOTPLUG_HANDLER },
         // { TYPE_NMI },
         { }
    },
};

static void pc_machine_init_xbox(void)
{
    type_register_static(&pc_machine_type_xbox);
}

type_init(pc_machine_init_xbox)
