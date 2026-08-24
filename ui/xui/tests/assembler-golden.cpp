// v2.87 current regression ownership.
#include "../x86-cheat-assembler.hh"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

uint64_t fnv1a64(const std::vector<uint8_t> &bytes)
{
    uint64_t hash = 1469598103934665603ULL;
    for (uint8_t byte : bytes) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    return hash;
}

struct Expected {
    const char *name;
    std::vector<const char *> lines;
    bool ok;
    int error_line;
    size_t byte_size;
    uint64_t byte_hash;
    size_t data_size;
    uint64_t data_hash;
    uint32_t preserve_bytes;
    uint32_t temp_bytes;
    const char *error;
};

bool run_case(const Expected &test)
{
    std::vector<XemuCheatAsmLine> lines;
    lines.reserve(test.lines.size());
    int source_line = 1;
    for (const char *text : test.lines) {
        lines.push_back({source_line++, text});
    }

    XemuCheatAsmResult result;
    const bool ok = xemu_cheat_assemble_x86_32_at(
        lines, 0x68010000u, 0x680F0000u, 0x680F1000u, result);

    const bool match =
        ok == test.ok &&
        result.error_line == test.error_line &&
        result.bytes.size() == test.byte_size &&
        fnv1a64(result.bytes) == test.byte_hash &&
        result.data.size() == test.data_size &&
        fnv1a64(result.data) == test.data_hash &&
        result.preserve_bytes == test.preserve_bytes &&
        result.temp_bytes == test.temp_bytes &&
        result.error == test.error;

    if (!match) {
        std::fprintf(stderr,
                     "FAIL %-16s ok=%d line=%d bytes=%zu/%016llX "
                     "data=%zu/%016llX preserve=%u temp=%u error='%s'\n",
                     test.name, ok ? 1 : 0, result.error_line,
                     result.bytes.size(),
                     static_cast<unsigned long long>(fnv1a64(result.bytes)),
                     result.data.size(),
                     static_cast<unsigned long long>(fnv1a64(result.data)),
                     result.preserve_bytes, result.temp_bytes,
                     result.error.c_str());
        return false;
    }
    return true;
}

} // namespace

int main()
{
    const std::vector<Expected> tests = {
        {"basic_mov",
         {"mov eax, 12345678", "add eax, 4", "cmp eax, 1234567C",
          "jne Skip", "xor ecx, ecx", "Skip:", "ret"},
         true, 0, 24, 0x86441D67C64591F0ULL,
         0, 0x14650FB0739D0383ULL, 0, 0, ""},
        {"call_label",
         {"call Worker", "jmp Done", "Worker:", "mov eax, 1", "ret",
          "Done:", "nop"},
         true, 0, 17, 0xB69BDF0E6181E301ULL,
         0, 0x14650FB0739D0383ULL, 0, 0, ""},
        {"direct_branch",
         {"jmp 68010020", "call 68010040", "jne 68010060"},
         true, 0, 16, 0x5CD1AF99D4D3EC5DULL,
         0, 0x14650FB0739D0383ULL, 0, 0, ""},
        {"dd_label",
         {"mov edx, CarList", "mov eax, [edx]", "ret", "CarList:",
          "dd 01D28710, 8B80C5FC, E3BDE8CB"},
         true, 0, 8, 0x04D151A86F814DD3ULL,
         12, 0xEE65274E1CF3034AULL, 0, 0, ""},
        {"preserve",
         {"PRESERVE EAX, ECX, EDX", "mov eax, 1234", "add ecx, eax",
          "RESTORE", "ret"},
         true, 0, 529, 0xE9A08D7E882F71ABULL,
         0, 0x14650FB0739D0383ULL, 784, 0, ""},
        {"preserve_all",
         {"PRESERVEALL", "xor eax, eax", "inc eax", "RESTORE", "ret"},
         true, 0, 559, 0x66D7AEFCECB2D1FAULL,
         0, 0x14650FB0739D0383ULL, 784, 0, ""},
        {"t_dlc",
         {"mov T0, CarList", "mov T1, 6", "CheckCar:",
          "cmp eax, dword ptr [T0]", "je Success", "add T0, 4",
          "dec T1", "jnz CheckCar", "jmp Original", "Success:",
          "mov eax, 1", "jmp Done", "Original:", "mov ecx, 2",
          "Done:", "ret", "CarList:",
          "dd 01D28710, 8B80C5FC, E3BDE8CB, 0001308E, 60D6890E, 0689441B"},
         true, 0, 234, 0x29D69F630DD581F5ULL,
         24, 0x68A13D6DCBEED082ULL, 0, 40, ""},
        {"t_indirect",
         {"mov T0, 81234567", "mov T1, [T0]", "add T1, 10",
          "mov [T0], T1", "cmp T1, 20", "setne al", "ret"},
         true, 0, 138, 0x7FCA9B10F386A2CCULL,
         0, 0x14650FB0739D0383ULL, 0, 40, ""},
        {"t_test_jcc",
         {"mov T2, 80000000", "test T2, T2", "jz Zero", "shr T2, 1",
          "Zero:", "ret"},
         true, 0, 121, 0xCF171381B659683BULL,
         0, 0x14650FB0739D0383ULL, 0, 40, ""},
        {"cmov",
         {"mov eax, 1", "cmp eax, 2", "mov ecx, 3", "mov edx, 4",
          "cmovne ecx, edx", "ret"},
         true, 0, 24, 0x6F4300A6600A9748ULL,
         0, 0x14650FB0739D0383ULL, 0, 0, ""},
        {"bitops",
         {"mov eax, 12345678", "and eax, FFFFFFF0", "or eax, 5",
          "xor eax, 10", "shl eax, 2", "shr eax, 1", "ret"},
         true, 0, 27, 0xE589D47BE24C2B0EULL,
         0, 0x14650FB0739D0383ULL, 0, 0, ""},
        {"mem_modes",
         {"mov eax, [ebx+ecx*4+20]", "mov [esi+10], eax",
          "lea edx, [ebp+eax*2-4]", "ret"},
         true, 0, 12, 0xE9EEB9A0BB151415ULL,
         0, 0x14650FB0739D0383ULL, 0, 0, ""},
        {"loop",
         {"mov ecx, 3", "Again:", "dec eax", "loop Again", "ret"},
         true, 0, 9, 0x15E88B36ACB63247ULL,
         0, 0x14650FB0739D0383ULL, 0, 0, ""},
        {"pushpop",
         {"push eax", "push 1234", "pop ecx", "pushfd", "popfd", "ret"},
         true, 0, 10, 0x6C805DDC538C6B9CULL,
         0, 0x14650FB0739D0383ULL, 0, 0, ""},
        {"error_badop", {"frobnicate eax, 1"},
         false, 1, 0, 0x14650FB0739D0383ULL,
         0, 0x14650FB0739D0383ULL, 0, 0,
         "unsupported x86 instruction 'FROBNICATE'"},
        {"error_bad_t", {"mov T8, 1"},
         false, 1, 0, 0x14650FB0739D0383ULL,
         0, 0x14650FB0739D0383ULL, 0, 0,
         "unsupported MOV operand combination"},
    };

    for (const Expected &test : tests) {
        if (!run_case(test)) {
            return 1;
        }
    }

    XemuCheatAsmResult change;
    if (!xemu_cheat_assemble_x86_32_change_instruction(
            "jmp 0008C5A2", 0x0008C590u, 2u, change) ||
        change.bytes != std::vector<uint8_t>({0xEB, 0x10})) {
        std::fprintf(stderr, "FAIL change_short_jmp\n");
        return 1;
    }
    if (!xemu_cheat_assemble_x86_32_change_instruction(
            "jne 0008C5A2", 0x0008C590u, 2u, change) ||
        change.bytes != std::vector<uint8_t>({0x75, 0x10})) {
        std::fprintf(stderr, "FAIL change_short_jcc\n");
        return 1;
    }
    if (!xemu_cheat_assemble_x86_32_change_instruction(
            "jmp 00100000", 0x0008C590u, 6u, change) ||
        change.bytes.size() != 5u || change.bytes[0] != 0xE9) {
        std::fprintf(stderr, "FAIL change_near_jmp\n");
        return 1;
    }

    std::printf("PASS: %zu Type-F0 assembler golden cases + Change direct-address branches\n",
                tests.size());
    return 0;
}
