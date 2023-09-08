/*
 * QAPI common helper functions
 *
 * This file provides helper functions related to types defined
 * in the QAPI schema.
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING.LIB file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qapi/type-helpers.h"

HumanReadableText *human_readable_text_from_str(GString *str)
{
    HumanReadableText *ret = g_new0(HumanReadableText, 1);

    ret->human_readable_text = g_steal_pointer(&str->str);

    return ret;
}
