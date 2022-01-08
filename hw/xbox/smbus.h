/*
 * QEMU Xbox System Emulator
 *
 * Copyright (c) 2013 espes
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

#ifndef HW_SMBUS_H
#define HW_SMBUS_H

void smbus_xbox_smc_init(I2CBus *smbus, int address);
void smbus_cx25871_init(I2CBus *smbus, int address);
void smbus_fs454_init(I2CBus *smbus, int address);
void smbus_xcalibur_init(I2CBus *smbus, int address);
void smbus_adm1032_init(I2CBus *smbus, int address);

bool xbox_smc_avpack_to_reg(const char *avpack, uint8_t *value);
void xbox_smc_append_avpack_hint(Error **errp);
void xbox_smc_append_smc_version_hint(Error **errp);
void xbox_smc_power_button(void);
void xbox_smc_eject_button(void);
void xbox_smc_update_tray_state(void);

#endif
