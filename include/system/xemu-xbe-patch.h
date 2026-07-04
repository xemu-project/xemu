/*
 * xemu temporary XBE patch support
 *
 * Copyright (c) 2026 JBW89
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later.  See the COPYING file in the top-level directory.
 */
#ifndef SYSTEM_XEMU_XBE_PATCH_H
#define SYSTEM_XEMU_XBE_PATCH_H

#include <stddef.h>
#include <stdint.h>

typedef struct BlockBackend BlockBackend;
typedef struct QEMUIOVector QEMUIOVector;

#define XEMU_XBE_PATCH_MAX_FILES 5

#ifdef __cplusplus
extern "C" {
#endif

void xemu_xbe_patch_prepare(BlockBackend *blk);
void xemu_xbe_patch_refresh_current(void);
void xemu_xbe_patch_reset(BlockBackend *blk);
void xemu_xbe_patch_apply_read(BlockBackend *blk, int64_t offset,
                               int64_t bytes, QEMUIOVector *qiov,
                               size_t qiov_offset);
char *xemu_xbe_patch_dup_status(void);
char *xemu_xbe_patch_dup_current_hash(void);
char *xemu_xbe_patch_dup_current_title_id(void);
char *xemu_xbe_patch_dup_current_title_name(void);
char *xemu_xbe_patch_dup_current_region(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_XEMU_XBE_PATCH_H */
