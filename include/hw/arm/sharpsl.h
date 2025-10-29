/*
 * Common declarations for the Zaurii.
 *
 * This file is licensed under the GNU GPL.
 */

#ifndef QEMU_SHARPSL_H
#define QEMU_SHARPSL_H

#include "exec/hwaddr.h"

/* zaurus.c */

#define SL_PXA_PARAM_BASE	0xa0000a00
void sl_bootparam_write(hwaddr ptr);

#endif
