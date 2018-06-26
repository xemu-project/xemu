/*
 * QEMU Xbox System Emulator
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2018 Matt Borgerson
 *
 * Based on pc.c and pc_piix.c
 * Copyright (c) 2003-2004 Fabrice Bellard
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

#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/loader.h"
#include "hw/i386/pc.h"
#include "hw/i386/apic.h"
#include "hw/smbios/smbios.h"
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
#include "hw/i2c/smbus.h"
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

#include "hw/timer/mc146818rtc.h"
#include "xbox_pci.h"
#include "smbus.h"

#include "qemu/option.h"
#include "xbox.h"

#define MAX_IDE_BUS 2

static char *machine_get_bootrom(Object *obj, Error **errp);
static void machine_set_bootrom(Object *obj, const char *value,
                                        Error **errp);
static void machine_set_short_animation(Object *obj, bool value,
                                        Error **errp);
static bool machine_get_short_animation(Object *obj, Error **errp);

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

static void xbox_flash_init(MemoryRegion *rom_memory)
{
    char *filename;
    int bios_size;
    int bootrom_size;

    MemoryRegion *bios;
    MemoryRegion *map_bios;

    uint32_t map_loc;
    int rc, fd = -1;

    char *bios_data;

    /* Locate BIOS ROM image */
    if (bios_name == NULL) {
        bios_name = "bios.bin";
    }

    filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, bios_name);
    if (filename) {
        bios_size = get_image_size(filename);
    } else {
        bios_size = -1;
    }
    if (bios_size <= 0 ||
        (bios_size % 65536) != 0) {
        goto bios_error;
    }

    /* Read BIOS ROM into memory */
    bios_data = g_malloc(bios_size);
    assert(bios_data != NULL);
    fd = open(filename, O_RDONLY | O_BINARY);
    assert(fd >= 0);
    rc = read(fd, bios_data, bios_size);
    assert(rc == bios_size);
    close(fd);
    g_free(filename);

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

        bootrom_size = get_image_size(filename);
        if (bootrom_size != 512) {
            fprintf(stderr, "MCPX bootrom should be 512 bytes, got %d\n",
                    bootrom_size);
            exit(1);
            return;
        }

        /* Read in MCPX ROM over last 512 bytes of BIOS data */
        fd = open(filename, O_RDONLY | O_BINARY);
        assert(fd >= 0);
        rc = read(fd, &bios_data[bios_size - bootrom_size], bootrom_size);
        assert(rc == bootrom_size);
        close(fd);
        g_free(filename);
    }

    /* Create BIOS region */
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
    for (map_loc = (uint32_t)(-bios_size);
         map_loc >= 0xff000000;
         map_loc -= bios_size) {
        map_bios = g_malloc(sizeof(*map_bios));
        memory_region_init_alias(map_bios, NULL, "pci-bios", bios, 0, bios_size);
        memory_region_add_subregion(rom_memory, map_loc, map_bios);
        memory_region_set_readonly(map_bios, true);
    }

    return;

bios_error:
    fprintf(stderr, "qemu: could not load xbox BIOS '%s'\n", bios_name);
    exit(1);
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
}


/* FIXME: Move to header file */
void nv2a_init(PCIBus *bus, int devfn, MemoryRegion *ram);

#include "hw/timer/i8254.h"
#include "hw/audio/pcspk.h"

/* PC hardware initialisation */
static void xbox_init(MachineState *machine)
{
    xbox_init_common(machine, default_eeprom, NULL, NULL);
}

void xbox_init_common(MachineState *machine,
                      const uint8_t *eeprom,
                      PCIBus **pci_bus_out,
                      ISABus **isa_bus_out)
{
    PCMachineState *pcms = PC_MACHINE(machine);
    PCMachineClass *pcmc = PC_MACHINE_GET_CLASS(pcms);

    MemoryRegion *system_memory = get_system_memory();
    // MemoryRegion *system_io = get_system_io();

    int i;

    PCIBus *pci_bus;
    ISABus *isa_bus;

    qemu_irq *i8259;
    // qemu_irq smi_irq; // XBOX_TODO: SMM support?

    GSIState *gsi_state;

    DriveInfo *hd[MAX_IDE_BUS * MAX_IDE_DEVS];
    BusState *idebus[MAX_IDE_BUS];
    ISADevice *rtc_state;
    // ISADevice *pit;

    MemoryRegion *ram_memory;
    MemoryRegion *pci_memory;
    MemoryRegion *rom_memory;

    I2CBus *smbus;
    PCIBus *agp_bus;

    pc_cpus_init(pcms);

    if (kvm_enabled() && pcmc->kvmclock_enabled) {
        kvmclock_create();
    }

    pci_memory = g_new(MemoryRegion, 1);
    memory_region_init(pci_memory, NULL, "pci", UINT64_MAX);
    rom_memory = pci_memory;

    pc_guest_info_init(pcms);

    /* allocate ram and load rom/bios */
    xbox_memory_init(pcms, system_memory, rom_memory, &ram_memory);

    gsi_state = g_malloc0(sizeof(*gsi_state));
    pcms->gsi = qemu_allocate_irqs(gsi_handler, gsi_state, GSI_NUM_PINS);

    xbox_pci_init(pcms->gsi,
                  get_system_memory(), get_system_io(),
                  pci_memory, ram_memory,
                  &pci_bus,
                  &isa_bus,
                  &smbus,
                  &agp_bus);

    pcms->bus = pci_bus;

    isa_bus_irqs(isa_bus, pcms->gsi);

    // if (kvm_pic_in_kernel()) {
    //     i8259 = kvm_i8259_init(isa_bus);
    // } else if (xen_enabled()) {
    //     i8259 = xen_interrupt_controller_init();
    // } else {
        i8259 = i8259_init(isa_bus, pc_allocate_cpu_irq());
    // }

    for (i = 0; i < ISA_NUM_IRQS; i++) {
        gsi_state->i8259_irq[i] = i8259[i];
    }
    g_free(i8259);

    // if (pcmc->pci_enabled) {
    //     ioapic_init_gsi(gsi_state, "i440fx");
    // }

    pc_register_ferr_irq(pcms->gsi[13]);

    // pc_vga_init(isa_bus, pcmc->pci_enabled ? pci_bus : NULL);

    assert(pcms->vmport != ON_OFF_AUTO__MAX);
    if (pcms->vmport == ON_OFF_AUTO_AUTO) {
        pcms->vmport = xen_enabled() ? ON_OFF_AUTO_OFF : ON_OFF_AUTO_ON;
    }

    /* init basic PC hardware */
    pcms->pit = 1; // XBOX_FIXME: What's the right way to do this?
    // pc_basic_device_init(isa_bus, pcms->gsi, &rtc_state, true,
    //                      (pcms->vmport != ON_OFF_AUTO_ON), pcms->pit, 0x4);
    rtc_state = mc146818_rtc_init(isa_bus, 2000, NULL);

    // qemu_register_boot_set(pc_boot_set, rtc_state);
    ISADevice *pit = NULL;

    if (kvm_pit_in_kernel()) {
        pit = kvm_pit_init(isa_bus, 0x40);
    } else {
        pit = i8254_pit_init(isa_bus, 0x40, 0, NULL);
    }
    // if (hpet) {
    //     /* connect PIT to output control line of the HPET */
    //     qdev_connect_gpio_out(hpet, 0, qdev_get_gpio_in(DEVICE(pit), 0));
    // }
    pcspk_init(isa_bus, pit);

    // i8257_dma_init(isa_bus, 0);

    // pc_nic_init(pcmc, isa_bus, pci_bus);

    ide_drive_get(hd, ARRAY_SIZE(hd));
    // if (pcmc->pci_enabled) {
        PCIDevice *dev;
        // if (xen_enabled()) {
            // dev = pci_piix3_xen_ide_init(pci_bus, hd, piix3_devfn + 1);
        // } else {
            dev = pci_piix3_ide_init(pci_bus, hd, PCI_DEVFN(9, 0));
        // }
        idebus[0] = qdev_get_child_bus(&dev->qdev, "ide.0");
        idebus[1] = qdev_get_child_bus(&dev->qdev, "ide.1");
    // } else {
    //     for(i = 0; i < MAX_IDE_BUS; i++) {
    //         ISADevice *dev;
    //         char busname[] = "ide.0";
    //         dev = isa_ide_init(isa_bus, ide_iobase[i], ide_iobase2[i],
    //                            ide_irq[i],
    //                            hd[MAX_IDE_DEVS * i], hd[MAX_IDE_DEVS * i + 1]);
    //         /*
    //          * The ide bus name is ide.0 for the first bus and ide.1 for the
    //          * second one.
    //          */
    //         busname[4] = '0' + i;
    //         idebus[i] = qdev_get_child_bus(DEVICE(dev), busname);
    //     }
    // }

    pc_cmos_init(pcms, idebus[0], idebus[1], rtc_state);

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
    smbus_eeprom_init_single(smbus, 0x54, eeprom_buf);

    smbus_xbox_smc_init(smbus, 0x10);
    smbus_cx25871_init(smbus, 0x45);
    smbus_adm1032_init(smbus, 0x4c);

    /* USB */
    PCIDevice *usb1 = pci_create(pci_bus, PCI_DEVFN(3, 0), "pci-ohci");
    qdev_prop_set_uint32(&usb1->qdev, "num-ports", 4);
    qdev_init_nofail(&usb1->qdev);

    PCIDevice *usb0 = pci_create(pci_bus, PCI_DEVFN(2, 0), "pci-ohci");
    qdev_prop_set_uint32(&usb0->qdev, "num-ports", 4);
    qdev_init_nofail(&usb0->qdev);

    /* Ethernet! */
    PCIDevice *nvnet = pci_create(pci_bus, PCI_DEVFN(4, 0), "nvnet");

    for (i = 0; i < nb_nics; i++) {
        NICInfo *nd = &nd_table[i];
        qemu_check_nic_model(nd, "nvnet");
        qdev_set_nic_properties(&nvnet->qdev, nd);
        qdev_init_nofail(&nvnet->qdev);
    }

    /* APU! */
    pci_create_simple(pci_bus, PCI_DEVFN(5, 0), "mcpx-apu");

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
    m->default_cpu_type  = X86_CPU_TYPE_NAME("486");

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
                            machine_set_bootrom, NULL);
    object_property_set_description(obj, "bootrom",
                                    "Xbox bootrom file", NULL);

    object_property_add_bool(obj, "short-animation",
                             machine_get_short_animation,
                             machine_set_short_animation, NULL);
    object_property_set_description(obj, "short-animation",
                                    "Skip Xbox boot animation",
                                    NULL);
    object_property_set_bool(obj, false, "short-animation", NULL);

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
