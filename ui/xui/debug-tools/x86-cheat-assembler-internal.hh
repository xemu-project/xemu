//
// xemu RAW Cheat Engine - internal compact x86 assembler encoder API
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace xemu_cheat_assembler_internal {

std::string trim(const std::string &s);
std::string upper(std::string s);
std::string strip_comment(const std::string &s);
bool parse_register(const std::string &name, int &code, int &width);
int reg32(const std::string &name);
bool valid_label(const std::string &name);
bool parse_number(const std::string &text, int64_t &value);
std::vector<std::string> split_operands(const std::string &s);
void emit_u32(std::vector<uint8_t> &out, uint32_t v);
bool condition_code(const std::string &mnemonic, uint8_t &cc);
bool encode_instruction(const std::string &text, size_t offset,
                        const std::unordered_map<std::string, size_t> *labels,
                        uint32_t cave_base, std::vector<uint8_t> &out,
                        std::string &error);

} // namespace xemu_cheat_assembler_internal
