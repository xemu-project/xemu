/*
 * QEMU SMBus Generic Storage Device
 *
 * Copyright (c) 2020 Mike Davis
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
#include "hw/loader.h"
#include "migration/vmstate.h"

#define TYPE_SMBUS_STORAGE "smbus-storage"
#define SMBUS_STORAGE(obj) OBJECT_CHECK(SMBusStorageDevice, (obj), \
    TYPE_SMBUS_STORAGE)

//#define DEBUG
#ifdef DEBUG
# define DPRINTF(format, ...) printf(format, ## __VA_ARGS__)
#else
# define DPRINTF(format, ...) do { } while (0)
#endif

typedef struct SMBusStorageDevice {
    SMBusDevice smbusdev;
    char* file;
    uint8_t *data;
    uint32_t size;
    uint8_t addr;
    uint32_t offset;
    bool persist;
} SMBusStorageDevice;

// FIXME: remove, file should be generated upstream if unspecified
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

static void smbus_storage_realize(DeviceState *dev, Error **errp)
{
    SMBusStorageDevice *s = SMBUS_STORAGE(dev);
    qdev_prop_set_uint8(dev, "address", s->addr);
    s->data = g_malloc0(s->size);
    s->offset = 0;

    if (s->file) {
        int size = get_image_size(s->file);
        if (size != s->size) {
            error_setg(errp, "%s: file '%s' size of %d, expected %d\n",
                __func__, s->file, size, s->size);
            return;
        }

        int fd = open(s->file, O_RDONLY | O_BINARY);
        if (fd < 0) {
            error_setg(errp, "%s: file '%s' could not be opened\n",
                __func__, s->file);
            return;
        }

        int rc = read(fd, s->data, s->size);
        if (rc != s->size) {
            error_setg(errp, "%s: file '%s' read failure\n", __func__, s->file);
            close(fd);
            return;
        }
        close(fd);
    } else {
        // FIXME: remove, file should be generated upstream if unspecified
        memcpy(s->data, default_eeprom, MIN(s->size, sizeof(default_eeprom)));
    }
}

static int smbus_storage_write_data(SMBusDevice *dev, uint8_t *buf, uint8_t len)
{
    SMBusStorageDevice *s = SMBUS_STORAGE(dev);

    DPRINTF("%s: addr=0x%02x cmd=0x%02x val=0x%02x\n",
        __func__, s->addr, buf[0], buf[1]);

    /* len is guaranteed to be > 0 */
    s->offset = buf[0];
    buf++;
    len--;

    bool changed = false;
    for (; len > 0; len--) {
        changed = true;
        s->data[s->offset] = *buf++;
        DPRINTF("%s: addr=0x%02x off=0x%02x, data=0x%02x\n",
            __func__, s->addr, s->offset, s->data[s->offset]);
        s->offset = (s->offset + 1) % s->size;
    }

    if (changed && s->file && s->persist) {
        int fd = open(s->file, O_WRONLY | O_BINARY);
        if (fd < 0) {
            DPRINTF("%s: file '%s' could not be opened\n", __func__, s->file);
            return -1;
        }

        int wc = write(fd, s->data, s->size);
        if (wc != s->size) {
            DPRINTF( "%s: file '%s' write failure\n", __func__, s->file);
            close(fd);
            return -1;
        }
        close(fd);
    }

    return 0;
}

static uint8_t smbus_storage_receive_byte(SMBusDevice *dev)
{
    SMBusStorageDevice *s = SMBUS_STORAGE(dev);

    uint8_t val = s->data[s->offset];
    DPRINTF("%s: addr=0x%02x off=0x%02x val=0x%02x\n",
        __func__, s->addr, s->offset, val);
    s->offset = (s->offset + 1) % s->size;

    return val;
}

static const VMStateDescription vmstate_smbus_storage = {
    .name = "smbus-storage",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields      = (VMStateField[]) {
        VMSTATE_SMBUS_DEVICE(smbusdev, SMBusStorageDevice),
        VMSTATE_VBUFFER_UINT32(data, SMBusStorageDevice, 1, NULL, size),
        VMSTATE_UINT32(offset, SMBusStorageDevice),
        VMSTATE_END_OF_LIST()
    }
};

// default xbox eeprom with persistence
static Property smbus_storage_props[] = {
    DEFINE_PROP_UINT8("addr", SMBusStorageDevice, addr, 0x54),
    DEFINE_PROP_UINT32("size", SMBusStorageDevice, size, 256),
    DEFINE_PROP_BOOL("persist", SMBusStorageDevice, persist, true),
    DEFINE_PROP_STRING("file", SMBusStorageDevice, file),
    DEFINE_PROP_END_OF_LIST()
};

static void smbus_storage_class_initfn(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SMBusDeviceClass *sc = SMBUS_DEVICE_CLASS(klass);

    dc->vmsd = &vmstate_smbus_storage;
    dc->realize = smbus_storage_realize;
    sc->receive_byte = smbus_storage_receive_byte;
    sc->write_data = smbus_storage_write_data;
    device_class_set_props(dc, smbus_storage_props);
}

static TypeInfo smbus_storage_info = {
    .name = TYPE_SMBUS_STORAGE,
    .parent = TYPE_SMBUS_DEVICE,
    .instance_size = sizeof(SMBusStorageDevice),
    .class_init = smbus_storage_class_initfn
};

static void smbus_storage_register_devices(void)
{
    type_register_static(&smbus_storage_info);
}

type_init(smbus_storage_register_devices)
