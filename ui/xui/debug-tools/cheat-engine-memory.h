//
// xemu RAW Cheat Engine - QEMU memory/debug bridge
//
// This header intentionally exposes only plain C types so it is safe to
// include from the C++ ImGui cheat-engine frontend. QEMU's C-only headers
// remain isolated in cheat-engine-memory.c.
//
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XemuCheatX86Registers {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t esp;
    uint32_t ebp;
    uint32_t eip;
    uint32_t pc;
    uint32_t eflags;
    uint32_t cs;
    uint32_t ds;
    uint32_t es;
    uint32_t fs;
    uint32_t gs;
    uint32_t ss;
    uint32_t cr0;
    uint32_t cr2;
    uint32_t cr3;
    uint32_t cr4;
} XemuCheatX86Registers;

/* Extra IA-32 floating-point/SIMD state shown by the shared register-view
 * tabs for Current Registers and Last BP. x87 values are the architectural
 * ST0-ST7 logical stack order;
 * MMX values are the physical MM0-MM7 aliases; XMM values are four low-to-high
 * 32-bit lanes for each 128-bit register. */
typedef struct XemuCheatX86ExtraRegisters {
    uint64_t st_low[8];
    uint16_t st_high[8];
    uint64_t mmx[8];
    uint32_t xmm[8][4];
    uint32_t fctrl;
    uint32_t fstat;
    uint32_t fop;
    uint32_t mxcsr;
    uint8_t fp_top;
} XemuCheatX86ExtraRegisters;

typedef struct XemuCheatDisasmRow {
    uint32_t virtual_address;
    uint64_t physical_address;
    uint8_t physical_valid;
    uint8_t size;
    uint8_t bytes[15];
    char mnemonic[32];
    char operands[96];
} XemuCheatDisasmRow;

int xemu_cheat_cpu_available(void);
int xemu_cheat_memory_read(int is_virtual, uint32_t address,
                           void *buffer, size_t size);
int xemu_cheat_memory_write(int is_virtual, uint32_t address,
                            const void *buffer, size_t size);
/* Transactional executable-code write: pause a running guest, synchronize the
 * accelerator-backed CPU state, patch the virtual instruction bytes, force a
 * guest translation/code-fetch synchronization, then resume only if this
 * helper paused the VM. F0 hooks and debugger Inject operations use this path. */
int xemu_cheat_patch_virtual(uint32_t address, const void *buffer, size_t size);
/* Monotonic notification used by the x86 debugger to invalidate a displayed
 * disassembly when executable bytes/caves change outside the debugger pane. */
uint64_t xemu_cheat_code_patch_generation(void);
void xemu_cheat_notify_code_patch(void);
uint64_t xemu_cheat_ram_size(void);

/* Type-F external x86 code-cave storage. The backing pages are outside the
 * Xbox machine RAM allocation but are mapped into a private 1 MiB guest
 * virtual arena for normal WHPX/TCG execution. v0.1.62 reserves the top 64 KiB
 * for private PRESERVE/T0-T7/TFLAGS state, so normal F0/F1 code + DD allocations use the lower
 * 960 KiB at 16-byte boundaries; cheat authors never select a block. */
int xemu_cheat_external_code_allocate(size_t size, uint32_t *virtual_address);
const char *xemu_cheat_external_code_last_error(void);
int xemu_cheat_external_code_free(uint32_t virtual_address, size_t size);
int xemu_cheat_external_code_write(uint32_t virtual_address, size_t offset,
                                   const void *data, size_t size);
/* v0.1.62 private preservation storage. This allocator owns only the reserved
 * 0x680F0000-0x680FFFFF subrange and never overlaps executable/DD caves. */
int xemu_cheat_external_preserve_allocate(size_t size, uint32_t *virtual_address);
int xemu_cheat_external_preserve_free(uint32_t virtual_address, size_t size);
void xemu_cheat_external_code_reset_allocations(void);
/* Synchronize CPU state once before walking the guest virtual page map. */
int xemu_cheat_prepare_virtual_map(void);
/* Translate one guest virtual address. Returns 0 when unmapped. */
int xemu_cheat_virtual_to_physical(uint32_t address, uint64_t *physical_address);

/* Point-in-time snapshot of the guest page tables, restricted to installed Xbox
 * RAM. Unlike probing all 1,048,576 4 KiB virtual pages individually, this
 * walks the active x86 paging structures once and returns contiguous mappings.
 * The caller owns *mappings and must release it with
 * xemu_cheat_free_virtual_mappings(). */
typedef struct XemuCheatVirtualMapping {
    uint32_t virtual_start;
    uint64_t physical_start;
    uint64_t length;
} XemuCheatVirtualMapping;

int xemu_cheat_collect_ram_virtual_mappings(
    uint64_t ram_size, XemuCheatVirtualMapping **mappings, size_t *count);
void xemu_cheat_free_virtual_mappings(XemuCheatVirtualMapping *mappings);
int xemu_cheat_get_executable_dir(char *buffer, size_t buffer_size);

/* Xbox x86 debugger helpers. Execute breakpoints are guest virtual addresses. */
int xemu_cheat_get_x86_registers(XemuCheatX86Registers *registers);
int xemu_cheat_get_x86_extra_registers(XemuCheatX86ExtraRegisters *registers);
/* Write one live IA-32 register while the guest is paused. The register name
 * uses the same lowercase/uppercase-insensitive names exposed by QEMU's GDB
 * register table (eax, eip, eflags, cr3, etc.). */
int xemu_cheat_set_x86_register(const char *name, uint32_t value);
int xemu_cheat_breakpoint_insert(uint32_t address);
int xemu_cheat_breakpoint_remove(uint32_t address);

/* Native data watchpoints (TCG/WHPX/KVM guest-debug backends). */
enum {
    XEMU_CHEAT_WATCH_READ = 0x01,
    XEMU_CHEAT_WATCH_WRITE = 0x02,
    XEMU_CHEAT_WATCH_ACCESS = XEMU_CHEAT_WATCH_READ | XEMU_CHEAT_WATCH_WRITE,
};
typedef struct XemuCheatWatchpointHit {
    uint32_t watch_address;
    uint32_t hit_address;
    uint32_t length;
    int access_flags;
} XemuCheatWatchpointHit;
int xemu_cheat_guest_debug_supported(void);
int xemu_cheat_watchpoint_supported(void);
int xemu_cheat_watchpoint_access_supported(int access_flags);
int xemu_cheat_watchpoint_insert(uint32_t address, uint32_t length, int access_flags);
int xemu_cheat_watchpoint_remove(uint32_t address, uint32_t length, int access_flags);
enum {
    XEMU_CHEAT_WATCH_HIT_NONE = 0,
    XEMU_CHEAT_WATCH_HIT_REPORTED = 1,
};

int xemu_cheat_watchpoint_get_hit(XemuCheatWatchpointHit *hit);

enum {
    XEMU_CHEAT_DEBUG_BACKEND_OTHER = 0,
    XEMU_CHEAT_DEBUG_BACKEND_TCG = 1,
    XEMU_CHEAT_DEBUG_BACKEND_WHPX = 2,
    XEMU_CHEAT_DEBUG_BACKEND_KVM = 3,
    XEMU_CHEAT_DEBUG_BACKEND_HVF = 4,
};
/* Report the active accelerator so Resume can use the backend's native
 * breakpoint behavior where available (notably WHPX). */
int xemu_cheat_debug_backend(void);
/* Enable/disable architectural single-step for debugger Step/Continue. */
int xemu_cheat_single_step(int enabled);

/* Start a true one-instruction debugger step. This uses QEMU's step-aware
 * VM resume path so accelerators such as WHPX intercept the next debug trap. */
int xemu_cheat_start_single_step(void);
enum {
    XEMU_CHEAT_DISAS_OK = 1,
    XEMU_CHEAT_DISAS_ERROR = 0,
    XEMU_CHEAT_DISAS_UNMAPPED = -1,
    XEMU_CHEAT_DISAS_NO_BACKEND = -2,
};
/* Returns nonzero when the build contains the x86 decoder backend. */
int xemu_cheat_disassembler_available(void);
/* Decode one virtual instruction stream while also reporting the exact
 * physical backing address for each instruction start. */
int xemu_cheat_disassemble_paired(uint32_t address, int instruction_count,
                                  XemuCheatDisasmRow *rows, size_t row_capacity,
                                  size_t *row_count);
/* Decode exactly the 4 KiB virtual page containing address. This is used
 * by the debugger so a breakpoint can be shown with scrollable context both
 * before and after the stop location without crossing into another page. */
int xemu_cheat_disassemble_page(uint32_t address, XemuCheatDisasmRow *rows,
                                size_t row_capacity, size_t *row_count);
#ifdef __cplusplus
}
#endif
