/*
 * Core Definitions for QAPI/QMP Dispatch
 *
 * Copyright IBM, Corp. 2011
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING.LIB file in the top-level directory.
 *
 */

#ifndef QAPI_QMP_DISPATCH_H
#define QAPI_QMP_DISPATCH_H

#include "monitor/monitor.h"
#include "qemu/queue.h"

typedef void (QmpCommandFunc)(QDict *, QObject **, Error **);

typedef enum QmpCommandOptions
{
    QCO_NO_SUCCESS_RESP       =  (1U << 0),
    QCO_ALLOW_OOB             =  (1U << 1),
    QCO_ALLOW_PRECONFIG       =  (1U << 2),
    QCO_COROUTINE             =  (1U << 3),
} QmpCommandOptions;

typedef struct QmpCommand
{
    const char *name;
    /* Runs in coroutine context if QCO_COROUTINE is set */
    QmpCommandFunc *fn;
    QmpCommandOptions options;
    unsigned special_features;
    QTAILQ_ENTRY(QmpCommand) node;
    bool enabled;
    const char *disable_reason;
} QmpCommand;

typedef QTAILQ_HEAD(QmpCommandList, QmpCommand) QmpCommandList;

void qmp_register_command(QmpCommandList *cmds, const char *name,
                          QmpCommandFunc *fn, QmpCommandOptions options,
                          unsigned special_features);
const QmpCommand *qmp_find_command(const QmpCommandList *cmds,
                                   const char *name);
void qmp_disable_command(QmpCommandList *cmds, const char *name,
                         const char *err_msg);
void qmp_enable_command(QmpCommandList *cmds, const char *name);

bool qmp_command_is_enabled(const QmpCommand *cmd);
bool qmp_command_available(const QmpCommand *cmd, Error **errp);
const char *qmp_command_name(const QmpCommand *cmd);
bool qmp_has_success_response(const QmpCommand *cmd);
QDict *qmp_error_response(Error *err);
QDict *qmp_dispatch(const QmpCommandList *cmds, QObject *request,
                    bool allow_oob, Monitor *cur_mon);
bool qmp_is_oob(const QDict *dict);

typedef void (*qmp_cmd_callback_fn)(const QmpCommand *cmd, void *opaque);

void qmp_for_each_command(const QmpCommandList *cmds, qmp_cmd_callback_fn fn,
                          void *opaque);

#endif
