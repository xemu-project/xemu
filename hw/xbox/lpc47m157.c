/*
 * QEMU SMSC LPC47M157 (Super I/O)
 *
 * Copyright (c) 2013 espes
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
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "migration/vmstate.h"
#include "system/system.h"
#include "hw/char/serial.h"
#include "hw/isa/isa.h"
#include "qapi/error.h"

#define MAX_DEVICE 0xC
#define DEVICE_FDD              0x0
#define DEVICE_PARALLEL_PORT    0x3
#define DEVICE_SERIAL_PORT_1    0x4
#define DEVICE_SERIAL_PORT_2    0x5
#define DEVICE_KEYBOARD         0x7
#define DEVICE_GAME_PORT        0x9
#define DEVICE_PME              0xA
#define DEVICE_MPU_401          0xB

#define CONFIG_PORT 0x00
#define INDEX_PORT  CONFIG_PORT
#define DATA_PORT   0x01

#define ENTER_CONFIG_KEY    0x55
#define EXIT_CONFIG_KEY     0xAA

#define MAX_CONFIG_REG  0x30
#define MAX_DEVICE_REGS 0xFF

#define CONFIG_DEVICE_NUMBER    0x07
#define CONFIG_PORT_LOW         0x26
#define CONFIG_PORT_HIGH        0x27

#define CONFIG_DEVICE_ACTIVATE              0x30
#define CONFIG_DEVICE_BASE_ADDRESS_HIGH     0x60
#define CONFIG_DEVICE_BASE_ADDRESS_LOW      0x61
#define CONFIG_DEVICE_INTERRUPT             0x70

#define TYPE_ISA_LPC47M157_DEVICE "lpc47m157"
#define ISA_LPC47M157_DEVICE(obj) \
    OBJECT_CHECK(ISALPC47M157State, (obj), TYPE_ISA_LPC47M157_DEVICE)

// #define DEBUG
#ifdef DEBUG
# define DPRINTF(format, ...) printf(format, ## __VA_ARGS__)
#else
# define DPRINTF(format, ...) do { } while (0)
#endif

typedef struct LPC47M157State_Serial {
    bool active;
    uint16_t iobase;
    uint8_t irq;

    SerialState state;
} LPC47M157State_Serial;

typedef struct LPC47M157State {
    MemoryRegion io;

    bool configuration_mode;
    uint32_t selected_reg;
    uint8_t config_regs[MAX_CONFIG_REG];
    uint8_t device_regs[MAX_DEVICE][MAX_DEVICE_REGS];

    LPC47M157State_Serial serial[2];
} LPC47M157State;

typedef struct ISALPC47M157State {
    ISADevice parent_obj;

    bool sysopt;
    uint16_t iobase;
    LPC47M157State state;
} ISALPC47M157State;

static void update_devices(ISALPC47M157State *isa)
{
    LPC47M157State *s = &isa->state;
    LPC47M157State_Serial *serial;
    SerialState *ss;
    uint8_t *dev;
    uint16_t iobase;
    uint8_t irq;
    int i;

    /* update serial devices */
    for (i = 0; i < ARRAY_SIZE(s->serial); i++) {
        serial = &s->serial[i];
        ss = &serial->state;
        dev = s->device_regs[DEVICE_SERIAL_PORT_1 + i];
        iobase = (dev[CONFIG_DEVICE_BASE_ADDRESS_HIGH] << 8)
                  | dev[CONFIG_DEVICE_BASE_ADDRESS_LOW];
        irq = dev[CONFIG_DEVICE_INTERRUPT] & 0x0f;
        if (serial->active && (!dev[CONFIG_DEVICE_ACTIVATE]
                               || serial->iobase != iobase
                               || serial->irq != irq)) {
            isa_unregister_ioport(NULL, &ss->io);
            memory_region_destroy(&ss->io);
            ss->irq = NULL;
            serial->active = false;
            DPRINTF("lpc47m157 COM%d disabled @ iobase=0x%x irq=%u\n",
                    i + 1, serial->iobase, serial->irq);
        }
        if (!serial->active && dev[CONFIG_DEVICE_ACTIVATE]) {
            ss->irq = irq != 0 ? isa_get_irq(&isa->parent_obj, irq) : NULL;
            memory_region_init_io(&ss->io, OBJECT(s),
                                  &serial_io_ops, ss, "serial", 8);
            isa_register_ioport(NULL, &ss->io, iobase);
            serial->iobase = iobase;
            serial->irq = irq;
            serial->active = true;
            DPRINTF("lpc47m157 COM%d enabled @ iobase=0x%x irq=%u\n",
                    i + 1, serial->iobase, serial->irq);
        }
    }
}

static void lpc47m157_io_write(void *opaque, hwaddr addr, uint64_t val,
                               unsigned int size)
{
    ISALPC47M157State *isa = ISA_LPC47M157_DEVICE(opaque);
    LPC47M157State *s = &isa->state;
    uint8_t *dev;

    DPRINTF("lpc47m157 io write 0x%02" HWADDR_PRIx " = 0x%02" PRIx64 "\n", addr, val);

    if (addr == CONFIG_PORT && (val == ENTER_CONFIG_KEY || val == EXIT_CONFIG_KEY)) {
        if (val == ENTER_CONFIG_KEY) {
            s->configuration_mode = true;
            DPRINTF("lpc47m157 enter configuration mode\n");
        } else if (val == EXIT_CONFIG_KEY) {
            if (s->configuration_mode) {
                update_devices(isa);
            }
            s->configuration_mode = false;
            DPRINTF("lpc47m157 exit configuration mode\n");
        }
    } else if (s->configuration_mode) {
        if (addr == INDEX_PORT) {
            s->selected_reg = val;
        } else if (addr == DATA_PORT) {
            if (s->selected_reg < MAX_CONFIG_REG) {
                /* global configuration register */
                s->config_regs[s->selected_reg] = val;
            } else {
                /* device register */
                assert(s->config_regs[CONFIG_DEVICE_NUMBER] < MAX_DEVICE);
                dev = s->device_regs[s->config_regs[CONFIG_DEVICE_NUMBER]];
                dev[s->selected_reg] = val;
                DPRINTF("lpc47m157 dev 0x%02x . 0x%02x = 0x%02" PRIx64 "\n",
                        s->config_regs[CONFIG_DEVICE_NUMBER], s->selected_reg, val);
            }
        } else {
            assert(false);
        }
    }
}

static uint64_t lpc47m157_io_read(void *opaque, hwaddr addr, unsigned int size)
{
    ISALPC47M157State *isa = ISA_LPC47M157_DEVICE(opaque);
    LPC47M157State *s = &isa->state;
    uint8_t *dev;
    uint32_t val = 0;

    if (s->configuration_mode) {
        if (addr == DATA_PORT) {
            if (s->selected_reg < MAX_CONFIG_REG) {
                /* global configuration register */
                val = s->config_regs[s->selected_reg];
            } else {
                /* device register */
                assert(s->config_regs[CONFIG_DEVICE_NUMBER] < MAX_DEVICE);
                dev = s->device_regs[s->config_regs[CONFIG_DEVICE_NUMBER]];
                val = dev[s->selected_reg];
            }
        } else if (addr != INDEX_PORT) {
            assert(false);
        }
    }

    DPRINTF("lpc47m157 io read 0x%02" HWADDR_PRIx " -> 0x%02x\n", addr, val);

    return val;
}

static const MemoryRegionOps lpc47m157_io_ops = {
    .read = lpc47m157_io_read,
    .write = lpc47m157_io_write,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
};

static void lpc47m157_realize(DeviceState *dev, Error **errp)
{
    ISADevice *isadev = ISA_DEVICE(dev);
    ISALPC47M157State *isa = ISA_LPC47M157_DEVICE(isadev);
    LPC47M157State *s = &isa->state;
    SerialState *ss;
    Chardev *chr;
    char *name;
    int i;

    isa->iobase = isa->sysopt ? 0x4e : 0x2e;
    s->config_regs[CONFIG_PORT_LOW] = isa->iobase & 0xff;
    s->config_regs[CONFIG_PORT_HIGH] = isa->iobase >> 8;

    memory_region_init_io(&s->io, OBJECT(s),
                          &lpc47m157_io_ops, isa, TYPE_ISA_LPC47M157_DEVICE, 2);
    isa_register_ioport(isadev, &s->io, isa->iobase);

    /* init serial cores */
    for (i = 0; i < ARRAY_SIZE(s->serial); i++) {
        ss = &s->serial[i].state;
        chr = serial_hd(i);
        if (chr == NULL) {
            name = g_strdup_printf("ser%d", i);
            chr = qemu_chr_new(name, "null", NULL);
            g_free(name);
        }
        qdev_prop_set_chr(dev, i == 0 ? "chardev0" : "chardev1", chr);
        qdev_realize(DEVICE(ss), NULL, errp);
    }
}

static int lpc47m157_post_load(void *opaque, int version_id)
{
    ISALPC47M157State *isa = ISA_LPC47M157_DEVICE(opaque);

    /* reconfigure devices */
    update_devices(isa);

    return 0;
}

static const VMStateDescription vmstate_lpc47m157 = {
    .name = "lpc47m157",
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = lpc47m157_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_BOOL(sysopt, ISALPC47M157State),
        VMSTATE_UINT16(iobase, ISALPC47M157State),
        VMSTATE_BOOL(state.configuration_mode, ISALPC47M157State),
        VMSTATE_UINT32(state.selected_reg, ISALPC47M157State),
        VMSTATE_UINT8_ARRAY(state.config_regs, ISALPC47M157State, MAX_CONFIG_REG),
        VMSTATE_UINT8_2DARRAY(state.device_regs, ISALPC47M157State, MAX_DEVICE, MAX_DEVICE_REGS),
        VMSTATE_STRUCT(state.serial[0].state, ISALPC47M157State, 0,
                       vmstate_serial, SerialState),
        VMSTATE_STRUCT(state.serial[1].state, ISALPC47M157State, 0,
                       vmstate_serial, SerialState),
        VMSTATE_END_OF_LIST()
    }
};

static const Property lpc47m157_properties[] = {
    DEFINE_PROP_BOOL("sysopt", ISALPC47M157State, sysopt, false),
    DEFINE_PROP_CHR("chardev0", ISALPC47M157State, state.serial[0].state.chr),
    DEFINE_PROP_CHR("chardev1", ISALPC47M157State, state.serial[1].state.chr),
};

static void lpc47m157_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = lpc47m157_realize;
    dc->vmsd = &vmstate_lpc47m157;
    //dc->reset = pc87312_reset;
    device_class_set_props(dc, lpc47m157_properties);
}

static void lpc47m157_initfn(Object *o)
{
    ISALPC47M157State *self = ISA_LPC47M157_DEVICE(o);

    object_initialize_child(o, "serial0", &self->state.serial[0].state, TYPE_SERIAL);
    object_initialize_child(o, "serial1", &self->state.serial[1].state, TYPE_SERIAL);
}

static const TypeInfo lpc47m157_type_info = {
    .name          = TYPE_ISA_LPC47M157_DEVICE,
    .parent        = TYPE_ISA_DEVICE,
    .instance_init = lpc47m157_initfn,
    .instance_size = sizeof(ISALPC47M157State),
    .class_init    = lpc47m157_class_init,
};

static void lpc47m157_register_types(void)
{
    type_register_static(&lpc47m157_type_info);
}

type_init(lpc47m157_register_types)
