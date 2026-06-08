#ifndef SPIRV_REFLECT_OUTPUT_STREAM_H
#define SPIRV_REFLECT_OUTPUT_STREAM_H

#include <map>
#include <ostream>
#include <string>

#include "spirv_reflect.h"

std::string ToStringSpvSourceLanguage(SpvSourceLanguage lang);
std::string ToStringSpvExecutionModel(SpvExecutionModel model);
std::string ToStringSpvStorageClass(int storage_class);
std::string ToStringSpvDim(SpvDim dim);
std::string ToStringSpvBuiltIn(const SpvReflectInterfaceVariable& variable, bool preface);
std::string ToStringSpvImageFormat(SpvImageFormat fmt);

std::string ToStringGenerator(SpvReflectGenerator generator);
std::string ToStringShaderStage(SpvReflectShaderStageFlagBits stage);
std::string ToStringResourceType(SpvReflectResourceType type);
std::string ToStringDescriptorType(SpvReflectDescriptorType value);
std::string ToStringTypeFlags(SpvReflectTypeFlags type_flags);
std::string ToStringVariableFlags(SpvReflectVariableFlags flags);
std::string ToStringDecorationFlags(SpvReflectDecorationFlags decoration_flags);
std::string ToStringDescriptorType(SpvReflectDescriptorType value);
std::string ToStringFormat(SpvReflectFormat fmt);
std::string ToStringComponentType(const SpvReflectTypeDescription& type, uint32_t member_decoration_flags);
std::string ToStringType(SpvSourceLanguage src_lang, const SpvReflectTypeDescription& type);

// std::ostream& operator<<(std::ostream& os, const spv_reflect::ShaderModule& obj);
void WriteReflection(const spv_reflect::ShaderModule& obj, bool flatten_cbuffers, std::ostream& os);

class SpvReflectToYaml {
 public:
  // verbosity = 0: top-level tables only (module, block variables, interface variables, descriptor bindings).
  // verbosity = 1: everything above, plus type description tables for all public objects.
  // verbosity = 2: everything above, plus SPIRV bytecode and full type description table. HUGE.
  explicit SpvReflectToYaml(const SpvReflectShaderModule& shader_module, uint32_t verbosity = 0);

  friend std::ostream& operator<<(std::ostream& os, SpvReflectToYaml& to_yaml) {
    to_yaml.Write(os);
    return os;
  }

 private:
  void Write(std::ostream& os);

  SpvReflectToYaml(const SpvReflectToYaml&) = delete;
  SpvReflectToYaml(const SpvReflectToYaml&&) = delete;
  static std::string Indent(uint32_t level) { return std::string(2 * level, ' '); }
  static std::string SafeString(const char* str) { return str ? (std::string("\"") + str + "\"") : ""; }
  void WriteTypeDescription(std::ostream& os, const SpvReflectTypeDescription& td, uint32_t indent_level);
  void WriteBlockVariable(std::ostream& os, const SpvReflectBlockVariable& bv, uint32_t indent_level);
  void WriteDescriptorBinding(std::ostream& os, const SpvReflectDescriptorBinding& db, uint32_t indent_level);
  void WriteInterfaceVariable(std::ostream& os, const SpvReflectInterfaceVariable& iv, uint32_t indent_level);

  // Write all SpvReflectTypeDescription objects reachable from the specified objects, if they haven't been
  // written already.
  void WriteBlockVariableTypes(std::ostream& os, const SpvReflectBlockVariable& bv, uint32_t indent_level);
  void WriteDescriptorBindingTypes(std::ostream& os, const SpvReflectDescriptorBinding& db, uint32_t indent_level);
  void WriteInterfaceVariableTypes(std::ostream& os, const SpvReflectInterfaceVariable& iv, uint32_t indent_level);

  const SpvReflectShaderModule& sm_;
  uint32_t verbosity_ = 0;
  std::map<const SpvReflectTypeDescription*, uint32_t> type_description_to_index_;
  std::map<const SpvReflectBlockVariable*, uint32_t> block_variable_to_index_;
  std::map<const SpvReflectDescriptorBinding*, uint32_t> descriptor_binding_to_index_;
  std::map<const SpvReflectInterfaceVariable*, uint32_t> interface_variable_to_index_;
};

#endif
