/*
 * QEMU Guest Agent command state interfaces
 *
 * Copyright IBM Corp. 2011
 *
 * Authors:
 *  Michael Roth      <mdroth@linux.vnet.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */
#include "qemu/osdep.h"
#include "guest-agent-core.h"

struct GACommandState {
    GSList *groups;
};

typedef struct GACommandGroup {
    void (*init)(void);
    void (*cleanup)(void);
} GACommandGroup;

/* handle init/cleanup for stateful guest commands */

void ga_command_state_add(GACommandState *cs,
                          void (*init)(void),
                          void (*cleanup)(void))
{
    GACommandGroup *cg = g_new0(GACommandGroup, 1);
    cg->init = init;
    cg->cleanup = cleanup;
    cs->groups = g_slist_append(cs->groups, cg);
}

static void ga_command_group_init(gpointer opaque, gpointer unused)
{
    GACommandGroup *cg = opaque;

    g_assert(cg);
    if (cg->init) {
        cg->init();
    }
}

void ga_command_state_init_all(GACommandState *cs)
{
    g_assert(cs);
    g_slist_foreach(cs->groups, ga_command_group_init, NULL);
}

static void ga_command_group_cleanup(gpointer opaque, gpointer unused)
{
    GACommandGroup *cg = opaque;

    g_assert(cg);
    if (cg->cleanup) {
        cg->cleanup();
    }
}

void ga_command_state_cleanup_all(GACommandState *cs)
{
    g_assert(cs);
    g_slist_foreach(cs->groups, ga_command_group_cleanup, NULL);
}

GACommandState *ga_command_state_new(void)
{
    GACommandState *cs = g_new0(GACommandState, 1);
    cs->groups = NULL;
    return cs;
}

void ga_command_state_free(GACommandState *cs)
{
    g_slist_free_full(cs->groups, g_free);
    g_free(cs);
}
