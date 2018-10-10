/*
 * QEMU SMBus Conexant CX25871 Video Encoder
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
#include "hw/i2c/i2c.h"
#include "hw/i2c/smbus.h"
#include "smbus.h"

// #define DEBUG
#ifdef DEBUG
# define DPRINTF(format, ...) printf(format, ## __VA_ARGS__)
#else
# define DPRINTF(format, ...) do { } while (0)
#endif

typedef struct SMBusCX25871Device {
    SMBusDevice smbusdev;
    uint8_t registers[256];
} SMBusCX25871Device;

static void cx_quick_cmd(SMBusDevice *dev, uint8_t read)
{
    DPRINTF("cx_quick_cmd: addr=0x%02x read=%d\n", dev->i2c.address, read);
}

static void cx_send_byte(SMBusDevice *dev, uint8_t val)
{
    DPRINTF("cx_send_byte: addr=0x%02x val=0x%02x\n", dev->i2c.address, val);
}

static uint8_t cx_receive_byte(SMBusDevice *dev)
{
    DPRINTF("cx_receive_byte: addr=0x%02x\n", dev->i2c.address);
    return 0;
}

static void cx_write_data(SMBusDevice *dev, uint8_t cmd, uint8_t *buf, int len)
{
    SMBusCX25871Device *cx = (SMBusCX25871Device *)dev;
    DPRINTF("cx_write_byte: addr=0x%02x cmd=0x%02x val=0x%02x\n",
            dev->i2c.address, cmd, buf[0]);

    memcpy(cx->registers + cmd, buf, MIN(len, 256 - cmd));
}

static uint8_t cx_read_data(SMBusDevice *dev, uint8_t cmd, int n)
{
    SMBusCX25871Device *cx = (SMBusCX25871Device *)dev;
    DPRINTF("cx_read_data: addr=0x%02x cmd=0x%02x n=%d\n",
            dev->i2c.address, cmd, n);

    return cx->registers[cmd];
}

static void smbus_cx25871_class_initfn(ObjectClass *klass, void *data)
{
    SMBusDeviceClass *sc = SMBUS_DEVICE_CLASS(klass);

    sc->quick_cmd = cx_quick_cmd;
    sc->send_byte = cx_send_byte;
    sc->receive_byte = cx_receive_byte;
    sc->write_data = cx_write_data;
    sc->read_data = cx_read_data;
}

static TypeInfo smbus_cx25871_info = {
    .name = "smbus-cx25871",
    .parent = TYPE_SMBUS_DEVICE,
    .instance_size = sizeof(SMBusCX25871Device),
    .class_init = smbus_cx25871_class_initfn,
};

static void smbus_cx25871_register_devices(void)
{
    type_register_static(&smbus_cx25871_info);
}

type_init(smbus_cx25871_register_devices)

void smbus_cx25871_init(I2CBus *smbus, int address)
{
    DeviceState *cx;
    cx = qdev_create((BusState *)smbus, "smbus-cx25871");
    qdev_prop_set_uint8(cx, "address", address);
    qdev_init_nofail(cx);
}
