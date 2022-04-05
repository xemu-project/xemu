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

#include "xemu-snapshots.h"
#include "xemu-settings.h"
#include "xemu-xbe.h"

#include <SDL2/SDL.h>
#include <epoxy/gl.h>

#include "block/aio.h"
#include "block/block_int.h"
#include "block/qapi.h"
#include "block/qdict.h"
#include "migration/qemu-file.h"
#include "migration/snapshot.h"
#include "qapi/error.h"
#include "sysemu/runstate.h"
#include "qemu-common.h"

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

static bool xemu_snapshots_load_thumbnail(BlockDriverState *bs_ro,
                                          XemuSnapshotData *data,
                                          int64_t *offset)
{
    int res = bdrv_load_vmstate(bs_ro, (uint8_t *)&data->thumbnail, *offset,
                                sizeof(TextureBuffer) -
                                    sizeof(data->thumbnail.buffer));
    if (res != sizeof(TextureBuffer) - sizeof(data->thumbnail.buffer))
        return false;
    *offset += res;

    data->thumbnail.buffer = g_malloc(data->thumbnail.size);

    res = bdrv_load_vmstate(bs_ro, (uint8_t *)data->thumbnail.buffer, *offset,
                            data->thumbnail.size);
    if (res != data->thumbnail.size) {
        return false;
    }
    *offset += res;

    return true;
}

static void xemu_snapshots_load_data(BlockDriverState *bs_ro,
                                     QEMUSnapshotInfo *info,
                                     XemuSnapshotData *data, Error **err)
{
    int res;
    XemuSnapshotHeader header;
    int64_t offset = 0;

    data->xbe_title_present = false;
    data->thumbnail_present = false;
    res = bdrv_snapshot_load_tmp(bs_ro, info->id_str, info->name, err);
    if (res < 0)
        return;

    res = bdrv_load_vmstate(bs_ro, (uint8_t *)&header, offset, sizeof(header));
    if (res != sizeof(header))
        goto error;
    offset += res;

    if (header.magic != XEMU_SNAPSHOT_DATA_MAGIC)
        goto error;

    res = bdrv_load_vmstate(bs_ro, (uint8_t *)&data->xbe_title_len, offset,
                            sizeof(data->xbe_title_len));
    if (res != sizeof(data->xbe_title_len))
        goto error;
    offset += res;

    data->xbe_title = (char *)g_malloc(data->xbe_title_len);

    res = bdrv_load_vmstate(bs_ro, (uint8_t *)data->xbe_title, offset,
                            data->xbe_title_len);
    if (res != data->xbe_title_len)
        goto error;
    offset += res;

    data->xbe_title_present = (offset <= sizeof(header) + header.size);

    if (offset == sizeof(header) + header.size)
        return;

    if (!xemu_snapshots_load_thumbnail(bs_ro, data, &offset)) {
        goto error;
    }

    data->thumbnail_present = (offset <= sizeof(header) + header.size);

    if (data->thumbnail_present) {
        glGenTextures(1, &data->gl_thumbnail);
        xemu_snapshots_render_thumbnail(data->gl_thumbnail, &data->thumbnail);
    }
    return;

error:
    g_free(data->xbe_title);
    data->xbe_title_present = false;

    g_free(data->thumbnail.buffer);
    data->thumbnail_present = false;
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
            if ((*data)[i].xbe_title_present) {
                g_free((*data)[i].xbe_title);
            }

            if ((*data)[i].thumbnail_present) {
                g_free((*data)[i].thumbnail.buffer);
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
    bdrv_unref(bs_ro);
    assert(bs_ro->refcnt == 0);
    if (!(*err))
        xemu_snapshots_dirty = false;
}

int xemu_snapshots_list(QEMUSnapshotInfo **info, XemuSnapshotData **extra_data,
                        Error **err)
{
    BlockDriverState *bs;
    AioContext *aio_context;
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

    aio_context = bdrv_get_aio_context(bs);

    aio_context_acquire(aio_context);
    snapshots_len = bdrv_snapshot_list(bs, &xemu_snapshots_metadata);
    aio_context_release(aio_context);
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
    struct xbe *xbe_data = xemu_get_xbe_info();

    int64_t xbe_title_len = 0;
    char *xbe_title = g_utf16_to_utf8(xbe_data->cert->m_title_name, 40, NULL,
                                      &xbe_title_len, NULL);
    xbe_title_len++;

    XemuSnapshotHeader header = { XEMU_SNAPSHOT_DATA_MAGIC, 0 };

    header.size += sizeof(xbe_title_len);
    header.size += xbe_title_len;

    TextureBuffer *thumbnail = xemu_snapshots_extract_thumbnail();
    if (thumbnail && thumbnail->buffer) {
        header.size += sizeof(TextureBuffer) - sizeof(thumbnail->buffer);
        header.size += thumbnail->size;
    }

    qemu_put_buffer(f, (const uint8_t *)&header, sizeof(header));
    qemu_put_buffer(f, (const uint8_t *)&xbe_title_len, sizeof(xbe_title_len));
    qemu_put_buffer(f, (const uint8_t *)xbe_title, xbe_title_len);

    if (thumbnail && thumbnail->buffer) {
        qemu_put_buffer(f, (const uint8_t *)thumbnail,
                        sizeof(TextureBuffer) - sizeof(thumbnail->buffer));
        qemu_put_buffer(f, (const uint8_t *)thumbnail->buffer, thumbnail->size);
    }

    g_free(xbe_title);

    if (thumbnail && thumbnail->buffer) {
        g_free(thumbnail->buffer);
    }

    g_free(thumbnail);

    xemu_snapshots_dirty = true;
}

bool xemu_snapshots_offset_extra_data(QEMUFile *f)
{
    size_t ret;
    XemuSnapshotHeader header;
    ret = qemu_get_buffer(f, (uint8_t *)&header, sizeof(header));
    if (ret != sizeof(header)) {
        return false;
    }

    if (header.magic == XEMU_SNAPSHOT_DATA_MAGIC) {
        /*
         * qemu_file_skip only works if you aren't skipping past its buffer.
         * Unfortunately, it's not usable here.
         */
        void *buf = g_malloc(header.size);
        qemu_get_buffer(f, buf, header.size);
        g_free(buf);
    } else {
        qemu_file_skip(f, -((int)sizeof(header)));
    }

    return true;
}

void xemu_snapshots_mark_dirty(void)
{
    xemu_snapshots_dirty = true;
}
