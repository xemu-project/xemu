/*
 * Block layer qmp and info dump related functions
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef BLOCK_QAPI_H
#define BLOCK_QAPI_H

#include "block/graph-lock.h"
#include "block/snapshot.h"
#include "qapi/qapi-types-block-core.h"

BlockDeviceInfo * GRAPH_RDLOCK
bdrv_block_device_info(BlockBackend *blk, BlockDriverState *bs,
                       bool flat, Error **errp);

int GRAPH_RDLOCK
bdrv_query_snapshot_info_list(BlockDriverState *bs,
                              SnapshotInfoList **p_list,
                              Error **errp);
void GRAPH_RDLOCK
bdrv_query_image_info(BlockDriverState *bs, ImageInfo **p_info, bool flat,
                      bool skip_implicit_filters, Error **errp);
void GRAPH_RDLOCK
bdrv_query_block_graph_info(BlockDriverState *bs, BlockGraphInfo **p_info,
                            Error **errp);

void bdrv_snapshot_dump(QEMUSnapshotInfo *sn);
void bdrv_image_info_specific_dump(ImageInfoSpecific *info_spec,
                                   const char *prefix,
                                   int indentation);
void bdrv_node_info_dump(BlockNodeInfo *info, int indentation, bool protocol);
#endif
