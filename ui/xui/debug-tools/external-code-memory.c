//
// xemu RAW Cheat Engine - External x86 code memory
//
// Host-backed guest executable storage for Type-F code caves. The backing
// pages are deliberately outside machine->ram_size. A single guest page-
// directory entry maps the private pages into a reserved linear range so
// WHPX/TCG execute the bytes as normal guest x86 instructions.
//

#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qemu/bswap.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "system/address-spaces.h"
#include "system/memory.h"

#include "cheat-engine-memory.h"
#include "backend/xemu-dbg.h"

#define XEMU_EXT_CODE_VIRT_BASE  0x68000000u
#define XEMU_EXT_CODE_PDE_INDEX  (XEMU_EXT_CODE_VIRT_BASE >> 22)
#define XEMU_EXT_CODE_PAGE_SIZE  0x00001000u
#define XEMU_EXT_CODE_PAGES      256u
#define XEMU_EXT_TOTAL_ARENA_SIZE (XEMU_EXT_CODE_PAGE_SIZE * XEMU_EXT_CODE_PAGES)
/* v0.1.62: keep the top 64 KiB permanently out of the executable/DD allocator.
 * It is guest-visible xemu-owned memory dedicated to PRESERVE frames. */
#define XEMU_EXT_CODE_ARENA_SIZE  0x000F0000u
#define XEMU_EXT_PRESERVE_OFFSET  XEMU_EXT_CODE_ARENA_SIZE
#define XEMU_EXT_PRESERVE_SIZE    0x00010000u
#define XEMU_EXT_PRESERVE_VIRT_BASE (XEMU_EXT_CODE_VIRT_BASE + XEMU_EXT_PRESERVE_OFFSET)
#define XEMU_EXT_CODE_ALIGNMENT  0x00000010u
#define XEMU_EXT_PT_BYTES        0x00001000u
#define XEMU_EXT_REGION_BYTES    0x00200000u
#define XEMU_EXT_CODE_OFFSET     XEMU_EXT_PT_BYTES

static MemoryRegion xemu_ext_code_region;
static bool xemu_ext_region_initialized;
static hwaddr xemu_ext_phys_base;
static uint32_t xemu_ext_mapped_cr3;

typedef struct XemuExtFreeBlock {
    uint32_t offset;
    uint32_t size;
} XemuExtFreeBlock;

/* Free-list allocator for the 1 MiB Type-F arena. Allocated caves never move.
 * Disabling a cave returns only that fixed block to this list; adjacent free
 * ranges are coalesced so a later, larger cave can reuse contiguous space
 * without relocating any still-active cave. */
static GArray *xemu_ext_free_blocks;
static GArray *xemu_ext_preserve_free_blocks;
static const char *xemu_ext_last_alloc_error;


static uint32_t xemu_ext_align_size(size_t size, uint32_t arena_size)
{
    if (size == 0 || size > arena_size) {
        return 0;
    }
    return ((uint32_t)size + XEMU_EXT_CODE_ALIGNMENT - 1u) &
           ~(XEMU_EXT_CODE_ALIGNMENT - 1u);
}

static int xemu_ext_reset_one_free_list(GArray **list, uint32_t offset,
                                        uint32_t size)
{
    XemuExtFreeBlock whole = { offset, size };
    if (*list == NULL) {
        *list = g_array_new(false, false, sizeof(XemuExtFreeBlock));
        if (*list == NULL) {
            return 0;
        }
    }
    g_array_set_size(*list, 0);
    g_array_append_val(*list, whole);
    return 1;
}

static int xemu_ext_reset_free_list(void)
{
    return xemu_ext_reset_one_free_list(&xemu_ext_free_blocks,
                                        0, XEMU_EXT_CODE_ARENA_SIZE) &&
           xemu_ext_reset_one_free_list(&xemu_ext_preserve_free_blocks,
                                        XEMU_EXT_PRESERVE_OFFSET,
                                        XEMU_EXT_PRESERVE_SIZE);
}

static int xemu_ext_find_first_fit(const GArray *list, uint32_t size,
                                   guint *index, uint32_t *offset)
{
    if (list == NULL || index == NULL || offset == NULL) {
        return 0;
    }

    for (guint i = 0; i < list->len; ++i) {
        const XemuExtFreeBlock *block =
            &g_array_index(list, XemuExtFreeBlock, i);
        if (block->size >= size) {
            *index = i;
            *offset = block->offset;
            return 1;
        }
    }
    return 0;
}

static void xemu_ext_consume_free_block(GArray *list, guint index, uint32_t size)
{
    XemuExtFreeBlock *block =
        &g_array_index(list, XemuExtFreeBlock, index);
    if (block->size == size) {
        g_array_remove_index(list, index);
    } else {
        block->offset += size;
        block->size -= size;
    }
}

static int xemu_ext_find_free_insert(const GArray *list, uint32_t offset,
                                     uint32_t size, guint *insert_at)
{
    const uint64_t end = (uint64_t)offset + size;
    guint position = 0;

    if (list == NULL || insert_at == NULL) {
        return 0;
    }

    for (guint i = 0; i < list->len; ++i) {
        const XemuExtFreeBlock *block =
            &g_array_index(list, XemuExtFreeBlock, i);
        const uint64_t block_end = (uint64_t)block->offset + block->size;
        if ((uint64_t)offset < block_end && end > (uint64_t)block->offset) {
            return 0;
        }
        if (block->offset < offset) {
            position = i + 1;
        }
    }

    *insert_at = position;
    return 1;
}

static void xemu_ext_insert_and_coalesce(GArray *list, guint insert_at,
                                         uint32_t offset, uint32_t size)
{
    XemuExtFreeBlock freed = { offset, size };
    g_array_insert_val(list, insert_at, freed);

    if (insert_at > 0) {
        XemuExtFreeBlock *prev =
            &g_array_index(list, XemuExtFreeBlock, insert_at - 1);
        XemuExtFreeBlock *cur =
            &g_array_index(list, XemuExtFreeBlock, insert_at);
        if ((uint64_t)prev->offset + prev->size == cur->offset) {
            prev->size += cur->size;
            g_array_remove_index(list, insert_at);
            --insert_at;
        }
    }

    if (insert_at + 1 < list->len) {
        XemuExtFreeBlock *cur =
            &g_array_index(list, XemuExtFreeBlock, insert_at);
        XemuExtFreeBlock *next =
            &g_array_index(list, XemuExtFreeBlock, insert_at + 1);
        if ((uint64_t)cur->offset + cur->size == next->offset) {
            cur->size += next->size;
            g_array_remove_index(list, insert_at + 1);
        }
    }
}

static const hwaddr xemu_ext_phys_candidates[] = {
    0x70000000u,
    0x60000000u,
    0x50000000u,
    0x40000000u,
    0x30000000u,
    0x20000000u,
};

static uint32_t xemu_ext_le32_load(const uint8_t bytes[4])
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static void xemu_ext_le32_store(uint8_t bytes[4], uint32_t value)
{
    bytes[0] = (uint8_t)(value & 0xffu);
    bytes[1] = (uint8_t)((value >> 8) & 0xffu);
    bytes[2] = (uint8_t)((value >> 16) & 0xffu);
    bytes[3] = (uint8_t)((value >> 24) & 0xffu);
}

static bool xemu_ext_phys_range_unused(hwaddr base, uint64_t size)
{
    MemoryRegionSection section = memory_region_find(get_system_memory(),
                                                     base, size);
    if (section.mr == NULL) {
        return true;
    }

    memory_region_unref(section.mr);
    return false;
}

static int xemu_ext_init_region(void)
{
    Error *local_err = NULL;
    size_t i;

    if (xemu_ext_region_initialized) {
        return 1;
    }
    if (current_machine == NULL) {
        return 0;
    }

    for (i = 0; i < G_N_ELEMENTS(xemu_ext_phys_candidates); ++i) {
        hwaddr candidate = xemu_ext_phys_candidates[i];
        if (candidate < current_machine->ram_size) {
            continue;
        }
        if (xemu_ext_phys_range_unused(candidate, XEMU_EXT_REGION_BYTES)) {
            xemu_ext_phys_base = candidate;
            break;
        }
    }

    if (xemu_ext_phys_base == 0) {
        error_report("xemu cheat external code: no unused physical aperture found");
        return 0;
    }

    if (!memory_region_init_ram(&xemu_ext_code_region, NULL,
                                "xemu.cheat-external-code",
                                XEMU_EXT_REGION_BYTES, &local_err)) {
        if (local_err != NULL) {
            error_report_err(local_err);
        }
        return 0;
    }

    /* The Xbox PCI hole aliases the otherwise-unused physical aperture. Put
     * the cheat-owned RAM above that alias only after verifying the selected
     * range has no active flat-view mapping. Memory listeners (including WHPX)
     * receive this dynamic RAM mapping normally. */
    memory_region_add_subregion_overlap(get_system_memory(), xemu_ext_phys_base,
                                        &xemu_ext_code_region, 1000);
    memset(memory_region_get_ram_ptr(&xemu_ext_code_region), 0,
           XEMU_EXT_REGION_BYTES);
    xemu_ext_region_initialized = true;
    return xemu_ext_reset_free_list();
}

static int xemu_ext_flush_guest_translation(void)
{
    /* Keep accelerator-specific checks out of this generic XUI translation
     * unit. CONFIG_WHPX is intentionally poisoned here by QEMU; the
     * target-specific debugger backend performs the appropriate WHPX CR3
     * reload or TCG software-TLB flush. */
    return xemu_dbg_flush_guest_translation();
}

static int xemu_ext_clear_backing_range(uint32_t arena_offset, uint32_t size)
{
    uint8_t *ram_ptr = memory_region_get_ram_ptr(&xemu_ext_code_region);
    memset(ram_ptr + XEMU_EXT_CODE_OFFSET + arena_offset, 0, size);
    memory_region_set_dirty(&xemu_ext_code_region,
                            XEMU_EXT_CODE_OFFSET + arena_offset, size);
    xemu_cheat_notify_code_patch();
    return xemu_ext_flush_guest_translation();
}

static int xemu_ext_map_current_cr3(void)
{
    XemuCheatX86Registers regs;
    uint8_t pde_bytes[4];
    uint32_t cr3;
    uint32_t pde_phys;
    uint32_t current_pde;
    uint32_t wanted_pde;
    uint8_t *ram_ptr;
    uint32_t *pt;
    uint32_t page;

    if (!xemu_ext_init_region() ||
        !xemu_cheat_get_x86_registers(&regs)) {
        return 0;
    }

    /* Xbox/Pentium III paging is ordinary 32-bit paging. Refuse PAE rather
     * than writing the wrong paging format if a future configuration changes
     * that architectural assumption. */
    if ((regs.cr0 & (1u << 31)) == 0 || (regs.cr4 & (1u << 5)) != 0) {
        return 0;
    }

    cr3 = regs.cr3 & 0xfffff000u;
    pde_phys = cr3 + XEMU_EXT_CODE_PDE_INDEX * 4u;
    if (address_space_read(&address_space_memory, pde_phys,
                           MEMTXATTRS_UNSPECIFIED, pde_bytes,
                           sizeof(pde_bytes)) != MEMTX_OK) {
        return 0;
    }

    current_pde = xemu_ext_le32_load(pde_bytes);
    wanted_pde = ((uint32_t)xemu_ext_phys_base & 0xfffff000u) | 0x007u;

    /* The CPU is allowed to set status bits such as PDE.A (bit 5) after the
     * Type-F mapping has been used. Comparing the complete PDE against the
     * pristine value therefore makes the second cave allocation look like a
     * conflicting guest mapping. Validate the page-table base and the mapping
     * bits we actually require, while deliberately ignoring CPU-maintained
     * status/available bits. */
    const bool pde_present = (current_pde & 0x001u) != 0;
    const bool pde_matches =
        pde_present &&
        (current_pde & 0xfffff000u) == (wanted_pde & 0xfffff000u) &&
        (current_pde & 0x007u) == 0x007u &&
        (current_pde & 0x080u) == 0; /* PS must remain clear: points to PT. */

    if (pde_present && !pde_matches) {
        /* 0x68000000 is the private Type-F arena base. Never replace a real
         * guest mapping if a title already uses this PDE. */
        return 0;
    }

    ram_ptr = memory_region_get_ram_ptr(&xemu_ext_code_region);
    pt = (uint32_t *)ram_ptr;
    if (xemu_ext_mapped_cr3 != cr3 || !pde_matches) {
        memset(ram_ptr, 0, XEMU_EXT_PT_BYTES);
        for (page = 0; page < XEMU_EXT_CODE_PAGES; ++page) {
            const uint32_t code_phys =
                (uint32_t)xemu_ext_phys_base + XEMU_EXT_CODE_OFFSET +
                page * XEMU_EXT_CODE_PAGE_SIZE;
            pt[page] = cpu_to_le32(code_phys | 0x007u);
        }

        xemu_ext_le32_store(pde_bytes, wanted_pde);
        if (address_space_write(&address_space_memory, pde_phys,
                                MEMTXATTRS_UNSPECIFIED, pde_bytes,
                                sizeof(pde_bytes)) != MEMTX_OK ||
            !xemu_ext_flush_guest_translation()) {
            return 0;
        }
        xemu_ext_mapped_cr3 = cr3;
    }

    return 1;
}

int xemu_cheat_external_code_allocate(size_t size, uint32_t *virtual_address)
{
    uint32_t aligned_size;
    guint i;

    xemu_ext_last_alloc_error = NULL;
    aligned_size = xemu_ext_align_size(size, XEMU_EXT_CODE_ARENA_SIZE);
    if (virtual_address == NULL || aligned_size == 0) {
        xemu_ext_last_alloc_error =
            "invalid Type-F cave size or allocation request";
        return 0;
    }
    if (!xemu_ext_map_current_cr3()) {
        xemu_ext_last_alloc_error =
            "0x68000000 guest mapping is unavailable or conflicts with an existing PDE";
        return 0;
    }
    if (xemu_ext_free_blocks == NULL && !xemu_ext_reset_free_list()) {
        xemu_ext_last_alloc_error =
            "external code/DD free-list initialization failed";
        return 0;
    }

    /* First-fit is intentional. A cave always receives one fixed block and is
     * never shifted later. When an earlier cave is freed, that hole becomes a
     * normal candidate for a future cave; coalescing in free() limits
     * fragmentation without touching active allocations. */
    uint32_t offset;
    if (xemu_ext_find_first_fit(xemu_ext_free_blocks, aligned_size, &i, &offset)) {
        xemu_ext_consume_free_block(xemu_ext_free_blocks, i, aligned_size);
        *virtual_address = XEMU_EXT_CODE_VIRT_BASE + offset;
        return 1;
    }

    xemu_ext_last_alloc_error =
        "960 KiB external code/DD arena is exhausted or too fragmented for this cave";
    error_report("xemu cheat external code: %s", xemu_ext_last_alloc_error);
    return 0;
}

const char *xemu_cheat_external_code_last_error(void)
{
    return xemu_ext_last_alloc_error;
}

int xemu_cheat_external_code_free(uint32_t virtual_address, size_t size)
{
    uint32_t arena_offset;
    uint32_t aligned_size;
    uint64_t end;
    guint insert_at = 0;

    aligned_size = xemu_ext_align_size(size, XEMU_EXT_CODE_ARENA_SIZE);
    if (aligned_size == 0 ||
        virtual_address < XEMU_EXT_CODE_VIRT_BASE ||
        virtual_address >= XEMU_EXT_CODE_VIRT_BASE + XEMU_EXT_CODE_ARENA_SIZE ||
        !xemu_ext_init_region()) {
        return 0;
    }

    arena_offset = virtual_address - XEMU_EXT_CODE_VIRT_BASE;
    if ((arena_offset & (XEMU_EXT_CODE_ALIGNMENT - 1u)) != 0) {
        return 0;
    }
    end = (uint64_t)arena_offset + aligned_size;
    if (end > XEMU_EXT_CODE_ARENA_SIZE) {
        return 0;
    }
    if (xemu_ext_free_blocks == NULL && !xemu_ext_reset_free_list()) {
        return 0;
    }

    /* Reject an overlap/double-free before clearing memory or mutating the
     * allocator. */
    if (!xemu_ext_find_free_insert(xemu_ext_free_blocks, arena_offset,
                                   aligned_size, &insert_at)) {
        return 0;
    }

    /* The caller only invokes free after the guest has been stopped and the
     * allocation is known unreachable (or its restored hook plus EIP/CALL
     * retirement checks prove it safe). Clear the retired bytes before this
     * block can be reused. */
    if (!xemu_ext_clear_backing_range(arena_offset, aligned_size)) {
        return 0;
    }

    xemu_ext_insert_and_coalesce(xemu_ext_free_blocks, insert_at,
                                 arena_offset, aligned_size);

    return 1;
}


int xemu_cheat_external_preserve_allocate(size_t size, uint32_t *virtual_address)
{
    uint32_t aligned_size;
    guint i;

    aligned_size = xemu_ext_align_size(size, XEMU_EXT_PRESERVE_SIZE);
    if (virtual_address == NULL || aligned_size == 0 ||
        !xemu_ext_map_current_cr3()) {
        return 0;
    }
    if (xemu_ext_preserve_free_blocks == NULL && !xemu_ext_reset_free_list()) {
        return 0;
    }

    uint32_t offset;
    if (xemu_ext_find_first_fit(xemu_ext_preserve_free_blocks, aligned_size,
                                &i, &offset)) {
        /* A newly assigned preserve block always begins completely clear. Do
         * this before mutating the free list, so a translation-sync failure
         * leaves allocator state unchanged. */
        if (!xemu_ext_clear_backing_range(offset, aligned_size)) {
            return 0;
        }

        xemu_ext_consume_free_block(xemu_ext_preserve_free_blocks, i,
                                    aligned_size);
        *virtual_address = XEMU_EXT_CODE_VIRT_BASE + offset;
        return 1;
    }

    error_report("xemu cheat external code: 64 KiB preservation arena exhausted or fragmented");
    return 0;
}

int xemu_cheat_external_preserve_free(uint32_t virtual_address, size_t size)
{
    uint32_t arena_offset;
    uint32_t aligned_size;
    uint64_t end;
    guint insert_at = 0;

    aligned_size = xemu_ext_align_size(size, XEMU_EXT_PRESERVE_SIZE);
    if (aligned_size == 0 ||
        virtual_address < XEMU_EXT_PRESERVE_VIRT_BASE ||
        virtual_address >= XEMU_EXT_CODE_VIRT_BASE + XEMU_EXT_TOTAL_ARENA_SIZE ||
        !xemu_ext_init_region()) {
        return 0;
    }

    arena_offset = virtual_address - XEMU_EXT_CODE_VIRT_BASE;
    if ((arena_offset & (XEMU_EXT_CODE_ALIGNMENT - 1u)) != 0) {
        return 0;
    }
    end = (uint64_t)arena_offset + aligned_size;
    if (arena_offset < XEMU_EXT_PRESERVE_OFFSET ||
        end > XEMU_EXT_TOTAL_ARENA_SIZE) {
        return 0;
    }
    if (xemu_ext_preserve_free_blocks == NULL && !xemu_ext_reset_free_list()) {
        return 0;
    }

    if (!xemu_ext_find_free_insert(xemu_ext_preserve_free_blocks, arena_offset,
                                   aligned_size, &insert_at)) {
        return 0;
    }

    if (!xemu_ext_clear_backing_range(arena_offset, aligned_size)) {
        return 0;
    }

    xemu_ext_insert_and_coalesce(xemu_ext_preserve_free_blocks, insert_at,
                                 arena_offset, aligned_size);
    return 1;
}

int xemu_cheat_external_code_write(uint32_t virtual_address, size_t offset,
                                   const void *data, size_t size)
{
    uint32_t arena_offset;
    uint8_t *ram_ptr;
    uint64_t region_offset;

    if (data == NULL || virtual_address < XEMU_EXT_CODE_VIRT_BASE ||
        virtual_address >= XEMU_EXT_CODE_VIRT_BASE + XEMU_EXT_TOTAL_ARENA_SIZE ||
        !xemu_ext_init_region()) {
        return 0;
    }

    arena_offset = virtual_address - XEMU_EXT_CODE_VIRT_BASE;
    if (offset > XEMU_EXT_TOTAL_ARENA_SIZE - arena_offset ||
        size > XEMU_EXT_TOTAL_ARENA_SIZE - arena_offset - offset) {
        return 0;
    }

    /* allocate() already establishes the guest page-table mapping. Write the
     * cheat-owned RAM backing directly rather than performing a second guest-
     * mapping/AddressSpace transaction for every cave fragment. This makes an
     * edit/reactivation deterministic while the Type-F install guard has the
     * guest paused, and avoids the old second-map failure between allocation
     * and cave write. */
    region_offset = (uint64_t)XEMU_EXT_CODE_OFFSET + arena_offset + offset;
    ram_ptr = memory_region_get_ram_ptr(&xemu_ext_code_region);
    memcpy(ram_ptr + region_offset, data, size);
    memory_region_set_dirty(&xemu_ext_code_region, region_offset, size);
    xemu_cheat_notify_code_patch();

    /* Reused cave addresses must not retain an old TCG translation; WHPX uses
     * the backend CR3 reload path for the equivalent guest-translation sync. */
    return xemu_ext_flush_guest_translation();
}

void xemu_cheat_external_code_reset_allocations(void)
{
    xemu_ext_mapped_cr3 = 0;
    if (xemu_ext_region_initialized) {
        uint8_t *ram_ptr = memory_region_get_ram_ptr(&xemu_ext_code_region);
        memset(ram_ptr + XEMU_EXT_CODE_OFFSET, 0, XEMU_EXT_TOTAL_ARENA_SIZE);
        memory_region_set_dirty(&xemu_ext_code_region, XEMU_EXT_CODE_OFFSET,
                                XEMU_EXT_TOTAL_ARENA_SIZE);
    }
    xemu_ext_reset_free_list();
}
