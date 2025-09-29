#include "output_stream.h"

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

enum TextLineType {
  TEXT_LINE_TYPE_BLOCK_BEGIN = 0x01,
  TEXT_LINE_TYPE_BLOCK_END = 0x02,
  TEXT_LINE_TYPE_STRUCT_BEGIN = 0x04,
  TEXT_LINE_TYPE_STRUCT_END = 0x08,
  TEXT_LINE_TYPE_REF_BEGIN = 0x10,
  TEXT_LINE_TYPE_REF_END = 0x20,
  TEXT_LINE_TYPE_LINES = 0x40,
};

struct TextLine {
  std::string indent;
  std::string modifier;
  std::string type_name;
  std::string name;
  uint32_t absolute_offset;
  uint32_t relative_offset;
  uint32_t size;
  uint32_t padded_size;
  uint32_t array_stride;
  uint32_t block_variable_flags;
  std::vector<uint32_t> array_dims;
  // Text Data
  uint32_t text_line_flags;
  std::vector<TextLine> lines;
  std::string formatted_line;
  std::string formatted_absolute_offset;
  std::string formatted_relative_offset;
  std::string formatted_size;
  std::string formatted_padded_size;
  std::string formatted_array_stride;
  std::string formatted_block_variable_flags;
};

static std::string AsHexString(uint32_t n) {
  // std::iomanip can die in a fire.
  char out_word[11];
  int len = snprintf(out_word, 11, "0x%08X", n);
  assert(len == 10);
  (void)len;
  return std::string(out_word);
}

std::string ToStringGenerator(SpvReflectGenerator generator) {
  switch (generator) {
    case SPV_REFLECT_GENERATOR_KHRONOS_LLVM_SPIRV_TRANSLATOR:
      return "Khronos LLVM/SPIR-V Translator";
      break;
    case SPV_REFLECT_GENERATOR_KHRONOS_SPIRV_TOOLS_ASSEMBLER:
      return "Khronos SPIR-V Tools Assembler";
      break;
    case SPV_REFLECT_GENERATOR_KHRONOS_GLSLANG_REFERENCE_FRONT_END:
      return "Khronos Glslang Reference Front End";
      break;
    case SPV_REFLECT_GENERATOR_GOOGLE_SHADERC_OVER_GLSLANG:
      return "Google Shaderc over Glslang";
      break;
    case SPV_REFLECT_GENERATOR_GOOGLE_SPIREGG:
      return "Google spiregg";
      break;
    case SPV_REFLECT_GENERATOR_GOOGLE_RSPIRV:
      return "Google rspirv";
      break;
    case SPV_REFLECT_GENERATOR_X_LEGEND_MESA_MESAIR_SPIRV_TRANSLATOR:
      return "X-LEGEND Mesa-IR/SPIR-V Translator";
      break;
    case SPV_REFLECT_GENERATOR_KHRONOS_SPIRV_TOOLS_LINKER:
      return "Khronos SPIR-V Tools Linker";
      break;
    case SPV_REFLECT_GENERATOR_WINE_VKD3D_SHADER_COMPILER:
      return "Wine VKD3D Shader Compiler";
      break;
    case SPV_REFLECT_GENERATOR_CLAY_CLAY_SHADER_COMPILER:
      return "Clay Clay Shader Compiler";
      break;
  }
  // unhandled SpvReflectGenerator enum value
  return "???";
}

std::string ToStringSpvSourceLanguage(SpvSourceLanguage lang) {
  switch (lang) {
    case SpvSourceLanguageESSL:
      return "ESSL";
    case SpvSourceLanguageGLSL:
      return "GLSL";
    case SpvSourceLanguageOpenCL_C:
      return "OpenCL_C";
    case SpvSourceLanguageOpenCL_CPP:
      return "OpenCL_CPP";
    case SpvSourceLanguageHLSL:
      return "HLSL";
    case SpvSourceLanguageCPP_for_OpenCL:
      return "CPP_for_OpenCL";
    case SpvSourceLanguageSYCL:
      return "SYCL";
    case SpvSourceLanguageHERO_C:
      return "Hero C";
    case SpvSourceLanguageNZSL:
      return "NZSL";

    default:
      break;
  }
  // SpvSourceLanguageUnknown, SpvSourceLanguageMax, or another value that does
  // not correspond to a known language.
  return "Unknown";
}

std::string ToStringSpvExecutionModel(SpvExecutionModel model) {
  switch (model) {
    case SpvExecutionModelVertex:
      return "Vertex";
    case SpvExecutionModelTessellationControl:
      return "TessellationControl";
    case SpvExecutionModelTessellationEvaluation:
      return "TessellationEvaluation";
    case SpvExecutionModelGeometry:
      return "Geometry";
    case SpvExecutionModelFragment:
      return "Fragment";
    case SpvExecutionModelGLCompute:
      return "GLCompute";
    case SpvExecutionModelKernel:
      return "Kernel";
    case SpvExecutionModelTaskNV:
      return "TaskNV";
    case SpvExecutionModelMeshNV:
      return "MeshNV";
    case SpvExecutionModelTaskEXT:
      return "TaskEXT";
    case SpvExecutionModelMeshEXT:
      return "MeshEXT";
    case SpvExecutionModelRayGenerationKHR:
      return "RayGenerationKHR";
    case SpvExecutionModelIntersectionKHR:
      return "IntersectionKHR";
    case SpvExecutionModelAnyHitKHR:
      return "AnyHitKHR";
    case SpvExecutionModelClosestHitKHR:
      return "ClosestHitKHR";
    case SpvExecutionModelMissKHR:
      return "MissKHR";
    case SpvExecutionModelCallableKHR:
      return "CallableKHR";

    case SpvExecutionModelMax:
      break;

    default:
      break;
  }

  // unhandled SpvExecutionModel enum value
  return "???";
}

std::string ToStringShaderStage(SpvReflectShaderStageFlagBits stage) {
  switch (stage) {
    case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
      return "VS";
    case SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
      return "HS";
    case SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
      return "DS";
    case SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT:
      return "GS";
    case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
      return "PS";
    case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
      return "CS";
    case SPV_REFLECT_SHADER_STAGE_TASK_BIT_NV:
      return "TASK";
    case SPV_REFLECT_SHADER_STAGE_MESH_BIT_NV:
      return "MESH";
    case SPV_REFLECT_SHADER_STAGE_RAYGEN_BIT_KHR:
      return "RAYGEN";
    case SPV_REFLECT_SHADER_STAGE_ANY_HIT_BIT_KHR:
      return "ANY_HIT";
    case SPV_REFLECT_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
      return "CLOSEST_HIT";
    case SPV_REFLECT_SHADER_STAGE_MISS_BIT_KHR:
      return "MISS";
    case SPV_REFLECT_SHADER_STAGE_INTERSECTION_BIT_KHR:
      return "INTERSECTION";
    case SPV_REFLECT_SHADER_STAGE_CALLABLE_BIT_KHR:
      return "CALLABLE";

    default:
      break;
  }

  // Unhandled SpvReflectShaderStageFlagBits enum value
  return "???";
}

std::string ToStringSpvStorageClass(int storage_class) {
  switch (storage_class) {
    case SpvStorageClassUniformConstant:
      return "UniformConstant";
    case SpvStorageClassInput:
      return "Input";
    case SpvStorageClassUniform:
      return "Uniform";
    case SpvStorageClassOutput:
      return "Output";
    case SpvStorageClassWorkgroup:
      return "Workgroup";
    case SpvStorageClassCrossWorkgroup:
      return "CrossWorkgroup";
    case SpvStorageClassPrivate:
      return "Private";
    case SpvStorageClassFunction:
      return "Function";
    case SpvStorageClassGeneric:
      return "Generic";
    case SpvStorageClassPushConstant:
      return "PushConstant";
    case SpvStorageClassAtomicCounter:
      return "AtomicCounter";
    case SpvStorageClassImage:
      return "Image";
    case SpvStorageClassStorageBuffer:
      return "StorageBuffer";
    case SpvStorageClassCallableDataKHR:
      return "CallableDataKHR";
    case SpvStorageClassIncomingCallableDataKHR:
      return "IncomingCallableDataKHR";
    case SpvStorageClassRayPayloadKHR:
      return "RayPayloadKHR";
    case SpvStorageClassHitAttributeKHR:
      return "HitAttributeKHR";
    case SpvStorageClassIncomingRayPayloadKHR:
      return "IncomingRayPayloadKHR";
    case SpvStorageClassShaderRecordBufferKHR:
      return "ShaderRecordBufferKHR";
    case SpvStorageClassPhysicalStorageBuffer:
      return "PhysicalStorageBuffer";
    case SpvStorageClassCodeSectionINTEL:
      return "CodeSectionINTEL";
    case SpvStorageClassDeviceOnlyINTEL:
      return "DeviceOnlyINTEL";
    case SpvStorageClassHostOnlyINTEL:
      return "HostOnlyINTEL";
    case SpvStorageClassMax:
      break;

    default:
      break;
  }

  // Special case: this specific "unhandled" value does actually seem to show
  // up.
  if (storage_class == (SpvStorageClass)-1) {
    return "NOT APPLICABLE";
  }

  // unhandled SpvStorageClass enum value
  return "???";
}

std::string ToStringSpvDim(SpvDim dim) {
  switch (dim) {
    case SpvDim1D:
      return "1D";
    case SpvDim2D:
      return "2D";
    case SpvDim3D:
      return "3D";
    case SpvDimCube:
      return "Cube";
    case SpvDimRect:
      return "Rect";
    case SpvDimBuffer:
      return "Buffer";
    case SpvDimSubpassData:
      return "SubpassData";
    case SpvDimTileImageDataEXT:
      return "DimTileImageDataEXT";

    case SpvDimMax:
      break;
  }
  // unhandled SpvDim enum value
  return "???";
}

std::string ToStringResourceType(SpvReflectResourceType res_type) {
  switch (res_type) {
    case SPV_REFLECT_RESOURCE_FLAG_UNDEFINED:
      return "UNDEFINED";
    case SPV_REFLECT_RESOURCE_FLAG_SAMPLER:
      return "SAMPLER";
    case SPV_REFLECT_RESOURCE_FLAG_CBV:
      return "CBV";
    case SPV_REFLECT_RESOURCE_FLAG_SRV:
      return "SRV";
    case SPV_REFLECT_RESOURCE_FLAG_UAV:
      return "UAV";
  }
  // unhandled SpvReflectResourceType enum value
  return "???";
}

std::string ToStringDescriptorType(SpvReflectDescriptorType value) {
  switch (value) {
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
      return "VK_DESCRIPTOR_TYPE_SAMPLER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      return "VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      return "VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE";
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      return "VK_DESCRIPTOR_TYPE_STORAGE_IMAGE";
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
      return "VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
      return "VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      return "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      return "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      return "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC";
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
      return "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC";
    case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
      return "VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT";
    case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
      return "VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR";
  }
  // unhandled SpvReflectDescriptorType enum value
  return "VK_DESCRIPTOR_TYPE_???";
}

static std::string ToStringSpvBuiltIn(int built_in) {
  switch (built_in) {
    case SpvBuiltInPosition:
      return "Position";
    case SpvBuiltInPointSize:
      return "PointSize";
    case SpvBuiltInClipDistance:
      return "ClipDistance";
    case SpvBuiltInCullDistance:
      return "CullDistance";
    case SpvBuiltInVertexId:
      return "VertexId";
    case SpvBuiltInInstanceId:
      return "InstanceId";
    case SpvBuiltInPrimitiveId:
      return "PrimitiveId";
    case SpvBuiltInInvocationId:
      return "InvocationId";
    case SpvBuiltInLayer:
      return "Layer";
    case SpvBuiltInViewportIndex:
      return "ViewportIndex";
    case SpvBuiltInTessLevelOuter:
      return "TessLevelOuter";
    case SpvBuiltInTessLevelInner:
      return "TessLevelInner";
    case SpvBuiltInTessCoord:
      return "TessCoord";
    case SpvBuiltInPatchVertices:
      return "PatchVertices";
    case SpvBuiltInFragCoord:
      return "FragCoord";
    case SpvBuiltInPointCoord:
      return "PointCoord";
    case SpvBuiltInFrontFacing:
      return "FrontFacing";
    case SpvBuiltInSampleId:
      return "SampleId";
    case SpvBuiltInSamplePosition:
      return "SamplePosition";
    case SpvBuiltInSampleMask:
      return "SampleMask";
    case SpvBuiltInFragDepth:
      return "FragDepth";
    case SpvBuiltInHelperInvocation:
      return "HelperInvocation";
    case SpvBuiltInNumWorkgroups:
      return "NumWorkgroups";
    case SpvBuiltInWorkgroupSize:
      return "WorkgroupSize";
    case SpvBuiltInWorkgroupId:
      return "WorkgroupId";
    case SpvBuiltInLocalInvocationId:
      return "LocalInvocationId";
    case SpvBuiltInGlobalInvocationId:
      return "GlobalInvocationId";
    case SpvBuiltInLocalInvocationIndex:
      return "LocalInvocationIndex";
    case SpvBuiltInWorkDim:
      return "WorkDim";
    case SpvBuiltInGlobalSize:
      return "GlobalSize";
    case SpvBuiltInEnqueuedWorkgroupSize:
      return "EnqueuedWorkgroupSize";
    case SpvBuiltInGlobalOffset:
      return "GlobalOffset";
    case SpvBuiltInGlobalLinearId:
      return "GlobalLinearId";
    case SpvBuiltInSubgroupSize:
      return "SubgroupSize";
    case SpvBuiltInSubgroupMaxSize:
      return "SubgroupMaxSize";
    case SpvBuiltInNumSubgroups:
      return "NumSubgroups";
    case SpvBuiltInNumEnqueuedSubgroups:
      return "NumEnqueuedSubgroups";
    case SpvBuiltInSubgroupId:
      return "SubgroupId";
    case SpvBuiltInSubgroupLocalInvocationId:
      return "SubgroupLocalInvocationId";
    case SpvBuiltInVertexIndex:
      return "VertexIndex";
    case SpvBuiltInInstanceIndex:
      return "InstanceIndex";
    case SpvBuiltInSubgroupEqMaskKHR:
      return "SubgroupEqMaskKHR";
    case SpvBuiltInSubgroupGeMaskKHR:
      return "SubgroupGeMaskKHR";
    case SpvBuiltInSubgroupGtMaskKHR:
      return "SubgroupGtMaskKHR";
    case SpvBuiltInSubgroupLeMaskKHR:
      return "SubgroupLeMaskKHR";
    case SpvBuiltInSubgroupLtMaskKHR:
      return "SubgroupLtMaskKHR";
    case SpvBuiltInBaseVertex:
      return "BaseVertex";
    case SpvBuiltInBaseInstance:
      return "BaseInstance";
    case SpvBuiltInDrawIndex:
      return "DrawIndex";
    case SpvBuiltInDeviceIndex:
      return "DeviceIndex";
    case SpvBuiltInViewIndex:
      return "ViewIndex";
    case SpvBuiltInBaryCoordNoPerspAMD:
      return "BaryCoordNoPerspAMD";
    case SpvBuiltInBaryCoordNoPerspCentroidAMD:
      return "BaryCoordNoPerspCentroidAMD";
    case SpvBuiltInBaryCoordNoPerspSampleAMD:
      return "BaryCoordNoPerspSampleAMD";
    case SpvBuiltInBaryCoordSmoothAMD:
      return "BaryCoordSmoothAMD";
    case SpvBuiltInBaryCoordSmoothCentroidAMD:
      return "BaryCoordSmoothCentroidAMD";
    case SpvBuiltInBaryCoordSmoothSampleAMD:
      return "BaryCoordSmoothSampleAMD";
    case SpvBuiltInBaryCoordPullModelAMD:
      return "BaryCoordPullModelAMD";
    case SpvBuiltInFragStencilRefEXT:
      return "FragStencilRefEXT";
    case SpvBuiltInViewportMaskNV:
      return "ViewportMaskNV";
    case SpvBuiltInSecondaryPositionNV:
      return "SecondaryPositionNV";
    case SpvBuiltInSecondaryViewportMaskNV:
      return "SecondaryViewportMaskNV";
    case SpvBuiltInPositionPerViewNV:
      return "PositionPerViewNV";
    case SpvBuiltInViewportMaskPerViewNV:
      return "ViewportMaskPerViewNV";
    case SpvBuiltInLaunchIdKHR:
      return "InLaunchIdKHR";
    case SpvBuiltInLaunchSizeKHR:
      return "InLaunchSizeKHR";
    case SpvBuiltInWorldRayOriginKHR:
      return "InWorldRayOriginKHR";
    case SpvBuiltInWorldRayDirectionKHR:
      return "InWorldRayDirectionKHR";
    case SpvBuiltInObjectRayOriginKHR:
      return "InObjectRayOriginKHR";
    case SpvBuiltInObjectRayDirectionKHR:
      return "InObjectRayDirectionKHR";
    case SpvBuiltInRayTminKHR:
      return "InRayTminKHR";
    case SpvBuiltInRayTmaxKHR:
      return "InRayTmaxKHR";
    case SpvBuiltInInstanceCustomIndexKHR:
      return "InInstanceCustomIndexKHR";
    case SpvBuiltInObjectToWorldKHR:
      return "InObjectToWorldKHR";
    case SpvBuiltInWorldToObjectKHR:
      return "InWorldToObjectKHR";
    case SpvBuiltInHitTNV:
      return "InHitTNV";
    case SpvBuiltInHitKindKHR:
      return "InHitKindKHR";
    case SpvBuiltInIncomingRayFlagsKHR:
      return "InIncomingRayFlagsKHR";
    case SpvBuiltInRayGeometryIndexKHR:
      return "InRayGeometryIndexKHR";

    case SpvBuiltInMax:
    default:
      break;
  }
  // unhandled SpvBuiltIn enum value
  std::stringstream ss;
  ss << "??? (" << built_in << ")";
  return ss.str();
}

std::string ToStringSpvBuiltIn(const SpvReflectInterfaceVariable& variable, bool preface) {
  std::stringstream ss;
  if (variable.decoration_flags & SPV_REFLECT_DECORATION_BLOCK) {
    if (preface) {
      ss << "(built-in block) ";
    }
    ss << "[";
    for (uint32_t i = 0; i < variable.member_count; i++) {
      ss << ToStringSpvBuiltIn(variable.members[i].built_in);
      if (i < (variable.member_count - 1)) {
        ss << ", ";
      }
    }
    ss << "]";
  } else {
    if (preface) {
      ss << "(built-in) ";
    }
    ss << ToStringSpvBuiltIn(variable.built_in);
  }
  return ss.str();
}

std::string ToStringSpvImageFormat(SpvImageFormat fmt) {
  switch (fmt) {
    case SpvImageFormatUnknown:
      return "Unknown";
    case SpvImageFormatRgba32f:
      return "Rgba32f";
    case SpvImageFormatRgba16f:
      return "Rgba16f";
    case SpvImageFormatR32f:
      return "R32f";
    case SpvImageFormatRgba8:
      return "Rgba8";
    case SpvImageFormatRgba8Snorm:
      return "Rgba8Snorm";
    case SpvImageFormatRg32f:
      return "Rg32f";
    case SpvImageFormatRg16f:
      return "Rg16f";
    case SpvImageFormatR11fG11fB10f:
      return "R11fG11fB10f";
    case SpvImageFormatR16f:
      return "R16f";
    case SpvImageFormatRgba16:
      return "Rgba16";
    case SpvImageFormatRgb10A2:
      return "Rgb10A2";
    case SpvImageFormatRg16:
      return "Rg16";
    case SpvImageFormatRg8:
      return "Rg8";
    case SpvImageFormatR16:
      return "R16";
    case SpvImageFormatR8:
      return "R8";
    case SpvImageFormatRgba16Snorm:
      return "Rgba16Snorm";
    case SpvImageFormatRg16Snorm:
      return "Rg16Snorm";
    case SpvImageFormatRg8Snorm:
      return "Rg8Snorm";
    case SpvImageFormatR16Snorm:
      return "R16Snorm";
    case SpvImageFormatR8Snorm:
      return "R8Snorm";
    case SpvImageFormatRgba32i:
      return "Rgba32i";
    case SpvImageFormatRgba16i:
      return "Rgba16i";
    case SpvImageFormatRgba8i:
      return "Rgba8i";
    case SpvImageFormatR32i:
      return "R32i";
    case SpvImageFormatRg32i:
      return "Rg32i";
    case SpvImageFormatRg16i:
      return "Rg16i";
    case SpvImageFormatRg8i:
      return "Rg8i";
    case SpvImageFormatR16i:
      return "R16i";
    case SpvImageFormatR8i:
      return "R8i";
    case SpvImageFormatRgba32ui:
      return "Rgba32ui";
    case SpvImageFormatRgba16ui:
      return "Rgba16ui";
    case SpvImageFormatRgba8ui:
      return "Rgba8ui";
    case SpvImageFormatR32ui:
      return "R32ui";
    case SpvImageFormatRgb10a2ui:
      return "Rgb10a2ui";
    case SpvImageFormatRg32ui:
      return "Rg32ui";
    case SpvImageFormatRg16ui:
      return "Rg16ui";
    case SpvImageFormatRg8ui:
      return "Rg8ui";
    case SpvImageFormatR16ui:
      return "R16ui";
    case SpvImageFormatR8ui:
      return "R8ui";
    case SpvImageFormatR64ui:
      return "R64ui";
    case SpvImageFormatR64i:
      return "R64i";

    case SpvImageFormatMax:
      break;
  }
  // unhandled SpvImageFormat enum value
  return "???";
}

std::string ToStringUserType(SpvReflectUserType user_type) {
  switch (user_type) {
    case SPV_REFLECT_USER_TYPE_CBUFFER:
      return "cbuffer";
    case SPV_REFLECT_USER_TYPE_TBUFFER:
      return "tbuffer";
    case SPV_REFLECT_USER_TYPE_APPEND_STRUCTURED_BUFFER:
      return "AppendStructuredBuffer";
    case SPV_REFLECT_USER_TYPE_BUFFER:
      return "Buffer";
    case SPV_REFLECT_USER_TYPE_BYTE_ADDRESS_BUFFER:
      return "ByteAddressBuffer";
    case SPV_REFLECT_USER_TYPE_CONSTANT_BUFFER:
      return "ConstantBuffer";
    case SPV_REFLECT_USER_TYPE_CONSUME_STRUCTURED_BUFFER:
      return "ConsumeStructuredBuffer";
    case SPV_REFLECT_USER_TYPE_INPUT_PATCH:
      return "InputPatch";
    case SPV_REFLECT_USER_TYPE_OUTPUT_PATCH:
      return "OutputPatch";
    case SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_BUFFER:
      return "RasterizerOrderedBuffer";
    case SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_BYTE_ADDRESS_BUFFER:
      return "RasterizerOrderedByteAddressBuffer";
    case SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_STRUCTURED_BUFFER:
      return "RasterizerOrderedStructuredBuffer";
    case SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_TEXTURE_1D:
      return "RasterizerOrderedTexture1D";
    case SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_TEXTURE_1D_ARRAY:
      return "RasterizerOrderedTexture1DArray";
    case SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_TEXTURE_2D:
      return "RasterizerOrderedTexture2D";
    case SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_TEXTURE_2D_ARRAY:
      return "RasterizerOrderedTexture2DArray";
    case SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_TEXTURE_3D:
      return "RasterizerOrderedTexture3D";
    case SPV_REFLECT_USER_TYPE_RAYTRACING_ACCELERATION_STRUCTURE:
      return "RaytracingAccelerationStructure";
    case SPV_REFLECT_USER_TYPE_RW_BUFFER:
      return "RWBuffer";
    case SPV_REFLECT_USER_TYPE_RW_BYTE_ADDRESS_BUFFER:
      return "RWByteAddressBuffer";
    case SPV_REFLECT_USER_TYPE_RW_STRUCTURED_BUFFER:
      return "RWStructuredBuffer";
    case SPV_REFLECT_USER_TYPE_RW_TEXTURE_1D:
      return "RWTexture1D";
    case SPV_REFLECT_USER_TYPE_RW_TEXTURE_1D_ARRAY:
      return "RWTexture1DArray";
    case SPV_REFLECT_USER_TYPE_RW_TEXTURE_2D:
      return "RWTexture2D";
    case SPV_REFLECT_USER_TYPE_RW_TEXTURE_2D_ARRAY:
      return "RWTexture2DArray";
    case SPV_REFLECT_USER_TYPE_RW_TEXTURE_3D:
      return "RWTexture3D";
    case SPV_REFLECT_USER_TYPE_STRUCTURED_BUFFER:
      return "StructuredBuffer";
    case SPV_REFLECT_USER_TYPE_SUBPASS_INPUT:
      return "SubpassInput";
    case SPV_REFLECT_USER_TYPE_SUBPASS_INPUT_MS:
      return "SubpassInputMS";
    case SPV_REFLECT_USER_TYPE_TEXTURE_1D:
      return "Texture1D";
    case SPV_REFLECT_USER_TYPE_TEXTURE_1D_ARRAY:
      return "Texture1DArray";
    case SPV_REFLECT_USER_TYPE_TEXTURE_2D:
      return "Texture2D";
    case SPV_REFLECT_USER_TYPE_TEXTURE_2D_ARRAY:
      return "Texture2DArray";
    case SPV_REFLECT_USER_TYPE_TEXTURE_2DMS:
      return "Texture2DMS";
    case SPV_REFLECT_USER_TYPE_TEXTURE_2DMS_ARRAY:
      return "Texture2DMSArray";
    case SPV_REFLECT_USER_TYPE_TEXTURE_3D:
      return "Texture3D";
    case SPV_REFLECT_USER_TYPE_TEXTURE_BUFFER:
      return "TextureBuffer";
    case SPV_REFLECT_USER_TYPE_TEXTURE_CUBE:
      return "TextureCube";
    case SPV_REFLECT_USER_TYPE_TEXTURE_CUBE_ARRAY:
      return "TextureCubeArray";
    default:
      return "???";
  }
}

std::string ToStringTypeFlags(SpvReflectTypeFlags type_flags) {
  if (type_flags == SPV_REFLECT_TYPE_FLAG_UNDEFINED) {
    return "UNDEFINED";
  }

#define PRINT_AND_CLEAR_TYPE_FLAG(stream, flags, bit)                               \
  if (((flags) & (SPV_REFLECT_TYPE_FLAG_##bit)) == (SPV_REFLECT_TYPE_FLAG_##bit)) { \
    stream << #bit << " ";                                                          \
    flags ^= SPV_REFLECT_TYPE_FLAG_##bit;                                           \
  }
  std::stringstream sstream;
  PRINT_AND_CLEAR_TYPE_FLAG(sstream, type_flags, ARRAY);
  PRINT_AND_CLEAR_TYPE_FLAG(sstream, type_flags, STRUCT);
  PRINT_AND_CLEAR_TYPE_FLAG(sstream, type_flags, REF);
  PRINT_AND_CLEAR_TYPE_FLAG(sstream, type_flags, EXTERNAL_MASK);
  PRINT_AND_CLEAR_TYPE_FLAG(sstream, type_flags, EXTERNAL_BLOCK);
  PRINT_AND_CLEAR_TYPE_FLAG(sstream, type_flags, EXTERNAL_SAMPLED_IMAGE);
  PRINT_AND_CLEAR_TYPE_FLAG(sstream, type_flags, EXTERNAL_SAMPLER);
  PRINT_AND_CLEAR_TYPE_FLAG(sstream, type_flags, EXTERNAL_IMAGE);
  PRINT_AND_CLEAR_TYPE_FLAG(sstream, type_flags, MATRIX);
  PRINT_AND_CLEAR_TYPE_FLAG(sstream, type_flags, VECTOR);
  PRINT_AND_CLEAR_TYPE_FLAG(sstream, type_flags, FLOAT);
  PRINT_AND_CLEAR_TYPE_FLAG(sstream, type_flags, INT);
  PRINT_AND_CLEAR_TYPE_FLAG(sstream, type_flags, BOOL);
  PRINT_AND_CLEAR_TYPE_FLAG(sstream, type_flags, VOID);
#undef PRINT_AND_CLEAR_TYPE_FLAG
  if (type_flags != 0) {
    // Unhandled SpvReflectTypeFlags bit
    sstream << "???";
  }
  return sstream.str();
}

std::string ToStringVariableFlags(SpvReflectVariableFlags var_flags) {
  if (var_flags == SPV_REFLECT_VARIABLE_FLAGS_NONE) {
    return "NONE";
  }

#define PRINT_AND_CLEAR_TYPE_FLAG(stream, flags, bit)                                         \
  if (((flags) & (SPV_REFLECT_VARIABLE_FLAGS_##bit)) == (SPV_REFLECT_VARIABLE_FLAGS_##bit)) { \
    stream << #bit << " ";                                                                    \
    flags ^= SPV_REFLECT_VARIABLE_FLAGS_##bit;                                                \
  }
  std::stringstream sstream;
  PRINT_AND_CLEAR_TYPE_FLAG(sstream, var_flags, UNUSED);
  PRINT_AND_CLEAR_TYPE_FLAG(sstream, var_flags, PHYSICAL_POINTER_COPY);
#undef PRINT_AND_CLEAR_TYPE_FLAG
  if (var_flags != 0) {
    // Unhandled SpvReflectVariableFlags bit
    sstream << "???";
  }
  return sstream.str();
}

std::string ToStringDecorationFlags(SpvReflectDecorationFlags decoration_flags) {
  if (decoration_flags == SPV_REFLECT_DECORATION_NONE) {
    return "NONE";
  }

#define PRINT_AND_CLEAR_DECORATION_FLAG(stream, flags, bit)                           \
  if (((flags) & (SPV_REFLECT_DECORATION_##bit)) == (SPV_REFLECT_DECORATION_##bit)) { \
    stream << #bit << " ";                                                            \
    flags ^= SPV_REFLECT_DECORATION_##bit;                                            \
  }
  std::stringstream sstream;
  PRINT_AND_CLEAR_DECORATION_FLAG(sstream, decoration_flags, NON_WRITABLE);
  PRINT_AND_CLEAR_DECORATION_FLAG(sstream, decoration_flags, NON_READABLE);
  PRINT_AND_CLEAR_DECORATION_FLAG(sstream, decoration_flags, FLAT);
  PRINT_AND_CLEAR_DECORATION_FLAG(sstream, decoration_flags, NOPERSPECTIVE);
  PRINT_AND_CLEAR_DECORATION_FLAG(sstream, decoration_flags, BUILT_IN);
  PRINT_AND_CLEAR_DECORATION_FLAG(sstream, decoration_flags, COLUMN_MAJOR);
  PRINT_AND_CLEAR_DECORATION_FLAG(sstream, decoration_flags, ROW_MAJOR);
  PRINT_AND_CLEAR_DECORATION_FLAG(sstream, decoration_flags, BUFFER_BLOCK);
  PRINT_AND_CLEAR_DECORATION_FLAG(sstream, decoration_flags, BLOCK);
  PRINT_AND_CLEAR_DECORATION_FLAG(sstream, decoration_flags, PATCH);
  PRINT_AND_CLEAR_DECORATION_FLAG(sstream, decoration_flags, PER_VERTEX);
  PRINT_AND_CLEAR_DECORATION_FLAG(sstream, decoration_flags, PER_TASK);
#undef PRINT_AND_CLEAR_DECORATION_FLAG
  if (decoration_flags != 0) {
    // Unhandled SpvReflectDecorationFlags bit
    sstream << "???";
  }
  return sstream.str();
}

std::string ToStringFormat(SpvReflectFormat fmt) {
  switch (fmt) {
    case SPV_REFLECT_FORMAT_UNDEFINED:
      return "VK_FORMAT_UNDEFINED";
    case SPV_REFLECT_FORMAT_R16_UINT:
      return "VK_FORMAT_R16_UINT";
    case SPV_REFLECT_FORMAT_R16_SINT:
      return "VK_FORMAT_R16_SINT";
    case SPV_REFLECT_FORMAT_R16_SFLOAT:
      return "VK_FORMAT_R16_SFLOAT";
    case SPV_REFLECT_FORMAT_R16G16_UINT:
      return "VK_FORMAT_R16G16_UINT";
    case SPV_REFLECT_FORMAT_R16G16_SINT:
      return "VK_FORMAT_R16G16_SINT";
    case SPV_REFLECT_FORMAT_R16G16_SFLOAT:
      return "VK_FORMAT_R16G16_SFLOAT";
    case SPV_REFLECT_FORMAT_R16G16B16_UINT:
      return "VK_FORMAT_R16G16B16_UINT";
    case SPV_REFLECT_FORMAT_R16G16B16_SINT:
      return "VK_FORMAT_R16G16B16_SINT";
    case SPV_REFLECT_FORMAT_R16G16B16_SFLOAT:
      return "VK_FORMAT_R16G16B16_SFLOAT";
    case SPV_REFLECT_FORMAT_R16G16B16A16_UINT:
      return "VK_FORMAT_R16G16B16A16_UINT";
    case SPV_REFLECT_FORMAT_R16G16B16A16_SINT:
      return "VK_FORMAT_R16G16B16A16_SINT";
    case SPV_REFLECT_FORMAT_R16G16B16A16_SFLOAT:
      return "VK_FORMAT_R16G16B16A16_SFLOAT";
    case SPV_REFLECT_FORMAT_R32_UINT:
      return "VK_FORMAT_R32_UINT";
    case SPV_REFLECT_FORMAT_R32_SINT:
      return "VK_FORMAT_R32_SINT";
    case SPV_REFLECT_FORMAT_R32_SFLOAT:
      return "VK_FORMAT_R32_SFLOAT";
    case SPV_REFLECT_FORMAT_R32G32_UINT:
      return "VK_FORMAT_R32G32_UINT";
    case SPV_REFLECT_FORMAT_R32G32_SINT:
      return "VK_FORMAT_R32G32_SINT";
    case SPV_REFLECT_FORMAT_R32G32_SFLOAT:
      return "VK_FORMAT_R32G32_SFLOAT";
    case SPV_REFLECT_FORMAT_R32G32B32_UINT:
      return "VK_FORMAT_R32G32B32_UINT";
    case SPV_REFLECT_FORMAT_R32G32B32_SINT:
      return "VK_FORMAT_R32G32B32_SINT";
    case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
      return "VK_FORMAT_R32G32B32_SFLOAT";
    case SPV_REFLECT_FORMAT_R32G32B32A32_UINT:
      return "VK_FORMAT_R32G32B32A32_UINT";
    case SPV_REFLECT_FORMAT_R32G32B32A32_SINT:
      return "VK_FORMAT_R32G32B32A32_SINT";
    case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:
      return "VK_FORMAT_R32G32B32A32_SFLOAT";
    case SPV_REFLECT_FORMAT_R64_UINT:
      return "VK_FORMAT_R64_UINT";
    case SPV_REFLECT_FORMAT_R64_SINT:
      return "VK_FORMAT_R64_SINT";
    case SPV_REFLECT_FORMAT_R64_SFLOAT:
      return "VK_FORMAT_R64_SFLOAT";
    case SPV_REFLECT_FORMAT_R64G64_UINT:
      return "VK_FORMAT_R64G64_UINT";
    case SPV_REFLECT_FORMAT_R64G64_SINT:
      return "VK_FORMAT_R64G64_SINT";
    case SPV_REFLECT_FORMAT_R64G64_SFLOAT:
      return "VK_FORMAT_R64G64_SFLOAT";
    case SPV_REFLECT_FORMAT_R64G64B64_UINT:
      return "VK_FORMAT_R64G64B64_UINT";
    case SPV_REFLECT_FORMAT_R64G64B64_SINT:
      return "VK_FORMAT_R64G64B64_SINT";
    case SPV_REFLECT_FORMAT_R64G64B64_SFLOAT:
      return "VK_FORMAT_R64G64B64_SFLOAT";
    case SPV_REFLECT_FORMAT_R64G64B64A64_UINT:
      return "VK_FORMAT_R64G64B64A64_UINT";
    case SPV_REFLECT_FORMAT_R64G64B64A64_SINT:
      return "VK_FORMAT_R64G64B64A64_SINT";
    case SPV_REFLECT_FORMAT_R64G64B64A64_SFLOAT:
      return "VK_FORMAT_R64G64B64A64_SFLOAT";
  }
  // unhandled SpvReflectFormat enum value
  return "VK_FORMAT_???";
}

static std::string ToStringScalarType(const SpvReflectTypeDescription& type) {
  switch (type.op) {
    case SpvOpTypeVoid: {
      return "void";
      break;
    }
    case SpvOpTypeBool: {
      return "bool";
      break;
    }
    case SpvOpTypeInt: {
      if (type.traits.numeric.scalar.signedness)
        return "int";
      else
        return "uint";
    }
    case SpvOpTypeFloat: {
      switch (type.traits.numeric.scalar.width) {
        case 32:
          return "float";
        case 64:
          return "double";
        default:
          break;
      }
      break;
    }
    case SpvOpTypeStruct: {
      return "struct";
    }
    case SpvOpTypePointer: {
      return "ptr";
    }
    default: {
      break;
    }
  }
  return "";
}

static std::string ToStringGlslType(const SpvReflectTypeDescription& type) {
  switch (type.op) {
    case SpvOpTypeVector: {
      switch (type.traits.numeric.scalar.width) {
        case 32: {
          switch (type.traits.numeric.vector.component_count) {
            case 2:
              return "vec2";
            case 3:
              return "vec3";
            case 4:
              return "vec4";
          }
        } break;

        case 64: {
          switch (type.traits.numeric.vector.component_count) {
            case 2:
              return "dvec2";
            case 3:
              return "dvec3";
            case 4:
              return "dvec4";
          }
        } break;
      }
    } break;
    default:
      break;
  }
  return ToStringScalarType(type);
}

static std::string ToStringHlslType(const SpvReflectTypeDescription& type) {
  switch (type.op) {
    case SpvOpTypeVector: {
      switch (type.traits.numeric.scalar.width) {
        case 32: {
          switch (type.traits.numeric.vector.component_count) {
            case 2:
              return "float2";
            case 3:
              return "float3";
            case 4:
              return "float4";
          }
        } break;

        case 64: {
          switch (type.traits.numeric.vector.component_count) {
            case 2:
              return "double2";
            case 3:
              return "double3";
            case 4:
              return "double4";
          }
        } break;
      }
    } break;

    default:
      break;
  }
  return ToStringScalarType(type);
}

std::string ToStringType(SpvSourceLanguage src_lang, const SpvReflectTypeDescription& type) {
  if (src_lang == SpvSourceLanguageHLSL) {
    return ToStringHlslType(type);
  }

  return ToStringGlslType(type);
}

std::string ToStringComponentType(const SpvReflectTypeDescription& type, uint32_t member_decoration_flags) {
  uint32_t masked_type = type.type_flags & 0xF;
  if (masked_type == 0) {
    return "";
  }

  std::stringstream ss;

  if (type.type_flags & SPV_REFLECT_TYPE_FLAG_MATRIX) {
    if (member_decoration_flags & SPV_REFLECT_DECORATION_COLUMN_MAJOR) {
      ss << "column_major"
         << " ";
    } else if (member_decoration_flags & SPV_REFLECT_DECORATION_ROW_MAJOR) {
      ss << "row_major"
         << " ";
    }
  }

  switch (masked_type) {
    default:
      assert(false && "unsupported component type");
      break;
    case SPV_REFLECT_TYPE_FLAG_BOOL:
      ss << "bool";
      break;
    case SPV_REFLECT_TYPE_FLAG_INT:
      ss << (type.traits.numeric.scalar.signedness ? "int" : "uint");
      break;
    case SPV_REFLECT_TYPE_FLAG_FLOAT:
      ss << "float";
      break;
  }

  if (type.type_flags & SPV_REFLECT_TYPE_FLAG_MATRIX) {
    ss << type.traits.numeric.matrix.row_count;
    ss << "x";
    ss << type.traits.numeric.matrix.column_count;
  } else if (type.type_flags & SPV_REFLECT_TYPE_FLAG_VECTOR) {
    ss << type.traits.numeric.vector.component_count;
  }

  if (type.type_flags & SPV_REFLECT_TYPE_FLAG_REF) {
    ss << "*";
  }

  return ss.str();
}

void ParseBlockMembersToTextLines(const char* indent, int indent_depth, bool flatten_cbuffers, const std::string& parent_name,
                                  uint32_t member_count, const SpvReflectBlockVariable* p_members,
                                  std::vector<TextLine>* p_text_lines, std::unordered_set<uint32_t>& physical_pointer_spirv_id) {
  const char* t = indent;
  for (uint32_t member_index = 0; member_index < member_count; ++member_index) {
    indent_depth = flatten_cbuffers ? 2 : indent_depth;
    std::stringstream ss_indent;
    for (int indent_count = 0; indent_count < indent_depth; ++indent_count) {
      ss_indent << t;
    }
    std::string expanded_indent = ss_indent.str();

    const auto& member = p_members[member_index];
    if (!member.type_description) {
      // TODO 212 - If a buffer ref has an array of itself, all members are null
      continue;
    }

    bool is_struct = ((member.type_description->type_flags & static_cast<SpvReflectTypeFlags>(SPV_REFLECT_TYPE_FLAG_STRUCT)) != 0);
    bool is_ref = ((member.type_description->type_flags & static_cast<SpvReflectTypeFlags>(SPV_REFLECT_TYPE_FLAG_REF)) != 0);
    bool is_array = ((member.type_description->type_flags & static_cast<SpvReflectTypeFlags>(SPV_REFLECT_TYPE_FLAG_ARRAY)) != 0);
    if (is_struct) {
      const std::string name = (member.name == nullptr ? "" : member.name);

      // Begin struct
      TextLine tl = {};
      tl.indent = expanded_indent;
      tl.type_name = (member.type_description->type_name == nullptr ? "" : member.type_description->type_name);
      tl.absolute_offset = member.absolute_offset;
      tl.relative_offset = member.offset;
      tl.size = member.size;
      tl.padded_size = member.padded_size;
      tl.array_stride = member.array.stride;
      tl.block_variable_flags = member.flags;
      tl.text_line_flags = is_ref ? TEXT_LINE_TYPE_REF_BEGIN : TEXT_LINE_TYPE_STRUCT_BEGIN;
      if (!flatten_cbuffers) {
        p_text_lines->push_back(tl);
      }

      const bool array_of_structs = is_array && member.type_description->struct_type_description;
      const uint32_t struct_id =
          array_of_structs ? member.type_description->struct_type_description->id : member.type_description->id;

      if (physical_pointer_spirv_id.count(struct_id) == 0) {
        physical_pointer_spirv_id.insert(member.type_description->id);
        if (array_of_structs) {
          physical_pointer_spirv_id.insert(member.type_description->struct_type_description->id);
        }

        // Members
        tl = {};
        std::string current_parent_name;
        if (flatten_cbuffers) {
          current_parent_name = parent_name.empty() ? name : (parent_name + "." + name);
        }
        std::vector<TextLine>* p_target_text_line = flatten_cbuffers ? p_text_lines : &tl.lines;
        ParseBlockMembersToTextLines(t, indent_depth + 1, flatten_cbuffers, current_parent_name, member.member_count,
                                     member.members, p_target_text_line, physical_pointer_spirv_id);
        tl.text_line_flags = TEXT_LINE_TYPE_LINES;
        p_text_lines->push_back(tl);
      }
      physical_pointer_spirv_id.erase(member.type_description->id);

      // End struct
      tl = {};
      tl.indent = expanded_indent;
      tl.name = name;
      if ((member.array.dims_count > 0) || (member.type_description->traits.array.dims[0] > 0)) {
        const SpvReflectArrayTraits* p_array_info = (member.array.dims_count > 0) ? &member.array : nullptr;
        if (p_array_info == nullptr) {
          //
          // glslang based compilers stores array information in the type and
          // not the variable
          //
          p_array_info = (member.type_description->traits.array.dims[0] > 0) ? &member.type_description->traits.array : nullptr;
        }
        if (p_array_info != nullptr) {
          std::stringstream ss_array;
          for (uint32_t array_dim_index = 0; array_dim_index < p_array_info->dims_count; ++array_dim_index) {
            uint32_t dim = p_array_info->dims[array_dim_index];
            //
            // dim = 0 means it's an unbounded array
            //
            if (dim > 0) {
              ss_array << "[" << dim << "]";
            } else {
              ss_array << "[]";
            }
          }
          tl.name += ss_array.str();
        }
      }
      tl.absolute_offset = member.absolute_offset;
      tl.relative_offset = member.offset;
      tl.size = member.size;
      tl.padded_size = member.padded_size;
      tl.array_stride = member.array.stride;
      tl.block_variable_flags = member.flags;
      tl.text_line_flags = is_ref ? TEXT_LINE_TYPE_REF_END : TEXT_LINE_TYPE_STRUCT_END;
      if (!flatten_cbuffers) {
        p_text_lines->push_back(tl);
      }
    } else {
      std::string name = (member.name == nullptr ? "" : member.name);
      if (flatten_cbuffers) {
        if (!parent_name.empty()) {
          name = parent_name + "." + name;
        }
      }

      TextLine tl = {};
      tl.indent = expanded_indent;
      tl.type_name = ToStringComponentType(*member.type_description, member.decoration_flags);
      tl.name = name;
      if (member.array.dims_count > 0) {
        std::stringstream ss_array;
        for (uint32_t array_dim_index = 0; array_dim_index < member.array.dims_count; ++array_dim_index) {
          uint32_t dim = member.array.dims[array_dim_index];
          ss_array << "[" << dim << "]";
        }
        tl.name += ss_array.str();
      }
      tl.absolute_offset = member.absolute_offset;
      tl.relative_offset = member.offset;
      tl.size = member.size;
      tl.padded_size = member.padded_size;
      tl.array_stride = member.array.stride;
      tl.block_variable_flags = member.flags;
      p_text_lines->push_back(tl);
    }
  }
}

void ParseBlockVariableToTextLines(const char* indent, bool flatten_cbuffers, const SpvReflectBlockVariable& block_var,
                                   std::vector<TextLine>* p_text_lines) {
  // Begin block
  TextLine tl = {};
  tl.indent = indent;
  tl.type_name = (block_var.type_description->type_name != nullptr) ? block_var.type_description->type_name : "<unnamed>";
  tl.size = block_var.size;
  tl.padded_size = block_var.padded_size;
  tl.text_line_flags = TEXT_LINE_TYPE_BLOCK_BEGIN;
  p_text_lines->push_back(tl);

  // Members
  tl = {};
  std::unordered_set<uint32_t> physical_pointer_spirv_id;
  ParseBlockMembersToTextLines(indent, 2, flatten_cbuffers, "", block_var.member_count, block_var.members, &tl.lines,
                               physical_pointer_spirv_id);
  tl.text_line_flags = TEXT_LINE_TYPE_LINES;
  p_text_lines->push_back(tl);

  // End block
  tl = {};
  tl.indent = indent;
  tl.name = (block_var.name != nullptr) ? block_var.name : "<unnamed>";
  tl.absolute_offset = 0;
  tl.relative_offset = 0;
  tl.size = block_var.size;
  tl.padded_size = block_var.padded_size;
  tl.text_line_flags = TEXT_LINE_TYPE_BLOCK_END;
  p_text_lines->push_back(tl);
}

void FormatTextLines(const std::vector<TextLine>& text_lines, const char* indent, std::vector<TextLine>* p_formatted_lines) {
  size_t modifier_width = 0;
  size_t type_name_width = 0;
  size_t name_width = 0;

  // Widths
  for (auto& tl : text_lines) {
    if (tl.text_line_flags != 0) {
      continue;
    }
    modifier_width = std::max<size_t>(modifier_width, tl.modifier.length());
    type_name_width = std::max<size_t>(type_name_width, tl.type_name.length());
    name_width = std::max<size_t>(name_width, tl.name.length());
  }

  // Output
  size_t n = text_lines.size();
  for (size_t i = 0; i < n; ++i) {
    auto& tl = text_lines[i];

    std::stringstream ss;
    if ((tl.text_line_flags == TEXT_LINE_TYPE_BLOCK_BEGIN) || (tl.text_line_flags == TEXT_LINE_TYPE_STRUCT_BEGIN) ||
        (tl.text_line_flags == TEXT_LINE_TYPE_REF_BEGIN)) {
      ss << indent;
      ss << tl.indent;
      if (tl.text_line_flags == TEXT_LINE_TYPE_REF_BEGIN) ss << "ref ";
      ss << "struct ";
      ss << tl.type_name;
      ss << " {";
    } else if ((tl.text_line_flags == TEXT_LINE_TYPE_BLOCK_END) || (tl.text_line_flags == TEXT_LINE_TYPE_STRUCT_END) ||
               (tl.text_line_flags == TEXT_LINE_TYPE_REF_END)) {
      ss << indent;
      ss << tl.indent;
      ss << "} ";
      ss << tl.name;
      ss << ";";
    } else if (tl.text_line_flags == TEXT_LINE_TYPE_LINES) {
      FormatTextLines(tl.lines, indent, p_formatted_lines);
    } else {
      ss << indent;
      ss << tl.indent;
      if (modifier_width > 0) {
        ss << std::setw(modifier_width) << std::left << tl.modifier;
        ss << " ";
      }
      ss << std::setw(type_name_width) << std::left << tl.type_name;
      ss << " ";
      ss << std::setw(name_width) << (tl.name + ";");
    }

    // Reuse the various strings to store the formatted texts
    TextLine out_tl = {};
    out_tl.formatted_line = ss.str();
    if (out_tl.formatted_line.length() > 0) {
      out_tl.array_stride = tl.array_stride;
      out_tl.text_line_flags = tl.text_line_flags;
      out_tl.formatted_absolute_offset = std::to_string(tl.absolute_offset);
      out_tl.formatted_relative_offset = std::to_string(tl.relative_offset);
      out_tl.formatted_size = std::to_string(tl.size);
      out_tl.formatted_padded_size = std::to_string(tl.padded_size);
      out_tl.formatted_array_stride = std::to_string(tl.array_stride);
      // Block variable flags
      if (tl.block_variable_flags != 0) {
        std::stringstream ss_flags;
        if (tl.block_variable_flags & SPV_REFLECT_VARIABLE_FLAGS_UNUSED) {
          ss_flags << "UNUSED";
        }
        out_tl.formatted_block_variable_flags = ss_flags.str();
      }
      p_formatted_lines->push_back(out_tl);
    }
  }
}

void StreamWriteTextLines(std::ostream& os, const char* indent, bool flatten_cbuffers, const std::vector<TextLine>& text_lines) {
  std::vector<TextLine> formatted_lines;
  FormatTextLines(text_lines, indent, &formatted_lines);

  size_t line_width = 0;
  size_t offset_width = 0;
  size_t absolute_offset_width = 0;
  size_t size_width = 0;
  size_t padded_size_width = 0;
  size_t array_stride_width = 0;

  // Width
  for (auto& tl : formatted_lines) {
    if (tl.text_line_flags != 0) {
      continue;
    }
    line_width = std::max<size_t>(line_width, tl.formatted_line.length());
    absolute_offset_width = std::max<size_t>(absolute_offset_width, tl.formatted_absolute_offset.length());
    offset_width = std::max<size_t>(offset_width, tl.formatted_relative_offset.length());
    size_width = std::max<size_t>(size_width, tl.formatted_size.length());
    padded_size_width = std::max<size_t>(padded_size_width, tl.formatted_padded_size.length());
    array_stride_width = std::max<size_t>(array_stride_width, tl.formatted_array_stride.length());
  }

  size_t n = formatted_lines.size();
  for (size_t i = 0; i < n; ++i) {
    auto& tl = formatted_lines[i];

    if (tl.text_line_flags == TEXT_LINE_TYPE_BLOCK_BEGIN) {
      if (i > 0) {
        os << "\n";
      }

      size_t pos = tl.formatted_line.find_first_not_of(' ');
      if (pos != std::string::npos) {
        std::string s(pos, ' ');
        os << s << "//"
           << " ";
        os << "size = " << tl.formatted_size << ", ";
        os << "padded size = " << tl.formatted_padded_size;
        os << "\n";
      }

      os << std::setw(line_width) << std::left << tl.formatted_line;
    } else if (tl.text_line_flags == TEXT_LINE_TYPE_BLOCK_END) {
      os << std::setw(line_width) << std::left << tl.formatted_line;
      if (i < (n - 1)) {
        os << "\n";
      }
    } else if (tl.text_line_flags == TEXT_LINE_TYPE_STRUCT_BEGIN || tl.text_line_flags == TEXT_LINE_TYPE_REF_BEGIN) {
      if (!flatten_cbuffers) {
        if (i > 0) {
          os << "\n";
        }

        size_t pos = tl.formatted_line.find_first_not_of(' ');
        if (pos != std::string::npos) {
          std::string s(pos, ' ');
          os << s << "//"
             << " ";
          os << "abs offset = " << tl.formatted_absolute_offset << ", ";
          os << "rel offset = " << tl.formatted_relative_offset << ", ";
          os << "size = " << tl.formatted_size << ", ";
          os << "padded size = " << tl.formatted_padded_size;
          if (tl.array_stride > 0) {
            os << ", ";
            os << "array stride = " << tl.formatted_array_stride;
          }
          if (!tl.formatted_block_variable_flags.empty()) {
            os << " ";
            os << tl.formatted_block_variable_flags;
          }
          os << "\n";
        }

        os << std::setw(line_width) << std::left << tl.formatted_line;
      }
    } else if (tl.text_line_flags == TEXT_LINE_TYPE_STRUCT_END || tl.text_line_flags == TEXT_LINE_TYPE_REF_END) {
      if (!flatten_cbuffers) {
        os << std::setw(line_width) << std::left << tl.formatted_line;
        if (i < (n - 1)) {
          os << "\n";
        }
      }
    } else {
      os << std::setw(line_width) << std::left << tl.formatted_line;
      os << " "
         << "//"
         << " ";
      os << "abs offset = " << std::setw(absolute_offset_width) << std::right << tl.formatted_absolute_offset << ", ";
      if (!flatten_cbuffers) {
        os << "rel offset = " << std::setw(offset_width) << std::right << tl.formatted_relative_offset << ", ";
      }
      os << "size = " << std::setw(size_width) << std::right << tl.formatted_size << ", ";
      os << "padded size = " << std::setw(padded_size_width) << std::right << tl.formatted_padded_size;
      if (tl.array_stride > 0) {
        os << ", ";
        os << "array stride = " << std::setw(array_stride_width) << tl.formatted_array_stride;
      }
      if (!tl.formatted_block_variable_flags.empty()) {
        os << " ";
        os << tl.formatted_block_variable_flags;
      }
    }

    if (i < (n - 1)) {
      os << "\n";
    }
  }
}

void StreamWritePushConstantsBlock(std::ostream& os, const SpvReflectBlockVariable& obj, bool flatten_cbuffers,
                                   const char* indent) {
  const char* t = indent;
  os << t << "spirv id : " << obj.spirv_id << "\n";

  os << t << "name     : " << ((obj.name != nullptr) ? obj.name : "<unnamed>");
  if ((obj.type_description->type_name != nullptr) && (strlen(obj.type_description->type_name) > 0)) {
    os << " "
       << "(" << obj.type_description->type_name << ")";
  }

  std::vector<TextLine> text_lines;
  ParseBlockVariableToTextLines("    ", flatten_cbuffers, obj, &text_lines);
  if (!text_lines.empty()) {
    os << "\n";
    StreamWriteTextLines(os, t, flatten_cbuffers, text_lines);
    os << "\n";
  }
}

void StreamWriteDescriptorBinding(std::ostream& os, const SpvReflectDescriptorBinding& obj, bool write_set, bool flatten_cbuffers,
                                  const char* indent) {
  const char* t = indent;
  os << t << "spirv id : " << obj.spirv_id << "\n";
  if (write_set) {
    os << t << "set      : " << obj.set << "\n";
  }
  os << t << "binding  : " << obj.binding << "\n";
  os << t << "type     : " << ToStringDescriptorType(obj.descriptor_type);
  os << " "
     << "(" << ToStringResourceType(obj.resource_type) << ")"
     << "\n";

  // count
  os << t << "count    : " << obj.count << "\n";

  // array
  if (obj.array.dims_count > 0) {
    os << t << "array    : ";
    for (uint32_t dim_index = 0; dim_index < obj.array.dims_count; ++dim_index) {
      os << "[" << obj.array.dims[dim_index] << "]";
    }
    os << "\n";
  }

  // counter
  if (obj.uav_counter_binding != nullptr) {
    os << t << "counter  : ";
    os << "(";
    os << "set=" << obj.uav_counter_binding->set << ", ";
    os << "binding=" << obj.uav_counter_binding->binding << ", ";
    os << "name=" << obj.uav_counter_binding->name;
    os << ");";
    os << "\n";
  }

  // accessed
  os << t << "accessed : " << (obj.accessed ? "true" : "false") << "\n";

  os << t << "name     : " << ((obj.name != nullptr) ? obj.name : "<unnamed>");
  if ((obj.type_description->type_name != nullptr) && (strlen(obj.type_description->type_name) > 0)) {
    os << " "
       << "(" << obj.type_description->type_name << ")";
  }

  if (obj.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
      obj.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
    std::vector<TextLine> text_lines;
    ParseBlockVariableToTextLines("    ", flatten_cbuffers, obj.block, &text_lines);
    if (!text_lines.empty()) {
      os << "\n";
      StreamWriteTextLines(os, t, flatten_cbuffers, text_lines);
      os << "\n";
    }
  }
}

void StreamWriteInterfaceVariable(std::ostream& os, const SpvReflectInterfaceVariable& obj, const char* indent) {
  const char* t = indent;
  os << t << "spirv id  : " << obj.spirv_id << "\n";
  os << t << "location  : ";
  if (obj.decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN) {
    os << ToStringSpvBuiltIn(obj, true);
  } else {
    os << obj.location;
  }
  os << "\n";
  os << t << "type      : " << ToStringComponentType(*obj.type_description, 0) << "\n";

  // array
  if (obj.array.dims_count > 0) {
    os << t << "array     : ";
    for (uint32_t dim_index = 0; dim_index < obj.array.dims_count; ++dim_index) {
      os << "[" << obj.array.dims[dim_index] << "]";
    }
    os << "\n";
  }

  os << t << "semantic  : " << (obj.semantic != NULL ? obj.semantic : "") << "\n";
  os << t << "name      : " << (obj.name != NULL ? obj.name : "");
  if ((obj.type_description->type_name != nullptr) && (strlen(obj.type_description->type_name) > 0)) {
    os << " "
       << "(" << obj.type_description->type_name << ")";
  }
  os << "\n";
  os << t << "qualifier : ";
  if (obj.decoration_flags & SPV_REFLECT_DECORATION_FLAT) {
    os << "flat";
  } else if (obj.decoration_flags & SPV_REFLECT_DECORATION_NOPERSPECTIVE) {
    os << "noperspective";
  }
}

void StreamWriteEntryPoint(std::ostream& os, const SpvReflectEntryPoint& obj, const char* indent) {
  os << indent << "entry point     : " << obj.name;
  os << " (stage=" << ToStringShaderStage(obj.shader_stage) << ")";
  if (obj.shader_stage == SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT) {
    os << "\n";
    os << "local size      : "
       << "(" << (obj.local_size.x == SPV_REFLECT_EXECUTION_MODE_SPEC_CONSTANT ? "Spec Constant" : std::to_string(obj.local_size.x))
       << ", "
       << (obj.local_size.y == SPV_REFLECT_EXECUTION_MODE_SPEC_CONSTANT ? "Spec Constant" : std::to_string(obj.local_size.y))
       << ", "
       << (obj.local_size.z == SPV_REFLECT_EXECUTION_MODE_SPEC_CONSTANT ? "Spec Constant" : std::to_string(obj.local_size.z))
       << ")";
  }
}

void StreamWriteShaderModule(std::ostream& os, const SpvReflectShaderModule& obj, const char* indent) {
  (void)indent;
  os << "generator       : " << ToStringGenerator(obj.generator) << "\n";
  os << "source lang     : " << spvReflectSourceLanguage(obj.source_language) << "\n";
  os << "source lang ver : " << obj.source_language_version << "\n";
  os << "source file     : " << (obj.source_file != NULL ? obj.source_file : "") << "\n";
  // os << "shader stage    : " << ToStringShaderStage(obj.shader_stage) <<
  // "\n";

  for (uint32_t i = 0; i < obj.entry_point_count; ++i) {
    StreamWriteEntryPoint(os, obj.entry_points[i], "");
    if (i < (obj.entry_point_count - 1)) {
      os << "\n";
    }
  }
}

// Avoid unused variable warning/error on Linux
#ifndef NDEBUG
#define USE_ASSERT(x) assert(x)
#else
#define USE_ASSERT(x) ((void)(x))
#endif

void WriteReflection(const spv_reflect::ShaderModule& obj, bool flatten_cbuffers, std::ostream& os) {
  const char* t = "  ";
  const char* tt = "    ";
  const char* ttt = "      ";

  StreamWriteShaderModule(os, obj.GetShaderModule(), "");

  uint32_t count = 0;
  std::vector<SpvReflectInterfaceVariable*> variables;
  std::vector<SpvReflectDescriptorBinding*> bindings;
  std::vector<SpvReflectDescriptorSet*> sets;
  std::vector<SpvReflectBlockVariable*> push_constant_bocks;

  count = 0;
  SpvReflectResult result = obj.EnumerateInputVariables(&count, nullptr);
  USE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
  variables.resize(count);
  result = obj.EnumerateInputVariables(&count, variables.data());
  USE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
  if (count > 0) {
    os << "\n";
    os << "\n";
    os << "\n";
    os << t << "Input variables: " << count << "\n\n";
    for (size_t i = 0; i < variables.size(); ++i) {
      auto p_var = variables[i];
      USE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
      os << tt << i << ":"
         << "\n";
      StreamWriteInterfaceVariable(os, *p_var, ttt);
      if (i < (count - 1)) {
        os << "\n";
      }
    }
  }

  count = 0;
  result = obj.EnumerateOutputVariables(&count, nullptr);
  USE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
  variables.resize(count);
  result = obj.EnumerateOutputVariables(&count, variables.data());
  USE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
  if (count > 0) {
    os << "\n";
    os << "\n";
    os << "\n";
    os << t << "Output variables: " << count << "\n\n";
    for (size_t i = 0; i < variables.size(); ++i) {
      auto p_var = variables[i];
      USE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
      os << tt << i << ":"
         << "\n";
      StreamWriteInterfaceVariable(os, *p_var, ttt);
      if (i < (count - 1)) {
        os << "\n";
      }
    }
  }

  count = 0;
  result = obj.EnumeratePushConstantBlocks(&count, nullptr);
  USE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
  push_constant_bocks.resize(count);
  result = obj.EnumeratePushConstantBlocks(&count, push_constant_bocks.data());
  USE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
  if (count > 0) {
    os << "\n";
    os << "\n";
    os << "\n";
    os << t << "Push constant blocks: " << count << "\n\n";
    for (size_t i = 0; i < push_constant_bocks.size(); ++i) {
      auto p_block = push_constant_bocks[i];
      os << tt << i << ":"
         << "\n";
      StreamWritePushConstantsBlock(os, *p_block, flatten_cbuffers, ttt);
    }
  }

  count = 0;
  result = obj.EnumerateDescriptorBindings(&count, nullptr);
  USE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
  bindings.resize(count);
  result = obj.EnumerateDescriptorBindings(&count, bindings.data());
  USE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
  std::sort(std::begin(bindings), std::end(bindings), [](SpvReflectDescriptorBinding* a, SpvReflectDescriptorBinding* b) -> bool {
    if (a->set != b->set) {
      return a->set < b->set;
    }
    return a->binding < b->binding;
  });
  if (count > 0) {
    os << "\n";
    os << "\n";
    os << "\n";
    os << t << "Descriptor bindings: " << count << "\n\n";
    for (size_t i = 0; i < bindings.size(); ++i) {
      auto p_binding = bindings[i];
      USE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
      os << tt << "Binding"
         << " " << p_binding->set << "." << p_binding->binding << ""
         << "\n";
      StreamWriteDescriptorBinding(os, *p_binding, true, flatten_cbuffers, ttt);
      if (i < (count - 1)) {
        os << "\n\n";
      }
    }
  }
}

//////////////////////////////////

SpvReflectToYaml::SpvReflectToYaml(const SpvReflectShaderModule& shader_module, uint32_t verbosity)
    : sm_(shader_module), verbosity_(verbosity) {}

void SpvReflectToYaml::WriteTypeDescription(std::ostream& os, const SpvReflectTypeDescription& td, uint32_t indent_level) {
  // YAML anchors can only refer to points earlier in the doc, so child type
  // descriptions must be processed before the parent.
  if (!td.copied) {
    for (uint32_t i = 0; i < td.member_count; ++i) {
      WriteTypeDescription(os, td.members[i], indent_level);
    }
  }
  const std::string t0 = Indent(indent_level);
  const std::string t1 = Indent(indent_level + 1);
  const std::string t2 = Indent(indent_level + 2);
  const std::string t3 = Indent(indent_level + 3);
  const std::string t4 = Indent(indent_level + 4);

  // Determine the index of this type within the shader module's list.
  uint32_t type_description_index = static_cast<uint32_t>(type_description_to_index_.size());
  type_description_to_index_[&td] = type_description_index;

  os << t0 << "- &td" << type_description_index << std::endl;
  // typedef struct SpvReflectTypeDescription {
  //   uint32_t                          id;
  os << t1 << "id: " << td.id << std::endl;
  //   SpvOp                             op;
  os << t1 << "op: " << td.op << std::endl;
  //   const char*                       type_name;
  os << t1 << "type_name: " << SafeString(td.type_name) << std::endl;
  //   const char*                       struct_member_name;
  os << t1 << "struct_member_name: " << SafeString(td.struct_member_name) << std::endl;
  //   SpvStorageClass                   storage_class;
  os << t1 << "storage_class: " << td.storage_class << " # " << ToStringSpvStorageClass(td.storage_class) << std::endl;
  //   SpvReflectTypeFlags               type_flags;
  os << t1 << "type_flags: " << AsHexString(td.type_flags) << " # " << ToStringTypeFlags(td.type_flags) << std::endl;
  //   SpvReflectDecorationFlags         decoration_flags;
  os << t1 << "decoration_flags: " << AsHexString(td.decoration_flags) << " # " << ToStringDecorationFlags(td.decoration_flags)
     << std::endl;
  //   struct Traits {
  os << t1 << "traits:" << std::endl;
  //     SpvReflectNumericTraits         numeric;
  // typedef struct SpvReflectNumericTraits {
  os << t2 << "numeric:" << std::endl;
  //   struct Scalar {
  //     uint32_t                        width;
  //     uint32_t                        signedness;
  //   } scalar;
  os << t3 << "scalar: { ";
  os << "width: " << td.traits.numeric.scalar.width << ", ";
  os << "signedness: " << td.traits.numeric.scalar.signedness;
  os << " }" << std::endl;
  //   struct Vector {
  //     uint32_t                        component_count;
  //   } vector;
  os << t3 << "vector: { ";
  os << "component_count: " << td.traits.numeric.vector.component_count;
  os << " }" << std::endl;
  //   struct Matrix {
  //     uint32_t                        column_count;
  //     uint32_t                        row_count;
  //     uint32_t                        stride; // Measured in bytes
  //   } matrix;
  os << t3 << "matrix: { ";
  os << "column_count: " << td.traits.numeric.matrix.column_count << ", ";
  os << "row_count: " << td.traits.numeric.matrix.row_count << ", ";
  ;
  os << "stride: " << td.traits.numeric.matrix.stride;
  os << " }" << std::endl;
  // } SpvReflectNumericTraits;

  //     SpvReflectImageTraits           image;
  os << t2 << "image: { ";
  // typedef struct SpvReflectImageTraits {
  //   SpvDim                            dim;
  os << "dim: " << td.traits.image.dim << ", ";
  //   uint32_t                          depth;
  os << "depth: " << td.traits.image.depth << ", ";
  //   uint32_t                          arrayed;
  os << "arrayed: " << td.traits.image.arrayed << ", ";
  //   uint32_t                          ms;
  os << "ms: " << td.traits.image.ms << ", ";
  //   uint32_t                          sampled;
  os << "sampled: " << td.traits.image.sampled << ", ";
  //   SpvImageFormat                    image_format;
  os << "image_format: " << td.traits.image.image_format;
  // } SpvReflectImageTraits;
  os << " }"
     << " # dim=" << ToStringSpvDim(td.traits.image.dim) << " image_format=" << ToStringSpvImageFormat(td.traits.image.image_format)
     << std::endl;

  //     SpvReflectArrayTraits           array;
  os << t2 << "array: { ";
  // typedef struct SpvReflectArrayTraits {
  //   uint32_t                          dims_count;
  os << "dims_count: " << td.traits.array.dims_count << ", ";
  //   uint32_t                          dims[SPV_REFLECT_MAX_ARRAY_DIMS];
  os << "dims: [";
  for (uint32_t i_dim = 0; i_dim < td.traits.array.dims_count; ++i_dim) {
    os << td.traits.array.dims[i_dim] << ",";
  }
  os << "], ";
  //   uint32_t                          stride; // Measured in bytes
  os << "stride: " << td.traits.array.stride;
  // } SpvReflectArrayTraits;
  os << " }" << std::endl;
  //   } traits;

  //   uint32_t                          member_count;
  os << t1 << "member_count: " << td.member_count << std::endl;
  //   struct SpvReflectTypeDescription* members;
  os << t1 << "members:" << std::endl;
  if (td.copied) {
    os << t1 << "- [forward pointer]" << std::endl;
  } else {
    for (uint32_t i_member = 0; i_member < td.member_count; ++i_member) {
      os << t2 << "- *td" << type_description_to_index_[&(td.members[i_member])] << std::endl;
    }
  }
  // } SpvReflectTypeDescription;
}

void SpvReflectToYaml::WriteBlockVariable(std::ostream& os, const SpvReflectBlockVariable& bv, uint32_t indent_level) {
  if ((bv.flags & SPV_REFLECT_VARIABLE_FLAGS_PHYSICAL_POINTER_COPY)) {
    return;  // catches recursive buffer references
  }

  for (uint32_t i = 0; i < bv.member_count; ++i) {
    WriteBlockVariable(os, bv.members[i], indent_level);
  }

  const std::string t0 = Indent(indent_level);
  const std::string t1 = Indent(indent_level + 1);
  const std::string t2 = Indent(indent_level + 2);
  const std::string t3 = Indent(indent_level + 3);

  assert(block_variable_to_index_.find(&bv) == block_variable_to_index_.end());
  uint32_t block_variable_index = static_cast<uint32_t>(block_variable_to_index_.size());
  block_variable_to_index_[&bv] = block_variable_index;

  os << t0 << "- &bv" << block_variable_index << std::endl;
  // typedef struct SpvReflectBlockVariable {
  //   const char*                       name;
  os << t1 << "name: " << SafeString(bv.name) << std::endl;
  //   uint32_t                          offset;           // Measured in bytes
  os << t1 << "offset: " << bv.offset << std::endl;
  //   uint32_t                          absolute_offset;  // Measured in bytes
  os << t1 << "absolute_offset: " << bv.absolute_offset << std::endl;
  //   uint32_t                          size;             // Measured in bytes
  os << t1 << "size: " << bv.size << std::endl;
  //   uint32_t                          padded_size;      // Measured in bytes
  os << t1 << "padded_size: " << bv.padded_size << std::endl;
  //   SpvReflectDecorationFlags         decoration_flags;
  os << t1 << "decorations: " << AsHexString(bv.decoration_flags) << " # " << ToStringDecorationFlags(bv.decoration_flags)
     << std::endl;
  //   SpvReflectNumericTraits           numeric;
  // typedef struct SpvReflectNumericTraits {
  os << t1 << "numeric:" << std::endl;
  //   struct Scalar {
  //     uint32_t                        width;
  //     uint32_t                        signedness;
  //   } scalar;
  os << t2 << "scalar: { ";
  os << "width: " << bv.numeric.scalar.width << ", ";
  os << "signedness: " << bv.numeric.scalar.signedness << " }" << std::endl;
  //   struct Vector {
  //     uint32_t                        component_count;
  //   } vector;
  os << t2 << "vector: { ";
  os << "component_count: " << bv.numeric.vector.component_count << " }" << std::endl;
  //   struct Matrix {
  //     uint32_t                        column_count;
  //     uint32_t                        row_count;
  //     uint32_t                        stride; // Measured in bytes
  //   } matrix;
  os << t2 << "matrix: { ";
  os << "column_count: " << bv.numeric.matrix.column_count << ", ";
  os << "row_count: " << bv.numeric.matrix.row_count << ", ";
  os << "stride: " << bv.numeric.matrix.stride << " }" << std::endl;
  // } SpvReflectNumericTraits;

  //     SpvReflectArrayTraits           array;
  os << t1 << "array: { ";
  // typedef struct SpvReflectArrayTraits {
  //   uint32_t                          dims_count;
  os << "dims_count: " << bv.array.dims_count << ", ";
  //   uint32_t                          dims[SPV_REFLECT_MAX_ARRAY_DIMS];
  os << "dims: [";
  for (uint32_t i_dim = 0; i_dim < bv.array.dims_count; ++i_dim) {
    os << bv.array.dims[i_dim] << ",";
  }
  os << "], ";
  //   uint32_t                          stride; // Measured in bytes
  os << "stride: " << bv.array.stride;
  // } SpvReflectArrayTraits;
  os << " }" << std::endl;

  //   SpvReflectVariableFlags           flags;
  os << t1 << "flags: " << AsHexString(bv.flags) << " # " << ToStringVariableFlags(bv.flags) << std::endl;

  //   uint32_t                          member_count;
  os << t1 << "member_count: " << bv.member_count << std::endl;
  //   struct SpvReflectBlockVariable*   members;
  os << t1 << "members:" << std::endl;
  for (uint32_t i = 0; i < bv.member_count; ++i) {
    auto itor = block_variable_to_index_.find(&bv.members[i]);
    if (itor != block_variable_to_index_.end()) {
      os << t2 << "- *bv" << itor->second << std::endl;
    } else {
      os << t2 << "- [recursive]" << std::endl;
    }
  }
  if (verbosity_ >= 1) {
    //   SpvReflectTypeDescription*        type_description;
    if (bv.type_description == nullptr) {
      os << t1 << "type_description:" << std::endl;
    } else {
      auto itor = type_description_to_index_.find(bv.type_description);
      assert(itor != type_description_to_index_.end());
      os << t1 << "type_description: *td" << itor->second << std::endl;
    }
  }
  // } SpvReflectBlockVariable;
}

void SpvReflectToYaml::WriteDescriptorBinding(std::ostream& os, const SpvReflectDescriptorBinding& db, uint32_t indent_level) {
  if (db.uav_counter_binding != nullptr) {
    auto itor = descriptor_binding_to_index_.find(db.uav_counter_binding);
    if (itor == descriptor_binding_to_index_.end()) {
      WriteDescriptorBinding(os, *(db.uav_counter_binding), indent_level);
    }
  }

  const std::string t0 = Indent(indent_level);
  const std::string t1 = Indent(indent_level + 1);
  const std::string t2 = Indent(indent_level + 2);
  const std::string t3 = Indent(indent_level + 3);

  // A binding's UAV binding later may appear later in the table than the
  // binding itself, in which case we've already output entries for both
  // bindings, and can just write another reference here.
  {
    auto itor = descriptor_binding_to_index_.find(&db);
    if (itor != descriptor_binding_to_index_.end()) {
      os << t0 << "- *db" << itor->second << std::endl;
      return;
    }
  }

  uint32_t descriptor_binding_index = static_cast<uint32_t>(descriptor_binding_to_index_.size());
  descriptor_binding_to_index_[&db] = descriptor_binding_index;

  os << t0 << "- &db" << descriptor_binding_index << std::endl;
  // typedef struct SpvReflectDescriptorBinding {
  //   uint32_t                            spirv_id;
  os << t1 << "spirv_id: " << db.spirv_id << std::endl;
  //   const char*                         name;
  os << t1 << "name: " << SafeString(db.name) << std::endl;
  //   uint32_t                            binding;
  os << t1 << "binding: " << db.binding << std::endl;
  //   uint32_t                            input_attachment_index;
  os << t1 << "input_attachment_index: " << db.input_attachment_index << std::endl;
  //   uint32_t                            set;
  os << t1 << "set: " << db.set << std::endl;
  //   SpvReflectDecorationFlags           decoration_flags;
  os << t1 << "decoration_flags: " << AsHexString(db.decoration_flags) << " # " << ToStringDecorationFlags(db.decoration_flags)
     << std::endl;
  //   SpvReflectDescriptorType            descriptor_type;
  os << t1 << "descriptor_type: " << db.descriptor_type << " # " << ToStringDescriptorType(db.descriptor_type) << std::endl;
  //   SpvReflectResourceType              resource_type;
  os << t1 << "resource_type: " << db.resource_type << " # " << ToStringResourceType(db.resource_type) << std::endl;
  //   SpvReflectImageTraits           image;
  os << t1 << "image: { ";
  // typedef struct SpvReflectImageTraits {
  //   SpvDim                            dim;
  os << "dim: " << db.image.dim << ", ";
  //   uint32_t                          depth;
  os << "depth: " << db.image.depth << ", ";
  //   uint32_t                          arrayed;
  os << "arrayed: " << db.image.arrayed << ", ";
  //   uint32_t                          ms;
  os << "ms: " << db.image.ms << ", ";
  //   uint32_t                          sampled;
  os << "sampled: " << db.image.sampled << ", ";
  //   SpvImageFormat                    image_format;
  os << "image_format: " << db.image.image_format;
  // } SpvReflectImageTraits;
  os << " }"
     << " # dim=" << ToStringSpvDim(db.image.dim) << " image_format=" << ToStringSpvImageFormat(db.image.image_format) << std::endl;

  //   SpvReflectBlockVariable             block;
  {
    auto itor = block_variable_to_index_.find(&db.block);
    assert(itor != block_variable_to_index_.end());
    os << t1 << "block: *bv" << itor->second << " # " << SafeString(db.block.name) << std::endl;
  }
  //   SpvReflectBindingArrayTraits        array;
  os << t1 << "array: { ";
  // typedef struct SpvReflectBindingArrayTraits {
  //   uint32_t                          dims_count;
  os << "dims_count: " << db.array.dims_count << ", ";
  //   uint32_t                          dims[SPV_REFLECT_MAX_ARRAY_DIMS];
  os << "dims: [";
  for (uint32_t i_dim = 0; i_dim < db.array.dims_count; ++i_dim) {
    os << db.array.dims[i_dim] << ",";
  }
  // } SpvReflectBindingArrayTraits;
  os << "] }" << std::endl;

  //   uint32_t                            accessed;
  os << t1 << "accessed: " << db.accessed << std::endl;

  //   uint32_t                            uav_counter_id;
  os << t1 << "uav_counter_id: " << db.uav_counter_id << std::endl;
  //   struct SpvReflectDescriptorBinding* uav_counter_binding;
  if (db.uav_counter_binding == nullptr) {
    os << t1 << "uav_counter_binding:" << std::endl;
  } else {
    auto itor = descriptor_binding_to_index_.find(db.uav_counter_binding);
    assert(itor != descriptor_binding_to_index_.end());
    os << t1 << "uav_counter_binding: *db" << itor->second << " # " << SafeString(db.uav_counter_binding->name) << std::endl;
  }

  if (db.byte_address_buffer_offset_count > 0) {
    os << t1 << "ByteAddressBuffer offsets: [";
    for (uint32_t i = 0; i < db.byte_address_buffer_offset_count; i++) {
      os << db.byte_address_buffer_offsets[i];
      if (i < (db.byte_address_buffer_offset_count - 1)) {
        os << ", ";
      }
    }
    os << "]\n";
  }

  if (verbosity_ >= 1) {
    //   SpvReflectTypeDescription*        type_description;
    if (db.type_description == nullptr) {
      os << t1 << "type_description:" << std::endl;
    } else {
      auto itor = type_description_to_index_.find(db.type_description);
      assert(itor != type_description_to_index_.end());
      os << t1 << "type_description: *td" << itor->second << std::endl;
    }
  }
  //   struct {
  //     uint32_t                        binding;
  //     uint32_t                        set;
  //   } word_offset;
  os << t1 << "word_offset: { binding: " << db.word_offset.binding;
  os << ", set: " << db.word_offset.set << " }" << std::endl;

  if (db.user_type != SPV_REFLECT_USER_TYPE_INVALID) {
    os << t1 << "user_type: " << ToStringUserType(db.user_type) << std::endl;
  }
  // } SpvReflectDescriptorBinding;
}

void SpvReflectToYaml::WriteInterfaceVariable(std::ostream& os, const SpvReflectInterfaceVariable& iv, uint32_t indent_level) {
  for (uint32_t i = 0; i < iv.member_count; ++i) {
    assert(interface_variable_to_index_.find(&iv.members[i]) == interface_variable_to_index_.end());
    WriteInterfaceVariable(os, iv.members[i], indent_level);
  }

  const std::string t0 = Indent(indent_level);
  const std::string t1 = Indent(indent_level + 1);
  const std::string t2 = Indent(indent_level + 2);
  const std::string t3 = Indent(indent_level + 3);

  uint32_t interface_variable_index = static_cast<uint32_t>(interface_variable_to_index_.size());
  interface_variable_to_index_[&iv] = interface_variable_index;

  // typedef struct SpvReflectInterfaceVariable {
  os << t0 << "- &iv" << interface_variable_index << std::endl;
  //   uint32_t                            spirv_id;
  os << t1 << "spirv_id: " << iv.spirv_id << std::endl;
  //   const char*                         name;
  os << t1 << "name: " << SafeString(iv.name) << std::endl;
  //   uint32_t                            location;
  os << t1 << "location: " << iv.location << std::endl;
  //   SpvStorageClass                     storage_class;
  os << t1 << "storage_class: " << iv.storage_class << " # " << ToStringSpvStorageClass(iv.storage_class) << std::endl;
  //   const char*                         semantic;
  os << t1 << "semantic: " << SafeString(iv.semantic) << std::endl;
  //   SpvReflectDecorationFlags           decoration_flags;
  os << t1 << "decoration_flags: " << AsHexString(iv.decoration_flags) << " # " << ToStringDecorationFlags(iv.decoration_flags)
     << std::endl;
  //   SpvBuiltIn                          built_in;
  os << t1 << "built_in: ";
  if (iv.decoration_flags & SPV_REFLECT_DECORATION_BLOCK) {
    for (uint32_t i = 0; i < iv.member_count; i++) {
      os << iv.members[i].built_in;
      if (i < (iv.member_count - 1)) {
        os << ", ";
      }
    }
  } else {
    os << iv.built_in;
  }
  os << " # " << ToStringSpvBuiltIn(iv, false) << std::endl;
  //   SpvReflectNumericTraits             numeric;
  // typedef struct SpvReflectNumericTraits {
  os << t1 << "numeric:" << std::endl;
  //   struct Scalar {
  //     uint32_t                        width;
  //     uint32_t                        signedness;
  //   } scalar;
  os << t2 << "scalar: { ";
  os << "width: " << iv.numeric.scalar.width << ", ";
  os << "signedness: " << iv.numeric.scalar.signedness << " }" << std::endl;
  //   struct Vector {
  //     uint32_t                        component_count;
  //   } vector;
  os << t2 << "vector: { ";
  os << "component_count: " << iv.numeric.vector.component_count << " }" << std::endl;
  //   struct Matrix {
  //     uint32_t                        column_count;
  //     uint32_t                        row_count;
  //     uint32_t                        stride; // Measured in bytes
  //   } matrix;
  os << t2 << "matrix: { ";
  os << "column_count: " << iv.numeric.matrix.column_count << ", ";
  os << "row_count: " << iv.numeric.matrix.row_count << ", ";
  os << "stride: " << iv.numeric.matrix.stride << " }" << std::endl;
  // } SpvReflectNumericTraits;

  //     SpvReflectArrayTraits           array;
  os << t1 << "array: { ";
  // typedef struct SpvReflectArrayTraits {
  //   uint32_t                          dims_count;
  os << "dims_count: " << iv.array.dims_count << ", ";
  //   uint32_t                          dims[SPV_REFLECT_MAX_ARRAY_DIMS];
  os << "dims: [";
  for (uint32_t i_dim = 0; i_dim < iv.array.dims_count; ++i_dim) {
    os << iv.array.dims[i_dim] << ",";
  }
  os << "], ";
  //   uint32_t                          stride; // Measured in bytes
  os << "stride: " << iv.array.stride;
  // } SpvReflectArrayTraits;
  os << " }" << std::endl;

  //   uint32_t                            member_count;
  os << t1 << "member_count: " << iv.member_count << std::endl;
  //   struct SpvReflectInterfaceVariable* members;
  os << t1 << "members:" << std::endl;
  for (uint32_t i = 0; i < iv.member_count; ++i) {
    auto itor = interface_variable_to_index_.find(&iv.members[i]);
    assert(itor != interface_variable_to_index_.end());
    os << t2 << "- *iv" << itor->second << " # " << SafeString(iv.members[i].name) << std::endl;
  }

  //   SpvReflectFormat                    format;
  os << t1 << "format: " << iv.format << " # " << ToStringFormat(iv.format) << std::endl;

  if (verbosity_ >= 1) {
    //   SpvReflectTypeDescription*        type_description;
    if (!iv.type_description) {
      os << t1 << "type_description:" << std::endl;
    } else {
      auto itor = type_description_to_index_.find(iv.type_description);
      assert(itor != type_description_to_index_.end());
      os << t1 << "type_description: *td" << itor->second << std::endl;
    }
  }

  //   struct {
  //     uint32_t                        location;
  //   } word_offset;
  os << t1 << "word_offset: { location: " << iv.word_offset.location << " }" << std::endl;

  // } SpvReflectInterfaceVariable;
}

void SpvReflectToYaml::WriteBlockVariableTypes(std::ostream& os, const SpvReflectBlockVariable& bv, uint32_t indent_level) {
  const auto* td = bv.type_description;
  if (td && type_description_to_index_.find(td) == type_description_to_index_.end()) {
    WriteTypeDescription(os, *td, indent_level);
  }

  if (bv.flags & SPV_REFLECT_VARIABLE_FLAGS_PHYSICAL_POINTER_COPY) {
    return;
  }
  for (uint32_t i = 0; i < bv.member_count; ++i) {
    WriteBlockVariableTypes(os, bv.members[i], indent_level);
  }
}
void SpvReflectToYaml::WriteDescriptorBindingTypes(std::ostream& os, const SpvReflectDescriptorBinding& db, uint32_t indent_level) {
  WriteBlockVariableTypes(os, db.block, indent_level);

  if (db.uav_counter_binding) {
    WriteDescriptorBindingTypes(os, *(db.uav_counter_binding), indent_level);
  }

  const auto* td = db.type_description;
  if (td && type_description_to_index_.find(td) == type_description_to_index_.end()) {
    WriteTypeDescription(os, *td, indent_level);
  }
}
void SpvReflectToYaml::WriteInterfaceVariableTypes(std::ostream& os, const SpvReflectInterfaceVariable& iv, uint32_t indent_level) {
  const auto* td = iv.type_description;
  if (td && type_description_to_index_.find(td) == type_description_to_index_.end()) {
    WriteTypeDescription(os, *td, indent_level);
  }

  for (uint32_t i = 0; i < iv.member_count; ++i) {
    WriteInterfaceVariableTypes(os, iv.members[i], indent_level);
  }
}

void SpvReflectToYaml::Write(std::ostream& os) {
  if (!sm_._internal) {
    return;
  }

  uint32_t indent_level = 0;
  const std::string t0 = Indent(indent_level);
  const std::string t1 = Indent(indent_level + 1);
  const std::string t2 = Indent(indent_level + 2);
  const std::string t3 = Indent(indent_level + 3);

  os << "%YAML 1.1" << std::endl;
  os << "---" << std::endl;

  type_description_to_index_.clear();
  if (verbosity_ >= 2) {
    os << t0 << "all_type_descriptions:" << std::endl;
    // Write the entire internal type_description table; all type descriptions
    // are reachable from there, though most of them are purely internal & not
    // referenced by any of the public-facing structures.
    for (size_t i = 0; i < sm_._internal->type_description_count; ++i) {
      WriteTypeDescription(os, sm_._internal->type_descriptions[i], indent_level + 1);
    }
  } else if (verbosity_ >= 1) {
    os << t0 << "all_type_descriptions:" << std::endl;
    // Iterate through all public-facing structures and write any type
    // descriptions we find (and their children).
    for (uint32_t i = 0; i < sm_.descriptor_binding_count; ++i) {
      WriteDescriptorBindingTypes(os, sm_.descriptor_bindings[i], indent_level + 1);
    }
    for (uint32_t i = 0; i < sm_.push_constant_block_count; ++i) {
      WriteBlockVariableTypes(os, sm_.push_constant_blocks[i], indent_level + 1);
    }
    for (uint32_t i = 0; i < sm_.input_variable_count; ++i) {
      WriteInterfaceVariableTypes(os, *sm_.input_variables[i], indent_level + 1);
    }
    for (uint32_t i = 0; i < sm_.output_variable_count; ++i) {
      WriteInterfaceVariableTypes(os, *sm_.output_variables[i], indent_level + 1);
    }
  }

  block_variable_to_index_.clear();
  os << t0 << "all_block_variables:" << std::endl;
  for (uint32_t i = 0; i < sm_.descriptor_binding_count; ++i) {
    WriteBlockVariable(os, sm_.descriptor_bindings[i].block, indent_level + 1);
  }
  for (uint32_t i = 0; i < sm_.push_constant_block_count; ++i) {
    WriteBlockVariable(os, sm_.push_constant_blocks[i], indent_level + 1);
  }

  descriptor_binding_to_index_.clear();
  os << t0 << "all_descriptor_bindings:" << std::endl;
  for (uint32_t i = 0; i < sm_.descriptor_binding_count; ++i) {
    WriteDescriptorBinding(os, sm_.descriptor_bindings[i], indent_level + 1);
  }

  interface_variable_to_index_.clear();
  os << t0 << "all_interface_variables:" << std::endl;
  for (uint32_t i = 0; i < sm_.input_variable_count; ++i) {
    WriteInterfaceVariable(os, *sm_.input_variables[i], indent_level + 1);
  }
  for (uint32_t i = 0; i < sm_.output_variable_count; ++i) {
    WriteInterfaceVariable(os, *sm_.output_variables[i], indent_level + 1);
  }

  // struct SpvReflectShaderModule {
  os << t0 << "module:" << std::endl;
  // uint16_t                          generator;
  os << t1 << "generator: " << sm_.generator << " # " << ToStringGenerator(sm_.generator) << std::endl;
  // const char*                       entry_point_name;
  os << t1 << "entry_point_name: " << SafeString(sm_.entry_point_name) << std::endl;
  // uint32_t                          entry_point_id;
  os << t1 << "entry_point_id: " << sm_.entry_point_id << std::endl;
  // SpvSourceLanguage                 source_language;
  os << t1 << "source_language: " << sm_.source_language << " # " << ToStringSpvSourceLanguage(sm_.source_language) << std::endl;
  // uint32_t                          source_language_version;
  os << t1 << "source_language_version: " << sm_.source_language_version << std::endl;
  // SpvExecutionModel                 spirv_execution_model;
  os << t1 << "spirv_execution_model: " << sm_.spirv_execution_model << " # "
     << ToStringSpvExecutionModel(sm_.spirv_execution_model) << std::endl;
  // SpvShaderStageFlagBits             shader_stage;
  os << t1 << "shader_stage: " << AsHexString(sm_.shader_stage) << " # " << ToStringShaderStage(sm_.shader_stage) << std::endl;
  // uint32_t                          descriptor_binding_count;
  os << t1 << "descriptor_binding_count: " << sm_.descriptor_binding_count << std::endl;
  // SpvReflectDescriptorBinding*      descriptor_bindings;
  os << t1 << "descriptor_bindings:" << std::endl;
  for (uint32_t i = 0; i < sm_.descriptor_binding_count; ++i) {
    auto itor = descriptor_binding_to_index_.find(&sm_.descriptor_bindings[i]);
    assert(itor != descriptor_binding_to_index_.end());
    os << t2 << "- *db" << itor->second << " # " << SafeString(sm_.descriptor_bindings[i].name) << std::endl;
  }
  // uint32_t                          descriptor_set_count;
  os << t1 << "descriptor_set_count: " << sm_.descriptor_set_count << std::endl;
  // SpvReflectDescriptorSet descriptor_sets[SPV_REFLECT_MAX_DESCRIPTOR_SETS];
  os << t1 << "descriptor_sets:" << std::endl;
  for (uint32_t i_set = 0; i_set < sm_.descriptor_set_count; ++i_set) {
    // typedef struct SpvReflectDescriptorSet {
    const auto& dset = sm_.descriptor_sets[i_set];
    //   uint32_t                          set;
    os << t1 << "- "
       << "set: " << dset.set << std::endl;
    //   uint32_t                          binding_count;
    os << t2 << "binding_count: " << dset.binding_count << std::endl;
    //   SpvReflectDescriptorBinding**     bindings;
    os << t2 << "bindings:" << std::endl;
    for (uint32_t i_binding = 0; i_binding < dset.binding_count; ++i_binding) {
      auto itor = descriptor_binding_to_index_.find(dset.bindings[i_binding]);
      assert(itor != descriptor_binding_to_index_.end());
      os << t3 << "- *db" << itor->second << " # " << SafeString(dset.bindings[i_binding]->name) << std::endl;
    }
    // } SpvReflectDescriptorSet;
  }
  // uint32_t                          input_variable_count;
  os << t1 << "input_variable_count: " << sm_.input_variable_count << ",\n";
  // SpvReflectInterfaceVariable*      input_variables;
  os << t1 << "input_variables:" << std::endl;
  for (uint32_t i = 0; i < sm_.input_variable_count; ++i) {
    auto itor = interface_variable_to_index_.find(sm_.input_variables[i]);
    assert(itor != interface_variable_to_index_.end());
    os << t2 << "- *iv" << itor->second << " # " << SafeString(sm_.input_variables[i]->name) << std::endl;
  }
  // uint32_t                          output_variable_count;
  os << t1 << "output_variable_count: " << sm_.output_variable_count << ",\n";
  // SpvReflectInterfaceVariable*      output_variables;
  os << t1 << "output_variables:" << std::endl;
  for (uint32_t i = 0; i < sm_.output_variable_count; ++i) {
    auto itor = interface_variable_to_index_.find(sm_.output_variables[i]);
    assert(itor != interface_variable_to_index_.end());
    os << t2 << "- *iv" << itor->second << " # " << SafeString(sm_.output_variables[i]->name) << std::endl;
  }
  // uint32_t                          push_constant_count;
  os << t1 << "push_constant_count: " << sm_.push_constant_block_count << ",\n";
  // SpvReflectBlockVariable*          push_constants;
  os << t1 << "push_constants:" << std::endl;
  for (uint32_t i = 0; i < sm_.push_constant_block_count; ++i) {
    auto itor = block_variable_to_index_.find(&sm_.push_constant_blocks[i]);
    assert(itor != block_variable_to_index_.end());
    os << t2 << "- *bv" << itor->second << " # " << SafeString(sm_.push_constant_blocks[i].name) << std::endl;
  }

  // uint32_t                            spec_constant_count;
  os << t1 << "specialization_constant_count: " << sm_.spec_constant_count << ",\n";
  // SpvReflectSpecializationConstant*   spec_constants;
  os << t1 << "specialization_constants:" << std::endl;
  for (uint32_t i = 0; i < sm_.spec_constant_count; ++i) {
    os << t3 << "- name: " << SafeString(sm_.spec_constants[i].name) << std::endl;
    os << t3 << "  spirv_id: " << sm_.spec_constants[i].spirv_id << std::endl;
    os << t3 << "  constant_id: " << sm_.spec_constants[i].constant_id << std::endl;
  }

  if (verbosity_ >= 2) {
    // struct Internal {
    os << t1 << "_internal:" << std::endl;
    if (sm_._internal) {
      //   size_t                          spirv_size;
      os << t2 << "spirv_size: " << sm_._internal->spirv_size << std::endl;
      //   uint32_t*                       spirv_code;
      os << t2 << "spirv_code: [";
      for (size_t i = 0; i < sm_._internal->spirv_word_count; ++i) {
        if ((i % 6) == 0) {
          os << std::endl << t3;
        }
        os << AsHexString(sm_._internal->spirv_code[i]) << ",";
      }
      os << "]" << std::endl;
      //   uint32_t                        spirv_word_count;
      os << t2 << "spirv_word_count: " << sm_._internal->spirv_word_count << std::endl;
      //   size_t                          type_description_count;
      os << t2 << "type_description_count: " << sm_._internal->type_description_count << std::endl;
      //   SpvReflectTypeDescription*      type_descriptions;
      os << t2 << "type_descriptions:" << std::endl;
      for (uint32_t i = 0; i < sm_._internal->type_description_count; ++i) {
        auto itor = type_description_to_index_.find(&sm_._internal->type_descriptions[i]);
        assert(itor != type_description_to_index_.end());
        os << t3 << "- *td" << itor->second << std::endl;
      }
    }
    // } * _internal;
  }

  os << "..." << std::endl;
}
