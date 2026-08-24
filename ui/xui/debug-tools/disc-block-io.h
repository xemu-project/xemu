//
// xemu mounted-disc BlockBackend bridge
//
// Keeps QEMU block-layer C headers out of the C++ debugger/frontend sources.
//
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *XemuDiscBlockHandle;

XemuDiscBlockHandle xemu_disc_block_by_name(const char *name);
bool xemu_disc_block_is_available(XemuDiscBlockHandle handle);
uintptr_t xemu_disc_block_identity(XemuDiscBlockHandle handle);
int64_t xemu_disc_block_get_length(XemuDiscBlockHandle handle);
bool xemu_disc_block_pread(XemuDiscBlockHandle handle, uint64_t offset,
                           void *buffer, size_t size);

#ifdef __cplusplus
}
#endif
