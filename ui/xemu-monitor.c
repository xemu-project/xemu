/*
 * xemu QEMU Monitor Interface
 *
 * Copyright (c) 2020-2021 Matt Borgerson
 *
 * Based on gdbstub.c
 *
 * Copyright (c) 2003-2005 Fabrice Bellard
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
#include "qapi/error.h"
#include "ui/console.h"
#include "ui/input.h"
#include "sysemu/sysemu.h"
#include "monitor/monitor.h"
#include "chardev/char.h"

#include "xemu-monitor.h"

#define TYPE_CHARDEV_XEMU_MONITOR "chardev-xemu-monitor"

static Chardev *mon_chr;
static char mon_buffer[12*4096];
static const size_t mon_buffer_size = sizeof(mon_buffer);
static size_t offset;

static void char_xemu_class_init(ObjectClass *oc, void *data);
static void xemu_monitor_open(Chardev *chr, ChardevBackend *backend,
                             bool *be_opened, Error **errp);
static int xemu_monitor_buffer_append(Chardev *chr, const uint8_t *buf, int len);

static void char_xemu_class_init(ObjectClass *oc, void *data)
{
    ChardevClass *cc = CHARDEV_CLASS(oc);

    cc->internal = true;
    cc->open = xemu_monitor_open;
    cc->chr_write = xemu_monitor_buffer_append;
}

static void xemu_monitor_open(Chardev *chr, ChardevBackend *backend,
                             bool *be_opened, Error **errp)
{
    *be_opened = false;
}

void xemu_monitor_init(void)
{
    /* For simplicity, assume this is only created once */
    assert(mon_chr == NULL);
    mon_chr = qemu_chardev_new(NULL, TYPE_CHARDEV_XEMU_MONITOR,
                               NULL, NULL, &error_abort);
    monitor_init_hmp(mon_chr, false, &error_abort);
}

char *xemu_get_monitor_buffer(void)
{
    return mon_buffer;
}

static int xemu_monitor_buffer_append(Chardev *chr, const uint8_t *buf, int len)
{
    if ((offset+len+1) >= mon_buffer_size) {
        /* Reached the end of the buffer. Keep it simple and
         * just start back at the beginning.
         */
        offset = 0;
    }

    /* Copy string into the monitor buffer and terminate */
    assert((len+1) <= mon_buffer_size);
    memcpy(&mon_buffer[offset], buf, len);
    offset += len;
    mon_buffer[offset] = '\x00';

    return len;
}

void xemu_run_monitor_command(const char *cmd)
{
    /* Copy command into buffer */
    xemu_monitor_buffer_append(mon_chr, (const uint8_t*)"# ", 2);
    xemu_monitor_buffer_append(mon_chr, (const uint8_t*)cmd, strlen(cmd));
    xemu_monitor_buffer_append(mon_chr, (const uint8_t*)"\n", 1);

    /* Send command to monitor */
    int len = strlen(cmd)+1;

    /* FIXME: qemu_chr_be_write needs to be fixed to declare inbuf as const. It
     * does not modify the data. Cast for now.
     */
    qemu_chr_be_write(mon_chr, (unsigned char*)cmd, len);
}

static const TypeInfo char_xemu_type_info = {
    .name = TYPE_CHARDEV_XEMU_MONITOR,
    .parent = TYPE_CHARDEV,
    .class_init = char_xemu_class_init,
};

static void register_types(void)
{
    type_register_static(&char_xemu_type_info);
}

type_init(register_types);
