/*
 * QEMU Xbox System Emulator
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2018 Matt Borgerson
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
#include "hw/hw.h"
#include "hw/loader.h"
#include "hw/i386/pc.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_ids.h"
#include "hw/usb.h"
#include "net/net.h"
#include "hw/boards.h"
#include "hw/ide/pci.h"
#include "sysemu/sysemu.h"
#include "sysemu/kvm.h"
#include "kvm_i386.h"
#include "hw/kvm/clock.h"
#include "hw/dma/i8257.h"

#include "hw/sysbus.h"
#include "sysemu/arch_init.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
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
#include "hw/xbox/mcpx_apu.h"
 
#include "hw/xbox/xbox.h"
#include "smbus.h"

#define MAX_IDE_BUS 2

// XBOX_TODO: Should be passed in through configuration
/* bunnie's eeprom */
const uint8_t default_eeprom[] = {
    0xe3, 0x1c, 0x5c, 0x23, 0x6a, 0x58, 0x68, 0x37,
    0xb7, 0x12, 0x26, 0x6c, 0x99, 0x11, 0x30, 0xd1,
    0xe2, 0x3e, 0x4d, 0x56, 0xf7, 0x73, 0x2b, 0x73,
    0x85, 0xfe, 0x7f, 0x0a, 0x08, 0xef, 0x15, 0x3c,
    0x77, 0xee, 0x6d, 0x4e, 0x93, 0x2f, 0x28, 0xee,
    0xf8, 0x61, 0xf7, 0x94, 0x17, 0x1f, 0xfc, 0x11,
    0x0b, 0x84, 0x44, 0xed, 0x31, 0x30, 0x35, 0x35,
    0x38, 0x31, 0x31, 0x31, 0x34, 0x30, 0x30, 0x33,
    0x00, 0x50, 0xf2, 0x4f, 0x65, 0x52, 0x00, 0x00,
    0x0a, 0x1e, 0x35, 0x33, 0x71, 0x85, 0x31, 0x4d,
    0x59, 0x12, 0x38, 0x48, 0x1c, 0x91, 0x53, 0x60,
    0x00, 0x01, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x75, 0x61, 0x57, 0xfb, 0x2c, 0x01, 0x00, 0x00,
    0x45, 0x53, 0x54, 0x00, 0x45, 0x44, 0x54, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0a, 0x05, 0x00, 0x02, 0x04, 0x01, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xc4, 0xff, 0xff, 0xff,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* FIXME: Clean this up and propagate errors to UI */
static void xbox_flash_init(MemoryRegion *rom_memory)
{
    /* Locate BIOS ROM image */
    if (bios_name == NULL) {
        bios_name = "bios.bin";
    }

    int failed_to_load_bios = 1;
    char *filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, bios_name);
    int bios_size = 256*1024;

    if (filename != NULL) {
        int bios_file_size = get_image_size(filename);
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
        int fd = open(filename, O_RDONLY | O_BINARY);
        if (fd >= 0) {
            int rc = read(fd, bios_data, bios_size);
            if (rc == bios_size) {
                failed_to_load_bios = 0;
            }
            close(fd);
        }

    }

    if (failed_to_load_bios) {
        fprintf(stderr, "Failed to load BIOS '%s'\n", filename);
        memset(bios_data, 0xff, bios_size);
    }
    if (filename != NULL) {
        g_free(filename);
    }

    /* XBOX_FIXME: What follows is a big hack to overlay the MCPX ROM on the
     * top 512 bytes of the ROM region. This differs from original XQEMU
     * sources which copied it in at lpc init; new QEMU seems to be different
     * now in that the BIOS images supplied to rom_add_file_fixed will be
     * loaded *after* lpc init is called, so the MCPX ROM would get
     * overwritten. Instead, let's just map it in right here while we map in
     * BIOS.
     *
     * Anyway it behaves the same as before--that is, wrongly. Really, we
     * should let the CPU execute from MMIO emulating the TSOP access with
     * bootrom overlay being controlled by the magic bit..but this is "good
     * enough" for now ;).
     */

    /* Locate and overlay MCPX ROM image into new copy of BIOS if provided */
    const char *bootrom_file = object_property_get_str(qdev_get_machine(),
                                                       "bootrom", NULL);

    if ((bootrom_file != NULL) && *bootrom_file) {
        filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, bootrom_file);
        assert(filename);

        int bootrom_size = get_image_size(filename);
        if (bootrom_size != 512) {
            fprintf(stderr, "MCPX bootrom should be 512 bytes, got %d\n",
                    bootrom_size);
            exit(1);
            return;
        }

        /* Read in MCPX ROM over last 512 bytes of BIOS data */
        int fd = open(filename, O_RDONLY | O_BINARY);
        assert(fd >= 0);
        int rc = read(fd, &bios_data[bios_size - bootrom_size], bootrom_size);
        assert(rc == bootrom_size);
        close(fd);
        g_free(filename);
    }

    /* Create BIOS region */
    MemoryRegion *bios;
    bios = g_malloc(sizeof(*bios));
    assert(bios != NULL);
    memory_region_init_ram(bios, NULL, "xbox.bios", bios_size, &error_fatal);
    memory_region_set_readonly(bios, true);
    rom_add_blob_fixed("xbox.bios", bios_data, bios_size,
                       (uint32_t)(-2 * bios_size));

    /* Assuming bios_data will be needed for duration of execution
     * so no free(bios) here.
     */

    /* Mirror ROM from 0xff000000 - 0xffffffff */
    uint32_t map_loc;
    for (map_loc = (uint32_t)(-bios_size);
         map_loc >= 0xff000000;
         map_loc -= bios_size) {
        MemoryRegion *map_bios = g_malloc(sizeof(*map_bios));
        memory_region_init_alias(map_bios, NULL, "pci-bios", bios, 0, bios_size);
        memory_region_add_subregion(rom_memory, map_loc, map_bios);
        memory_region_set_readonly(map_bios, true);
    }
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

    xbox_flash_init(rom_memory);
    pc_system_flash_cleanup_unused(pcms);
}

uint8_t *load_eeprom(void)
{
    char *filename;
    int fd;
    int rc;
    int eeprom_file_size;
    const int eeprom_size = 256;

    uint8_t *eeprom_data = g_malloc(eeprom_size);

    const char *eeprom_file = object_property_get_str(qdev_get_machine(),
                                                      "eeprom", NULL);
    if ((eeprom_file != NULL) && *eeprom_file) {
        filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, eeprom_file);
        assert(filename);

        eeprom_file_size = get_image_size(filename);
        if (eeprom_size != eeprom_file_size) {
            fprintf(stderr,
                    "qemu: EEPROM file size != %d bytes. (Is %d bytes)\n",
                    eeprom_size, eeprom_file_size);
            g_free(filename);
            exit(1);
            return NULL;
        }

        fd = open(filename, O_RDONLY | O_BINARY);
        if (fd < 0) {
            fprintf(stderr, "qemu: EEPROM file '%s' could not be opened.\n", filename);
            g_free(filename);
            exit(1);
            return NULL;
        }

        rc = read(fd, eeprom_data, eeprom_size);
        if (rc != eeprom_size) {
            fprintf(stderr, "qemu: Could not read the full EEPROM file.\n");
            close(fd);
            g_free(filename);
            exit(1);
            return NULL;
        }

        close(fd);
        g_free(filename);
    } else {
        memcpy(eeprom_data, default_eeprom, eeprom_size);
    }
    return eeprom_data;
}

/* PC hardware initialisation */
static void xbox_init(MachineState *machine)
{
    uint8_t *eeprom_data = load_eeprom();
    xbox_init_common(machine, eeprom_data, NULL, NULL);
}

void xbox_init_common(MachineState *machine,
                      const uint8_t *eeprom,
                      PCIBus **pci_bus_out,
                      ISABus **isa_bus_out)
{
    PCMachineState *pcms = PC_MACHINE(machine);
    PCMachineClass *pcmc = PC_MACHINE_GET_CLASS(pcms);
    X86MachineState *x86ms = X86_MACHINE(machine);
    MemoryRegion *system_memory = get_system_memory();
    // MemoryRegion *system_io = get_system_io();

    int i;

    PCIBus *pci_bus;
    ISABus *isa_bus;

    // qemu_irq *i8259;
    // qemu_irq smi_irq; // XBOX_TODO: SMM support?

    GSIState *gsi_state;

    // DriveInfo *hd[MAX_IDE_BUS * MAX_IDE_DEVS];
    // BusState *idebus[MAX_IDE_BUS];
    ISADevice *rtc_state;
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

    if (kvm_enabled() && pcmc->kvmclock_enabled) {
        kvmclock_create();
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
                  pci_memory, ram_memory,
                  &pci_bus,
                  &isa_bus,
                  &smbus,
                  &agp_bus);

    pcms->bus = pci_bus;

    isa_bus_irqs(isa_bus, x86ms->gsi);

    pc_i8259_create(isa_bus, gsi_state->i8259_irq);

    if (tcg_enabled()) {
        x86_register_ferr_irq(x86ms->gsi[13]);
    }

    /* init basic PC hardware */
    pcms->pit_enabled = 1; // XBOX_FIXME: What's the right way to do this?
    rtc_state = mc146818_rtc_init(isa_bus, 2000, NULL);

    if (kvm_pit_in_kernel()) {
        pit = kvm_pit_init(isa_bus, 0x40);
    } else {
        pit = i8254_pit_init(isa_bus, 0x40, pit_isa_irq, pit_alt_irq);
    }

    i8257_dma_init(isa_bus, 0);

    pcspk_init(pcms->pcspk, isa_bus, pit);

    PCIDevice *dev = pci_create_simple(pci_bus, PCI_DEVFN(9, 0), "piix3-ide");
    pci_ide_create_devs(dev);
    // idebus[0] = qdev_get_child_bus(&dev->qdev, "ide.0");
    // idebus[1] = qdev_get_child_bus(&dev->qdev, "ide.1");

    // xbox bios wants this bit pattern set to mark the data as valid
    uint8_t bits = 0x55;
    for (i = 0x10; i < 0x70; i++) {
        rtc_set_memory(rtc_state, i, bits);
        bits = ~bits;
    }
    bits = 0x55;
    for (i = 0x80; i < 0x100; i++) {
        rtc_set_memory(rtc_state, i, bits);
        bits = ~bits;
    }

    /* smbus devices */
    uint8_t *eeprom_buf = g_malloc0(256);
    memcpy(eeprom_buf, eeprom, 256);
    smbus_eeprom_init_one(smbus, 0x54, eeprom_buf);

    smbus_xbox_smc_init(smbus, 0x10);
    smbus_cx25871_init(smbus, 0x45);
    smbus_adm1032_init(smbus, 0x4c);

    /* USB */
    PCIDevice *usb1 = pci_new(PCI_DEVFN(3, 0), "pci-ohci");
    qdev_prop_set_uint32(&usb1->qdev, "num-ports", 4);
    pci_realize_and_unref(usb1, pci_bus, &error_fatal);

    PCIDevice *usb0 = pci_new(PCI_DEVFN(2, 0), "pci-ohci");
    qdev_prop_set_uint32(&usb0->qdev, "num-ports", 4);
    pci_realize_and_unref(usb0, pci_bus, &error_fatal);

    /* Ethernet! */
    PCIDevice *nvnet = pci_new(PCI_DEVFN(4, 0), "nvnet");

    for (i = 0; i < nb_nics; i++) {
        NICInfo *nd = &nd_table[i];
        qemu_check_nic_model(nd, "nvnet");
        qdev_set_nic_properties(&nvnet->qdev, nd);
        pci_realize_and_unref(nvnet, pci_bus, &error_fatal);
    }

    /* APU! */
    mcpx_apu_init(pci_bus, PCI_DEVFN(5, 0), ram_memory);

    /* ACI! */
    pci_create_simple(pci_bus, PCI_DEVFN(6, 0), "mcpx-aci");

    /* GPU! */
    nv2a_init(agp_bus, PCI_DEVFN(0, 0), ram_memory);

    if (pci_bus_out) {
        *pci_bus_out = pci_bus;
    }
    if (isa_bus_out) {
        *isa_bus_out = isa_bus;
    }
}

static void xbox_machine_options(MachineClass *m)
{
    PCMachineClass *pcmc = PC_MACHINE_CLASS(m);
    m->desc              = "Microsoft Xbox";
    m->max_cpus          = 1;
    m->option_rom_has_mr = true;
    m->rom_file_has_mr   = false;
    m->no_floppy         = 1,
    m->no_cdrom          = 1,
    m->no_sdcard         = 1,
    m->default_cpu_type  = X86_CPU_TYPE_NAME("pentium3");

    pcmc->pci_enabled         = true;
    pcmc->has_acpi_build      = false;
    pcmc->smbios_defaults     = false;
    pcmc->gigabyte_align      = false;
    pcmc->smbios_legacy_mode  = true;
    pcmc->has_reserved_memory = false;
    pcmc->default_nic_model   = "nvnet";
}

static char *machine_get_bootrom(Object *obj, Error **errp)
{
    XboxMachineState *ms = XBOX_MACHINE(obj);

    return g_strdup(ms->bootrom);
}

static void machine_set_bootrom(Object *obj, const char *value,
                                        Error **errp)
{
    XboxMachineState *ms = XBOX_MACHINE(obj);

    g_free(ms->bootrom);
    ms->bootrom = g_strdup(value);
}

static char *machine_get_eeprom(Object *obj, Error **errp)
{
    XboxMachineState *ms = XBOX_MACHINE(obj);

    return g_strdup(ms->eeprom);
}

static void machine_set_eeprom(Object *obj, const char *value,
                                        Error **errp)
{
    XboxMachineState *ms = XBOX_MACHINE(obj);

    g_free(ms->eeprom);
    ms->eeprom = g_strdup(value);
}

static char *machine_get_avpack(Object *obj, Error **errp)
{
    XboxMachineState *ms = XBOX_MACHINE(obj);

    return g_strdup(ms->avpack);
}

static void machine_set_avpack(Object *obj, const char *value,
                               Error **errp)
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

static void machine_set_short_animation(Object *obj, bool value,
                                        Error **errp)
{
    XboxMachineState *ms = XBOX_MACHINE(obj);

    ms->short_animation = value;
}

static bool machine_get_short_animation(Object *obj, Error **errp)
{
    XboxMachineState *ms = XBOX_MACHINE(obj);
    return ms->short_animation;
}

static inline void xbox_machine_initfn(Object *obj)
{
    object_property_add_str(obj, "bootrom", machine_get_bootrom,
                            machine_set_bootrom);
    object_property_set_description(obj, "bootrom",
                                    "Xbox bootrom file");

    object_property_add_str(obj, "eeprom", machine_get_eeprom,
                            machine_set_eeprom);
    object_property_set_description(obj, "eeprom",
                                    "Xbox EEPROM file");

    object_property_add_str(obj, "avpack", machine_get_avpack,
                            machine_set_avpack);
    object_property_set_description(obj, "avpack",
                                    "Xbox video connector: composite (default), scart, svideo, vga, rfu, hdtv, none");
    object_property_set_str(obj, "avpack", "composite", &error_fatal);

    object_property_add_bool(obj, "short-animation",
                             machine_get_short_animation,
                             machine_set_short_animation);
    object_property_set_description(obj, "short-animation",
                                    "Skip Xbox boot animation");
    object_property_set_bool(obj, "short-animation", false, &error_fatal);

}

static void xbox_machine_class_init(ObjectClass *oc, void *data)
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
    type_register(&pc_machine_type_xbox);
}

type_init(pc_machine_init_xbox)
