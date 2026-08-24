//
// xemu RAW Cheat Engine - QEMU memory bridge
//
// Keep QEMU C headers in a C translation unit. Some QEMU headers rely on
// GNU C constructs and identifiers that are not valid when parsed as C++.
//

#include "qemu/osdep.h"
#include "qemu/accel.h"
#include "cheat-engine-memory.h"
#include "backend/xemu-dbg.h"

#include "hw/core/cpu.h"
#include "exec/target_page.h"
#include "hw/boards.h"
#include "qemu/cutils.h"
#include "qemu/atomic.h"
#include "system/address-spaces.h"
#include "system/hw_accel.h"
#include "system/memory.h"
#include "system/memory_mapping.h"
#include "qapi/error.h"
#include "system/runstate.h"
#include "system/cpus.h"
#include "exec/gdbstub.h"

/* xemu/QEMU uses Capstone for x86 monitor disassembly. Keep the optional
 * decoder API entirely on this C side of the bridge so none of those
 * headers leak into the ImGui C++ frontend. */
#include "disas/capstone.h"

int xemu_cheat_cpu_available(void)
{
    return qemu_get_cpu(0) != NULL;
}

int xemu_cheat_memory_read(int is_virtual, uint32_t address,
                           void *buffer, size_t size)
{
    if (is_virtual) {
        CPUState *cpu = qemu_get_cpu(0);
        if (cpu == NULL) {
            return 0;
        }
        return cpu_memory_rw_debug(cpu, (vaddr)address,
                                   buffer, size, false) == 0;
    }

    return address_space_read(&address_space_memory,
                              (hwaddr)address,
                              MEMTXATTRS_UNSPECIFIED,
                              buffer, size) == MEMTX_OK;
}

int xemu_cheat_memory_write(int is_virtual, uint32_t address,
                            const void *buffer, size_t size)
{
    if (is_virtual) {
        CPUState *cpu = qemu_get_cpu(0);
        if (cpu == NULL) {
            return 0;
        }
        return cpu_memory_rw_debug(cpu, (vaddr)address,
                                   (void *)buffer, size, true) == 0;
    }

    return address_space_write(&address_space_memory,
                               (hwaddr)address,
                               MEMTXATTRS_UNSPECIFIED,
                               buffer, size) == MEMTX_OK;
}

static uint64_t xemu_cheat_code_patch_generation_value;

uint64_t xemu_cheat_code_patch_generation(void)
{
    return qatomic_read(&xemu_cheat_code_patch_generation_value);
}

void xemu_cheat_notify_code_patch(void)
{
    qatomic_inc(&xemu_cheat_code_patch_generation_value);
}

int xemu_cheat_patch_virtual(uint32_t address, const void *buffer, size_t size)
{
    CPUState *cpu = qemu_get_cpu(0);
    int ok;
    const bool was_running = runstate_is_running();

    if (cpu == NULL || buffer == NULL || size == 0) {
        return 0;
    }
    if (was_running && vm_stop(RUN_STATE_PAUSED) != 0) {
        return 0;
    }

    /* WHPX/KVM/HVF keep architectural state in accelerator-private storage
     * while the vCPU runs. Synchronize after the stop so cpu_memory_rw_debug()
     * translates the virtual patch through the current guest CR3. */
    cpu_synchronize_state(cpu);
    ok = cpu_memory_rw_debug(cpu, (vaddr)address, (void *)buffer, size, true) == 0;
    if (ok) {
        /* A virtual instruction edit is more than an ordinary cheat-data
         * write. Force the backend's translation/code-fetch synchronization
         * before the guest can resume. The helper is deliberately best-effort
         * for accelerators without an explicit flush implementation: the write
         * itself keeps its historical success semantics. */
        xemu_cheat_notify_code_patch();
        (void)xemu_dbg_flush_guest_translation();
    }

    if (was_running) {
        vm_start();
    }
    return ok;
}

uint64_t xemu_cheat_ram_size(void)
{
    if (current_machine == NULL) {
        return 0;
    }
    return (uint64_t)current_machine->ram_size;
}

int xemu_cheat_prepare_virtual_map(void)
{
    CPUState *cpu = qemu_get_cpu(0);
    if (cpu == NULL) {
        return 0;
    }

    /* Match xemu-xbe.c's virtual-to-physical path. This is important for
     * hardware accelerators such as WHPX, where the architectural CPU state
     * may need to be synchronized before CR3/page-table inspection. */
    cpu_synchronize_state(cpu);
    return 1;
}

static int xemu_cheat_virtual_to_physical_cpu(
    CPUState *cpu, uint32_t address, uint64_t *physical_address)
{
    MemTxAttrs attrs;
    vaddr page;
    hwaddr physical;

    if (cpu == NULL || physical_address == NULL) {
        return 0;
    }

    /* cpu_get_phys_page_attrs_debug() returns the physical address of the
     * translated target page, not the exact byte address. Keep the 4 KiB
     * offset so debugger/breakpoint translations such as V 00579003 report
     * P 0087C003 instead of only P 0087C000. */
    page = (vaddr)address & TARGET_PAGE_MASK;
    physical = cpu_get_phys_page_attrs_debug(cpu, page, &attrs);
    if (physical == (hwaddr)-1) {
        return 0;
    }

    physical += (hwaddr)((vaddr)address - page);
    *physical_address = (uint64_t)physical;
    return 1;
}

int xemu_cheat_virtual_to_physical(uint32_t address, uint64_t *physical_address)
{
    return xemu_cheat_virtual_to_physical_cpu(qemu_get_cpu(0), address,
                                               physical_address);
}

static int xemu_cheat_virtual_mapping_compare(const void *lhs, const void *rhs)
{
    const XemuCheatVirtualMapping *a = lhs;
    const XemuCheatVirtualMapping *b = rhs;

    if (a->virtual_start < b->virtual_start) {
        return -1;
    }
    if (a->virtual_start > b->virtual_start) {
        return 1;
    }
    if (a->physical_start < b->physical_start) {
        return -1;
    }
    if (a->physical_start > b->physical_start) {
        return 1;
    }
    return 0;
}

int xemu_cheat_collect_ram_virtual_mappings(
    uint64_t ram_size, XemuCheatVirtualMapping **mappings, size_t *count)
{
    CPUState *cpu = qemu_get_cpu(0);
    MemoryMappingList list;
    MemoryMapping *mapping;
    XemuCheatVirtualMapping *result = NULL;
    size_t result_count = 0;
    Error *local_err = NULL;

    if (mappings == NULL || count == NULL || cpu == NULL ||
        ram_size == 0 || ram_size > UINT32_MAX) {
        return 0;
    }
    *mappings = NULL;
    *count = 0;

    /* The accelerator CPU state must be synchronized before the generic x86
     * page-table walker reads CR3/CR4, just like the single-address path. */
    cpu_synchronize_state(cpu);
    memory_mapping_list_init(&list);
    if (!cpu_get_memory_mapping(cpu, &list, &local_err)) {
        if (local_err != NULL) {
            error_free(local_err);
        }
        memory_mapping_list_free(&list);
        return 0;
    }

    /* cpu_get_memory_mapping() already tells us the maximum number of list
     * entries. Allocate once instead of growing the result one element at a
     * time with g_renew(); invalid/MMIO entries are simply skipped below. */
    if (list.num != 0) {
        result = g_new(XemuCheatVirtualMapping, list.num);
    }

    QTAILQ_FOREACH(mapping, &list.head, next) {
        uint64_t physical_start = (uint64_t)mapping->phys_addr;
        uint64_t virtual_start = (uint64_t)mapping->virt_addr;
        uint64_t length = (uint64_t)mapping->length;
        uint64_t max_virtual_length;
        uint64_t max_physical_length;

        /* Our Xbox debugger exposes the 32-bit guest virtual address space and
         * only RAM-backed mappings. Ignore MMIO and mappings outside installed
         * RAM, matching the old per-page filter. */
        if (length == 0 || virtual_start > UINT32_MAX ||
            physical_start >= ram_size) {
            continue;
        }
        max_virtual_length = 0x100000000ULL - virtual_start;
        max_physical_length = ram_size - physical_start;
        if (length > max_virtual_length) {
            length = max_virtual_length;
        }
        if (length > max_physical_length) {
            length = max_physical_length;
        }
        if (length == 0) {
            continue;
        }

        result[result_count].virtual_start = (uint32_t)virtual_start;
        result[result_count].physical_start = physical_start;
        result[result_count].length = length;
        result_count++;
    }
    memory_mapping_list_free(&list);

    if (result_count == 0) {
        g_free(result);
        result = NULL;
    }
    if (result_count > 1) {
        qsort(result, result_count, sizeof(*result),
              xemu_cheat_virtual_mapping_compare);
    }
    *mappings = result;
    *count = result_count;
    return 1;
}

void xemu_cheat_free_virtual_mappings(XemuCheatVirtualMapping *mappings)
{
    g_free(mappings);
}

static int xemu_cheat_find_gdb_register(const GArray *register_list,
                                        const char *name)
{
    guint i;

    if (register_list == NULL || name == NULL) {
        return -1;
    }

    for (i = 0; i < register_list->len; ++i) {
        const GDBRegDesc *desc = &g_array_index(register_list, GDBRegDesc, i);
        if (desc->name != NULL && g_ascii_strcasecmp(desc->name, name) == 0) {
            return desc->gdb_reg;
        }
    }
    return -1;
}

static int xemu_cheat_read_gdb_u32(CPUState *cpu,
                                    const GArray *register_list,
                                    GByteArray *bytes, const char *name,
                                    uint32_t *value)
{
    const int register_number =
        xemu_cheat_find_gdb_register(register_list, name);
    int read_size;

    if (cpu == NULL || bytes == NULL || value == NULL ||
        register_number < 0) {
        return 0;
    }

    /* gdb_read_register() appends to the supplied array. Reuse one byte
     * buffer for the complete register snapshot instead of allocating a new
     * GByteArray for every individual register. */
    g_byte_array_set_size(bytes, 0);
    read_size = gdb_read_register(cpu, bytes, register_number);
    if (read_size < 4 || bytes->len < 4) {
        return 0;
    }

    /* The Xbox target is IA-32/little-endian. gdb_read_register() returns
     * target-order bytes, so decode the low 32 bits explicitly instead of
     * depending on host endianness. This also remains safe if the enclosing
     * QEMU build happens to expose a wider register representation. */
    *value = ((uint32_t)bytes->data[0]) |
             ((uint32_t)bytes->data[1] << 8) |
             ((uint32_t)bytes->data[2] << 16) |
             ((uint32_t)bytes->data[3] << 24);
    return 1;
}

int xemu_cheat_set_x86_register(const char *name, uint32_t value)
{
    CPUState *cpu = qemu_get_cpu(0);
    GArray *register_list;
    int register_number;
    int write_size;
    uint8_t bytes[4];

    if (cpu == NULL || name == NULL || name[0] == '\0' ||
        runstate_is_running()) {
        return 0;
    }

    /* Pull accelerator state into CPUState first. WHPX marks vcpu_dirty while
     * synchronized, so a successful GDB-register write below is pushed back to
     * the virtual processor automatically before execution resumes. */
    cpu_synchronize_state(cpu);

    register_list = gdb_get_register_list(cpu);
    if (register_list == NULL) {
        return 0;
    }
    register_number = xemu_cheat_find_gdb_register(register_list, name);
    g_array_free(register_list, true);

    if (register_number < 0) {
        return 0;
    }

    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)((value >> 8) & 0xFFu);
    bytes[2] = (uint8_t)((value >> 16) & 0xFFu);
    bytes[3] = (uint8_t)((value >> 24) & 0xFFu);

    write_size = gdb_write_register(cpu, bytes, register_number);
    return write_size >= 4;
}

int xemu_cheat_get_x86_registers(XemuCheatX86Registers *registers)
{
    CPUState *cpu = qemu_get_cpu(0);
    GArray *register_list;
    GByteArray *bytes;
    uint32_t pc;
    int ok;

    if (cpu == NULL || registers == NULL) {
        return 0;
    }

    /* Keep accelerator-backed architectural state current before asking the
     * generic GDB register bridge to read it. Crucially, this file no longer
     * includes target/i386/cpu.h; target-specific register access stays in
     * QEMU's per-target gdbstub implementation. */
    cpu_synchronize_state(cpu);
    memset(registers, 0, sizeof(*registers));

    /* The register descriptor set is static for an initialized CPU. Build it
     * once for this complete snapshot and reuse one append buffer for all
     * values, rather than rebuilding both objects for each register. */
    register_list = gdb_get_register_list(cpu);
    if (register_list == NULL) {
        return 0;
    }
    bytes = g_byte_array_new();
    if (bytes == NULL) {
        g_array_free(register_list, true);
        return 0;
    }

    ok = xemu_cheat_read_gdb_u32(cpu, register_list, bytes, "eax",
                                 &registers->eax) &&
         xemu_cheat_read_gdb_u32(cpu, register_list, bytes, "ebx",
                                 &registers->ebx) &&
         xemu_cheat_read_gdb_u32(cpu, register_list, bytes, "ecx",
                                 &registers->ecx) &&
         xemu_cheat_read_gdb_u32(cpu, register_list, bytes, "edx",
                                 &registers->edx) &&
         xemu_cheat_read_gdb_u32(cpu, register_list, bytes, "esi",
                                 &registers->esi) &&
         xemu_cheat_read_gdb_u32(cpu, register_list, bytes, "edi",
                                 &registers->edi) &&
         xemu_cheat_read_gdb_u32(cpu, register_list, bytes, "esp",
                                 &registers->esp) &&
         xemu_cheat_read_gdb_u32(cpu, register_list, bytes, "ebp",
                                 &registers->ebp) &&
         xemu_cheat_read_gdb_u32(cpu, register_list, bytes, "eip",
                                 &registers->eip) &&
         xemu_cheat_read_gdb_u32(cpu, register_list, bytes, "eflags",
                                 &registers->eflags) &&
         xemu_cheat_read_gdb_u32(cpu, register_list, bytes, "cs",
                                 &registers->cs) &&
         xemu_cheat_read_gdb_u32(cpu, register_list, bytes, "ds",
                                 &registers->ds) &&
         xemu_cheat_read_gdb_u32(cpu, register_list, bytes, "es",
                                 &registers->es) &&
         xemu_cheat_read_gdb_u32(cpu, register_list, bytes, "fs",
                                 &registers->fs) &&
         xemu_cheat_read_gdb_u32(cpu, register_list, bytes, "gs",
                                 &registers->gs) &&
         xemu_cheat_read_gdb_u32(cpu, register_list, bytes, "ss",
                                 &registers->ss) &&
         xemu_cheat_read_gdb_u32(cpu, register_list, bytes, "cr0",
                                 &registers->cr0) &&
         xemu_cheat_read_gdb_u32(cpu, register_list, bytes, "cr2",
                                 &registers->cr2) &&
         xemu_cheat_read_gdb_u32(cpu, register_list, bytes, "cr3",
                                 &registers->cr3) &&
         xemu_cheat_read_gdb_u32(cpu, register_list, bytes, "cr4",
                                 &registers->cr4);

    g_byte_array_free(bytes, true);
    g_array_free(register_list, true);
    if (!ok) {
        return 0;
    }

    /* QEMU's generic CPUClass PC callback already returns linear PC
     * (CS.base + EIP) for i386, without requiring target/i386/cpu.h here. */
    if (cpu->cc != NULL && cpu->cc->get_pc != NULL) {
        pc = (uint32_t)cpu->cc->get_pc(cpu);
    } else {
        pc = registers->eip;
    }
    registers->pc = pc;
    return 1;
}

int xemu_cheat_get_x86_extra_registers(XemuCheatX86ExtraRegisters *registers)
{
    if (registers == NULL) {
        return 0;
    }
    memset(registers, 0, sizeof(*registers));
    return xemu_dbg_get_extra_registers(registers->st_low, registers->st_high,
                                        registers->mmx, registers->xmm,
                                        &registers->fctrl, &registers->fstat,
                                        &registers->fop, &registers->mxcsr,
                                        &registers->fp_top);
}

/* Breakpoint/watchpoint operations are delegated to the target-specific
 * Josh-style debugger backend. Keep this file as the plain C bridge consumed
 * by the C++ Memory Tools UI. */
int xemu_cheat_breakpoint_insert(uint32_t address)
{
    return xemu_dbg_bp_insert(address);
}

int xemu_cheat_breakpoint_remove(uint32_t address)
{
    return xemu_dbg_bp_remove(address);
}

int xemu_cheat_guest_debug_supported(void)
{
    return xemu_dbg_guest_debug_supported();
}

int xemu_cheat_watchpoint_supported(void)
{
    return xemu_dbg_wp_supported();
}

int xemu_cheat_watchpoint_access_supported(int access_flags)
{
    return xemu_dbg_wp_access_supported(access_flags);
}

int xemu_cheat_watchpoint_insert(uint32_t address, uint32_t length,
                                 int access_flags)
{
    return xemu_dbg_wp_insert(address, length, access_flags);
}

int xemu_cheat_watchpoint_remove(uint32_t address, uint32_t length,
                                 int access_flags)
{
    return xemu_dbg_wp_remove(address, length, access_flags);
}

int xemu_cheat_watchpoint_get_hit(XemuCheatWatchpointHit *hit)
{
    XemuDbgWatchpointHit backend_hit;
    int result;

    if (hit == NULL) {
        return XEMU_CHEAT_WATCH_HIT_NONE;
    }

    result = xemu_dbg_wp_get_hit(&backend_hit);
    if (result != XEMU_DBG_WATCH_HIT_REPORTED) {
        return result;
    }

    hit->watch_address = backend_hit.watch_address;
    hit->hit_address = backend_hit.hit_address;
    hit->length = backend_hit.length;
    hit->access_flags = backend_hit.access_flags;
    return XEMU_CHEAT_WATCH_HIT_REPORTED;
}

int xemu_cheat_debug_backend(void)
{
    const char *name;

    /* current_accel_name() assumes accelerator initialization is complete. */
    if (qemu_get_cpu(0) == NULL) {
        return XEMU_CHEAT_DEBUG_BACKEND_OTHER;
    }
    name = current_accel_name();
    if (name == NULL) {
        return XEMU_CHEAT_DEBUG_BACKEND_OTHER;
    }
    if (strcmp(name, "whpx") == 0) {
        return XEMU_CHEAT_DEBUG_BACKEND_WHPX;
    }
    if (strcmp(name, "tcg") == 0) {
        return XEMU_CHEAT_DEBUG_BACKEND_TCG;
    }
    if (strcmp(name, "kvm") == 0) {
        return XEMU_CHEAT_DEBUG_BACKEND_KVM;
    }
    if (strcmp(name, "hvf") == 0) {
        return XEMU_CHEAT_DEBUG_BACKEND_HVF;
    }
    return XEMU_CHEAT_DEBUG_BACKEND_OTHER;
}

int xemu_cheat_single_step(int enabled)
{
    CPUState *cpu = qemu_get_cpu(0);

    if (cpu == NULL || (enabled && !xemu_dbg_guest_debug_supported())) {
        return 0;
    }

    /* Explicit single-step is used for user Step Into on every backend.
     * TCG also uses it internally for ContinuePastBreakpoint because TCG
     * intentionally lets single-step override a breakpoint at the current PC.
     * WHPX Resume does not use this helper; WHPX performs its own native
     * breakpoint step-over in whpx_vcpu_run(). */
    cpu_single_step(cpu, enabled ? (SSTEP_ENABLE | SSTEP_NOIRQ |
                                   SSTEP_NOTIMER) : 0);
    qemu_cpu_kick(cpu);
    return 1;
}

int xemu_cheat_start_single_step(void)
{
    CPUState *cpu = qemu_get_cpu(0);

    if (cpu == NULL || !xemu_dbg_guest_debug_supported()) {
        return 0;
    }

    /* WHPX must be told before the vCPU resumes that a debugger step is
     * pending. vm_start() always passes step_pending=false, which leaves the
     * WHPX debug-trap exception intercept disabled when no other breakpoint
     * or watchpoint requires it. Mirror the QEMU gdbstub single-step startup:
     * prepare the VM with step_pending=true, arm CPU single-step, then resume. */
    if (vm_prepare_start(true) != 0) {
        return 0;
    }

    cpu_single_step(cpu, SSTEP_ENABLE | SSTEP_NOIRQ | SSTEP_NOTIMER);
    resume_all_vcpus();
    qemu_cpu_kick(cpu);
    return 1;
}

int xemu_cheat_disassembler_available(void)
{
#ifdef CONFIG_CAPSTONE
    return 1;
#else
    return 0;
#endif
}

#ifdef CONFIG_CAPSTONE
typedef struct XemuCheatDisasmContext {
    csh handle;
    cs_insn *insn;
    uint8_t *paired_page_bytes[2];
    uint8_t *page_bytes;
    size_t page_size;
} XemuCheatDisasmContext;

static void xemu_cheat_disasm_context_free(gpointer opaque)
{
    XemuCheatDisasmContext *context = opaque;

    if (context == NULL) {
        return;
    }
    g_free(context->paired_page_bytes[0]);
    g_free(context->paired_page_bytes[1]);
    g_free(context->page_bytes);
    if (context->insn != NULL) {
        cs_free(context->insn, 1);
    }
    if (context->handle != 0) {
        cs_close(&context->handle);
    }
    g_free(context);
}

/* Disassembly requests come from the UI/debugger path today, but keep the
 * decoder state thread-local rather than assuming that remains true forever.
 * Each calling thread pays cs_open()/cs_malloc() and page-buffer allocation
 * once, then reuses that state for subsequent refreshes and helper decodes. */
static GPrivate xemu_cheat_disasm_context_private =
    G_PRIVATE_INIT(xemu_cheat_disasm_context_free);

static XemuCheatDisasmContext *xemu_cheat_disasm_context_get(
    size_t page_size, int *error_result)
{
    XemuCheatDisasmContext *context =
        g_private_get(&xemu_cheat_disasm_context_private);

    *error_result = XEMU_CHEAT_DISAS_ERROR;
    if (context == NULL) {
        cs_err err;

        context = g_new0(XemuCheatDisasmContext, 1);
        err = cs_open(CS_ARCH_X86, CS_MODE_32, &context->handle);
        if (err != CS_ERR_OK) {
            g_free(context);
            *error_result = XEMU_CHEAT_DISAS_NO_BACKEND;
            return NULL;
        }
        cs_option(context->handle, CS_OPT_SYNTAX, CS_OPT_SYNTAX_INTEL);
        cs_option(context->handle, CS_OPT_SKIPDATA, CS_OPT_ON);
        context->insn = cs_malloc(context->handle);
        if (context->insn == NULL) {
            cs_close(&context->handle);
            g_free(context);
            return NULL;
        }
        g_private_set(&xemu_cheat_disasm_context_private, context);
    }

    /* TARGET_PAGE_SIZE is runtime-variable in generic QEMU builds. Grow or
     * resize the reusable scratch buffers if that size ever changes. */
    if (context->page_size != page_size) {
        context->paired_page_bytes[0] =
            g_realloc(context->paired_page_bytes[0], page_size);
        context->paired_page_bytes[1] =
            g_realloc(context->paired_page_bytes[1], page_size);
        context->page_bytes = g_realloc(context->page_bytes, page_size);
        context->page_size = page_size;
    }
    return context;
}

typedef struct XemuCheatDisasmPageCache {
    uint32_t page_base;
    uint64_t physical_base;
    uint8_t *bytes;
    uint8_t valid;
    uint8_t physical_valid;
} XemuCheatDisasmPageCache;

static int xemu_cheat_disasm_load_page(CPUState *cpu, uint32_t page_base,
                                       XemuCheatDisasmPageCache *cache)
{
    uint64_t physical = 0;

    if (cache->valid && cache->page_base == page_base) {
        return 1;
    }
    if (cache->bytes == NULL) {
        return 0;
    }

    /* Keep the backing page buffer for the lifetime of the paired
     * disassembly call.  Only invalidate the metadata when this cache slot
     * is replaced; clearing the whole struct would discard bytes. */
    cache->valid = 0;
    cache->physical_valid = 0;
    cache->physical_base = 0;
    cache->page_base = page_base;
    if (cpu_memory_rw_debug(cpu, (vaddr)page_base, cache->bytes,
                            TARGET_PAGE_SIZE, false) != 0) {
        return 0;
    }
    cache->valid = 1;
    if (xemu_cheat_virtual_to_physical_cpu(cpu, page_base, &physical)) {
        cache->physical_base = physical;
        cache->physical_valid = 1;
    }
    return 1;
}

static XemuCheatDisasmPageCache *xemu_cheat_disasm_get_page(
    CPUState *cpu, uint32_t page_base, XemuCheatDisasmPageCache caches[2],
    unsigned *replacement)
{
    unsigned i;

    for (i = 0; i < 2; ++i) {
        if (caches[i].valid && caches[i].page_base == page_base) {
            return &caches[i];
        }
    }
    i = *replacement & 1u;
    *replacement = (*replacement + 1u) & 1u;
    if (!xemu_cheat_disasm_load_page(cpu, page_base, &caches[i])) {
        return NULL;
    }
    return &caches[i];
}

/* Build the <=15-byte x86 decode window from page-sized cached reads.  The
 * old implementation performed up to fifteen cpu_memory_rw_debug() calls per
 * instruction; this keeps identical partial-window semantics at an unmapped
 * page boundary while normally reading each 4 KiB page only once. */
static size_t xemu_cheat_disasm_window(
    CPUState *cpu, uint64_t pc, uint8_t code[15],
    XemuCheatDisasmPageCache caches[2], unsigned *replacement,
    uint64_t *physical, int *physical_valid)
{
    size_t available = 0;

    *physical = 0;
    *physical_valid = 0;
    while (available < 15 && pc + available <= UINT32_MAX) {
        uint32_t address = (uint32_t)(pc + available);
        uint32_t page_base = address & (uint32_t)TARGET_PAGE_MASK;
        size_t page_offset = (size_t)(address - page_base);
        size_t amount = MIN((size_t)15 - available,
                            (size_t)TARGET_PAGE_SIZE - page_offset);
        XemuCheatDisasmPageCache *cache = xemu_cheat_disasm_get_page(
            cpu, page_base, caches, replacement);
        if (cache == NULL) {
            break;
        }
        if (available == 0 && cache->physical_valid) {
            *physical = cache->physical_base + page_offset;
            *physical_valid = 1;
        }
        memcpy(code + available, cache->bytes + page_offset, amount);
        available += amount;
    }
    return available;
}
#endif

int xemu_cheat_disassemble_paired(uint32_t address, int instruction_count,
                                  XemuCheatDisasmRow *rows, size_t row_capacity,
                                  size_t *row_count)
{
    CPUState *cpu = qemu_get_cpu(0);

    if (row_count != NULL) {
        *row_count = 0;
    }
    if (rows == NULL || row_capacity == 0 || row_count == NULL ||
        cpu == NULL || instruction_count <= 0) {
        return XEMU_CHEAT_DISAS_ERROR;
    }

    cpu_synchronize_state(cpu);

#ifndef CONFIG_CAPSTONE
    {
        uint8_t probe;
        if (cpu_memory_rw_debug(cpu, (vaddr)address, &probe, 1, false) != 0) {
            return XEMU_CHEAT_DISAS_UNMAPPED;
        }
    }
    return XEMU_CHEAT_DISAS_NO_BACKEND;
#else
    {
        uint64_t pc = address;
        int i;
        size_t produced = 0;
        const size_t page_size = (size_t)TARGET_PAGE_SIZE;
        int context_error = XEMU_CHEAT_DISAS_ERROR;
        XemuCheatDisasmContext *context =
            xemu_cheat_disasm_context_get(page_size, &context_error);
        XemuCheatDisasmPageCache caches[2] = {0};
        unsigned replacement = 0;

        if (context == NULL) {
            return context_error;
        }
        caches[0].bytes = context->paired_page_bytes[0];
        caches[1].bytes = context->paired_page_bytes[1];

        for (i = 0; i < instruction_count && produced < row_capacity &&
                    pc <= UINT32_MAX; ++i) {
            uint8_t code[15];
            uint64_t physical = 0;
            int physical_valid = 0;
            size_t available = xemu_cheat_disasm_window(
                cpu, pc, code, caches, &replacement,
                &physical, &physical_valid);
            XemuCheatDisasmRow *row = &rows[produced];
            const uint8_t *cursor;
            size_t remaining;
            uint64_t iter_pc;
            bool decoded;

            if (available == 0) {
                if (i == 0) {
                    return XEMU_CHEAT_DISAS_UNMAPPED;
                }
                break;
            }

            memset(row, 0, sizeof(*row));
            row->virtual_address = (uint32_t)pc;
            if (physical_valid) {
                row->physical_address = physical;
                row->physical_valid = 1;
            }

            cursor = code;
            remaining = available;
            iter_pc = pc;
            decoded = cs_disasm_iter(context->handle, &cursor, &remaining,
                                     &iter_pc, context->insn);
            if (!decoded || context->insn->size == 0 ||
                context->insn->size > available) {
                row->size = 1;
                row->bytes[0] = code[0];
                g_strlcpy(row->mnemonic, "db", sizeof(row->mnemonic));
                g_snprintf(row->operands, sizeof(row->operands),
                           "0x%02X", code[0]);
                ++pc;
            } else {
                row->size = (uint8_t)MIN(context->insn->size,
                                         sizeof(row->bytes));
                memcpy(row->bytes, context->insn->bytes, row->size);
                g_strlcpy(row->mnemonic, context->insn->mnemonic,
                          sizeof(row->mnemonic));
                g_strlcpy(row->operands, context->insn->op_str,
                          sizeof(row->operands));
                pc += context->insn->size;
            }
            ++produced;
        }

        *row_count = produced;
        return produced != 0 ? XEMU_CHEAT_DISAS_OK
                             : XEMU_CHEAT_DISAS_ERROR;
    }
#endif
}

int xemu_cheat_disassemble_page(uint32_t address, XemuCheatDisasmRow *rows,
                                size_t row_capacity, size_t *row_count)
{
    CPUState *cpu = qemu_get_cpu(0);
    const uint32_t page_base = address & (uint32_t)TARGET_PAGE_MASK;
    const size_t page_size = (size_t)TARGET_PAGE_SIZE;

    if (row_count != NULL) {
        *row_count = 0;
    }
    if (rows == NULL || row_capacity == 0 || row_count == NULL || cpu == NULL) {
        return XEMU_CHEAT_DISAS_ERROR;
    }

    cpu_synchronize_state(cpu);

#ifndef CONFIG_CAPSTONE
    {
        uint8_t *page_bytes = g_malloc(page_size);
        int read_ok = cpu_memory_rw_debug(cpu, (vaddr)page_base, page_bytes,
                                          page_size, false) == 0;
        g_free(page_bytes);
        if (!read_ok) {
            return XEMU_CHEAT_DISAS_UNMAPPED;
        }
    }
    return XEMU_CHEAT_DISAS_NO_BACKEND;
#else
    {
        int context_error = XEMU_CHEAT_DISAS_ERROR;
        XemuCheatDisasmContext *context =
            xemu_cheat_disasm_context_get(page_size, &context_error);
        size_t offset = 0;
        size_t produced = 0;
        uint64_t physical_page = 0;
        int physical_page_valid;

        if (context == NULL) {
            return context_error;
        }
        if (cpu_memory_rw_debug(cpu, (vaddr)page_base, context->page_bytes,
                                page_size, false) != 0) {
            return XEMU_CHEAT_DISAS_UNMAPPED;
        }
        physical_page_valid =
            xemu_cheat_virtual_to_physical_cpu(cpu, page_base,
                                                &physical_page);

        while (offset < page_size && produced < row_capacity) {
            const uint64_t pc = (uint64_t)page_base + offset;
            const size_t available = MIN((size_t)15, page_size - offset);
            XemuCheatDisasmRow *row = &rows[produced];
            const uint8_t *cursor = context->page_bytes + offset;
            size_t remaining = available;
            uint64_t iter_pc = pc;
            bool decoded;

            memset(row, 0, sizeof(*row));
            row->virtual_address = (uint32_t)pc;
            if (physical_page_valid) {
                row->physical_address = physical_page + offset;
                row->physical_valid = 1;
            }

            decoded = cs_disasm_iter(context->handle, &cursor, &remaining,
                                     &iter_pc, context->insn);
            /* The page start is not guaranteed to be an x86 instruction
             * boundary. Never let a speculative decode cross the exact focus
             * address; resynchronize there exactly as the previous path did. */
            if (decoded && context->insn->size != 0 &&
                pc < address && pc + context->insn->size > address) {
                decoded = false;
            }

            if (!decoded || context->insn->size == 0 ||
                context->insn->size > available) {
                row->size = 1;
                row->bytes[0] = context->page_bytes[offset];
                g_strlcpy(row->mnemonic, "db", sizeof(row->mnemonic));
                g_snprintf(row->operands, sizeof(row->operands),
                           "0x%02X", context->page_bytes[offset]);
                ++offset;
            } else {
                row->size = (uint8_t)MIN(context->insn->size,
                                         sizeof(row->bytes));
                memcpy(row->bytes, context->insn->bytes, row->size);
                g_strlcpy(row->mnemonic, context->insn->mnemonic,
                          sizeof(row->mnemonic));
                g_strlcpy(row->operands, context->insn->op_str,
                          sizeof(row->operands));
                offset += context->insn->size;
            }
            ++produced;
        }

        *row_count = produced;
        return produced != 0 ? XEMU_CHEAT_DISAS_OK
                             : XEMU_CHEAT_DISAS_ERROR;
    }
#endif
}

int xemu_cheat_get_executable_dir(char *buffer, size_t buffer_size)
{
    char *path;
    size_t path_len;

    if (buffer == NULL || buffer_size == 0) {
        return 0;
    }
    buffer[0] = '\0';

    /* qemu_init_exec_dir() is called during normal xemu startup. Asking
     * QEMU to relocate CONFIG_BINDIR therefore gives us the directory that
     * actually contains the running xemu executable, independent of the
     * process working directory. */
    path = get_relocated_path(CONFIG_BINDIR);
    if (path == NULL || path[0] == '\0') {
        g_free(path);
        return 0;
    }

    path_len = strlen(path);
    if (path_len + 1 > buffer_size) {
        g_free(path);
        return 0;
    }

    memcpy(buffer, path, path_len + 1);
    g_free(path);
    return 1;
}

