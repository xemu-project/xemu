#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qemu/bswap.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "system/address-spaces.h"
#include "system/memory.h"

#include "cheat-engine-memory.h"
#include "backend/xemu-dbg.h"
#include "guest-kernel-rpc-memory.h"

#define XEMU_RPC_PAGE_SIZE       0x00001000u
#define XEMU_RPC_ARENA_SIZE      0x00010000u
#define XEMU_RPC_ARENA_PAGES     (XEMU_RPC_ARENA_SIZE / XEMU_RPC_PAGE_SIZE)
#define XEMU_RPC_PT_BYTES        0x00001000u
#define XEMU_RPC_ARENA_OFFSET    XEMU_RPC_PT_BYTES
#define XEMU_RPC_REGION_BYTES    0x00020000u

static const uint32_t xemu_rpc_virtual_candidates[] = {
    0x68400000u, 0x68800000u, 0x68c00000u, 0x69000000u,
    0x69400000u, 0x69800000u, 0x69c00000u, 0x6a000000u,
};

/* Offset these from the Type-F candidates (0x70000000, 0x60000000, ...).
 * The flat-view check is authoritative, so 64/128/256 MiB RAM and device
 * mappings are rejected rather than inferred from a fixed RAM assumption. */
static const hwaddr xemu_rpc_phys_candidates[] = {
    0x70400000u, 0x60400000u, 0x50400000u,
    0x40400000u, 0x30400000u, 0x20400000u,
};

static MemoryRegion xemu_rpc_region;
static bool xemu_rpc_region_initialized;
static bool xemu_rpc_mapping_active;
static hwaddr xemu_rpc_phys_base;
static uint32_t xemu_rpc_virtual_base;
static uint32_t xemu_rpc_mapped_cr3;
static uint32_t xemu_rpc_pde_phys;
static uint32_t xemu_rpc_wanted_pde;
static const char *xemu_rpc_error;

static uint32_t xemu_rpc_le32_load(const uint8_t p[4])
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void xemu_rpc_le32_store(uint8_t p[4], uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static bool xemu_rpc_phys_range_unused(hwaddr base, uint64_t size)
{
    MemoryRegionSection section = memory_region_find(get_system_memory(), base, size);
    if (section.mr == NULL) {
        return true;
    }
    memory_region_unref(section.mr);
    return false;
}

static int xemu_rpc_init_region(void)
{
    Error *local_err = NULL;
    if (xemu_rpc_region_initialized) {
        return 1;
    }
    if (current_machine == NULL) {
        xemu_rpc_error = "Xbox machine is not initialized";
        return 0;
    }
    for (size_t i = 0; i < G_N_ELEMENTS(xemu_rpc_phys_candidates); ++i) {
        const hwaddr candidate = xemu_rpc_phys_candidates[i];
        if (candidate < current_machine->ram_size) {
            continue;
        }
        if (xemu_rpc_phys_range_unused(candidate, XEMU_RPC_REGION_BYTES)) {
            xemu_rpc_phys_base = candidate;
            break;
        }
    }
    if (xemu_rpc_phys_base == 0) {
        xemu_rpc_error = "no unused physical aperture found for the Guest Kernel RPC arena";
        return 0;
    }
    if (!memory_region_init_ram(&xemu_rpc_region, NULL, "xemu.guest-kernel-rpc",
                                XEMU_RPC_REGION_BYTES, &local_err)) {
        if (local_err != NULL) {
            error_report_err(local_err);
        }
        xemu_rpc_error = "could not initialize Guest Kernel RPC RAM";
        return 0;
    }
    memory_region_add_subregion_overlap(get_system_memory(), xemu_rpc_phys_base,
                                        &xemu_rpc_region, 1000);
    memset(memory_region_get_ram_ptr(&xemu_rpc_region), 0, XEMU_RPC_REGION_BYTES);
    xemu_rpc_region_initialized = true;
    return 1;
}

static int xemu_rpc_install_mapping(void)
{
    XemuCheatX86Registers regs;
    uint8_t pde_bytes[4];
    uint8_t *ram_ptr;
    uint32_t *pt;

    if (!xemu_rpc_init_region() || !xemu_cheat_get_x86_registers(&regs)) {
        xemu_rpc_error = "could not read the current Xbox CPU/page-table state";
        return 0;
    }
    if ((regs.cr0 & (1u << 31)) == 0 || (regs.cr4 & (1u << 5)) != 0) {
        xemu_rpc_error = "Guest Kernel RPC requires normal non-PAE 32-bit paging";
        return 0;
    }

    const uint32_t cr3 = regs.cr3 & 0xfffff000u;
    uint32_t selected_base = 0;
    uint32_t selected_pde_phys = 0;
    for (size_t i = 0; i < G_N_ELEMENTS(xemu_rpc_virtual_candidates); ++i) {
        const uint32_t candidate = xemu_rpc_virtual_candidates[i];
        const uint32_t pde_phys = cr3 + (candidate >> 22) * 4u;
        if (address_space_read(&address_space_memory, pde_phys,
                               MEMTXATTRS_UNSPECIFIED, pde_bytes, 4) != MEMTX_OK) {
            continue;
        }
        if ((xemu_rpc_le32_load(pde_bytes) & 1u) == 0) {
            selected_base = candidate;
            selected_pde_phys = pde_phys;
            break;
        }
    }
    if (selected_base == 0) {
        xemu_rpc_error = "no unused candidate guest PDE is available for the RPC arena";
        return 0;
    }

    ram_ptr = memory_region_get_ram_ptr(&xemu_rpc_region);
    memset(ram_ptr, 0, XEMU_RPC_REGION_BYTES);
    pt = (uint32_t *)ram_ptr;
    for (uint32_t page = 0; page < XEMU_RPC_ARENA_PAGES; ++page) {
        const uint32_t phys = (uint32_t)xemu_rpc_phys_base +
                              XEMU_RPC_ARENA_OFFSET + page * XEMU_RPC_PAGE_SIZE;
        pt[page] = cpu_to_le32(phys | 0x007u);
    }
    memory_region_set_dirty(&xemu_rpc_region, 0, XEMU_RPC_REGION_BYTES);

    xemu_rpc_wanted_pde = ((uint32_t)xemu_rpc_phys_base & 0xfffff000u) | 0x007u;
    xemu_rpc_le32_store(pde_bytes, xemu_rpc_wanted_pde);
    if (address_space_write(&address_space_memory, selected_pde_phys,
                            MEMTXATTRS_UNSPECIFIED, pde_bytes, 4) != MEMTX_OK) {
        xemu_rpc_error = "could not install the private RPC page mapping";
        return 0;
    }

    /* Record ownership immediately after the PDE write. If accelerator
     * translation synchronization fails, acquire() can still roll this exact
     * mapping back. Never lose track of a guest page-table mutation. */
    xemu_rpc_virtual_base = selected_base;
    xemu_rpc_mapped_cr3 = cr3;
    xemu_rpc_pde_phys = selected_pde_phys;
    xemu_rpc_mapping_active = true;
    if (!xemu_dbg_flush_guest_translation()) {
        xemu_rpc_error = "private RPC PDE was written but translation synchronization failed";
        return 0;
    }
    return 1;
}

int xemu_guest_rpc_arena_acquire(XemuGuestRpcArenaInfo *info)
{
    xemu_rpc_error = NULL;
    if (info == NULL || xemu_rpc_mapping_active) {
        xemu_rpc_error = xemu_rpc_mapping_active
            ? "Guest Kernel RPC arena is already active"
            : "invalid Guest Kernel RPC arena request";
        return 0;
    }
    if (!xemu_rpc_install_mapping()) {
        const char *install_error = xemu_rpc_error;
        if (xemu_rpc_mapping_active) {
            if (!xemu_guest_rpc_arena_release()) {
                /* release() leaves ownership active when it cannot prove the
                 * PDE was cleared. The caller can therefore fail closed. */
                return 0;
            }
            xemu_rpc_error = install_error;
        }
        return 0;
    }
    info->virtual_base = xemu_rpc_virtual_base;
    info->physical_base = xemu_rpc_phys_base + XEMU_RPC_ARENA_OFFSET;
    info->size = XEMU_RPC_ARENA_SIZE;
    return 1;
}

int xemu_guest_rpc_arena_write(uint32_t offset, const void *data, size_t size)
{
    if (!xemu_rpc_mapping_active || data == NULL ||
        offset > XEMU_RPC_ARENA_SIZE || size > XEMU_RPC_ARENA_SIZE - offset) {
        xemu_rpc_error = "Guest Kernel RPC arena write is outside the active mapping";
        return 0;
    }
    uint8_t *ram_ptr = memory_region_get_ram_ptr(&xemu_rpc_region);
    memcpy(ram_ptr + XEMU_RPC_ARENA_OFFSET + offset, data, size);
    memory_region_set_dirty(&xemu_rpc_region, XEMU_RPC_ARENA_OFFSET + offset, size);
    xemu_cheat_notify_code_patch();
    if (!xemu_dbg_flush_guest_translation()) {
        xemu_rpc_error = "could not synchronize Guest Kernel RPC code/data writes";
        return 0;
    }
    return 1;
}

int xemu_guest_rpc_arena_read(uint32_t offset, void *data, size_t size)
{
    if (!xemu_rpc_mapping_active || data == NULL ||
        offset > XEMU_RPC_ARENA_SIZE || size > XEMU_RPC_ARENA_SIZE - offset) {
        xemu_rpc_error = "Guest Kernel RPC arena read is outside the active mapping";
        return 0;
    }
    const uint8_t *ram_ptr = memory_region_get_ram_ptr(&xemu_rpc_region);
    memcpy(data, ram_ptr + XEMU_RPC_ARENA_OFFSET + offset, size);
    return 1;
}

int xemu_guest_rpc_arena_release(void)
{
    xemu_rpc_error = NULL;
    if (!xemu_rpc_mapping_active) {
        return 1;
    }

    uint8_t pde_bytes[4] = {};
    bool cleared = false;
    XemuCheatX86Registers regs;
    if (xemu_cheat_get_x86_registers(&regs) &&
        (regs.cr3 & 0xfffff000u) == xemu_rpc_mapped_cr3 &&
        address_space_read(&address_space_memory, xemu_rpc_pde_phys,
                           MEMTXATTRS_UNSPECIFIED, pde_bytes, 4) == MEMTX_OK &&
        (xemu_rpc_le32_load(pde_bytes) & 0xfffff087u) ==
            (xemu_rpc_wanted_pde & 0xfffff087u)) {
        memset(pde_bytes, 0, sizeof(pde_bytes));
        if (address_space_write(&address_space_memory, xemu_rpc_pde_phys,
                                MEMTXATTRS_UNSPECIFIED, pde_bytes, 4) == MEMTX_OK &&
            xemu_dbg_flush_guest_translation()) {
            cleared = true;
        }
    }

    if (!cleared) {
        /* Keep the mapping marked active and keep its backing bytes intact.
         * That prevents a second RPC from reusing the arena and lets a later
         * cleanup attempt identify the exact PDE we still own. */
        xemu_rpc_error = "RPC completed, but its temporary guest PDE could not be safely cleared";
        return 0;
    }

    memset(memory_region_get_ram_ptr(&xemu_rpc_region), 0, XEMU_RPC_REGION_BYTES);
    memory_region_set_dirty(&xemu_rpc_region, 0, XEMU_RPC_REGION_BYTES);
    xemu_rpc_mapping_active = false;
    xemu_rpc_virtual_base = 0;
    xemu_rpc_mapped_cr3 = 0;
    xemu_rpc_pde_phys = 0;
    xemu_rpc_wanted_pde = 0;
    return 1;
}

int xemu_guest_rpc_arena_active(void)
{
    return xemu_rpc_mapping_active ? 1 : 0;
}

const char *xemu_guest_rpc_arena_last_error(void)
{
    return xemu_rpc_error;
}
