/*
 * xemu User Interface
 *
 * Copyright (C) 2020-2022 Matt Borgerson
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

#ifndef XEMU_SNAPSHOTS_H
#define XEMU_SNAPSHOTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "qemu/osdep.h"
#include "block/snapshot.h"

#define XEMU_SNAPSHOT_DATA_MAGIC 0x78656d75
#define XEMU_SNAPSHOT_HEIGHT 120
#define XEMU_SNAPSHOT_WIDTH 160

extern const char **g_snapshot_shortcut_index_key_map[];

#pragma pack(1)
typedef struct TextureBuffer {
    int channels;
    unsigned long size;
    void *buffer;
} TextureBuffer;
#pragma pack()

typedef struct XemuSnapshotHeader {
    uint32_t magic;
    uint32_t size;
} XemuSnapshotHeader;

typedef struct XemuSnapshotData {
    int64_t xbe_title_len;
    char *xbe_title;
    bool xbe_title_present;
    TextureBuffer thumbnail;
    bool thumbnail_present;
    unsigned int gl_thumbnail;
} XemuSnapshotData;

// Implemented in xemu-snapshots.c
int xemu_snapshots_list(QEMUSnapshotInfo **info, XemuSnapshotData **extra_data,
                        Error **err);
void xemu_snapshots_load(const char *vm_name, Error **err);
void xemu_snapshots_save(const char *vm_name, Error **err);
void xemu_snapshots_delete(const char *vm_name, Error **err);

void xemu_snapshots_save_extra_data(QEMUFile *f);
bool xemu_snapshots_offset_extra_data(QEMUFile *f);
void xemu_snapshots_mark_dirty(void);

// Implemented in xemu-thumbnail.cc
void xemu_snapshots_set_framebuffer_texture(unsigned int tex, bool flip);
void xemu_snapshots_render_thumbnail(unsigned int tex,
                                     TextureBuffer *thumbnail);
TextureBuffer *xemu_snapshots_extract_thumbnail(void);

#ifdef __cplusplus
}
#endif

#endif
