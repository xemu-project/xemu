//
// xemu RAW Cheat Engine - compact 32-bit x86 assembler encoder core
//
// Low-level operand parsing and instruction encoding used by the Type-F0
// assembler frontend.
//

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

namespace xemu_cheat_assembler_internal {

struct MemoryOperand {
    bool has_base = false;
    int base = 0;
    bool has_index = false;
    int index = 0;
    int scale = 1;
    int32_t disp = 0;
    int width = 0;
};

enum class OperandKind { Invalid, Reg, Imm, Mem, Label };

struct Operand {
    OperandKind kind = OperandKind::Invalid;
    int reg = -1;
    int width = 0;
    uint32_t imm = 0;
    MemoryOperand mem;
    std::string label;
};

struct ParsedLine {
    int source_line = 0;
    std::string instruction;
    size_t offset = 0;
    size_t size = 0;
};

std::string trim(const std::string &s)
{
    size_t a = 0;
    while (a < s.size() && std::isspace((unsigned char)s[a])) {
        ++a;
    }
    size_t b = s.size();
    while (b > a && std::isspace((unsigned char)s[b - 1])) {
        --b;
    }
    return s.substr(a, b - a);
}

std::string upper(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return (char)std::toupper(c);
    });
    return s;
}

std::string strip_comment(const std::string &s)
{
    size_t cut = s.size();
    size_t pos = s.find(';');
    if (pos != std::string::npos) cut = std::min(cut, pos);
    pos = s.find("//");
    if (pos != std::string::npos) cut = std::min(cut, pos);
    pos = s.find('#');
    if (pos != std::string::npos) cut = std::min(cut, pos);
    return trim(s.substr(0, cut));
}

bool parse_register(const std::string &name, int &code, int &width)
{
    const std::string n = upper(trim(name));
    static const char *const regs32[] = {
        "EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI",
    };
    static const char *const regs16[] = {
        "AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI",
    };
    static const char *const regs8[] = {
        "AL", "CL", "DL", "BL", "AH", "CH", "DH", "BH",
    };
    for (int i = 0; i < 8; ++i) {
        if (n == regs32[i]) {
            code = i;
            width = 32;
            return true;
        }
        if (n == regs16[i]) {
            code = i;
            width = 16;
            return true;
        }
        if (n == regs8[i]) {
            code = i;
            width = 8;
            return true;
        }
    }
    return false;
}

int reg32(const std::string &name)
{
    int code = -1;
    int width = 0;
    return parse_register(name, code, width) && width == 32 ? code : -1;
}

bool valid_label(const std::string &name)
{
    if (name.empty()) {
        return false;
    }
    unsigned char c0 = (unsigned char)name[0];
    if (!(std::isalpha(c0) || c0 == '_' || c0 == '.' || c0 == '$')) {
        return false;
    }
    for (unsigned char c : name) {
        if (!(std::isalnum(c) || c == '_' || c == '.' || c == '$')) {
            return false;
        }
    }
    return true;
}

/* Cheat files have historically been hex-first, so bare numbers are hex.
 * Accept 0x1234 and 1234h too. A leading +/- is supported for displacements. */
bool parse_number(const std::string &text, int64_t &value)
{
    std::string s = trim(text);
    if (s.empty()) {
        return false;
    }
    bool neg = false;
    if (s[0] == '+' || s[0] == '-') {
        neg = s[0] == '-';
        s.erase(0, 1);
    }
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s.erase(0, 2);
    }
    if (!s.empty() && (s.back() == 'h' || s.back() == 'H')) {
        s.pop_back();
    }
    if (s.empty()) {
        return false;
    }
    uint64_t v = 0;
    for (unsigned char c : s) {
        if (!std::isxdigit(c)) {
            return false;
        }
        unsigned digit = c >= '0' && c <= '9' ? c - '0'
                         : (unsigned)(std::toupper(c) - 'A' + 10);
        if (v > (std::numeric_limits<uint64_t>::max() - digit) / 16u) {
            return false;
        }
        v = v * 16u + digit;
    }
    if (neg) {
        if (v > 0x80000000ull) {
            return false;
        }
        value = -(int64_t)v;
    } else {
        if (v > 0xffffffffull) {
            return false;
        }
        value = (int64_t)v;
    }
    return true;
}

std::vector<std::string> split_operands(const std::string &s)
{
    std::vector<std::string> out;
    /* Most supported instructions have at most three operands. Reserving the
     * common case avoids repeated tiny allocations without changing parsing. */
    out.reserve(3);
    int brackets = 0;
    size_t start = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '[') ++brackets;
        else if (s[i] == ']') --brackets;
        else if (s[i] == ',' && brackets == 0) {
            out.push_back(trim(s.substr(start, i - start)));
            start = i + 1;
        }
    }
    if (start < s.size() || !s.empty()) {
        out.push_back(trim(s.substr(start)));
    }
    return out;
}

static bool parse_memory(std::string text, MemoryOperand &mem,
                         std::string &error)
{
    std::string t = trim(text);
    std::string u = upper(t);
    auto eat_prefix = [&](const char *p, int width) {
        const std::string pref = p;
        if (u.rfind(pref, 0) == 0) {
            mem.width = width;
            t = trim(t.substr(pref.size()));
            u = upper(t);
            return true;
        }
        return false;
    };
    eat_prefix("BYTE PTR ", 8) || eat_prefix("WORD PTR ", 16) ||
        eat_prefix("DWORD PTR ", 32);

    if (t.size() < 2 || t.front() != '[' || t.back() != ']') {
        return false;
    }
    std::string expr = t.substr(1, t.size() - 2);
    expr.erase(std::remove_if(expr.begin(), expr.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    }), expr.end());
    if (expr.empty()) {
        error = "empty memory operand";
        return false;
    }

    size_t pos = 0;
    int sign = +1;
    while (pos < expr.size()) {
        if (expr[pos] == '+') {
            sign = +1;
            ++pos;
            continue;
        }
        if (expr[pos] == '-') {
            sign = -1;
            ++pos;
            continue;
        }
        size_t end = pos;
        while (end < expr.size() && expr[end] != '+' && expr[end] != '-') {
            ++end;
        }
        std::string term = expr.substr(pos, end - pos);
        size_t star = term.find('*');
        if (star != std::string::npos) {
            if (sign < 0) {
                error = "negative index register is not supported";
                return false;
            }
            int r = reg32(term.substr(0, star));
            int64_t scale = 0;
            if (r < 0 || r == 4 || !parse_number(term.substr(star + 1), scale) ||
                !(scale == 1 || scale == 2 || scale == 4 || scale == 8)) {
                error = "invalid index*scale expression";
                return false;
            }
            if (mem.has_index) {
                error = "memory operand has more than one index register";
                return false;
            }
            mem.has_index = true;
            mem.index = r;
            mem.scale = (int)scale;
        } else {
            int r = reg32(term);
            if (r >= 0) {
                if (sign < 0) {
                    error = "negative base/index register is not supported";
                    return false;
                }
                if (!mem.has_base) {
                    mem.has_base = true;
                    mem.base = r;
                } else if (!mem.has_index && r != 4) {
                    mem.has_index = true;
                    mem.index = r;
                    mem.scale = 1;
                } else {
                    error = "memory operand has too many registers";
                    return false;
                }
            } else {
                int64_t n = 0;
                if (!parse_number(term, n)) {
                    error = "invalid memory displacement '" + term + "'";
                    return false;
                }
                int64_t disp = (int64_t)mem.disp + sign * n;
                if (disp < INT32_MIN || disp > (int64_t)UINT32_MAX) {
                    error = "memory displacement is out of 32-bit range";
                    return false;
                }
                mem.disp = (int32_t)(uint32_t)disp;
            }
        }
        sign = +1;
        pos = end;
    }
    return true;
}

static Operand parse_operand(const std::string &text, std::string &error)
{
    Operand op;
    int r = -1;
    int width = 0;
    if (parse_register(text, r, width)) {
        op.kind = OperandKind::Reg;
        op.reg = r;
        op.width = width;
        return op;
    }

    MemoryOperand mem;
    if (parse_memory(text, mem, error)) {
        op.kind = OperandKind::Mem;
        op.mem = mem;
        return op;
    }
    if (!error.empty()) {
        return op;
    }

    int64_t n = 0;
    if (parse_number(text, n)) {
        op.kind = OperandKind::Imm;
        op.imm = (uint32_t)n;
        return op;
    }

    std::string label = upper(trim(text));
    if (valid_label(label)) {
        op.kind = OperandKind::Label;
        op.label = label;
        return op;
    }
    error = "invalid operand '" + trim(text) + "'";
    return op;
}

static void emit_u16(std::vector<uint8_t> &out, uint16_t v)
{
    out.push_back((uint8_t)(v & 0xff));
    out.push_back((uint8_t)((v >> 8) & 0xff));
}

void emit_u32(std::vector<uint8_t> &out, uint32_t v)
{
    out.push_back((uint8_t)(v & 0xff));
    out.push_back((uint8_t)((v >> 8) & 0xff));
    out.push_back((uint8_t)((v >> 16) & 0xff));
    out.push_back((uint8_t)((v >> 24) & 0xff));
}

static bool encode_rm(std::vector<uint8_t> &out, int reg_field,
                      const Operand &rm, std::string &error)
{
    if (rm.kind == OperandKind::Reg) {
        out.push_back((uint8_t)(0xC0 | ((reg_field & 7) << 3) | (rm.reg & 7)));
        return true;
    }
    if (rm.kind != OperandKind::Mem) {
        error = "expected register or memory operand";
        return false;
    }

    const MemoryOperand &m = rm.mem;
    if (!m.has_base && !m.has_index) {
        out.push_back((uint8_t)(((reg_field & 7) << 3) | 0x05));
        emit_u32(out, (uint32_t)m.disp);
        return true;
    }

    int mod;
    bool disp8 = false;
    bool disp32 = false;
    if (!m.has_base) {
        mod = 0;
        disp32 = true;
    } else if (m.disp == 0 && m.base != 5) {
        mod = 0;
    } else if (m.disp >= -128 && m.disp <= 127) {
        mod = 1;
        disp8 = true;
    } else {
        mod = 2;
        disp32 = true;
    }

    const bool need_sib = m.has_index || !m.has_base || m.base == 4;
    const int rm_field = need_sib ? 4 : m.base;
    out.push_back((uint8_t)((mod << 6) | ((reg_field & 7) << 3) |
                            (rm_field & 7)));

    if (need_sib) {
        int scale_bits = 0;
        if (m.scale == 2) scale_bits = 1;
        else if (m.scale == 4) scale_bits = 2;
        else if (m.scale == 8) scale_bits = 3;
        const int index = m.has_index ? m.index : 4;
        const int base = m.has_base ? m.base : 5;
        out.push_back((uint8_t)((scale_bits << 6) | ((index & 7) << 3) |
                                (base & 7)));
    }

    if (disp8) {
        out.push_back((uint8_t)m.disp);
    } else if (disp32 || (mod == 0 && !m.has_base)) {
        emit_u32(out, (uint32_t)m.disp);
    }
    return true;
}

static bool width_ok(const Operand &op, int expected)
{
    if (op.kind == OperandKind::Reg) {
        return op.width == expected;
    }
    return op.kind != OperandKind::Mem || op.mem.width == 0 ||
           op.mem.width == expected;
}

static int operand_width(const Operand &op, int default_mem_width = 0)
{
    if (op.kind == OperandKind::Reg) {
        return op.width;
    }
    if (op.kind == OperandKind::Mem) {
        return op.mem.width != 0 ? op.mem.width : default_mem_width;
    }
    return 0;
}

static bool width_is_integer(int width)
{
    return width == 8 || width == 16 || width == 32;
}

static void emit_operand_size_prefix(std::vector<uint8_t> &out, int width)
{
    if (width == 16) {
        out.push_back(0x66);
    }
}

bool condition_code(const std::string &mnemonic, uint8_t &cc)
{
    static const std::unordered_map<std::string, uint8_t> codes = {
        {"O",0x0},{"NO",0x1},{"B",0x2},{"NAE",0x2},{"C",0x2},
        {"AE",0x3},{"NB",0x3},{"NC",0x3},{"E",0x4},{"Z",0x4},
        {"NE",0x5},{"NZ",0x5},{"BE",0x6},{"NA",0x6},{"A",0x7},
        {"NBE",0x7},{"S",0x8},{"NS",0x9},{"P",0xA},{"PE",0xA},
        {"NP",0xB},{"PO",0xB},{"L",0xC},{"NGE",0xC},{"GE",0xD},
        {"NL",0xD},{"LE",0xE},{"NG",0xE},{"G",0xF},{"NLE",0xF},
    };
    auto it = codes.find(mnemonic);
    if (it == codes.end()) {
        return false;
    }
    cc = it->second;
    return true;
}

bool encode_instruction(const std::string &text, size_t offset,
                               const std::unordered_map<std::string, size_t> *labels,
                               uint32_t base_address,
                               std::vector<uint8_t> &out, std::string &error)
{
    std::string line = trim(text);
    size_t ws = line.find_first_of(" \t");
    std::string mnemonic = upper(ws == std::string::npos ? line : line.substr(0, ws));
    std::string rest = ws == std::string::npos ? std::string() : trim(line.substr(ws + 1));
    std::vector<std::string> ops_text = split_operands(rest);
    if (rest.empty()) {
        ops_text.clear();
    }

    auto parse_ops = [&](size_t n, std::vector<Operand> &ops) -> bool {
        if (ops_text.size() != n) {
            error = mnemonic + " expects " + std::to_string(n) + " operand" +
                    (n == 1 ? "" : "s");
            return false;
        }
        ops.reserve(n);
        for (const std::string &s : ops_text) {
            std::string e;
            Operand op = parse_operand(s, e);
            if (op.kind == OperandKind::Invalid) {
                error = e.empty() ? "invalid operand" : e;
                return false;
            }
            ops.push_back(std::move(op));
        }
        return true;
    };

    if (mnemonic == "NOP") {
        if (!ops_text.empty()) { error = "NOP takes no operands"; return false; }
        out.push_back(0x90); return true;
    }
    if (mnemonic == "RET") {
        if (ops_text.empty()) {
            out.push_back(0xC3);
            return true;
        }
        std::vector<Operand> ops;
        if (!parse_ops(1, ops) || ops[0].kind != OperandKind::Imm ||
            ops[0].imm > 0xffffu) {
            if (error.empty()) error = "RET expects no operand or a 16-bit immediate";
            return false;
        }
        out.push_back(0xC2);
        emit_u16(out, (uint16_t)ops[0].imm);
        return true;
    }

    static const std::unordered_map<std::string, std::vector<uint8_t>> no_operand = {
        {"PUSHA", {0x60}}, {"PUSHAD", {0x60}},
        {"POPA", {0x61}}, {"POPAD", {0x61}},
        {"PUSHF", {0x9C}}, {"PUSHFD", {0x9C}},
        {"POPF", {0x9D}}, {"POPFD", {0x9D}},
        {"LAHF", {0x9F}}, {"SAHF", {0x9E}},
        {"CLC", {0xF8}}, {"STC", {0xF9}}, {"CMC", {0xF5}},
        {"CLD", {0xFC}}, {"STD", {0xFD}},
        {"CBW", {0x66,0x98}}, {"CWDE", {0x98}},
        {"CWD", {0x66,0x99}}, {"CDQ", {0x99}},
        {"LEAVE", {0xC9}}, {"INT3", {0xCC}}, {"IRET", {0xCF}},
        {"MOVSB", {0xA4}}, {"MOVSW", {0x66,0xA5}}, {"MOVSD", {0xA5}},
        {"STOSB", {0xAA}}, {"STOSW", {0x66,0xAB}}, {"STOSD", {0xAB}},
        {"LODSB", {0xAC}}, {"LODSW", {0x66,0xAD}}, {"LODSD", {0xAD}},
        {"SCASB", {0xAE}}, {"SCASW", {0x66,0xAF}}, {"SCASD", {0xAF}},
        {"CMPSB", {0xA6}}, {"CMPSW", {0x66,0xA7}}, {"CMPSD", {0xA7}},
    };
    auto noi = no_operand.find(mnemonic);
    if (noi != no_operand.end()) {
        if (!ops_text.empty()) {
            error = mnemonic + " takes no operands";
            return false;
        }
        out.insert(out.end(), noi->second.begin(), noi->second.end());
        return true;
    }

    if (mnemonic == "REP" || mnemonic == "REPE" || mnemonic == "REPZ" ||
        mnemonic == "REPNE" || mnemonic == "REPNZ") {
        if (rest.empty()) {
            error = mnemonic + " requires a following string instruction";
            return false;
        }
        std::vector<uint8_t> inner;
        std::string inner_error;
        if (!encode_instruction(rest, offset + 1, labels, base_address, inner, inner_error)) {
            error = inner_error;
            return false;
        }
        const std::string inner_mnemonic = upper(trim(rest.substr(
            0, rest.find_first_of(" \t"))));
        if (inner_mnemonic != "MOVSB" && inner_mnemonic != "MOVSW" &&
            inner_mnemonic != "MOVSD" && inner_mnemonic != "STOSB" &&
            inner_mnemonic != "STOSW" && inner_mnemonic != "STOSD" &&
            inner_mnemonic != "LODSB" && inner_mnemonic != "LODSW" &&
            inner_mnemonic != "LODSD" && inner_mnemonic != "SCASB" &&
            inner_mnemonic != "SCASW" && inner_mnemonic != "SCASD" &&
            inner_mnemonic != "CMPSB" && inner_mnemonic != "CMPSW" &&
            inner_mnemonic != "CMPSD") {
            error = mnemonic + " currently supports x86 string instructions only";
            return false;
        }
        out.push_back((mnemonic == "REPNE" || mnemonic == "REPNZ") ? 0xF2 : 0xF3);
        out.insert(out.end(), inner.begin(), inner.end());
        return true;
    }

    if (mnemonic == "JMP" || mnemonic == "CALL") {
        std::vector<Operand> ops;
        if (!parse_ops(1, ops)) return false;
        const Operand &target = ops[0];
        if (target.kind == OperandKind::Label || target.kind == OperandKind::Imm) {
            const size_t insn_size = 5;
            uint32_t rel32 = 0;
            if (target.kind == OperandKind::Label) {
                int64_t rel = 0;
                if (labels != nullptr) {
                    auto it = labels->find(target.label);
                    if (it == labels->end()) {
                        error = "undefined label '" + target.label + "'";
                        return false;
                    }
                    rel = (int64_t)it->second - (int64_t)(offset + insn_size);
                    if (rel < INT32_MIN || rel > INT32_MAX) {
                        error = "branch target is out of rel32 range";
                        return false;
                    }
                }
                rel32 = (uint32_t)(int32_t)rel;
            } else {
                const uint32_t next_eip =
                    base_address + (uint32_t)offset + (uint32_t)insn_size;
                rel32 = target.imm - next_eip;
            }
            out.push_back(mnemonic == "JMP" ? 0xE9 : 0xE8);
            emit_u32(out, rel32);
            return true;
        }
        if ((target.kind == OperandKind::Reg && target.width == 32) ||
            (target.kind == OperandKind::Mem && width_ok(target, 32))) {
            out.push_back(0xFF);
            return encode_rm(out, mnemonic == "JMP" ? 4 : 2, target, error);
        }
        error = mnemonic +
                " expects an address/label, reg32, or dword ptr [memory]";
        return false;
    }

    if (mnemonic == "LOOP" || mnemonic == "LOOPE" || mnemonic == "LOOPZ" ||
        mnemonic == "LOOPNE" || mnemonic == "LOOPNZ" || mnemonic == "JECXZ") {
        std::vector<Operand> ops;
        if (!parse_ops(1, ops) ||
            (ops[0].kind != OperandKind::Label && ops[0].kind != OperandKind::Imm)) {
            if (error.empty()) error = mnemonic + " target must be an address or label";
            return false;
        }
        int64_t rel = 0;
        if (ops[0].kind == OperandKind::Label) {
            if (labels != nullptr) {
                auto it = labels->find(ops[0].label);
                if (it == labels->end()) {
                    error = "undefined label '" + ops[0].label + "'";
                    return false;
                }
                rel = (int64_t)it->second - (int64_t)(offset + 2);
            }
        } else {
            const uint32_t next_eip = base_address + (uint32_t)offset + 2u;
            rel = (int32_t)(ops[0].imm - next_eip);
        }
        if (rel < INT8_MIN || rel > INT8_MAX) {
            error = mnemonic + " target is out of rel8 range";
            return false;
        }
        const uint8_t opcode = mnemonic == "LOOP" ? 0xE2 :
                               (mnemonic == "LOOPE" || mnemonic == "LOOPZ") ? 0xE1 :
                               (mnemonic == "LOOPNE" || mnemonic == "LOOPNZ") ? 0xE0 : 0xE3;
        out.push_back(opcode);
        out.push_back((uint8_t)(int8_t)rel);
        return true;
    }

    if (!mnemonic.empty() && mnemonic[0] == 'J') {
        uint8_t cc = 0;
        if (condition_code(mnemonic.substr(1), cc)) {
            std::vector<Operand> ops;
            if (!parse_ops(1, ops) ||
                (ops[0].kind != OperandKind::Label && ops[0].kind != OperandKind::Imm)) {
                if (error.empty()) error = mnemonic + " target must be an address or label";
                return false;
            }
            const size_t insn_size = 6;
            uint32_t rel32 = 0;
            if (ops[0].kind == OperandKind::Label) {
                int64_t rel = 0;
                if (labels != nullptr) {
                    auto it = labels->find(ops[0].label);
                    if (it == labels->end()) {
                        error = "undefined label '" + ops[0].label + "'";
                        return false;
                    }
                    rel = (int64_t)it->second - (int64_t)(offset + insn_size);
                    if (rel < INT32_MIN || rel > INT32_MAX) {
                        error = "branch target is out of rel32 range";
                        return false;
                    }
                }
                rel32 = (uint32_t)(int32_t)rel;
            } else {
                const uint32_t next_eip =
                    base_address + (uint32_t)offset + (uint32_t)insn_size;
                rel32 = ops[0].imm - next_eip;
            }
            out.push_back(0x0F);
            out.push_back((uint8_t)(0x80 + cc));
            emit_u32(out, rel32);
            return true;
        }
    }

    if (mnemonic == "PUSH" || mnemonic == "POP") {
        std::vector<Operand> ops;
        if (!parse_ops(1, ops)) return false;
        if (ops[0].kind == OperandKind::Reg &&
            (ops[0].width == 16 || ops[0].width == 32)) {
            emit_operand_size_prefix(out, ops[0].width);
            out.push_back((uint8_t)((mnemonic == "PUSH" ? 0x50 : 0x58) + ops[0].reg));
            return true;
        }
        if (mnemonic == "PUSH" && ops[0].kind == OperandKind::Imm) {
            out.push_back(0x68); emit_u32(out, ops[0].imm); return true;
        }
        if (ops[0].kind == OperandKind::Mem) {
            int width = operand_width(ops[0], 32);
            if (width != 16 && width != 32) {
                error = mnemonic + " memory operand must be word/dword";
                return false;
            }
            emit_operand_size_prefix(out, width);
            out.push_back(mnemonic == "PUSH" ? 0xFF : 0x8F);
            return encode_rm(out, mnemonic == "PUSH" ? 6 : 0, ops[0], error);
        }
        error = mnemonic + " supports reg16/reg32" +
                (mnemonic == "PUSH" ? ", immediate," : "") +
                " or word/dword ptr [memory]";
        return false;
    }

    if (mnemonic == "INC" || mnemonic == "DEC" || mnemonic == "NEG" ||
        mnemonic == "NOT") {
        std::vector<Operand> ops;
        if (!parse_ops(1, ops)) return false;
        if (ops[0].kind != OperandKind::Reg && ops[0].kind != OperandKind::Mem) {
            error = mnemonic + " expects a register or memory operand";
            return false;
        }
        int width = operand_width(ops[0], 32);
        if (!width_is_integer(width)) {
            error = mnemonic + " expects an 8/16/32-bit operand";
            return false;
        }
        if (mnemonic == "INC" || mnemonic == "DEC") {
            if (ops[0].kind == OperandKind::Reg && width != 8) {
                emit_operand_size_prefix(out, width);
                out.push_back((uint8_t)((mnemonic == "INC" ? 0x40 : 0x48) + ops[0].reg));
                return true;
            }
            if (width == 8) {
                out.push_back(0xFE);
            } else {
                emit_operand_size_prefix(out, width);
                out.push_back(0xFF);
            }
            return encode_rm(out, mnemonic == "INC" ? 0 : 1, ops[0], error);
        }
        if (width == 8) {
            out.push_back(0xF6);
        } else {
            emit_operand_size_prefix(out, width);
            out.push_back(0xF7);
        }
        return encode_rm(out, mnemonic == "NOT" ? 2 : 3, ops[0], error);
    }

    if (mnemonic == "LEA") {
        std::vector<Operand> ops;
        if (!parse_ops(2, ops)) return false;
        if (ops[0].kind != OperandKind::Reg ||
            (ops[0].width != 16 && ops[0].width != 32) ||
            ops[1].kind != OperandKind::Mem) {
            error = "LEA expects reg16/reg32, [memory]";
            return false;
        }
        emit_operand_size_prefix(out, ops[0].width);
        out.push_back(0x8D);
        return encode_rm(out, ops[0].reg, ops[1], error);
    }

    if (mnemonic == "MOVZX" || mnemonic == "MOVSX") {
        std::vector<Operand> ops;
        if (!parse_ops(2, ops)) return false;
        if (ops[0].kind != OperandKind::Reg ||
            (ops[0].width != 16 && ops[0].width != 32) ||
            (ops[1].kind != OperandKind::Reg && ops[1].kind != OperandKind::Mem)) {
            error = mnemonic + " expects reg16/reg32, reg8/reg16 or byte/word ptr [memory]";
            return false;
        }
        int src_width = operand_width(ops[1], 0);
        if (src_width == 0 && ops[1].kind == OperandKind::Mem) {
            error = mnemonic + " memory source requires byte ptr or word ptr";
            return false;
        }
        if (src_width != 8 && !(ops[0].width == 32 && src_width == 16)) {
            error = mnemonic + " source must be 8-bit, or 16-bit when destination is reg32";
            return false;
        }
        emit_operand_size_prefix(out, ops[0].width);
        out.push_back(0x0F);
        if (mnemonic == "MOVZX") out.push_back(src_width == 8 ? 0xB6 : 0xB7);
        else out.push_back(src_width == 8 ? 0xBE : 0xBF);
        return encode_rm(out, ops[0].reg, ops[1], error);
    }

    if (mnemonic == "IMUL") {
        std::vector<Operand> ops;
        if (ops_text.size() == 1) {
            if (!parse_ops(1, ops)) return false;
            if (ops[0].kind != OperandKind::Reg && ops[0].kind != OperandKind::Mem) {
                error = "IMUL one-operand form expects register or memory";
                return false;
            }
            int width = operand_width(ops[0], 32);
            if (!width_is_integer(width)) {
                error = "IMUL expects an 8/16/32-bit operand";
                return false;
            }
            if (width == 8) {
                out.push_back(0xF6);
            } else {
                emit_operand_size_prefix(out, width);
                out.push_back(0xF7);
            }
            return encode_rm(out, 5, ops[0], error);
        }
        if (ops_text.size() == 2) {
            if (!parse_ops(2, ops)) return false;
            if (ops[0].kind != OperandKind::Reg ||
                (ops[0].width != 16 && ops[0].width != 32) ||
                (ops[1].kind != OperandKind::Reg && ops[1].kind != OperandKind::Mem) ||
                !width_ok(ops[1], ops[0].width)) {
                error = "IMUL expects reg16/reg32, matching register/[memory]";
                return false;
            }
            emit_operand_size_prefix(out, ops[0].width);
            out.push_back(0x0F); out.push_back(0xAF);
            return encode_rm(out, ops[0].reg, ops[1], error);
        }
        if (ops_text.size() == 3) {
            if (!parse_ops(3, ops)) return false;
            if (ops[0].kind != OperandKind::Reg ||
                (ops[0].width != 16 && ops[0].width != 32) ||
                (ops[1].kind != OperandKind::Reg && ops[1].kind != OperandKind::Mem) ||
                !width_ok(ops[1], ops[0].width) || ops[2].kind != OperandKind::Imm) {
                error = "IMUL three-operand form expects reg16/reg32, matching register/[memory], immediate";
                return false;
            }
            emit_operand_size_prefix(out, ops[0].width);
            out.push_back(0x69);
            if (!encode_rm(out, ops[0].reg, ops[1], error)) return false;
            if (ops[0].width == 16) emit_u16(out, (uint16_t)ops[2].imm);
            else emit_u32(out, ops[2].imm);
            return true;
        }
        error = "IMUL expects 1, 2, or 3 operands";
        return false;
    }

    if (mnemonic == "MUL" || mnemonic == "DIV" || mnemonic == "IDIV") {
        std::vector<Operand> ops;
        if (!parse_ops(1, ops)) return false;
        if (ops[0].kind != OperandKind::Reg && ops[0].kind != OperandKind::Mem) {
            error = mnemonic + " expects a register or memory operand";
            return false;
        }
        int width = operand_width(ops[0], 32);
        if (!width_is_integer(width)) {
            error = mnemonic + " expects an 8/16/32-bit operand";
            return false;
        }
        if (width == 8) out.push_back(0xF6);
        else {
            emit_operand_size_prefix(out, width);
            out.push_back(0xF7);
        }
        const int ext = mnemonic == "MUL" ? 4 : mnemonic == "DIV" ? 6 : 7;
        return encode_rm(out, ext, ops[0], error);
    }

    if (mnemonic == "SHL" || mnemonic == "SAL" || mnemonic == "SHR" ||
        mnemonic == "SAR" || mnemonic == "ROL" || mnemonic == "ROR") {
        if (ops_text.size() != 2) {
            error = mnemonic + " expects 2 operands";
            return false;
        }
        std::string first_error;
        Operand target = parse_operand(ops_text[0], first_error);
        if (target.kind == OperandKind::Invalid ||
            (target.kind != OperandKind::Reg && target.kind != OperandKind::Mem)) {
            error = first_error.empty() ? mnemonic + " requires a register or memory target"
                                        : first_error;
            return false;
        }
        int width = operand_width(target, 32);
        if (!width_is_integer(width)) {
            error = mnemonic + " requires an 8/16/32-bit target";
            return false;
        }
        const int ext = (mnemonic == "ROL") ? 0 : (mnemonic == "ROR") ? 1 :
                        (mnemonic == "SHR") ? 5 : (mnemonic == "SAR") ? 7 : 4;
        if (upper(trim(ops_text[1])) == "CL") {
            if (width == 8) out.push_back(0xD2);
            else {
                emit_operand_size_prefix(out, width);
                out.push_back(0xD3);
            }
            return encode_rm(out, ext, target, error);
        }
        std::string count_error;
        Operand count = parse_operand(ops_text[1], count_error);
        if (count.kind != OperandKind::Imm || count.imm > 0xffu) {
            error = mnemonic + " count must be CL or an 8-bit immediate";
            return false;
        }
        if (width == 8) out.push_back(0xC0);
        else {
            emit_operand_size_prefix(out, width);
            out.push_back(0xC1);
        }
        if (!encode_rm(out, ext, target, error)) return false;
        out.push_back((uint8_t)count.imm);
        return true;
    }

    if (mnemonic == "MOV") {
        std::vector<Operand> ops;
        if (!parse_ops(2, ops)) return false;
        Operand &d = ops[0]; Operand &s = ops[1];

        if (d.kind == OperandKind::Reg && d.width == 32 &&
            s.kind == OperandKind::Label) {
            uint32_t absolute = base_address;
            if (labels != nullptr) {
                auto it = labels->find(s.label);
                if (it == labels->end()) {
                    error = "undefined label '" + s.label + "'";
                    return false;
                }
                absolute += (uint32_t)it->second;
            }
            out.push_back((uint8_t)(0xB8 + d.reg));
            emit_u32(out, absolute);
            return true;
        }

        if (d.kind == OperandKind::Reg && s.kind == OperandKind::Imm) {
            if (d.width == 8) {
                out.push_back((uint8_t)(0xB0 + d.reg));
                out.push_back((uint8_t)s.imm);
                return true;
            }
            if (d.width == 16 || d.width == 32) {
                emit_operand_size_prefix(out, d.width);
                out.push_back((uint8_t)(0xB8 + d.reg));
                if (d.width == 16) emit_u16(out, (uint16_t)s.imm);
                else emit_u32(out, s.imm);
                return true;
            }
        }

        if (d.kind == OperandKind::Reg &&
            (s.kind == OperandKind::Reg || s.kind == OperandKind::Mem)) {
            int width = d.width;
            if (!width_is_integer(width) || !width_ok(s, width)) {
                error = "MOV source width does not match destination register";
                return false;
            }
            if (s.kind == OperandKind::Reg && s.width != width) {
                error = "MOV register widths must match";
                return false;
            }
            if (width == 8) out.push_back(0x8A);
            else {
                emit_operand_size_prefix(out, width);
                out.push_back(0x8B);
            }
            return encode_rm(out, d.reg, s, error);
        }

        if (d.kind == OperandKind::Mem && s.kind == OperandKind::Reg) {
            int width = s.width;
            if (!width_is_integer(width) || !width_ok(d, width)) {
                error = "MOV memory width does not match source register";
                return false;
            }
            if (width == 8) out.push_back(0x88);
            else {
                emit_operand_size_prefix(out, width);
                out.push_back(0x89);
            }
            return encode_rm(out, s.reg, d, error);
        }

        if (d.kind == OperandKind::Mem && s.kind == OperandKind::Label) {
            int width = d.mem.width;
            if (width != 32) {
                error = "MOV label to memory requires dword ptr";
                return false;
            }
            uint32_t absolute = base_address;
            if (labels != nullptr) {
                auto it = labels->find(s.label);
                if (it == labels->end()) {
                    error = "undefined label '" + s.label + "'";
                    return false;
                }
                absolute += (uint32_t)it->second;
            }
            out.push_back(0xC7);
            if (!encode_rm(out, 0, d, error)) return false;
            emit_u32(out, absolute);
            return true;
        }

        if (d.kind == OperandKind::Mem && s.kind == OperandKind::Imm) {
            int width = d.mem.width;
            if (width == 0) {
                error = "MOV immediate to memory requires byte/word/dword ptr";
                return false;
            }
            if (width == 8) {
                out.push_back(0xC6);
                if (!encode_rm(out, 0, d, error)) return false;
                out.push_back((uint8_t)s.imm);
                return true;
            }
            if (width == 16) {
                out.push_back(0x66); out.push_back(0xC7);
                if (!encode_rm(out, 0, d, error)) return false;
                emit_u16(out, (uint16_t)s.imm);
                return true;
            }
            if (width == 32) {
                out.push_back(0xC7);
                if (!encode_rm(out, 0, d, error)) return false;
                emit_u32(out, s.imm);
                return true;
            }
        }
        error = "unsupported MOV operand combination";
        return false;
    }

    static const std::unordered_map<std::string, int> alu_ext = {
        {"ADD",0},{"OR",1},{"ADC",2},{"SBB",3},{"AND",4},{"SUB",5},{"XOR",6},{"CMP",7},
    };
    auto ait = alu_ext.find(mnemonic);
    if (ait != alu_ext.end()) {
        std::vector<Operand> ops;
        if (!parse_ops(2, ops)) return false;
        Operand &d = ops[0]; Operand &s = ops[1];
        if (d.kind != OperandKind::Reg && d.kind != OperandKind::Mem) {
            error = mnemonic + " destination must be register or memory";
            return false;
        }

        int width = operand_width(d, (s.kind == OperandKind::Reg) ? s.width : 32);
        if (!width_is_integer(width)) {
            error = mnemonic + " requires an 8/16/32-bit destination";
            return false;
        }

        if (s.kind == OperandKind::Imm) {
            if (d.kind == OperandKind::Mem && d.mem.width == 0) {
                width = 32; // preserve the original F0 default for [mem],imm
            }
            if (d.kind == OperandKind::Reg && d.reg == 0) {
                static const uint8_t acc8[] = {0x04,0x0C,0x14,0x1C,0x24,0x2C,0x34,0x3C};
                static const uint8_t acc[]  = {0x05,0x0D,0x15,0x1D,0x25,0x2D,0x35,0x3D};
                if (width == 8) {
                    out.push_back(acc8[ait->second]);
                    out.push_back((uint8_t)s.imm);
                } else {
                    emit_operand_size_prefix(out, width);
                    out.push_back(acc[ait->second]);
                    if (width == 16) emit_u16(out, (uint16_t)s.imm);
                    else emit_u32(out, s.imm);
                }
                return true;
            }
            if (width == 8) out.push_back(0x80);
            else {
                emit_operand_size_prefix(out, width);
                out.push_back(0x81);
            }
            if (!encode_rm(out, ait->second, d, error)) return false;
            if (width == 8) out.push_back((uint8_t)s.imm);
            else if (width == 16) emit_u16(out, (uint16_t)s.imm);
            else emit_u32(out, s.imm);
            return true;
        }

        if (s.kind == OperandKind::Reg) {
            if (s.width != width || !width_ok(d, width)) {
                error = mnemonic + " operand widths must match";
                return false;
            }
            static const uint8_t rm_reg8[] = {0x00,0x08,0x10,0x18,0x20,0x28,0x30,0x38};
            static const uint8_t rm_reg[]  = {0x01,0x09,0x11,0x19,0x21,0x29,0x31,0x39};
            if (width == 8) out.push_back(rm_reg8[ait->second]);
            else {
                emit_operand_size_prefix(out, width);
                out.push_back(rm_reg[ait->second]);
            }
            return encode_rm(out, s.reg, d, error);
        }

        if (d.kind == OperandKind::Reg && s.kind == OperandKind::Mem) {
            width = d.width;
            if (!width_ok(s, width)) {
                error = mnemonic + " operand widths must match";
                return false;
            }
            static const uint8_t reg_rm8[] = {0x02,0x0A,0x12,0x1A,0x22,0x2A,0x32,0x3A};
            static const uint8_t reg_rm[]  = {0x03,0x0B,0x13,0x1B,0x23,0x2B,0x33,0x3B};
            if (width == 8) out.push_back(reg_rm8[ait->second]);
            else {
                emit_operand_size_prefix(out, width);
                out.push_back(reg_rm[ait->second]);
            }
            return encode_rm(out, d.reg, s, error);
        }

        error = "unsupported " + mnemonic + " operand combination";
        return false;
    }

    if (mnemonic == "TEST") {
        std::vector<Operand> ops;
        if (!parse_ops(2, ops)) return false;
        Operand &d = ops[0]; Operand &s = ops[1];
        if (d.kind != OperandKind::Reg && d.kind != OperandKind::Mem) {
            error = "TEST destination must be register or memory";
            return false;
        }
        int width = operand_width(d, (s.kind == OperandKind::Reg) ? s.width : 32);
        if (!width_is_integer(width)) {
            error = "TEST requires an 8/16/32-bit destination";
            return false;
        }
        if (s.kind == OperandKind::Reg) {
            if (s.width != width || !width_ok(d, width)) {
                error = "TEST operand widths must match";
                return false;
            }
            if (width == 8) out.push_back(0x84);
            else {
                emit_operand_size_prefix(out, width);
                out.push_back(0x85);
            }
            return encode_rm(out, s.reg, d, error);
        }
        if (s.kind == OperandKind::Imm) {
            if (d.kind == OperandKind::Mem && d.mem.width == 0) width = 32;
            if (d.kind == OperandKind::Reg && d.reg == 0) {
                if (width == 8) {
                    out.push_back(0xA8);
                    out.push_back((uint8_t)s.imm);
                } else {
                    emit_operand_size_prefix(out, width);
                    out.push_back(0xA9);
                    if (width == 16) emit_u16(out, (uint16_t)s.imm);
                    else emit_u32(out, s.imm);
                }
                return true;
            }
            if (width == 8) out.push_back(0xF6);
            else {
                emit_operand_size_prefix(out, width);
                out.push_back(0xF7);
            }
            if (!encode_rm(out, 0, d, error)) return false;
            if (width == 8) out.push_back((uint8_t)s.imm);
            else if (width == 16) emit_u16(out, (uint16_t)s.imm);
            else emit_u32(out, s.imm);
            return true;
        }
        error = "unsupported TEST operand combination";
        return false;
    }

    if (mnemonic == "XCHG" || mnemonic == "XADD" || mnemonic == "CMPXCHG") {
        std::vector<Operand> ops;
        if (!parse_ops(2, ops)) return false;
        Operand &d = ops[0]; Operand &s = ops[1];
        if ((d.kind != OperandKind::Reg && d.kind != OperandKind::Mem) ||
            s.kind != OperandKind::Reg) {
            error = mnemonic + " expects register/[memory], register";
            return false;
        }
        int width = operand_width(d, s.width);
        if (!width_is_integer(width) || s.width != width || !width_ok(d, width)) {
            error = mnemonic + " operand widths must match (8/16/32-bit)";
            return false;
        }
        emit_operand_size_prefix(out, width);
        if (mnemonic == "XCHG") {
            out.push_back(width == 8 ? 0x86 : 0x87);
        } else {
            out.push_back(0x0F);
            if (mnemonic == "XADD") out.push_back(width == 8 ? 0xC0 : 0xC1);
            else out.push_back(width == 8 ? 0xB0 : 0xB1);
        }
        return encode_rm(out, s.reg, d, error);
    }

    if (mnemonic == "BSWAP") {
        std::vector<Operand> ops;
        if (!parse_ops(1, ops)) return false;
        if (ops[0].kind != OperandKind::Reg || ops[0].width != 32) {
            error = "BSWAP expects reg32";
            return false;
        }
        out.push_back(0x0F);
        out.push_back((uint8_t)(0xC8 + ops[0].reg));
        return true;
    }

    if (mnemonic == "BSF" || mnemonic == "BSR") {
        std::vector<Operand> ops;
        if (!parse_ops(2, ops)) return false;
        if (ops[0].kind != OperandKind::Reg ||
            (ops[0].width != 16 && ops[0].width != 32) ||
            (ops[1].kind != OperandKind::Reg && ops[1].kind != OperandKind::Mem) ||
            !width_ok(ops[1], ops[0].width) ||
            (ops[1].kind == OperandKind::Reg && ops[1].width != ops[0].width)) {
            error = mnemonic + " expects reg16/reg32, matching register/[memory]";
            return false;
        }
        emit_operand_size_prefix(out, ops[0].width);
        out.push_back(0x0F);
        out.push_back(mnemonic == "BSF" ? 0xBC : 0xBD);
        return encode_rm(out, ops[0].reg, ops[1], error);
    }

    if (mnemonic == "BT" || mnemonic == "BTS" || mnemonic == "BTR" || mnemonic == "BTC") {
        std::vector<Operand> ops;
        if (!parse_ops(2, ops)) return false;
        Operand &d = ops[0]; Operand &s = ops[1];
        if (d.kind != OperandKind::Reg && d.kind != OperandKind::Mem) {
            error = mnemonic + " expects register/[memory] destination";
            return false;
        }
        int width = operand_width(d, (s.kind == OperandKind::Reg) ? s.width : 32);
        if (width != 16 && width != 32) {
            error = mnemonic + " destination must be 16/32-bit";
            return false;
        }
        emit_operand_size_prefix(out, width);
        out.push_back(0x0F);
        if (s.kind == OperandKind::Reg) {
            if (s.width != width || !width_ok(d, width)) {
                error = mnemonic + " register operand widths must match";
                return false;
            }
            const uint8_t opcode = mnemonic == "BT" ? 0xA3 :
                                   mnemonic == "BTS" ? 0xAB :
                                   mnemonic == "BTR" ? 0xB3 : 0xBB;
            out.push_back(opcode);
            return encode_rm(out, s.reg, d, error);
        }
        if (s.kind == OperandKind::Imm && s.imm <= 0xffu) {
            out.push_back(0xBA);
            const int ext = mnemonic == "BT" ? 4 : mnemonic == "BTS" ? 5 :
                            mnemonic == "BTR" ? 6 : 7;
            if (!encode_rm(out, ext, d, error)) return false;
            out.push_back((uint8_t)s.imm);
            return true;
        }
        error = mnemonic + " second operand must be matching register or imm8";
        return false;
    }

    if (mnemonic == "SHLD" || mnemonic == "SHRD") {
        std::vector<Operand> ops;
        if (!parse_ops(3, ops)) return false;
        Operand &d = ops[0]; Operand &s = ops[1];
        if ((d.kind != OperandKind::Reg && d.kind != OperandKind::Mem) ||
            s.kind != OperandKind::Reg) {
            error = mnemonic + " expects register/[memory], register, CL/imm8";
            return false;
        }
        int width = operand_width(d, s.width);
        if ((width != 16 && width != 32) || s.width != width || !width_ok(d, width)) {
            error = mnemonic + " first two operands must have matching 16/32-bit widths";
            return false;
        }
        emit_operand_size_prefix(out, width);
        out.push_back(0x0F);
        const bool use_cl = ops[2].kind == OperandKind::Reg &&
                            ops[2].width == 8 && ops[2].reg == 1;
        if (use_cl) {
            out.push_back(mnemonic == "SHLD" ? 0xA5 : 0xAD);
            return encode_rm(out, s.reg, d, error);
        }
        if (ops[2].kind == OperandKind::Imm && ops[2].imm <= 0xffu) {
            out.push_back(mnemonic == "SHLD" ? 0xA4 : 0xAC);
            if (!encode_rm(out, s.reg, d, error)) return false;
            out.push_back((uint8_t)ops[2].imm);
            return true;
        }
        error = mnemonic + " count must be CL or imm8";
        return false;
    }

    if (mnemonic.rfind("SET", 0) == 0 && mnemonic.size() > 3) {
        uint8_t cc = 0;
        if (condition_code(mnemonic.substr(3), cc)) {
            std::vector<Operand> ops;
            if (!parse_ops(1, ops)) return false;
            if (!((ops[0].kind == OperandKind::Reg && ops[0].width == 8) ||
                  (ops[0].kind == OperandKind::Mem && width_ok(ops[0], 8)))) {
                error = mnemonic + " expects reg8 or byte ptr [memory]";
                return false;
            }
            out.push_back(0x0F);
            out.push_back((uint8_t)(0x90 + cc));
            return encode_rm(out, 0, ops[0], error);
        }
    }

    if (mnemonic.rfind("CMOV", 0) == 0 && mnemonic.size() > 4) {
        uint8_t cc = 0;
        if (condition_code(mnemonic.substr(4), cc)) {
            std::vector<Operand> ops;
            if (!parse_ops(2, ops)) return false;
            if (ops[0].kind != OperandKind::Reg ||
                (ops[0].width != 16 && ops[0].width != 32) ||
                (ops[1].kind != OperandKind::Reg && ops[1].kind != OperandKind::Mem) ||
                !width_ok(ops[1], ops[0].width) ||
                (ops[1].kind == OperandKind::Reg && ops[1].width != ops[0].width)) {
                error = mnemonic + " expects reg16/reg32, matching register/[memory]";
                return false;
            }
            emit_operand_size_prefix(out, ops[0].width);
            out.push_back(0x0F);
            out.push_back((uint8_t)(0x40 + cc));
            return encode_rm(out, ops[0].reg, ops[1], error);
        }
    }

    error = "unsupported x86 instruction '" + mnemonic + "'";
    return false;
}

} // namespace xemu_cheat_assembler_internal
