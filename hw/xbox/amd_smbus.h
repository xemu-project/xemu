/*
 * AMD756 SMBus implementation
 *
 * Copyright (C) 2012 espes
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

#ifndef AMD_SMBUS_H
#define AMD_SMBUS_H

typedef struct AMD756SMBus {
    I2CBus *smbus;

    uint8_t smb_stat;
    uint8_t smb_ctl;
    uint8_t smb_cmd;
    uint8_t smb_addr;
    uint8_t smb_data0;
    uint8_t smb_data1;
    uint8_t smb_data[32];
    uint8_t smb_index;

    qemu_irq irq;
} AMD756SMBus;

void amd756_smbus_init(DeviceState *parent, AMD756SMBus *smb, qemu_irq irq);
void amd756_smb_ioport_writeb(void *opaque, uint32_t addr, uint32_t val);
uint32_t amd756_smb_ioport_readb(void *opaque, uint32_t addr);

#endif /* !AMD_SMBUS_H */
