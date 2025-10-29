/*
 * QEMU SMBus ADM1032 Temperature Monitor
 *
 * Copyright (c) 2012 espes
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/qdev-properties.h"
#include "hw/i2c/i2c.h"
#include "hw/i2c/smbus_slave.h"
#include "smbus.h"
#include "qapi/error.h"

#define TYPE_SMBUS_ADM1032 "smbus-adm1032"
#define SMBUS_ADM1032(obj) OBJECT_CHECK(SMBusADM1032Device, (obj), TYPE_SMBUS_ADM1032)

// #define DEBUG
#ifdef DEBUG
# define DPRINTF(format, ...) printf(format, ## __VA_ARGS__)
#else
# define DPRINTF(format, ...) do { } while (0)
#endif

typedef struct SMBusADM1032Device {
    SMBusDevice smbusdev;
    uint8_t cmd;
} SMBusADM1032Device;

static void smbus_adm1032_quick_cmd(SMBusDevice *dev, uint8_t read)
{
    DPRINTF("smbus_adm1032_quick_cmd: addr=0x%02x read=%d\n", dev->i2c.address, read);
}

static int smbus_adm1032_write_data(SMBusDevice *dev, uint8_t *buf, uint8_t len)
{
    SMBusADM1032Device *cx = SMBUS_ADM1032(dev);
    DPRINTF("smbus_adm1032_write_data: addr=0x%02x val=0x%02x\n",
            dev->i2c.address, buf[0]);

    cx->cmd = buf[0];
    // buf++;
    // len--;

    return 0;
}

static uint8_t smbus_adm1032_receive_byte(SMBusDevice *dev)
{
    SMBusADM1032Device *cx = SMBUS_ADM1032(dev);
    DPRINTF("smbus_adm1032_receive_byte: addr=0x%02x cmd=0x%02x\n",
            dev->i2c.address, cx->cmd);

    uint8_t cmd = cx->cmd++;

    switch (cmd) {
    case 0x0:
    case 0x1:
        return 50;
    default:
        break;
    }

    return 0;
}

static void smbus_adm1032_realize(DeviceState *dev, Error **errp)
{
    SMBusADM1032Device *cx = SMBUS_ADM1032(dev);
    cx->cmd = 0;
}

static void smbus_adm1032_class_initfn(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SMBusDeviceClass *sc = SMBUS_DEVICE_CLASS(klass);

    dc->realize = smbus_adm1032_realize;
    sc->quick_cmd = smbus_adm1032_quick_cmd;
    sc->receive_byte = smbus_adm1032_receive_byte;
    sc->write_data = smbus_adm1032_write_data;
}

static TypeInfo smbus_adm1032_info = {
    .name = TYPE_SMBUS_ADM1032,
    .parent = TYPE_SMBUS_DEVICE,
    .instance_size = sizeof(SMBusADM1032Device),
    .class_init = smbus_adm1032_class_initfn,
};

static void smbus_adm1032_register_devices(void)
{
    type_register_static(&smbus_adm1032_info);
}

type_init(smbus_adm1032_register_devices)

void smbus_adm1032_init(I2CBus *smbus, int address)
{
    DeviceState *dev;
    dev = qdev_new(TYPE_SMBUS_ADM1032);
    qdev_prop_set_uint8(dev, "address", address);
    qdev_realize_and_unref(dev, (BusState *)smbus, &error_fatal);
}
