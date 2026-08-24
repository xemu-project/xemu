#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XemuGuestRpcArenaInfo {
    uint32_t virtual_base;
    uint64_t physical_base;
    uint32_t size;
} XemuGuestRpcArenaInfo;

/* One-at-a-time private guest RPC mapping. It owns a different page-table
 * entry and physical MemoryRegion from the Type-F/CodeCave allocator. */
int xemu_guest_rpc_arena_acquire(XemuGuestRpcArenaInfo *info);
int xemu_guest_rpc_arena_write(uint32_t offset, const void *data, size_t size);
int xemu_guest_rpc_arena_read(uint32_t offset, void *data, size_t size);
int xemu_guest_rpc_arena_release(void);
int xemu_guest_rpc_arena_active(void);
const char *xemu_guest_rpc_arena_last_error(void);

#ifdef __cplusplus
}
#endif
