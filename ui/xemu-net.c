/*
 * xemu Network Management
 *
 * Wrapper functions to configure network settings at runtime.
 *
 * Copyright (C) 2020 Matt Borgerson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "xemu-net.h"
#include "xemu-settings.h"

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "hw/qdev-core.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"
#include "monitor/qdev.h"
#include "qapi/qmp/qdict.h"
#include "qemu/option.h"
#include "qemu/config-file.h"
#include "net/net.h"
#include "net/hub.h"

static const char *id = "xemu-netdev";
static const char *id_hubport = "xemu-netdev-hubport";

void xemu_net_enable(void)
{
    Error *local_err = NULL;

    NetClientState *nc = qemu_find_netdev(id);
    if (nc != NULL) {
        return;
    }

    int backend;
    const char *local_addr, *remote_addr;
    xemu_settings_get_enum(XEMU_SETTINGS_NETWORK_BACKEND, &backend);
    xemu_settings_get_string(XEMU_SETTINGS_NETWORK_REMOTE_ADDR, &remote_addr);
    xemu_settings_get_string(XEMU_SETTINGS_NETWORK_LOCAL_ADDR, &local_addr);

    // Create the netdev
    QDict *qdict;
    if (backend == XEMU_NET_BACKEND_USER) {
        qdict = qdict_new();
        qdict_put_str(qdict, "id",   id);
        qdict_put_str(qdict, "type", "user");
    } else if (backend == XEMU_NET_BACKEND_SOCKET_UDP) {
        qdict = qdict_new();
        qdict_put_str(qdict, "id",        id);
        qdict_put_str(qdict, "type",      "socket");
        qdict_put_str(qdict, "udp",       remote_addr);
        qdict_put_str(qdict, "localaddr", local_addr);
    } else {
        // Unsupported backend type
        return;
    }

    QemuOpts *opts = qemu_opts_from_qdict(qemu_find_opts("netdev"), qdict, &error_abort);
    qobject_unref(qdict);
    netdev_add(opts, &local_err);
    if (local_err) {
        qemu_opts_del(opts);
        error_report_err(local_err);
        // error_propagate(errp, local_err);
        return;
    }

    // Create the hubport
    qdict = qdict_new();
    qdict_put_str(qdict, "id",     id_hubport);
    qdict_put_str(qdict, "type",   "hubport");
    qdict_put_int(qdict, "hubid",  0);
    qdict_put_str(qdict, "netdev", id);
    opts = qemu_opts_from_qdict(qemu_find_opts("netdev"), qdict, &error_abort);
    qobject_unref(qdict);
    netdev_add(opts, &local_err);
    if (local_err) {
        qemu_opts_del(opts);
        error_report_err(local_err);
        // error_propagate(errp, local_err);
    }
}

static void remove_netdev(const char *name)
{
    NetClientState *nc;
    QemuOpts *opts;

    nc = qemu_find_netdev(name);
    if (!nc) {
        // error_set(errp, ERROR_CLASS_DEVICE_NOT_FOUND,
        //           "Device '%s' not found", name);
        return;
    }

    opts = qemu_opts_find(qemu_find_opts_err("netdev", NULL), name);
    if (!opts) {
        // error_setg(errp, "Device '%s' is not a netdev", name);
        return;
    }

    qemu_opts_del(opts);
    qemu_del_net_client(nc);
}

void xemu_net_disable(void)
{
    remove_netdev(id);
    remove_netdev(id_hubport);
}

int xemu_net_is_enabled(void)
{
    NetClientState *nc;
    nc = qemu_find_netdev(id);
    return (nc != NULL); 
}
