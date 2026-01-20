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
#include <epoxy/gl.h>

#define XEMU_SNAPSHOT_DATA_MAGIC 0x78656d75 // 'xemu'
#define XEMU_SNAPSHOT_DATA_VERSION 1

#define XEMU_SNAPSHOT_THUMBNAIL_WIDTH 160
#define XEMU_SNAPSHOT_THUMBNAIL_HEIGHT 120

extern const char **g_snapshot_shortcut_index_key_map[];

typedef struct XemuSnapshotData {
    char *disc_path;
    char *xbe_title_name;
    GLuint gl_thumbnail;
} XemuSnapshotData;

// Implemented in xemu-snapshots.c
char *xemu_get_currently_loaded_disc_path(void);
int xemu_snapshots_list(QEMUSnapshotInfo **info, XemuSnapshotData **extra_data,
                        Error **err);
void xemu_snapshots_load(const char *vm_name, Error **err);
void xemu_snapshots_save(const char *vm_name, Error **err);
void xemu_snapshots_delete(const char *vm_name, Error **err);

void xemu_snapshots_save_extra_data(QEMUFile *f);
bool xemu_snapshots_offset_extra_data(QEMUFile *f);
void xemu_snapshots_mark_dirty(void);

// Implemented in xemu-thumbnail.cc
void xemu_snapshots_set_framebuffer_texture(GLuint tex, bool flip);
bool xemu_snapshots_load_png_to_texture(GLuint tex, void *buf, size_t size);
void *xemu_snapshots_create_framebuffer_thumbnail_png(size_t *size);

#ifdef __cplusplus
}
#endif

#endif
