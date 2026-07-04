/*
 * xemu Redump ISO support
 *
 * Copyright (c) 2026 JBW89
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later.  See the COPYING file in the top-level directory.
 */
#ifndef SYSTEM_XEMU_REDUMP_H
#define SYSTEM_XEMU_REDUMP_H

bool xemu_redump_detect_game_partition(const char *path, uint64_t *offset);

#endif /* SYSTEM_XEMU_REDUMP_H */
