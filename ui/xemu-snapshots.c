/*
 * xemu User Interface
 *
 * Copyright (C) 2020-2025 Matt Borgerson
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

#include "xemu-snapshots.h"
#include "xemu-settings.h"
#include "xemu-xbe.h"

#include <SDL3/SDL.h>
#include <epoxy/gl.h>

#include "block/aio.h"
#include "block/block_int.h"
#include "block/qapi.h"
#include "block/qdict.h"
#include "block/block-io.h"
#include "migration/qemu-file.h"
#include "migration/snapshot.h"
#include "qapi/error.h"
#include "qapi/qapi-commands-block.h"
#include "system/runstate.h"

#include "ui/console.h"
#include "ui/input.h"

static QEMUSnapshotInfo *xemu_snapshots_metadata = NULL;
static XemuSnapshotData *xemu_snapshots_extra_data = NULL;
static int xemu_snapshots_len = 0;
static bool xemu_snapshots_dirty = true;

const char **g_snapshot_shortcut_index_key_map[] = {
    &g_config.general.snapshots.shortcuts.f5,
    &g_config.general.snapshots.shortcuts.f6,
    &g_config.general.snapshots.shortcuts.f7,
    &g_config.general.snapshots.shortcuts.f8,
};

static void xemu_snapshots_load_data(BlockDriverState *bs_ro,
                                     QEMUSnapshotInfo *info,
                                     XemuSnapshotData *data, Error **err)
{
    data->disc_path = NULL;
    data->xbe_title_name = NULL;
    data->gl_thumbnail = 0;

    int res = bdrv_snapshot_load_tmp(bs_ro, info->id_str, info->name, err);
    if (res < 0) {
        return;
    }

    uint32_t header[3];
    int64_t offset = 0;
    res = bdrv_load_vmstate(bs_ro, (uint8_t *)&header, offset, sizeof(header));
    if (res != sizeof(header)) {
        return;
    }
    offset += res;

    if (be32_to_cpu(header[0]) != XEMU_SNAPSHOT_DATA_MAGIC ||
        be32_to_cpu(header[1]) != XEMU_SNAPSHOT_DATA_VERSION) {
        return;
    }

    size_t size = be32_to_cpu(header[2]);
    uint8_t *buf = g_malloc(size);
    res = bdrv_load_vmstate(bs_ro, buf, offset, size);
    if (res != size) {
        g_free(buf);
        return;
    }

    assert(size >= 9);

    offset = 0;

    const size_t disc_path_size = be32_to_cpu(*(uint32_t *)&buf[offset]);
    offset += 4;

    if (disc_path_size) {
        data->disc_path = (char *)g_malloc(disc_path_size + 1);
        assert(size >= (offset + disc_path_size));
        memcpy(data->disc_path, &buf[offset], disc_path_size);
        data->disc_path[disc_path_size] = 0;
        offset += disc_path_size;
    }

    assert(size >= (offset + 4));
    const size_t xbe_title_name_size = buf[offset];
    offset += 1;

    if (xbe_title_name_size) {
        data->xbe_title_name = (char *)g_malloc(xbe_title_name_size + 1);
        assert(size >= (offset + xbe_title_name_size));
        memcpy(data->xbe_title_name, &buf[offset], xbe_title_name_size);
        data->xbe_title_name[xbe_title_name_size] = 0;
        offset += xbe_title_name_size;
    }

    const size_t thumbnail_size = be32_to_cpu(*(uint32_t *)&buf[offset]);
    offset += 4;

    if (thumbnail_size) {
        GLuint thumbnail;
        glGenTextures(1, &thumbnail);
        assert(size >= (offset + thumbnail_size));
        if (xemu_snapshots_load_png_to_texture(thumbnail, &buf[offset],
                                               thumbnail_size)) {
            data->gl_thumbnail = thumbnail;
        } else {
            glDeleteTextures(1, &thumbnail);
        }
        offset += thumbnail_size;
    }

    g_free(buf);
}

static void xemu_snapshots_all_load_data(QEMUSnapshotInfo **info,
                                         XemuSnapshotData **data,
                                         int snapshots_len, Error **err)
{
    BlockDriverState *bs_ro;
    QDict *opts = qdict_new();

    assert(info && data);

    if (*data) {
        for (int i = 0; i < xemu_snapshots_len; ++i) {
            g_free((*data)[i].xbe_title_name);
            if ((*data)[i].gl_thumbnail) {
                glDeleteTextures(1, &((*data)[i].gl_thumbnail));
            }
        }
        g_free(*data);
    }

    *data =
        (XemuSnapshotData *)g_malloc(sizeof(XemuSnapshotData) * snapshots_len);
    memset(*data, 0, sizeof(XemuSnapshotData) * snapshots_len);

    qdict_put_bool(opts, BDRV_OPT_READ_ONLY, true);
    bs_ro = bdrv_open(g_config.sys.files.hdd_path, NULL, opts,
                      BDRV_O_RO_WRITE_SHARE | BDRV_O_AUTO_RDONLY, err);
    if (!bs_ro) {
        return;
    }

    for (int i = 0; i < snapshots_len; ++i) {
        xemu_snapshots_load_data(bs_ro, (*info) + i, (*data) + i, err);
        if (*err) {
            break;
        }
    }

    bdrv_flush(bs_ro);
    bdrv_drain(bs_ro);
    assert(bs_ro->refcnt == 1);
    bdrv_unref(bs_ro);
    if (!(*err))
        xemu_snapshots_dirty = false;
}

int xemu_snapshots_list(QEMUSnapshotInfo **info, XemuSnapshotData **extra_data,
                        Error **err)
{
    BlockDriverState *bs;
    int snapshots_len;
    assert(err);

    if (!xemu_snapshots_dirty && xemu_snapshots_extra_data &&
        xemu_snapshots_metadata) {
        goto done;
    }

    if (xemu_snapshots_metadata)
        g_free(xemu_snapshots_metadata);

    bs = bdrv_all_find_vmstate_bs(NULL, false, NULL, err);
    if (!bs) {
        return -1;
    }

    snapshots_len = bdrv_snapshot_list(bs, &xemu_snapshots_metadata);
    xemu_snapshots_all_load_data(&xemu_snapshots_metadata,
                                 &xemu_snapshots_extra_data, snapshots_len,
                                 err);
    if (*err) {
        return -1;
    }

    xemu_snapshots_len = snapshots_len;

done:
    if (info) {
        *info = xemu_snapshots_metadata;
    }

    if (extra_data) {
        *extra_data = xemu_snapshots_extra_data;
    }

    return xemu_snapshots_len;
}

char *xemu_get_currently_loaded_disc_path(void)
{
    char *file = NULL;
    BlockInfoList *block_list, *info;

    block_list = qmp_query_block(NULL);
    
    for (info = block_list; info; info = info->next) {
        if (strcmp("ide0-cd1", info->value->device)) {
            continue;
        }

        if (info->value->inserted && info->value->inserted->node_name) {
            file = g_strdup(info->value->inserted->file);
        }
    }

    qapi_free_BlockInfoList(block_list);
    return file;
}

void xemu_snapshots_load(const char *vm_name, Error **err)
{
    bool vm_running = runstate_is_running();
    vm_stop(RUN_STATE_RESTORE_VM);
    if (load_snapshot(vm_name, NULL, false, NULL, err) && vm_running) {
        vm_start();
    }
}

void xemu_snapshots_save(const char *vm_name, Error **err)
{
    save_snapshot(vm_name, true, NULL, false, NULL, err);
}

void xemu_snapshots_delete(const char *vm_name, Error **err)
{
    delete_snapshot(vm_name, false, NULL, err);
}

void xemu_snapshots_save_extra_data(QEMUFile *f)
{
    char *path = xemu_get_currently_loaded_disc_path();
    size_t path_size = path ? strlen(path) : 0;

    size_t xbe_title_name_size = 0;
    char *xbe_title_name = NULL;
    struct xbe *xbe_data = xemu_get_xbe_info();
    if (xbe_data && xbe_data->cert) {
        glong items_written = 0;
        xbe_title_name = g_utf16_to_utf8(xbe_data->cert->m_title_name, 40, NULL, &items_written, NULL);
        if (xbe_title_name) {
            xbe_title_name_size = items_written;
        }
    }

    size_t thumbnail_size = 0;
    void *thumbnail_buf = xemu_snapshots_create_framebuffer_thumbnail_png(&thumbnail_size);

    qemu_put_be32(f, XEMU_SNAPSHOT_DATA_MAGIC);
    qemu_put_be32(f, XEMU_SNAPSHOT_DATA_VERSION);
    qemu_put_be32(f, 4 + path_size + 1 + xbe_title_name_size + 4 + thumbnail_size);

    qemu_put_be32(f, path_size);
    if (path_size) {
        qemu_put_buffer(f, (const uint8_t *)path, path_size);
        g_free(path);
    }

    qemu_put_byte(f, xbe_title_name_size);
    if (xbe_title_name_size) {
        qemu_put_buffer(f, (const uint8_t *)xbe_title_name, xbe_title_name_size);
        g_free(xbe_title_name);
    }

    qemu_put_be32(f, thumbnail_size);
    if (thumbnail_size) {
        qemu_put_buffer(f, (const uint8_t *)thumbnail_buf, thumbnail_size);
        g_free(thumbnail_buf);
    }

    xemu_snapshots_dirty = true;
}

bool xemu_snapshots_offset_extra_data(QEMUFile *f)
{
    unsigned int v;
    uint32_t version;
    uint32_t size;

    v = qemu_get_be32(f);
    if (v != XEMU_SNAPSHOT_DATA_MAGIC) {
        qemu_file_skip(f, -4);
        return true;
    }

    version = qemu_get_be32(f);
    (void)version;

    /* qemu_file_skip only works if you aren't skipping past internal buffer limit.
     * Unfortunately, it's not usable here.
     */
    size = qemu_get_be32(f);
    void *buf = g_malloc(size);
    qemu_get_buffer(f, buf, size);
    g_free(buf);

    return true;
}

void xemu_snapshots_mark_dirty(void)
{
    xemu_snapshots_dirty = true;
}
