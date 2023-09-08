/*
 * QEMU HAXM support
 *
 * Copyright (c) 2015, Intel Corporation
 *
 * Copyright 2016 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * See the COPYING file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include "sysemu/hax.h"

bool hax_allowed;

int hax_sync_vcpus(void)
{
    return 0;
}
