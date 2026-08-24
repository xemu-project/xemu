//
// xemu RAW Cheat Engine - compact 32-bit x86 assembler for Type-F0 caves
//
// This is deliberately not a full general-purpose assembler.  It implements
// the integer/control-flow subset most useful for Xbox cheat code caves and
// fails explicitly for syntax/instructions it does not understand.
//

#include "x86-cheat-assembler.hh"
#include "x86-cheat-assembler-internal.hh"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>


using namespace xemu_cheat_assembler_internal;

namespace {

struct ParsedLine {
    int source_line = 0;
    std::string instruction;
    size_t offset = 0;
    size_t size = 0;
};

static std::string hex32(uint32_t value)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%08X", value);
    return buf;
}

/* F0 virtual temporary registers. T0-T7 are not architectural x86 registers;
 * each one is a persistent 32-bit slot in a private per-F0 bank. The text
 * pre-expander below lowers T operands to ordinary IA-32 instructions, so the
 * main assembler never needs a second register file. */
static constexpr uint32_t kTempRegisterCount = 8u;
static constexpr uint32_t kTempFlagsOffset = kTempRegisterCount * 4u;
static constexpr uint32_t kTempSavedGameFlagsOffset = kTempFlagsOffset + 4u;
/* T0-T7 + private TFLAGS + one internal saved-game-EFLAGS scratch slot.
 * External-state allocations are 16-byte aligned, so the 40-byte logical
 * bank occupies 48 bytes in the reserved private-state allocator. */
static constexpr uint32_t kTempBankSize = kTempSavedGameFlagsOffset + 4u;

static bool parse_temp_register(const std::string &text, unsigned &index)
{
    const std::string n = upper(trim(text));
    if (n.size() != 2 || n[0] != 'T' || n[1] < '0' || n[1] > '7') {
        return false;
    }
    index = (unsigned)(n[1] - '0');
    return true;
}

static bool parse_temp_indirect(std::string text, unsigned &index)
{
    text = trim(text);
    std::string u = upper(text);
    static const char *const prefixes[] = {
        "DWORD PTR ",
    };
    for (const char *prefix : prefixes) {
        const std::string p = prefix;
        if (u.rfind(p, 0) == 0) {
            text = trim(text.substr(p.size()));
            u = upper(text);
            break;
        }
    }
    if (text.size() < 3 || text.front() != '[' || text.back() != ']') {
        return false;
    }
    return parse_temp_register(text.substr(1, text.size() - 2), index);
}

static std::string temp_slot(uint32_t temp_base, unsigned index)
{
    return "dword ptr [" + hex32(temp_base + (uint32_t)index * 4u) + "]";
}

static std::string temp_flags_slot(uint32_t temp_base)
{
    return "dword ptr [" + hex32(temp_base + kTempFlagsOffset) + "]";
}

static std::string temp_saved_game_flags_slot(uint32_t temp_base)
{
    return "dword ptr [" + hex32(temp_base + kTempSavedGameFlagsOffset) + "]";
}

/* Instructions whose arithmetic/condition result belongs to the active flag
 * domain. For T-register instructions this result is captured into private
 * TFLAGS and the guest's architectural EFLAGS is restored immediately. */
static bool instruction_writes_condition_flags(const std::string &mnemonic)
{
    return mnemonic == "ADD" || mnemonic == "ADC" || mnemonic == "SUB" ||
           mnemonic == "SBB" || mnemonic == "CMP" || mnemonic == "TEST" ||
           mnemonic == "AND" || mnemonic == "OR" || mnemonic == "XOR" ||
           mnemonic == "INC" || mnemonic == "DEC" || mnemonic == "NEG" ||
           mnemonic == "ROL" || mnemonic == "ROR" || mnemonic == "RCL" ||
           mnemonic == "RCR" || mnemonic == "SHL" || mnemonic == "SAL" ||
           mnemonic == "SHR" || mnemonic == "SAR" || mnemonic == "SHLD" ||
           mnemonic == "SHRD" || mnemonic == "BT" || mnemonic == "BTS" ||
           mnemonic == "BTR" || mnemonic == "BTC" || mnemonic == "BSF" ||
           mnemonic == "BSR" || mnemonic == "IMUL" || mnemonic == "MUL" ||
           mnemonic == "CMPXCHG" || mnemonic == "XADD" ||
           mnemonic == "POPF" || mnemonic == "POPFD" ||
           mnemonic == "SAHF" || mnemonic == "CLC" ||
           mnemonic == "STC" || mnemonic == "CMC";
}

static bool instruction_is_flag_condition_consumer(const std::string &mnemonic)
{
    uint8_t cc = 0;
    if (!mnemonic.empty() && mnemonic[0] == 'J' &&
        mnemonic != "JMP" && mnemonic != "JECXZ" &&
        condition_code(mnemonic.substr(1), cc)) {
        return true;
    }
    if (mnemonic.rfind("SET", 0) == 0 && mnemonic.size() > 3 &&
        condition_code(mnemonic.substr(3), cc)) {
        return true;
    }
    if (mnemonic.rfind("CMOV", 0) == 0 && mnemonic.size() > 4 &&
        condition_code(mnemonic.substr(4), cc)) {
        return true;
    }
    return mnemonic == "LOOPE" || mnemonic == "LOOPZ" ||
           mnemonic == "LOOPNE" || mnemonic == "LOOPNZ";
}

static int real_reg32_code(const std::string &text)
{
    int code = -1;
    int width = 0;
    return parse_register(text, code, width) && width == 32 ? code : -1;
}

static const char *reg32_name(int code)
{
    static const char *const names[] = {
        "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
    };
    return code >= 0 && code < 8 ? names[code] : "eax";
}

static int choose_scratch_reg(const std::vector<std::string> &ops,
                              int avoid_extra = -1)
{
    bool used[8] = {};
    if (avoid_extra >= 0 && avoid_extra < 8) used[avoid_extra] = true;
    for (const std::string &op : ops) {
        int r = real_reg32_code(op);
        if (r >= 0) used[r] = true;
    }
    /* Never borrow ESP. Prefer volatile registers first, then EBX/ESI/EDI/EBP. */
    static const int order[] = {0, 1, 2, 3, 6, 7, 5};
    for (int r : order) {
        if (!used[r]) return r;
    }
    return -1;
}

static std::string join_operands(const std::vector<std::string> &ops)
{
    std::string out;
    size_t total = ops.size() > 1 ? (ops.size() - 1) * 2u : 0u;
    for (const std::string &op : ops) {
        total += op.size();
    }
    out.reserve(total);
    for (size_t i = 0; i < ops.size(); ++i) {
        if (i != 0) out += ", ";
        out += ops[i];
    }
    return out;
}

static void emit_temp_generated(std::vector<XemuCheatAsmLine> &out,
                                int source_line, const std::string &text)
{
    out.push_back(XemuCheatAsmLine{source_line, text});
}

/* Lower one normal F0 instruction containing T0-T7. Supported direct T
 * operands behave like persistent dword memory slots. [Tn] means a dword
 * memory access through the 32-bit pointer currently stored in Tn. For the
 * latter, a real x86 scratch register is borrowed with PUSH/POP; POP leaves
 * arithmetic flags intact, so CMP/TEST/ALU results survive the helper. */
static bool expand_temp_instruction(const std::string &input, int source_line,
                                    uint32_t temp_base,
                                    std::vector<XemuCheatAsmLine> &out,
                                    bool &uses_temp, std::string &error)
{
    std::string line = trim(input);
    std::string label_prefix;
    const size_t colon = line.find(':');
    if (colon != std::string::npos) {
        label_prefix = trim(line.substr(0, colon + 1));
        line = trim(line.substr(colon + 1));
        if (line.empty()) {
            out.push_back(XemuCheatAsmLine{source_line, label_prefix});
            return true;
        }
    }

    const size_t ws = line.find_first_of(" \t");
    const std::string mnemonic = upper(ws == std::string::npos ? line : line.substr(0, ws));
    const std::string rest = ws == std::string::npos ? std::string() : trim(line.substr(ws + 1));
    std::vector<std::string> ops = split_operands(rest);
    if (rest.empty()) ops.clear();

    bool any_temp = false;
    std::vector<int> direct(ops.size(), -1);
    std::vector<int> indirect(ops.size(), -1);
    for (size_t i = 0; i < ops.size(); ++i) {
        unsigned n = 0;
        if (parse_temp_register(ops[i], n)) {
            direct[i] = (int)n;
            any_temp = true;
        } else if (parse_temp_indirect(ops[i], n)) {
            indirect[i] = (int)n;
            any_temp = true;
        }
    }
    if (!any_temp) {
        out.push_back(XemuCheatAsmLine{source_line, input});
        return true;
    }
    uses_temp = true;
    if (!label_prefix.empty()) {
        out.push_back(XemuCheatAsmLine{source_line, label_prefix});
    }

    const auto slot = [&](int n) { return temp_slot(temp_base, (unsigned)n); };
    const auto emit = [&](const std::string &text) {
        emit_temp_generated(out, source_line, text);
    };

    /* TEST Tn,Tn has the same architecturally relevant flags as testing the
     * dword slot against all ones, and avoids an illegal memory-memory TEST. */
    if (mnemonic == "TEST" && ops.size() == 2 && direct[0] >= 0 &&
        direct[0] == direct[1]) {
        emit("test " + slot(direct[0]) + ", FFFFFFFF");
        return true;
    }

    const bool have_indirect =
        std::any_of(indirect.begin(), indirect.end(), [](int v) { return v >= 0; });

    /* Simple Tn operands can usually be lowered directly to absolute dword
     * memory. Handle memory-memory pairs through one preserved scratch GPR. */
    if (!have_indirect) {
        if (ops.size() == 2 && direct[0] >= 0 && direct[1] >= 0 &&
            (mnemonic == "MOV" || mnemonic == "ADD" || mnemonic == "ADC" ||
             mnemonic == "SUB" || mnemonic == "SBB" || mnemonic == "AND" ||
             mnemonic == "OR" || mnemonic == "XOR" || mnemonic == "CMP" ||
             mnemonic == "TEST")) {
            const int scratch = choose_scratch_reg(ops);
            if (scratch < 0) {
                error = "could not borrow a scratch register for T-register operation";
                return false;
            }
            const std::string sr = reg32_name(scratch);
            emit("push " + sr);
            emit("mov " + sr + ", " + slot(direct[1]));
            if (mnemonic == "MOV") {
                emit("mov " + slot(direct[0]) + ", " + sr);
            } else {
                emit(upper(mnemonic) + std::string(" ") + slot(direct[0]) + ", " + sr);
            }
            emit("pop " + sr);
            return true;
        }

        /* TEST reg,Tn is commutative; reverse it into the form supported by
         * the core encoder (memory destination, register source). */
        if (mnemonic == "TEST" && ops.size() == 2 && direct[1] >= 0 &&
            real_reg32_code(ops[0]) >= 0) {
            emit("test " + slot(direct[1]) + ", " + ops[0]);
            return true;
        }

        for (size_t i = 0; i < ops.size(); ++i) {
            if (direct[i] >= 0) ops[i] = slot(direct[i]);
        }
        emit(mnemonic + (ops.empty() ? std::string() : " " + join_operands(ops)));
        return true;
    }

    if (ops.size() != 2) {
        error = "[Tn] indirect syntax is currently supported on two-operand F0 instructions";
        return false;
    }

    const bool alu = mnemonic == "ADD" || mnemonic == "ADC" ||
                     mnemonic == "SUB" || mnemonic == "SBB" ||
                     mnemonic == "AND" || mnemonic == "OR" ||
                     mnemonic == "XOR" || mnemonic == "CMP" ||
                     mnemonic == "TEST";

    /* MOV reg32,[Tn] can use its destination register as the pointer scratch. */
    if (mnemonic == "MOV" && indirect[1] >= 0) {
        const int dst_reg = real_reg32_code(ops[0]);
        if (dst_reg >= 0 && dst_reg != 4) {
            const std::string dr = reg32_name(dst_reg);
            emit("mov " + dr + ", " + slot(indirect[1]));
            emit("mov " + dr + ", dword ptr [" + dr + "]");
            return true;
        }
        if (direct[0] >= 0) {
            const int scratch = choose_scratch_reg(ops);
            if (scratch < 0) {
                error = "could not borrow a scratch register for MOV Tn,[Tm]";
                return false;
            }
            const std::string sr = reg32_name(scratch);
            emit("push " + sr);
            emit("mov " + sr + ", " + slot(indirect[1]));
            emit("mov " + sr + ", dword ptr [" + sr + "]");
            emit("mov " + slot(direct[0]) + ", " + sr);
            emit("pop " + sr);
            return true;
        }
    }

    /* MOV [Tn],reg/imm/label uses one pointer scratch. */
    if (mnemonic == "MOV" && indirect[0] >= 0 && direct[1] < 0) {
        const int src_reg = real_reg32_code(ops[1]);
        const int scratch = choose_scratch_reg(ops, src_reg);
        if (scratch < 0) {
            error = "could not borrow a scratch register for MOV [Tn],source";
            return false;
        }
        const std::string sr = reg32_name(scratch);
        emit("push " + sr);
        emit("mov " + sr + ", " + slot(indirect[0]));
        emit("mov dword ptr [" + sr + "], " + ops[1]);
        emit("pop " + sr);
        return true;
    }

    /* MOV [Tn],Tm needs a pointer scratch and a value scratch. */
    if (mnemonic == "MOV" && indirect[0] >= 0 && direct[1] >= 0) {
        int p = choose_scratch_reg(ops);
        if (p < 0) { error = "could not borrow pointer scratch for MOV [Tn],Tm"; return false; }
        std::vector<std::string> avoid = ops;
        avoid.push_back(reg32_name(p));
        int v = choose_scratch_reg(avoid, p);
        if (v < 0) { error = "could not borrow value scratch for MOV [Tn],Tm"; return false; }
        const std::string pr = reg32_name(p), vr = reg32_name(v);
        emit("push " + pr); emit("push " + vr);
        emit("mov " + pr + ", " + slot(indirect[0]));
        emit("mov " + vr + ", " + slot(direct[1]));
        emit("mov dword ptr [" + pr + "], " + vr);
        emit("pop " + vr); emit("pop " + pr);
        return true;
    }

    /* ALU/CMP/TEST destination, [Tn]: load the pointed dword into a preserved
     * scratch register, then use the normal register-source encoding. */
    if (alu && indirect[1] >= 0 && indirect[0] < 0) {
        int avoid = real_reg32_code(ops[0]);
        const int scratch = choose_scratch_reg(ops, avoid);
        if (scratch < 0) {
            error = "could not borrow a scratch register for operation with [Tn]";
            return false;
        }
        const std::string sr = reg32_name(scratch);
        std::string dst = direct[0] >= 0 ? slot(direct[0]) : ops[0];
        emit("push " + sr);
        emit("mov " + sr + ", " + slot(indirect[1]));
        emit("mov " + sr + ", dword ptr [" + sr + "]");
        emit(mnemonic + " " + dst + ", " + sr);
        emit("pop " + sr);
        return true;
    }

    /* ALU/CMP/TEST [Tn],source: borrow a pointer register and operate on the
     * dword it points to. */
    if (alu && indirect[0] >= 0 && indirect[1] < 0 && direct[1] < 0) {
        const int src_reg = real_reg32_code(ops[1]);
        const int scratch = choose_scratch_reg(ops, src_reg);
        if (scratch < 0) {
            error = "could not borrow a scratch register for [Tn] destination";
            return false;
        }
        const std::string sr = reg32_name(scratch);
        emit("push " + sr);
        emit("mov " + sr + ", " + slot(indirect[0]));
        emit(mnemonic + " dword ptr [" + sr + "], " + ops[1]);
        emit("pop " + sr);
        return true;
    }

    error = "unsupported T0-T7 operand combination in '" + input + "'";
    return false;
}


static void append_temp_flag_save_game(std::vector<XemuCheatAsmLine> &out,
                                       int source_line, uint32_t temp_base)
{
    emit_temp_generated(out, source_line, "pushfd");
    emit_temp_generated(out, source_line,
                        "pop " + temp_saved_game_flags_slot(temp_base));
}

static void append_temp_flag_load_private(std::vector<XemuCheatAsmLine> &out,
                                          int source_line, uint32_t temp_base)
{
    emit_temp_generated(out, source_line,
                        "push " + temp_flags_slot(temp_base));
    emit_temp_generated(out, source_line, "popfd");
}

static void append_temp_flag_capture_private(std::vector<XemuCheatAsmLine> &out,
                                             int source_line,
                                             uint32_t temp_base)
{
    emit_temp_generated(out, source_line, "pushfd");
    emit_temp_generated(out, source_line,
                        "pop " + temp_flags_slot(temp_base));
}

static void append_temp_flag_restore_game(std::vector<XemuCheatAsmLine> &out,
                                          int source_line, uint32_t temp_base)
{
    emit_temp_generated(out, source_line,
                        "push " + temp_saved_game_flags_slot(temp_base));
    emit_temp_generated(out, source_line, "popfd");
}

static bool line_is_label_only(const XemuCheatAsmLine &line)
{
    const std::string text = trim(line.text);
    return !text.empty() && text.back() == ':';
}

/* Wrap a T-register instruction that produces flags. The guest's EFLAGS is
 * saved to private state before the operation, the operation's resulting
 * flags are captured into TFLAGS, and the guest flags are restored before
 * execution continues. ADC/SBB/RCL/RCR consume the prior TFLAGS when the
 * current compile-time flag domain is already private. Loading the previous
 * TFLAGS for every later T flag-writer also preserves flags such as CF across
 * INC/DEC exactly like architectural x86. */
static void append_temp_flag_writer(
    std::vector<XemuCheatAsmLine> &out,
    const std::vector<XemuCheatAsmLine> &lowered,
    int source_line, uint32_t temp_base, bool load_private_input)
{
    size_t first_instruction = 0;
    while (first_instruction < lowered.size() &&
           line_is_label_only(lowered[first_instruction])) {
        out.push_back(lowered[first_instruction]);
        ++first_instruction;
    }

    append_temp_flag_save_game(out, source_line, temp_base);
    if (load_private_input) {
        append_temp_flag_load_private(out, source_line, temp_base);
    }
    for (size_t i = first_instruction; i < lowered.size(); ++i) {
        out.push_back(lowered[i]);
    }
    append_temp_flag_capture_private(out, source_line, temp_base);
    append_temp_flag_restore_game(out, source_line, temp_base);
}

/* Run a condition consumer (Jcc/SETcc/CMOVcc/LOOPcc) against private TFLAGS
 * without exposing TFLAGS to the guest. Branch wrappers restore the original
 * guest EFLAGS on both the taken and fall-through paths. */
static bool append_temp_flag_consumer(
    std::vector<XemuCheatAsmLine> &out, int source_line,
    const std::string &label_prefix, const std::string &mnemonic,
    const std::string &rest, uint32_t temp_base, unsigned serial,
    std::string &error)
{
    if (!label_prefix.empty()) {
        emit_temp_generated(out, source_line, label_prefix);
    }

    append_temp_flag_save_game(out, source_line, temp_base);
    append_temp_flag_load_private(out, source_line, temp_base);

    uint8_t cc = 0;
    const bool jcc = !mnemonic.empty() && mnemonic[0] == 'J' &&
                     mnemonic != "JMP" && mnemonic != "JECXZ" &&
                     condition_code(mnemonic.substr(1), cc);
    const bool loopcc = mnemonic == "LOOPE" || mnemonic == "LOOPZ" ||
                        mnemonic == "LOOPNE" || mnemonic == "LOOPNZ";

    if (jcc || loopcc) {
        if (rest.empty() || !valid_label(trim(rest))) {
            error = mnemonic + " target must be a label";
            return false;
        }
        const std::string taken = "__xemu_tf_taken_" + std::to_string(serial);
        const std::string done = "__xemu_tf_done_" + std::to_string(serial);

        emit_temp_generated(out, source_line,
                            mnemonic + " " + taken);
        append_temp_flag_restore_game(out, source_line, temp_base);
        emit_temp_generated(out, source_line, "jmp " + done);
        emit_temp_generated(out, source_line, taken + ":");
        append_temp_flag_restore_game(out, source_line, temp_base);
        emit_temp_generated(out, source_line, "jmp " + trim(rest));
        emit_temp_generated(out, source_line, done + ":");
        return true;
    }

    uint8_t ignored = 0;
    const bool setcc = mnemonic.rfind("SET", 0) == 0 &&
                       mnemonic.size() > 3 &&
                       condition_code(mnemonic.substr(3), ignored);
    const bool cmovcc = mnemonic.rfind("CMOV", 0) == 0 &&
                        mnemonic.size() > 4 &&
                        condition_code(mnemonic.substr(4), ignored);
    if (setcc || cmovcc) {
        emit_temp_generated(out, source_line,
                            mnemonic + (rest.empty() ? std::string()
                                                     : " " + rest));
        append_temp_flag_restore_game(out, source_line, temp_base);
        return true;
    }

    error = "internal TFLAGS condition-consumer expansion failure";
    return false;
}

static bool preserve_register_bit(const std::string &name, uint32_t &bit)
{
    const std::string n = upper(trim(name));
    if (n == "EAX") bit = 1u << 0;
    else if (n == "EBX") bit = 1u << 1;
    else if (n == "ECX") bit = 1u << 2;
    else if (n == "EDX") bit = 1u << 3;
    else if (n == "ESI") bit = 1u << 4;
    else if (n == "EDI") bit = 1u << 5;
    else if (n == "EBP") bit = 1u << 6;
    else if (n == "EFLAGS" || n == "FLAGS") bit = 1u << 7;
    else return false;
    return true;
}

static bool parse_preserve_mask(std::string rest, uint32_t &mask,
                                std::string &error)
{
    mask = 0;
    rest = trim(rest);
    while (!rest.empty() && rest.front() == ',') {
        rest = trim(rest.substr(1));
    }
    if (rest.empty()) {
        error = "PRESERVE requires at least one register";
        return false;
    }
    std::vector<std::string> regs = split_operands(rest);
    for (const std::string &r : regs) {
        uint32_t bit = 0;
        if (!preserve_register_bit(r, bit)) {
            error = "PRESERVE/RESTORE supports EAX, EBX, ECX, EDX, ESI, EDI, EBP, and EFLAGS (not ESP/EIP): '" + trim(r) + "'";
            return false;
        }
        mask |= bit;
    }
    return mask != 0;
}

static void emit_generated(std::vector<XemuCheatAsmLine> &out,
                           int source_line, const std::string &text)
{
    out.push_back(XemuCheatAsmLine{source_line, text});
}

/* Private preservation block layout (per F0 hook):
 *   +00 depth (active real frames)
 *   +04 overflow depth (preserve attempts beyond the 16-frame capacity)
 *   +10 frame 0
 * Each frame is 0x30 bytes:
 *   +00 mask, +04 reserved,
 *   +08 EAX, +0C EBX, +10 ECX, +14 EDX,
 *   +18 ESI, +1C EDI, +20 EBP, +24 EFLAGS.
 *
 * PUSHFD/PUSHAD is used only transiently while moving values between the live
 * CPU and xemu's private preservation memory. The user's persistent saved
 * state never lives on the Xbox stack. */
static constexpr uint32_t kPreserveHeaderSize = 0x10u;
static constexpr uint32_t kPreserveFrameSize = 0x30u;
static constexpr uint32_t kPreserveFrameCount = 0x10u;
static constexpr uint32_t kPreserveBlockSize =
    kPreserveHeaderSize + kPreserveFrameSize * kPreserveFrameCount;
static constexpr uint32_t kPreserveAllMask = 0xFFu;

struct PreserveRegDesc {
    uint32_t bit;
    uint32_t frame_offset;
    uint32_t pushad_offset;
};

static const PreserveRegDesc kPreserveRegs[] = {
    {1u << 0, 0x08u, 0x1Cu}, /* EAX */
    {1u << 1, 0x0Cu, 0x10u}, /* EBX */
    {1u << 2, 0x10u, 0x18u}, /* ECX */
    {1u << 3, 0x14u, 0x14u}, /* EDX */
    {1u << 4, 0x18u, 0x04u}, /* ESI */
    {1u << 5, 0x1Cu, 0x00u}, /* EDI */
    {1u << 6, 0x20u, 0x08u}, /* EBP */
    {1u << 7, 0x24u, 0x20u}, /* EFLAGS (PUSHFD before PUSHAD) */
};

static void append_preserve_sequence(std::vector<XemuCheatAsmLine> &out,
                                     int source_line, uint32_t preserve_base,
                                     uint32_t mask, unsigned serial)
{
    const uint32_t depth_addr = preserve_base + 0x00u;
    const uint32_t overflow_addr = preserve_base + 0x04u;
    const uint32_t frames_addr = preserve_base + kPreserveHeaderSize;
    const std::string overflow = ".__XEMU_PRESERVE_OVERFLOW_" + std::to_string(serial);
    const std::string done = ".__XEMU_PRESERVE_DONE_" + std::to_string(serial);

    emit_generated(out, source_line, "pushfd");
    emit_generated(out, source_line, "pushad");
    emit_generated(out, source_line, "mov eax, dword ptr [" + hex32(overflow_addr) + "]");
    emit_generated(out, source_line, "test eax, eax");
    emit_generated(out, source_line, "jnz " + overflow);
    emit_generated(out, source_line, "mov eax, dword ptr [" + hex32(depth_addr) + "]");
    emit_generated(out, source_line, "cmp eax, 10");
    emit_generated(out, source_line, "jae " + overflow);
    emit_generated(out, source_line, "imul eax, eax, 30");
    emit_generated(out, source_line, "add eax, " + hex32(frames_addr));
    emit_generated(out, source_line, "mov dword ptr [eax], " + hex32(mask));

    for (const PreserveRegDesc &r : kPreserveRegs) {
        if ((mask & r.bit) == 0) {
            continue;
        }
        emit_generated(out, source_line,
                       "mov edx, dword ptr [esp+" + hex32(r.pushad_offset) + "]");
        emit_generated(out, source_line,
                       "mov dword ptr [eax+" + hex32(r.frame_offset) + "], edx");
    }
    emit_generated(out, source_line,
                   "add dword ptr [" + hex32(depth_addr) + "], 1");
    emit_generated(out, source_line, "jmp " + done);
    emit_generated(out, source_line, overflow + ":");
    emit_generated(out, source_line,
                   "add dword ptr [" + hex32(overflow_addr) + "], 1");
    emit_generated(out, source_line, done + ":");
    emit_generated(out, source_line, "popad");
    emit_generated(out, source_line, "popfd");
}

static void append_restore_sequence(std::vector<XemuCheatAsmLine> &out,
                                    int source_line, uint32_t preserve_base,
                                    uint32_t requested_mask, unsigned serial)
{
    const uint32_t depth_addr = preserve_base + 0x00u;
    const uint32_t overflow_addr = preserve_base + 0x04u;
    const uint32_t frames_addr = preserve_base + kPreserveHeaderSize;
    const std::string normal = ".__XEMU_RESTORE_NORMAL_" + std::to_string(serial);
    const std::string done = ".__XEMU_RESTORE_DONE_" + std::to_string(serial);
    const std::string keep = ".__XEMU_RESTORE_KEEP_" + std::to_string(serial);

    emit_generated(out, source_line, "pushfd");
    emit_generated(out, source_line, "pushad");
    emit_generated(out, source_line, "mov eax, dword ptr [" + hex32(overflow_addr) + "]");
    emit_generated(out, source_line, "test eax, eax");
    emit_generated(out, source_line, "jz " + normal);
    emit_generated(out, source_line,
                   "sub dword ptr [" + hex32(overflow_addr) + "], 1");
    emit_generated(out, source_line, "jmp " + done);

    emit_generated(out, source_line, normal + ":");
    emit_generated(out, source_line, "mov eax, dword ptr [" + hex32(depth_addr) + "]");
    emit_generated(out, source_line, "test eax, eax");
    emit_generated(out, source_line, "jz " + done);
    emit_generated(out, source_line, "sub eax, 1");
    emit_generated(out, source_line, "imul eax, eax, 30");
    emit_generated(out, source_line, "add eax, " + hex32(frames_addr));
    emit_generated(out, source_line, "mov edx, dword ptr [eax]");

    unsigned reg_serial = 0;
    for (const PreserveRegDesc &r : kPreserveRegs) {
        if ((requested_mask & r.bit) == 0) {
            continue;
        }
        const std::string skip = ".__XEMU_RESTORE_SKIP_" +
                                 std::to_string(serial) + "_" +
                                 std::to_string(reg_serial++);
        emit_generated(out, source_line, "test edx, " + hex32(r.bit));
        emit_generated(out, source_line, "jz " + skip);
        emit_generated(out, source_line,
                       "mov ecx, dword ptr [eax+" + hex32(r.frame_offset) + "]");
        emit_generated(out, source_line,
                       "mov dword ptr [esp+" + hex32(r.pushad_offset) + "], ecx");
        emit_generated(out, source_line,
                       "mov dword ptr [eax+" + hex32(r.frame_offset) + "], 0");
        emit_generated(out, source_line,
                       "and edx, " + hex32(~r.bit));
        emit_generated(out, source_line, skip + ":");
    }

    emit_generated(out, source_line, "mov dword ptr [eax], edx");
    emit_generated(out, source_line, "test edx, edx");
    emit_generated(out, source_line, "jnz " + keep);
    emit_generated(out, source_line,
                   "sub dword ptr [" + hex32(depth_addr) + "], 1");
    /* A fully-restored frame is explicitly cleared before it becomes reusable. */
    for (uint32_t off = 0; off < kPreserveFrameSize; off += 4u) {
        emit_generated(out, source_line,
                       "mov dword ptr [eax+" + hex32(off) + "], 0");
    }
    emit_generated(out, source_line, keep + ":");
    emit_generated(out, source_line, done + ":");
    emit_generated(out, source_line, "popad");
    emit_generated(out, source_line, "popfd");
}

static bool expand_f0_directives(const std::vector<XemuCheatAsmLine> &lines,
                                 uint32_t preserve_base,
                                 uint32_t temp_base,
                                 std::vector<XemuCheatAsmLine> &expanded,
                                 bool &uses_preserve, bool &uses_temp,
                                 int &error_line, std::string &error)
{
    expanded.clear();
    /* The non-directive case is one output line per source line. Directive/T
     * lowering can grow beyond this, but this reserve removes the common first
     * reallocation and the vector still grows normally when required. */
    expanded.reserve(lines.size());
    uses_preserve = false;
    uses_temp = false;
    unsigned serial = 0;
    bool temp_flags_live = false;

    for (size_t i = 0; i < lines.size(); ++i) {
        std::string line = strip_comment(lines[i].text);
        if (!line.empty() && line.front() == '$') {
            line = trim(line.substr(1));
        }
        if (line.empty()) {
            continue;
        }

        /* Inspect the instruction portion separately so "Label: jne Target"
         * participates in TFLAGS tracking exactly like a standalone Jcc. */
        std::string label_prefix;
        std::string instruction = line;
        const size_t colon = instruction.find(':');
        if (colon != std::string::npos) {
            label_prefix = trim(instruction.substr(0, colon + 1));
            instruction = trim(instruction.substr(colon + 1));
        }

        size_t ws = instruction.find_first_of(" \t");
        std::string mnemonic =
            upper(ws == std::string::npos ? instruction
                                          : instruction.substr(0, ws));
        std::string rest =
            ws == std::string::npos ? std::string()
                                    : trim(instruction.substr(ws + 1));

        /* Be tolerant of the cheat-code-style spelling "PRESERVE, EAX" /
         * "RESTORE, EAX" in addition to the assembly-like form without the
         * comma after the directive name. */
        if (!mnemonic.empty() && mnemonic.back() == ',') {
            mnemonic.pop_back();
        }

        if (mnemonic == "PRESERVEALL" || mnemonic == "PRESERVE") {
            const int preserve_source_line = lines[i].source_line;
            uint32_t mask = kPreserveAllMask;
            if (mnemonic == "PRESERVEALL") {
                if (!rest.empty()) {
                    error_line = lines[i].source_line;
                    error = "PRESERVEALL takes no operands";
                    return false;
                }
            } else if (!parse_preserve_mask(rest, mask, error)) {
                error_line = lines[i].source_line;
                return false;
            }

            /* Adjacent PRESERVE lines accumulate into one frame. This permits:
             *   PRESERVE EAX
             *   PRESERVE ECX
             * to behave like PRESERVE EAX, ECX. */
            size_t j = i + 1;
            while (j < lines.size()) {
                std::string next = strip_comment(lines[j].text);
                if (!next.empty() && next.front() == '$') next = trim(next.substr(1));
                if (next.empty()) break;
                size_t nws = next.find_first_of(" \t");
                std::string nm = upper(nws == std::string::npos ? next : next.substr(0, nws));
                std::string nr = nws == std::string::npos ? std::string() : trim(next.substr(nws + 1));
                if (!nm.empty() && nm.back() == ',') {
                    nm.pop_back();
                }
                if (nm == "PRESERVEALL") {
                    if (!nr.empty()) {
                        error_line = lines[j].source_line;
                        error = "PRESERVEALL takes no operands";
                        return false;
                    }
                    mask = kPreserveAllMask;
                    ++j;
                    continue;
                }
                if (nm == "PRESERVE") {
                    uint32_t more = 0;
                    if (!parse_preserve_mask(nr, more, error)) {
                        error_line = lines[j].source_line;
                        return false;
                    }
                    mask |= more;
                    ++j;
                    continue;
                }
                break;
            }
            i = j - 1;
            uses_preserve = true;
            append_preserve_sequence(expanded, preserve_source_line,
                                     preserve_base, mask, serial++);
            continue;
        }

        if (mnemonic == "RESTORE") {
            uint32_t mask = kPreserveAllMask;
            if (!rest.empty() && !parse_preserve_mask(rest, mask, error)) {
                error_line = lines[i].source_line;
                return false;
            }
            uses_preserve = true;
            append_restore_sequence(expanded, lines[i].source_line,
                                    preserve_base, mask, serial++);
            if ((mask & (1u << 7)) != 0) {
                temp_flags_live = false;
            }
            continue;
        }

        if (instruction.empty()) {
            expanded.push_back(XemuCheatAsmLine{lines[i].source_line,
                                                label_prefix});
            continue;
        }

        /* Once a T-register comparison/arithmetic operation owns the active
         * condition state, ordinary Jcc/SETcc/CMOVcc/LOOPcc consumes private
         * TFLAGS until a real x86 flag-writing instruction takes ownership. */
        if (temp_flags_live &&
            instruction_is_flag_condition_consumer(mnemonic)) {
            uses_temp = true;
            if (!append_temp_flag_consumer(
                    expanded, lines[i].source_line, label_prefix, mnemonic,
                    rest, temp_base, serial++, error)) {
                error_line = lines[i].source_line;
                return false;
            }
            continue;
        }

        std::vector<XemuCheatAsmLine> lowered;
        /* T lowering normally emits a handful of helper instructions. */
        lowered.reserve(8);
        bool line_uses_temp = false;
        if (!expand_temp_instruction(line, lines[i].source_line, temp_base,
                                     lowered, line_uses_temp, error)) {
            error_line = lines[i].source_line;
            return false;
        }
        uses_temp = uses_temp || line_uses_temp;

        if (line_uses_temp && instruction_writes_condition_flags(mnemonic)) {
            append_temp_flag_writer(
                expanded, lowered, lines[i].source_line, temp_base,
                temp_flags_live);
            temp_flags_live = true;
            continue;
        }

        expanded.insert(expanded.end(), lowered.begin(), lowered.end());

        /* A real x86 flag-producing instruction explicitly returns condition
         * ownership to architectural EFLAGS. MOV/LEA/PUSH/etc. leave the
         * current condition domain untouched, matching x86 flag semantics. */
        if (!line_uses_temp && instruction_writes_condition_flags(mnemonic)) {
            temp_flags_live = false;
        }
    }
    return true;
}

struct LabelDef {
    bool data = false;
    size_t offset = 0;
};

} // namespace

bool xemu_cheat_assemble_x86_32_at(const std::vector<XemuCheatAsmLine> &lines,
                                   uint32_t cave_base,
                                   uint32_t preserve_base,
                                   uint32_t temp_base,
                                   XemuCheatAsmResult &result)
{
    result = XemuCheatAsmResult{};
    std::vector<XemuCheatAsmLine> expanded;
    if (!expand_f0_directives(lines, preserve_base, temp_base, expanded,
                              result.uses_preserve, result.uses_temp,
                              result.error_line, result.error)) {
        return false;
    }
    result.preserve_bytes = result.uses_preserve ? kPreserveBlockSize : 0u;
    result.temp_bytes = result.uses_temp ? kTempBankSize : 0u;

    std::unordered_map<std::string, LabelDef> defs;
    defs.reserve(expanded.size());
    std::vector<std::string> pending_labels;
    pending_labels.reserve(4);
    std::vector<ParsedLine> parsed;
    parsed.reserve(expanded.size());
    size_t code_offset = 0;
    size_t data_offset = 0;

    auto define_pending = [&](bool is_data, size_t at, int source_line) -> bool {
        for (const std::string &name : pending_labels) {
            if (defs.find(name) != defs.end()) {
                result.error_line = source_line;
                result.error = "duplicate label '" + name + "'";
                return false;
            }
            defs.emplace(name, LabelDef{is_data, at});
        }
        pending_labels.clear();
        return true;
    };

    for (const auto &src : expanded) {
        std::string line = strip_comment(src.text);
        if (!line.empty() && line.front() == '$') line = trim(line.substr(1));
        if (line.empty()) continue;

        /* Permit one label prefix per line plus label-only lines. */
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string name = upper(trim(line.substr(0, colon)));
            unsigned temp_label_index = 0;
            if (parse_temp_register(name, temp_label_index)) {
                result.error_line = src.source_line;
                result.error = name + " is reserved for an F0 temp register";
                return false;
            }
            if (!valid_label(name)) {
                result.error_line = src.source_line;
                result.error = "invalid label name '" + trim(line.substr(0, colon)) + "'";
                return false;
            }
            if (defs.find(name) != defs.end() ||
                std::find(pending_labels.begin(), pending_labels.end(), name) != pending_labels.end()) {
                result.error_line = src.source_line;
                result.error = "duplicate label '" + name + "'";
                return false;
            }
            pending_labels.push_back(name);
            line = trim(line.substr(colon + 1));
            if (line.empty()) continue;
        }

        size_t ws = line.find_first_of(" \t");
        std::string mnemonic = upper(ws == std::string::npos ? line : line.substr(0, ws));
        std::string rest = ws == std::string::npos ? std::string() : trim(line.substr(ws + 1));

        if (mnemonic == "DD") {
            if (!define_pending(true, data_offset, src.source_line)) return false;
            std::vector<std::string> values = split_operands(rest);
            if (values.empty() || (values.size() == 1 && values[0].empty())) {
                result.error_line = src.source_line;
                result.error = "DD requires at least one 32-bit value";
                return false;
            }
            for (const std::string &value_text : values) {
                int64_t value = 0;
                if (!parse_number(value_text, value)) {
                    result.error_line = src.source_line;
                    result.error = "DD value must be a 32-bit hexadecimal number: '" + trim(value_text) + "'";
                    return false;
                }
                emit_u32(result.data, (uint32_t)value);
                data_offset += 4u;
                if (data_offset > 0x10000u) {
                    result.error_line = src.source_line;
                    result.error = "Type-F0 static DD data exceeds 64 KiB";
                    return false;
                }
            }
            continue;
        }

        if (!define_pending(false, code_offset, src.source_line)) return false;
        std::vector<uint8_t> probe;
        probe.reserve(16);
        std::string error;
        if (!encode_instruction(line, code_offset, nullptr, 0, probe, error)) {
            result.error_line = src.source_line;
            result.error = error;
            return false;
        }
        ParsedLine p;
        p.source_line = src.source_line;
        p.instruction = line;
        p.offset = code_offset;
        p.size = probe.size();
        parsed.push_back(std::move(p));
        code_offset += probe.size();
        if (code_offset > 0x10000u) {
            result.error_line = src.source_line;
            result.error = "Type-F0 assembly cave exceeds 64 KiB";
            return false;
        }
    }

    /* Preserve old assembler behavior for a label at the very end: it denotes
     * the current code end (the location immediately before DEADCODE's JMP). */
    if (!pending_labels.empty()) {
        if (!define_pending(false, code_offset,
                            expanded.empty() ? 0 : expanded.back().source_line)) {
            return false;
        }
    }

    if (parsed.empty()) {
        result.error = "Type-F0 assembly cave contains no instructions";
        result.error_line = lines.empty() ? 0 : lines.front().source_line;
        return false;
    }

    /* Data labels live after executable bytes AND the automatic 5-byte return
     * JMP generated for DEADCODE. */
    std::unordered_map<std::string, size_t> labels;
    labels.reserve(defs.size());
    for (const auto &entry : defs) {
        labels[entry.first] = entry.second.data
            ? code_offset + 5u + entry.second.offset
            : entry.second.offset;
    }

    result.bytes.reserve(code_offset);
    for (const ParsedLine &p : parsed) {
        std::vector<uint8_t> bytes;
        bytes.reserve(p.size);
        std::string error;
        if (!encode_instruction(p.instruction, p.offset, &labels,
                                cave_base, bytes, error)) {
            result.error_line = p.source_line;
            result.error = error;
            result.bytes.clear();
            result.data.clear();
            return false;
        }
        if (bytes.size() != p.size) {
            result.error_line = p.source_line;
            result.error = "internal assembler size mismatch";
            result.bytes.clear();
            result.data.clear();
            return false;
        }
        result.bytes.insert(result.bytes.end(), bytes.begin(), bytes.end());
    }

    if (result.bytes.size() + 5u + result.data.size() > 0x10000u) {
        result.error_line = lines.empty() ? 0 : lines.front().source_line;
        result.error = "Type-F0 code + DEADCODE JMP + DD data exceeds 64 KiB";
        result.bytes.clear();
        result.data.clear();
        return false;
    }

    result.ok = true;
    return true;
}

bool xemu_cheat_assemble_x86_32_change_instruction(
    const std::string &instruction, uint32_t address, size_t max_size,
    XemuCheatAsmResult &result)
{
    result = XemuCheatAsmResult{};

    std::string line = strip_comment(instruction);
    if (!line.empty() && line.front() == '$') {
        line = trim(line.substr(1));
    }
    if (line.empty()) {
        result.error_line = 1;
        result.error = "Change requires one x86 instruction";
        return false;
    }

    const size_t ws = line.find_first_of(" \t");
    const std::string mnemonic =
        upper(ws == std::string::npos ? line : line.substr(0, ws));
    const std::string rest =
        ws == std::string::npos ? std::string() : trim(line.substr(ws + 1));

    int64_t target_value = 0;
    const bool direct_target = !rest.empty() &&
                               rest.find(',') == std::string::npos &&
                               parse_number(rest, target_value);
    if (direct_target) {
        const uint32_t target = (uint32_t)target_value;

        if (mnemonic == "JMP" && max_size >= 2u) {
            const int32_t rel8 = (int32_t)(target - (address + 2u));
            if (rel8 >= INT8_MIN && rel8 <= INT8_MAX) {
                result.bytes = {0xEB, (uint8_t)(int8_t)rel8};
                result.ok = true;
                return true;
            }
        }

        uint8_t cc = 0;
        if (!mnemonic.empty() && mnemonic[0] == 'J' &&
            mnemonic != "JMP" && mnemonic != "JECXZ" &&
            condition_code(mnemonic.substr(1), cc) && max_size >= 2u) {
            const int32_t rel8 = (int32_t)(target - (address + 2u));
            if (rel8 >= INT8_MIN && rel8 <= INT8_MAX) {
                result.bytes = {(uint8_t)(0x70u + cc),
                                (uint8_t)(int8_t)rel8};
                result.ok = true;
                return true;
            }
        }
    }

    std::vector<XemuCheatAsmLine> lines;
    lines.push_back(XemuCheatAsmLine{1, line});
    return xemu_cheat_assemble_x86_32_at(lines, address, 0, 0, result);
}

bool xemu_cheat_assemble_x86_32(const std::vector<XemuCheatAsmLine> &lines,
                                XemuCheatAsmResult &result)
{
    return xemu_cheat_assemble_x86_32_at(lines, 0, 0, 0, result);
}
