/*
 *  Migration Threads info
 *
 *  Copyright (c) 2022 HUAWEI TECHNOLOGIES CO., LTD.
 *
 *  Authors:
 *  Jiang Jiacheng <jiangjiacheng@huawei.com>
 *
 *  This work is licensed under the terms of the GNU GPL, version 2 or later.
 *  See the COPYING file in the top-level directory.
 */

#include "qapi/error.h"
#include "qapi/qapi-commands-migration.h"

typedef struct MigrationThread MigrationThread;

struct MigrationThread {
    const char *name; /* the name of migration thread */
    int thread_id; /* ID of the underlying host thread */
    QLIST_ENTRY(MigrationThread) node;
};

MigrationThread *migration_threads_add(const char *name, int thread_id);
void migration_threads_remove(MigrationThread *info);
