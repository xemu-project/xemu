//
// xemu mounted-disc BlockBackend bridge
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#include "qemu/osdep.h"

#include "system/block-backend-global-state.h"
#include "system/block-backend-io.h"

#include "disc-block-io.h"

XemuDiscBlockHandle xemu_disc_block_by_name(const char *name)
{
    return (XemuDiscBlockHandle)blk_by_name(name);
}

bool xemu_disc_block_is_available(XemuDiscBlockHandle handle)
{
    BlockBackend *blk = (BlockBackend *)handle;
    return blk != NULL && blk_bs(blk) != NULL && blk_is_available(blk);
}

uintptr_t xemu_disc_block_identity(XemuDiscBlockHandle handle)
{
    BlockBackend *blk = (BlockBackend *)handle;
    return blk != NULL ? (uintptr_t)blk_bs(blk) : 0;
}

int64_t xemu_disc_block_get_length(XemuDiscBlockHandle handle)
{
    BlockBackend *blk = (BlockBackend *)handle;
    if (blk == NULL || blk_bs(blk) == NULL || !blk_is_available(blk)) {
        return -1;
    }
    return blk_getlength(blk);
}

bool xemu_disc_block_pread(XemuDiscBlockHandle handle, uint64_t offset,
                           void *buffer, size_t size)
{
    BlockBackend *blk = (BlockBackend *)handle;
    if (blk == NULL || buffer == NULL || offset > INT64_MAX ||
        size > INT64_MAX) {
        return false;
    }

    return blk_pread(blk, (int64_t)offset, (int64_t)size, buffer,
                     (BdrvRequestFlags)0) >= 0;
}
