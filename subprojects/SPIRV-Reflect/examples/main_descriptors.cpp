#include <cassert>

#include "common.h"
#include "sample_spv.h"

#if defined(SPIRV_REFLECT_HAS_VULKAN_H)
#include <vulkan/vulkan.h>
struct DescriptorSetLayoutData {
  uint32_t set_number;
  VkDescriptorSetLayoutCreateInfo create_info;
  std::vector<VkDescriptorSetLayoutBinding> bindings;
};
#endif

int main(int argn, char** argv) {
  SpvReflectShaderModule module = {};
  SpvReflectResult result = spvReflectCreateShaderModule(sizeof(k_sample_spv), k_sample_spv, &module);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  uint32_t count = 0;
  result = spvReflectEnumerateDescriptorSets(&module, &count, NULL);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  std::vector<SpvReflectDescriptorSet*> sets(count);
  result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

#if defined(SPIRV_REFLECT_HAS_VULKAN_H)
  // Demonstrates how to generate all necessary data structures to create a
  // VkDescriptorSetLayout for each descriptor set in this shader.
  std::vector<DescriptorSetLayoutData> set_layouts(sets.size(), DescriptorSetLayoutData{});
  for (size_t i_set = 0; i_set < sets.size(); ++i_set) {
    const SpvReflectDescriptorSet& refl_set = *(sets[i_set]);
    DescriptorSetLayoutData& layout = set_layouts[i_set];
    layout.bindings.resize(refl_set.binding_count);
    for (uint32_t i_binding = 0; i_binding < refl_set.binding_count; ++i_binding) {
      const SpvReflectDescriptorBinding& refl_binding = *(refl_set.bindings[i_binding]);
      VkDescriptorSetLayoutBinding& layout_binding = layout.bindings[i_binding];
      layout_binding.binding = refl_binding.binding;
      layout_binding.descriptorType = static_cast<VkDescriptorType>(refl_binding.descriptor_type);
      layout_binding.descriptorCount = 1;
      for (uint32_t i_dim = 0; i_dim < refl_binding.array.dims_count; ++i_dim) {
        layout_binding.descriptorCount *= refl_binding.array.dims[i_dim];
      }
      layout_binding.stageFlags = static_cast<VkShaderStageFlagBits>(module.shader_stage);
    }
    layout.set_number = refl_set.set;
    layout.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout.create_info.bindingCount = refl_set.binding_count;
    layout.create_info.pBindings = layout.bindings.data();
  }
  // Nothing further is done with set_layouts in this sample; in a real
  // application they would be merged with similar structures from other shader
  // stages and/or pipelines to create a VkPipelineLayout.
#endif

  // Log the descriptor set contents to stdout
  const char* t = "  ";
  const char* tt = "    ";

  PrintModuleInfo(std::cout, module);
  std::cout << "\n\n";

  std::cout << "Descriptor sets:"
            << "\n";
  for (size_t index = 0; index < sets.size(); ++index) {
    auto p_set = sets[index];

    // descriptor sets can also be retrieved directly from the module, by set
    // index
    auto p_set2 = spvReflectGetDescriptorSet(&module, p_set->set, &result);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);
    assert(p_set == p_set2);
    (void)p_set2;

    std::cout << t << index << ":"
              << "\n";
    PrintDescriptorSet(std::cout, *p_set, tt);
    std::cout << "\n\n";
  }

  spvReflectDestroyShaderModule(&module);

  return 0;
}