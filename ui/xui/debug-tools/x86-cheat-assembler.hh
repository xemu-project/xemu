//
// xemu RAW Cheat Engine - compact 32-bit x86 assembler for Type-F0 caves
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct XemuCheatAsmLine {
    int source_line = 0;
    std::string text;
};

struct XemuCheatAsmResult {
    bool ok = false;
    int error_line = 0;
    std::string error;
    /* Executable bytes before DEADCODE's generated return JMP. */
    std::vector<uint8_t> bytes;
    /* Static DD data. InstallFHook places this immediately after the generated
     * 5-byte DEADCODE return JMP, and labels resolve to that final location. */
    std::vector<uint8_t> data;
    bool uses_preserve = false;
    /* One private preservation block per F0 hook. The block contains a small
     * header plus 16 nested 48-byte frames. */
    uint32_t preserve_bytes = 0;
    bool uses_temp = false;
    /* T0-T7 are eight persistent 32-bit F0-only virtual scratch registers.
     * v0.1.65 adds a private TFLAGS state plus one internal saved-EFLAGS slot.
     * Each T-using F0 therefore owns a 40-byte logical private bank (rounded
     * to the allocator's 16-byte boundary) in 0x680F0000-0x680FFFFF. */
    uint32_t temp_bytes = 0;
};

/* Assemble the intentionally-focused Intel-syntax subset used by RAW Cheat
 * Engine code caves. Numeric literals are hexadecimal by default (0x and h
 * forms are also accepted). JMP/Jcc/CALL targets may be internal labels or absolute hexadecimal addresses.
 *
 * v0.1.62 additions:
 *   Label: / DD value[, value...]  -> static data attached after DEADCODE JMP
 *   PRESERVEALL
 *   PRESERVE EAX, ECX, ...
 *   RESTORE [EAX, ECX, ...]
 *
 * v0.1.64 additions:
 *   T0-T7 persistent F0 virtual 32-bit scratch registers.
 *   Common forms include MOV T0,label, MOV T1,[T0], MOV reg,T0,
 *   MOV T0,reg, CMP/TEST with T registers, and arithmetic such as ADD T0,4.
 *
 * v0.1.65 additions:
 *   T-register flag isolation. Flag-producing operations involving T0-T7
 *   capture their complete arithmetic EFLAGS result into private TFLAGS and
 *   restore the guest's EFLAGS immediately. Following Jcc/SETcc/CMOVcc and
 *   LOOPcc instructions consume TFLAGS until a normal x86 flag-writing
 *   instruction takes ownership of architectural EFLAGS again.
 *
 * cave_base is the final guest virtual address of the executable cave and is
 * used to resolve absolute references such as "mov edx, CarList". A zero base
 * is valid for a sizing/probe pass. preserve_base is the private per-hook
 * preservation block in the reserved 0x680F0000-0x680FFFFF area; zero is valid
 * for a sizing/probe pass and produces equal-sized placeholder encodings.
 * temp_base is the private T0-T7/TFLAGS bank; zero is likewise valid for a probe pass. */
bool xemu_cheat_assemble_x86_32_at(const std::vector<XemuCheatAsmLine> &lines,
                                   uint32_t cave_base,
                                   uint32_t preserve_base,
                                   uint32_t temp_base,
                                   XemuCheatAsmResult &result);

/* Assemble one debugger Inject > Change instruction at its real guest virtual
 * address. Direct hexadecimal JMP/Jcc targets use the shortest legal encoding
 * that fits max_size (for example a nearby JNE/JMP can remain a 2-byte short
 * branch); all other syntax falls back to the normal Type-F0 assembler. */
bool xemu_cheat_assemble_x86_32_change_instruction(
    const std::string &instruction, uint32_t address, size_t max_size,
    XemuCheatAsmResult &result);

/* Backward-compatible probe/default entry point. */
bool xemu_cheat_assemble_x86_32(const std::vector<XemuCheatAsmLine> &lines,
                                XemuCheatAsmResult &result);
