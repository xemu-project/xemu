/*
 * HMP commands related to run state
 *
 * Copyright IBM, Corp. 2011
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Contributions after 2012-01-13 are licensed under the terms of the
 * GNU GPL, version 2 or (at your option) any later version.
 */

#include "qemu/osdep.h"
#include "exec/cpu-common.h"
#include "monitor/hmp.h"
#include "monitor/monitor.h"
#include "qapi/error.h"
#include "qapi/qapi-commands-run-state.h"
#include "qapi/qmp/qdict.h"
#include "qemu/accel.h"

void hmp_info_status(Monitor *mon, const QDict *qdict)
{
    StatusInfo *info;

    info = qmp_query_status(NULL);

    monitor_printf(mon, "VM status: %s",
                   info->running ? "running" : "paused");

    if (!info->running && info->status != RUN_STATE_PAUSED) {
        monitor_printf(mon, " (%s)", RunState_str(info->status));
    }

    monitor_printf(mon, "\n");

    qapi_free_StatusInfo(info);
}

void hmp_one_insn_per_tb(Monitor *mon, const QDict *qdict)
{
    const char *option = qdict_get_try_str(qdict, "option");
    AccelState *accel = current_accel();
    bool newval;

    if (!object_property_find(OBJECT(accel), "one-insn-per-tb")) {
        monitor_printf(mon,
                       "This accelerator does not support setting one-insn-per-tb\n");
        return;
    }

    if (!option || !strcmp(option, "on")) {
        newval = true;
    } else if (!strcmp(option, "off")) {
        newval = false;
    } else {
        monitor_printf(mon, "unexpected option %s\n", option);
        return;
    }
    /* If the property exists then setting it can never fail */
    object_property_set_bool(OBJECT(accel), "one-insn-per-tb",
                             newval, &error_abort);
}

void hmp_watchdog_action(Monitor *mon, const QDict *qdict)
{
    Error *err = NULL;
    WatchdogAction action;
    char *qapi_value;

    qapi_value = g_ascii_strdown(qdict_get_str(qdict, "action"), -1);
    action = qapi_enum_parse(&WatchdogAction_lookup, qapi_value, -1, &err);
    g_free(qapi_value);
    if (err) {
        hmp_handle_error(mon, err);
        return;
    }
    qmp_watchdog_set_action(action, &error_abort);
}

void watchdog_action_completion(ReadLineState *rs, int nb_args, const char *str)
{
    int i;

    if (nb_args != 2) {
        return;
    }
    readline_set_completion_index(rs, strlen(str));
    for (i = 0; i < WATCHDOG_ACTION__MAX; i++) {
        readline_add_completion_of(rs, str, WatchdogAction_str(i));
    }
}
