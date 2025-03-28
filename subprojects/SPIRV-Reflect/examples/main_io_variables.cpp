#include <algorithm>
#include <cassert>

#include "common.h"
#include "sample_spv.h"

#if defined(SPIRV_REFLECT_HAS_VULKAN_H)
#include <vulkan/vulkan.h>
// Returns the size in bytes of the provided VkFormat.
// As this is only intended for vertex attribute formats, not all VkFormats are
// supported.
static uint32_t FormatSize(VkFormat format) {
  uint32_t result = 0;
  switch (format) {
    case VK_FORMAT_UNDEFINED:
      result = 0;
      break;
    case VK_FORMAT_R4G4_UNORM_PACK8:
      result = 1;
      break;
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
      result = 2;
      break;
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
      result = 2;
      break;
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
      result = 2;
      break;
    case VK_FORMAT_B5G6R5_UNORM_PACK16:
      result = 2;
      break;
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
      result = 2;
      break;
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
      result = 2;
      break;
    case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
      result = 2;
      break;
    case VK_FORMAT_R8_UNORM:
      result = 1;
      break;
    case VK_FORMAT_R8_SNORM:
      result = 1;
      break;
    case VK_FORMAT_R8_USCALED:
      result = 1;
      break;
    case VK_FORMAT_R8_SSCALED:
      result = 1;
      break;
    case VK_FORMAT_R8_UINT:
      result = 1;
      break;
    case VK_FORMAT_R8_SINT:
      result = 1;
      break;
    case VK_FORMAT_R8_SRGB:
      result = 1;
      break;
    case VK_FORMAT_R8G8_UNORM:
      result = 2;
      break;
    case VK_FORMAT_R8G8_SNORM:
      result = 2;
      break;
    case VK_FORMAT_R8G8_USCALED:
      result = 2;
      break;
    case VK_FORMAT_R8G8_SSCALED:
      result = 2;
      break;
    case VK_FORMAT_R8G8_UINT:
      result = 2;
      break;
    case VK_FORMAT_R8G8_SINT:
      result = 2;
      break;
    case VK_FORMAT_R8G8_SRGB:
      result = 2;
      break;
    case VK_FORMAT_R8G8B8_UNORM:
      result = 3;
      break;
    case VK_FORMAT_R8G8B8_SNORM:
      result = 3;
      break;
    case VK_FORMAT_R8G8B8_USCALED:
      result = 3;
      break;
    case VK_FORMAT_R8G8B8_SSCALED:
      result = 3;
      break;
    case VK_FORMAT_R8G8B8_UINT:
      result = 3;
      break;
    case VK_FORMAT_R8G8B8_SINT:
      result = 3;
      break;
    case VK_FORMAT_R8G8B8_SRGB:
      result = 3;
      break;
    case VK_FORMAT_B8G8R8_UNORM:
      result = 3;
      break;
    case VK_FORMAT_B8G8R8_SNORM:
      result = 3;
      break;
    case VK_FORMAT_B8G8R8_USCALED:
      result = 3;
      break;
    case VK_FORMAT_B8G8R8_SSCALED:
      result = 3;
      break;
    case VK_FORMAT_B8G8R8_UINT:
      result = 3;
      break;
    case VK_FORMAT_B8G8R8_SINT:
      result = 3;
      break;
    case VK_FORMAT_B8G8R8_SRGB:
      result = 3;
      break;
    case VK_FORMAT_R8G8B8A8_UNORM:
      result = 4;
      break;
    case VK_FORMAT_R8G8B8A8_SNORM:
      result = 4;
      break;
    case VK_FORMAT_R8G8B8A8_USCALED:
      result = 4;
      break;
    case VK_FORMAT_R8G8B8A8_SSCALED:
      result = 4;
      break;
    case VK_FORMAT_R8G8B8A8_UINT:
      result = 4;
      break;
    case VK_FORMAT_R8G8B8A8_SINT:
      result = 4;
      break;
    case VK_FORMAT_R8G8B8A8_SRGB:
      result = 4;
      break;
    case VK_FORMAT_B8G8R8A8_UNORM:
      result = 4;
      break;
    case VK_FORMAT_B8G8R8A8_SNORM:
      result = 4;
      break;
    case VK_FORMAT_B8G8R8A8_USCALED:
      result = 4;
      break;
    case VK_FORMAT_B8G8R8A8_SSCALED:
      result = 4;
      break;
    case VK_FORMAT_B8G8R8A8_UINT:
      result = 4;
      break;
    case VK_FORMAT_B8G8R8A8_SINT:
      result = 4;
      break;
    case VK_FORMAT_B8G8R8A8_SRGB:
      result = 4;
      break;
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2R10G10B10_SINT_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2B10G10R10_UINT_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2B10G10R10_SINT_PACK32:
      result = 4;
      break;
    case VK_FORMAT_R16_UNORM:
      result = 2;
      break;
    case VK_FORMAT_R16_SNORM:
      result = 2;
      break;
    case VK_FORMAT_R16_USCALED:
      result = 2;
      break;
    case VK_FORMAT_R16_SSCALED:
      result = 2;
      break;
    case VK_FORMAT_R16_UINT:
      result = 2;
      break;
    case VK_FORMAT_R16_SINT:
      result = 2;
      break;
    case VK_FORMAT_R16_SFLOAT:
      result = 2;
      break;
    case VK_FORMAT_R16G16_UNORM:
      result = 4;
      break;
    case VK_FORMAT_R16G16_SNORM:
      result = 4;
      break;
    case VK_FORMAT_R16G16_USCALED:
      result = 4;
      break;
    case VK_FORMAT_R16G16_SSCALED:
      result = 4;
      break;
    case VK_FORMAT_R16G16_UINT:
      result = 4;
      break;
    case VK_FORMAT_R16G16_SINT:
      result = 4;
      break;
    case VK_FORMAT_R16G16_SFLOAT:
      result = 4;
      break;
    case VK_FORMAT_R16G16B16_UNORM:
      result = 6;
      break;
    case VK_FORMAT_R16G16B16_SNORM:
      result = 6;
      break;
    case VK_FORMAT_R16G16B16_USCALED:
      result = 6;
      break;
    case VK_FORMAT_R16G16B16_SSCALED:
      result = 6;
      break;
    case VK_FORMAT_R16G16B16_UINT:
      result = 6;
      break;
    case VK_FORMAT_R16G16B16_SINT:
      result = 6;
      break;
    case VK_FORMAT_R16G16B16_SFLOAT:
      result = 6;
      break;
    case VK_FORMAT_R16G16B16A16_UNORM:
      result = 8;
      break;
    case VK_FORMAT_R16G16B16A16_SNORM:
      result = 8;
      break;
    case VK_FORMAT_R16G16B16A16_USCALED:
      result = 8;
      break;
    case VK_FORMAT_R16G16B16A16_SSCALED:
      result = 8;
      break;
    case VK_FORMAT_R16G16B16A16_UINT:
      result = 8;
      break;
    case VK_FORMAT_R16G16B16A16_SINT:
      result = 8;
      break;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
      result = 8;
      break;
    case VK_FORMAT_R32_UINT:
      result = 4;
      break;
    case VK_FORMAT_R32_SINT:
      result = 4;
      break;
    case VK_FORMAT_R32_SFLOAT:
      result = 4;
      break;
    case VK_FORMAT_R32G32_UINT:
      result = 8;
      break;
    case VK_FORMAT_R32G32_SINT:
      result = 8;
      break;
    case VK_FORMAT_R32G32_SFLOAT:
      result = 8;
      break;
    case VK_FORMAT_R32G32B32_UINT:
      result = 12;
      break;
    case VK_FORMAT_R32G32B32_SINT:
      result = 12;
      break;
    case VK_FORMAT_R32G32B32_SFLOAT:
      result = 12;
      break;
    case VK_FORMAT_R32G32B32A32_UINT:
      result = 16;
      break;
    case VK_FORMAT_R32G32B32A32_SINT:
      result = 16;
      break;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
      result = 16;
      break;
    case VK_FORMAT_R64_UINT:
      result = 8;
      break;
    case VK_FORMAT_R64_SINT:
      result = 8;
      break;
    case VK_FORMAT_R64_SFLOAT:
      result = 8;
      break;
    case VK_FORMAT_R64G64_UINT:
      result = 16;
      break;
    case VK_FORMAT_R64G64_SINT:
      result = 16;
      break;
    case VK_FORMAT_R64G64_SFLOAT:
      result = 16;
      break;
    case VK_FORMAT_R64G64B64_UINT:
      result = 24;
      break;
    case VK_FORMAT_R64G64B64_SINT:
      result = 24;
      break;
    case VK_FORMAT_R64G64B64_SFLOAT:
      result = 24;
      break;
    case VK_FORMAT_R64G64B64A64_UINT:
      result = 32;
      break;
    case VK_FORMAT_R64G64B64A64_SINT:
      result = 32;
      break;
    case VK_FORMAT_R64G64B64A64_SFLOAT:
      result = 32;
      break;
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
      result = 4;
      break;
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
      result = 4;
      break;

    default:
      break;
  }
  return result;
}
#endif

int main(int argn, char** argv) {
  SpvReflectShaderModule module = {};
  SpvReflectResult result = spvReflectCreateShaderModule(sizeof(k_sample_spv), k_sample_spv, &module);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  uint32_t count = 0;
  result = spvReflectEnumerateInputVariables(&module, &count, NULL);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  std::vector<SpvReflectInterfaceVariable*> input_vars(count);
  result = spvReflectEnumerateInputVariables(&module, &count, input_vars.data());
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  count = 0;
  result = spvReflectEnumerateOutputVariables(&module, &count, NULL);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  std::vector<SpvReflectInterfaceVariable*> output_vars(count);
  result = spvReflectEnumerateOutputVariables(&module, &count, output_vars.data());
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

#if defined(SPIRV_REFLECT_HAS_VULKAN_H)
  if (module.shader_stage == SPV_REFLECT_SHADER_STAGE_VERTEX_BIT) {
    // Demonstrates how to generate all necessary data structures to populate
    // a VkPipelineVertexInputStateCreateInfo structure, given the module's
    // expected input variables.
    //
    // Simplifying assumptions:
    // - All vertex input attributes are sourced from a single vertex buffer,
    //   bound to VB slot 0.
    // - Each vertex's attribute are laid out in ascending order by location.
    // - The format of each attribute matches its usage in the shader;
    //   float4 -> VK_FORMAT_R32G32B32A32_FLOAT, etc. No attribute compression
    //   is applied.
    // - All attributes are provided per-vertex, not per-instance.
    VkVertexInputBindingDescription binding_description = {};
    binding_description.binding = 0;
    binding_description.stride = 0;  // computed below
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
    attribute_descriptions.reserve(input_vars.size());
    for (size_t i_var = 0; i_var < input_vars.size(); ++i_var) {
      const SpvReflectInterfaceVariable& refl_var = *(input_vars[i_var]);
      // ignore built-in variables
      if (refl_var.decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN) {
        continue;
      }
      VkVertexInputAttributeDescription attr_desc{};
      attr_desc.location = refl_var.location;
      attr_desc.binding = binding_description.binding;
      attr_desc.format = static_cast<VkFormat>(refl_var.format);
      attr_desc.offset = 0;  // final offset computed below after sorting.
      attribute_descriptions.push_back(attr_desc);
    }
    // Sort attributes by location
    std::sort(std::begin(attribute_descriptions), std::end(attribute_descriptions),
              [](const VkVertexInputAttributeDescription& a, const VkVertexInputAttributeDescription& b) {
                return a.location < b.location;
              });
    // Compute final offsets of each attribute, and total vertex stride.
    for (auto& attribute : attribute_descriptions) {
      uint32_t format_size = FormatSize(attribute.format);
      attribute.offset = binding_description.stride;
      binding_description.stride += format_size;
    }
    // Nothing further is done with attribute_descriptions or
    // binding_description in this sample. A real application would probably
    // derive this information from its mesh format(s); a similar mechanism
    // could be used to ensure mesh/shader compatibility.
  }
#endif

  // Log the interface variables to stdout
  const char* t = "  ";
  const char* tt = "    ";

  PrintModuleInfo(std::cout, module);
  std::cout << "\n\n";

  std::cout << "Input variables:"
            << "\n";
  for (size_t index = 0; index < input_vars.size(); ++index) {
    auto p_var = input_vars[index];

    // input variables can also be retrieved directly from the module, by
    // location (unless the location is (uint32_t)-1, as is the case with
    // built-in inputs)
    auto p_var2 = spvReflectGetInputVariableByLocation(&module, p_var->location, &result);
    if (p_var->location == UINT32_MAX) {
      assert(result == SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
      assert(p_var2 == nullptr);
    } else {
      assert(result == SPV_REFLECT_RESULT_SUCCESS);
      assert(p_var == p_var2);
    }
    (void)p_var2;

    // input variables can also be retrieved directly from the module, by
    // semantic (if present)
    p_var2 = spvReflectGetInputVariableBySemantic(&module, p_var->semantic, &result);
    if (!p_var->semantic) {
      assert(result == SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
      assert(p_var2 == nullptr);
    } else if (p_var->semantic[0] != '\0') {
      assert(result == SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
      assert(p_var2 == nullptr);
    } else {
      assert(result == SPV_REFLECT_RESULT_SUCCESS);
      assert(p_var == p_var2);
    }
    (void)p_var2;

    std::cout << t << index << ":"
              << "\n";
    PrintInterfaceVariable(std::cout, module.source_language, *p_var, tt);
    std::cout << "\n\n";
  }

  std::cout << "Output variables:"
            << "\n";
  for (size_t index = 0; index < output_vars.size(); ++index) {
    auto p_var = output_vars[index];

    // output variables can also be retrieved directly from the module, by
    // location (unless the location is (uint32_t)-1, as is the case with
    // built-in outputs)
    auto p_var2 = spvReflectGetOutputVariableByLocation(&module, p_var->location, &result);
    if (p_var->location == UINT32_MAX) {
      assert(result == SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
      assert(p_var2 == nullptr);
    } else {
      assert(result == SPV_REFLECT_RESULT_SUCCESS);
      assert(p_var == p_var2);
    }
    (void)p_var2;

    // output variables can also be retrieved directly from the module, by
    // semantic (if present)
    p_var2 = spvReflectGetOutputVariableBySemantic(&module, p_var->semantic, &result);
    if (!p_var->semantic) {
      assert(result == SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
      assert(p_var2 == nullptr);
    } else if (p_var->semantic[0] != '\0') {
      assert(result == SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
      assert(p_var2 == nullptr);
    } else {
      assert(result == SPV_REFLECT_RESULT_SUCCESS);
      assert(p_var == p_var2);
    }
    (void)p_var2;

    std::cout << t << index << ":"
              << "\n";
    PrintInterfaceVariable(std::cout, module.source_language, *p_var, tt);
    std::cout << "\n\n";
  }

  spvReflectDestroyShaderModule(&module);

  return 0;
}
