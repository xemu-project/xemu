#include <cstdint>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>

#include "../common/output_stream.h"
#include "gtest/gtest.h"
#include "spirv_reflect.h"

#if defined(_MSC_VER)
#include <direct.h>
#define posix_chdir(d) _chdir(d)
#else
#include <unistd.h>
#define posix_chdir(d) chdir(d)
#endif

#if defined(SPIRV_REFLECT_HAS_VULKAN_H)
// clang-format off
// Verify that SpvReflect enums match their Vk equivalents where appropriate
#include <vulkan/vulkan.h>
// SpvReflectFormat == VkFormat
static_assert((uint32_t)SPV_REFLECT_FORMAT_UNDEFINED           == (uint32_t)VK_FORMAT_UNDEFINED, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_FORMAT_R32_UINT            == (uint32_t)VK_FORMAT_R32_UINT, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_FORMAT_R32_SINT            == (uint32_t)VK_FORMAT_R32_SINT, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_FORMAT_R32_SFLOAT          == (uint32_t)VK_FORMAT_R32_SFLOAT, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_FORMAT_R32G32_UINT         == (uint32_t)VK_FORMAT_R32G32_UINT, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_FORMAT_R32G32_SINT         == (uint32_t)VK_FORMAT_R32G32_SINT, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_FORMAT_R32G32_SFLOAT       == (uint32_t)VK_FORMAT_R32G32_SFLOAT, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_FORMAT_R32G32B32_UINT      == (uint32_t)VK_FORMAT_R32G32B32_UINT, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_FORMAT_R32G32B32_SINT      == (uint32_t)VK_FORMAT_R32G32B32_SINT, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_FORMAT_R32G32B32_SFLOAT    == (uint32_t)VK_FORMAT_R32G32B32_SFLOAT, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_FORMAT_R32G32B32A32_UINT   == (uint32_t)VK_FORMAT_R32G32B32A32_UINT, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_FORMAT_R32G32B32A32_SINT   == (uint32_t)VK_FORMAT_R32G32B32A32_SINT, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT == (uint32_t)VK_FORMAT_R32G32B32A32_SFLOAT, "SpvReflect/Vk enum mismatch");
// SpvReflectDescriptorType == VkDescriptorType
static_assert((uint32_t)SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER                == (uint32_t)VK_DESCRIPTOR_TYPE_SAMPLER, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER == (uint32_t)VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE          == (uint32_t)VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE          == (uint32_t)VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER   == (uint32_t)VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER   == (uint32_t)VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER         == (uint32_t)VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER         == (uint32_t)VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC == (uint32_t)VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC == (uint32_t)VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT       == (uint32_t)VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, "SpvReflect/Vk enum mismatch");
// SpvReflectShaderStageFlagBits == VkShaderStageFlagBits
static_assert((uint32_t)SPV_REFLECT_SHADER_STAGE_VERTEX_BIT                  == (uint32_t)VK_SHADER_STAGE_VERTEX_BIT, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT    == (uint32_t)VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == (uint32_t)VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT                == (uint32_t)VK_SHADER_STAGE_GEOMETRY_BIT, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT                == (uint32_t)VK_SHADER_STAGE_FRAGMENT_BIT, "SpvReflect/Vk enum mismatch");
static_assert((uint32_t)SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT                 == (uint32_t)VK_SHADER_STAGE_COMPUTE_BIT, "SpvReflect/Vk enum mismatch");
#endif  // defined(SPIRV_REFLECT_HAS_VULKAN_H)
// clang-format on

TEST(SpirvReflectTestCase, SourceLanguage) {
  EXPECT_STREQ(spvReflectSourceLanguage(SpvSourceLanguageESSL), "ESSL");
  EXPECT_STREQ(spvReflectSourceLanguage(SpvSourceLanguageGLSL), "GLSL");
  EXPECT_STREQ(spvReflectSourceLanguage(SpvSourceLanguageOpenCL_C), "OpenCL_C");
  EXPECT_STREQ(spvReflectSourceLanguage(SpvSourceLanguageOpenCL_CPP),
               "OpenCL_CPP");
  EXPECT_STREQ(spvReflectSourceLanguage(SpvSourceLanguageHLSL), "HLSL");

  EXPECT_STREQ(spvReflectSourceLanguage(SpvSourceLanguageUnknown), "Unknown");
  // Invalid inputs
  EXPECT_STREQ(spvReflectSourceLanguage(SpvSourceLanguageMax), "Unknown");
  EXPECT_STREQ(spvReflectSourceLanguage(
                   static_cast<SpvSourceLanguage>(SpvSourceLanguageMax - 1)),
               "Unknown");
}

class SpirvReflectTest : public ::testing::TestWithParam<const char*> {
 public:
  // optional: initialize static data to be shared by all tests in this test
  // case. Note that for parameterized tests, the specific parameter value is a
  // non-static member data
  static void SetUpTestCase() {
    FILE* f = fopen("tests/glsl/built_in_format.spv", "r");
    if (!f) {
      posix_chdir("..");
      f = fopen("tests/glsl/built_in_format.spv", "r");
    }
    EXPECT_NE(f, nullptr) << "Couldn't find test shaders!";
    if (f) {
      fclose(f);
    }
  }
  static void TearDownTestCase() {}

 protected:
  SpirvReflectTest() {
    // set-up work for each test
  }

  ~SpirvReflectTest() override {
    // clean-up work that doesn't throw exceptions
  }

  // optional: called after constructor & before destructor, respectively.
  // Used if you have initialization steps that can throw exceptions or must
  // otherwise be deferred.
  void SetUp() override {
    // called after constructor before each test
    spirv_path_ = GetParam();
    std::ifstream spirv_file(spirv_path_, std::ios::binary | std::ios::ate);
    std::streampos spirv_file_nbytes = spirv_file.tellg();
    spirv_file.seekg(0);
    spirv_.resize(spirv_file_nbytes);
    spirv_file.read(reinterpret_cast<char*>(spirv_.data()), spirv_.size());

    SpvReflectResult result =
        spvReflectCreateShaderModule(spirv_.size(), spirv_.data(), &module_);
    ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS, result)
        << "spvReflectCreateShaderModule() failed";
  }

  // optional:
  void TearDown() override {
    // called before destructor after all tests
    spvReflectDestroyShaderModule(&module_);
  }

  // members here will be accessible in all tests in this test case
  std::string spirv_path_;
  std::vector<uint8_t> spirv_;
  SpvReflectShaderModule module_;

  // static members will be accessible to all tests in this test case
  static std::string test_shaders_dir;
};

TEST_P(SpirvReflectTest, GetCodeSize) {
  EXPECT_EQ(spvReflectGetCodeSize(&module_), spirv_.size());
}
TEST(SpirvReflectTestCase, GetCodeSize_Errors) {
  // NULL module
  EXPECT_EQ(spvReflectGetCodeSize(nullptr), 0);
}

TEST_P(SpirvReflectTest, GetCode) {
  int code_compare =
      memcmp(spvReflectGetCode(&module_), spirv_.data(), spirv_.size());
  EXPECT_EQ(code_compare, 0);
}
TEST(SpirvReflectTestCase, GetCode_Errors) {
  // NULL module
  EXPECT_EQ(spvReflectGetCode(nullptr), nullptr);
}

TEST_P(SpirvReflectTest, GetDescriptorBinding) {
  uint32_t binding_count = 0;
  SpvReflectResult result;
  result =
      spvReflectEnumerateDescriptorBindings(&module_, &binding_count, nullptr);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  std::vector<SpvReflectDescriptorBinding*> bindings(binding_count);
  result = spvReflectEnumerateDescriptorBindings(&module_, &binding_count,
                                                 bindings.data());
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);

  for (const auto* db : bindings) {
    const SpvReflectDescriptorBinding* also_db =
        spvReflectGetDescriptorBinding(&module_, db->binding, db->set, &result);
    EXPECT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
    EXPECT_EQ(db, also_db);
  }
}
TEST_P(SpirvReflectTest, EnumerateDescriptorBindings_Errors) {
  uint32_t binding_count = 0;
  // NULL module
  EXPECT_EQ(
      spvReflectEnumerateDescriptorBindings(nullptr, &binding_count, nullptr),
      SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // NULL binding count
  EXPECT_EQ(spvReflectEnumerateDescriptorBindings(&module_, nullptr, nullptr),
            SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // binding count / module binding count mismatch
  EXPECT_EQ(
      spvReflectEnumerateDescriptorBindings(&module_, &binding_count, nullptr),
      SPV_REFLECT_RESULT_SUCCESS);
  uint32_t bad_binding_count = binding_count + 1;
  std::vector<SpvReflectDescriptorBinding*> bindings(bad_binding_count);
  EXPECT_EQ(spvReflectEnumerateDescriptorBindings(&module_, &bad_binding_count,
                                                  bindings.data()),
            SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH);
}
TEST_P(SpirvReflectTest, GetDescriptorBinding_Errors) {
  SpvReflectResult result;
  // NULL module
  EXPECT_EQ(spvReflectGetDescriptorBinding(nullptr, 0, 0, nullptr), nullptr);
  EXPECT_EQ(spvReflectGetDescriptorBinding(nullptr, 0, 0, &result), nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // Invalid binding number
  EXPECT_EQ(spvReflectGetDescriptorBinding(&module_, 0xdeadbeef, 0, nullptr),
            nullptr);
  EXPECT_EQ(spvReflectGetDescriptorBinding(&module_, 0xdeadbeef, 0, &result),
            nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
  // Invalid set number
  EXPECT_EQ(spvReflectGetDescriptorBinding(&module_, 0, 0xdeadbeef, nullptr),
            nullptr);
  EXPECT_EQ(spvReflectGetDescriptorBinding(&module_, 0, 0xdeadbeef, &result),
            nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
}

TEST_P(SpirvReflectTest, GetDescriptorSet) {
  uint32_t set_count = 0;
  SpvReflectResult result;
  result = spvReflectEnumerateDescriptorSets(&module_, &set_count, nullptr);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  std::vector<SpvReflectDescriptorSet*> sets(set_count);
  result = spvReflectEnumerateDescriptorSets(&module_, &set_count, sets.data());
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);

  for (const auto* ds : sets) {
    const SpvReflectDescriptorSet* also_ds =
        spvReflectGetDescriptorSet(&module_, ds->set, &result);
    EXPECT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
    EXPECT_EQ(ds, also_ds);
  }
}
TEST_P(SpirvReflectTest, EnumerateDescriptorSets_Errors) {
  uint32_t set_count = 0;
  // NULL module
  EXPECT_EQ(spvReflectEnumerateDescriptorSets(nullptr, &set_count, nullptr),
            SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // NULL set count
  EXPECT_EQ(spvReflectEnumerateDescriptorSets(&module_, nullptr, nullptr),
            SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // set count / module set count mismatch
  EXPECT_EQ(spvReflectEnumerateDescriptorSets(&module_, &set_count, nullptr),
            SPV_REFLECT_RESULT_SUCCESS);
  uint32_t bad_set_count = set_count + 1;
  std::vector<SpvReflectDescriptorSet*> sets(bad_set_count);
  EXPECT_EQ(
      spvReflectEnumerateDescriptorSets(&module_, &bad_set_count, sets.data()),
      SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH);
}
TEST_P(SpirvReflectTest, GetDescriptorSet_Errors) {
  SpvReflectResult result;
  // NULL module
  EXPECT_EQ(spvReflectGetDescriptorSet(nullptr, 0, nullptr), nullptr);
  EXPECT_EQ(spvReflectGetDescriptorSet(nullptr, 0, &result), nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // Invalid set number
  EXPECT_EQ(spvReflectGetDescriptorSet(&module_, 0xdeadbeef, nullptr), nullptr);
  EXPECT_EQ(spvReflectGetDescriptorSet(&module_, 0xdeadbeef, &result), nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
}

TEST_P(SpirvReflectTest, GetInputVariableByLocation) {
  uint32_t iv_count = 0;
  SpvReflectResult result;
  result = spvReflectEnumerateInputVariables(&module_, &iv_count, nullptr);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  std::vector<SpvReflectInterfaceVariable*> ivars(iv_count);
  result = spvReflectEnumerateInputVariables(&module_, &iv_count, ivars.data());
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);

  for (const auto* iv : ivars) {
    const SpvReflectInterfaceVariable* also_iv =
        spvReflectGetInputVariableByLocation(&module_, iv->location, &result);
    if (iv->location == UINT32_MAX) {
      // Not all elements have valid locations.
      EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
      EXPECT_EQ(also_iv, nullptr);
    } else {
      EXPECT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
      EXPECT_EQ(iv, also_iv);
    }
  }
}
TEST_P(SpirvReflectTest, EnumerateInputVariables_Errors) {
  uint32_t var_count = 0;
  // NULL module
  EXPECT_EQ(spvReflectEnumerateInputVariables(nullptr, &var_count, nullptr),
            SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // NULL var count
  EXPECT_EQ(spvReflectEnumerateInputVariables(&module_, nullptr, nullptr),
            SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // var count / module var count mismatch
  EXPECT_EQ(spvReflectEnumerateInputVariables(&module_, &var_count, nullptr),
            SPV_REFLECT_RESULT_SUCCESS);
  uint32_t bad_var_count = var_count + 1;
  std::vector<SpvReflectInterfaceVariable*> vars(bad_var_count);
  EXPECT_EQ(
      spvReflectEnumerateInputVariables(&module_, &bad_var_count, vars.data()),
      SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH);
}
TEST_P(SpirvReflectTest, GetInputVariableByLocation_Errors) {
  SpvReflectResult result;
  // NULL module
  EXPECT_EQ(spvReflectGetInputVariableByLocation(nullptr, 0, nullptr), nullptr);
  EXPECT_EQ(spvReflectGetInputVariableByLocation(nullptr, 0, &result), nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // explicitly invalid location (UINT32_MAX is *always* not found)
  EXPECT_EQ(spvReflectGetInputVariableByLocation(&module_, UINT32_MAX, nullptr),
            nullptr);
  EXPECT_EQ(spvReflectGetInputVariableByLocation(&module_, UINT32_MAX, &result),
            nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
  // implicitly invalid location (0xdeadbeef is potentially valid)
  EXPECT_EQ(spvReflectGetInputVariableByLocation(&module_, 0xdeadbeef, nullptr),
            nullptr);
  EXPECT_EQ(spvReflectGetInputVariableByLocation(&module_, 0xdeadbeef, &result),
            nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
}

TEST_P(SpirvReflectTest, GetInputVariableBySemantic) {
  uint32_t iv_count = 0;
  SpvReflectResult result;
  result = spvReflectEnumerateInputVariables(&module_, &iv_count, nullptr);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  std::vector<SpvReflectInterfaceVariable*> ivars(iv_count);
  result = spvReflectEnumerateInputVariables(&module_, &iv_count, ivars.data());
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);

  for (const auto* iv : ivars) {
    const SpvReflectInterfaceVariable* also_iv =
        spvReflectGetInputVariableBySemantic(&module_, iv->semantic, &result);
    if (iv->semantic == nullptr) {
      // Not all elements have valid semantics
      EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
      EXPECT_EQ(also_iv, nullptr);
    } else if (iv->semantic[0] == '\0') {
      // Not all elements have valid semantics
      EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
      EXPECT_EQ(also_iv, nullptr);
    } else {
      EXPECT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
      EXPECT_EQ(iv, also_iv);
    }
  }
}
TEST_P(SpirvReflectTest, GetInputVariableBySemantic_Errors) {
  SpvReflectResult result;
  // NULL module
  EXPECT_EQ(spvReflectGetInputVariableBySemantic(nullptr, nullptr, nullptr),
            nullptr);
  EXPECT_EQ(spvReflectGetInputVariableBySemantic(nullptr, nullptr, &result),
            nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // NULL semantic
  EXPECT_EQ(spvReflectGetInputVariableBySemantic(&module_, nullptr, nullptr),
            nullptr);
  EXPECT_EQ(spvReflectGetInputVariableBySemantic(&module_, nullptr, &result),
            nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // empty semantic ("" is explicitly not found)
  EXPECT_EQ(spvReflectGetInputVariableBySemantic(&module_, "", nullptr),
            nullptr);
  EXPECT_EQ(spvReflectGetInputVariableBySemantic(&module_, "", &result),
            nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
  // implicitly invalid semantic
  EXPECT_EQ(spvReflectGetInputVariableBySemantic(
                &module_, "SV_PLAUSIBLE_BUT_INVALID", nullptr),
            nullptr);
  EXPECT_EQ(spvReflectGetInputVariableBySemantic(
                &module_, "SV_PLAUSIBLE_BUT_INVALID", &result),
            nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
}

TEST_P(SpirvReflectTest, GetOutputVariableByLocation) {
  uint32_t ov_count = 0;
  SpvReflectResult result;
  result = spvReflectEnumerateOutputVariables(&module_, &ov_count, nullptr);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  std::vector<SpvReflectInterfaceVariable*> ovars(ov_count);
  result =
      spvReflectEnumerateOutputVariables(&module_, &ov_count, ovars.data());
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);

  for (const auto* ov : ovars) {
    const SpvReflectInterfaceVariable* also_ov =
        spvReflectGetOutputVariableByLocation(&module_, ov->location, &result);
    if (ov->location == UINT32_MAX) {
      // Not all elements have valid locations.
      EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
      EXPECT_EQ(also_ov, nullptr);
    } else {
      EXPECT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
      EXPECT_EQ(ov, also_ov);
    }
  }
}
TEST_P(SpirvReflectTest, EnumerateOutputVariables_Errors) {
  uint32_t var_count = 0;
  // NULL module
  EXPECT_EQ(spvReflectEnumerateOutputVariables(nullptr, &var_count, nullptr),
            SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // NULL var count
  EXPECT_EQ(spvReflectEnumerateOutputVariables(&module_, nullptr, nullptr),
            SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // var count / module var count mismatch
  EXPECT_EQ(spvReflectEnumerateOutputVariables(&module_, &var_count, nullptr),
            SPV_REFLECT_RESULT_SUCCESS);
  uint32_t bad_var_count = var_count + 1;
  std::vector<SpvReflectInterfaceVariable*> vars(bad_var_count);
  EXPECT_EQ(
      spvReflectEnumerateOutputVariables(&module_, &bad_var_count, vars.data()),
      SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH);
}
TEST_P(SpirvReflectTest, GetOutputVariableByLocation_Errors) {
  SpvReflectResult result;
  // NULL module
  EXPECT_EQ(spvReflectGetOutputVariableByLocation(nullptr, 0, nullptr),
            nullptr);
  EXPECT_EQ(spvReflectGetOutputVariableByLocation(nullptr, 0, &result),
            nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // explicitly invalid location (UINT32_MAX is *always* not found)
  EXPECT_EQ(
      spvReflectGetOutputVariableByLocation(&module_, UINT32_MAX, nullptr),
      nullptr);
  EXPECT_EQ(
      spvReflectGetOutputVariableByLocation(&module_, UINT32_MAX, &result),
      nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
  // implicitly invalid location (0xdeadbeef is potentially valid)
  EXPECT_EQ(
      spvReflectGetOutputVariableByLocation(&module_, 0xdeadbeef, nullptr),
      nullptr);
  EXPECT_EQ(
      spvReflectGetOutputVariableByLocation(&module_, 0xdeadbeef, &result),
      nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
}

TEST_P(SpirvReflectTest, GetOutputVariableBySemantic) {
  uint32_t ov_count = 0;
  SpvReflectResult result;
  result = spvReflectEnumerateOutputVariables(&module_, &ov_count, nullptr);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  std::vector<SpvReflectInterfaceVariable*> ovars(ov_count);
  result =
      spvReflectEnumerateOutputVariables(&module_, &ov_count, ovars.data());
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);

  for (const auto* ov : ovars) {
    const SpvReflectInterfaceVariable* also_ov =
        spvReflectGetOutputVariableBySemantic(&module_, ov->semantic, &result);
    if (ov->semantic == nullptr) {
      // Not all elements have valid semantics
      EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
      EXPECT_EQ(also_ov, nullptr);
    } else if (ov->semantic[0] == '\0') {
      // Not all elements have valid semantics
      EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
      EXPECT_EQ(also_ov, nullptr);
    } else {
      EXPECT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
      EXPECT_EQ(ov, also_ov);
    }
  }
}
TEST_P(SpirvReflectTest, GetOutputVariableBySemantic_Errors) {
  SpvReflectResult result;
  // NULL module
  EXPECT_EQ(spvReflectGetOutputVariableBySemantic(nullptr, nullptr, nullptr),
            nullptr);
  EXPECT_EQ(spvReflectGetOutputVariableBySemantic(nullptr, nullptr, &result),
            nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // NULL semantic
  EXPECT_EQ(spvReflectGetOutputVariableBySemantic(&module_, nullptr, nullptr),
            nullptr);
  EXPECT_EQ(spvReflectGetOutputVariableBySemantic(&module_, nullptr, &result),
            nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // empty semantic ("" is explicitly not found)
  EXPECT_EQ(spvReflectGetOutputVariableBySemantic(&module_, "", nullptr),
            nullptr);
  EXPECT_EQ(spvReflectGetOutputVariableBySemantic(&module_, "", &result),
            nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
  // implicitly invalid semantic
  EXPECT_EQ(spvReflectGetOutputVariableBySemantic(
                &module_, "SV_PLAUSIBLE_BUT_INVALID", nullptr),
            nullptr);
  EXPECT_EQ(spvReflectGetOutputVariableBySemantic(
                &module_, "SV_PLAUSIBLE_BUT_INVALID", &result),
            nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
}

TEST_P(SpirvReflectTest, GetPushConstantBlock) {
  uint32_t block_count = 0;
  SpvReflectResult result;
  result =
      spvReflectEnumeratePushConstantBlocks(&module_, &block_count, nullptr);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  std::vector<SpvReflectBlockVariable*> blocks(block_count);
  result = spvReflectEnumeratePushConstantBlocks(&module_, &block_count,
                                                 blocks.data());
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);

  for (uint32_t i = 0; i < block_count; ++i) {
    const SpvReflectBlockVariable* b = blocks[i];
    const SpvReflectBlockVariable* also_b =
        spvReflectGetPushConstantBlock(&module_, i, &result);
    EXPECT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
    EXPECT_EQ(b, also_b);
  }
}
TEST_P(SpirvReflectTest, EnumeratePushConstantBlocks_Errors) {
  uint32_t block_count = 0;
  // NULL module
  EXPECT_EQ(
      spvReflectEnumeratePushConstantBlocks(nullptr, &block_count, nullptr),
      SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // NULL var count
  EXPECT_EQ(spvReflectEnumeratePushConstantBlocks(&module_, nullptr, nullptr),
            SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // var count / module var count mismatch
  EXPECT_EQ(
      spvReflectEnumeratePushConstantBlocks(&module_, &block_count, nullptr),
      SPV_REFLECT_RESULT_SUCCESS);
  uint32_t bad_block_count = block_count + 1;
  std::vector<SpvReflectBlockVariable*> blocks(bad_block_count);
  EXPECT_EQ(spvReflectEnumeratePushConstantBlocks(&module_, &bad_block_count,
                                                  blocks.data()),
            SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH);
}
TEST_P(SpirvReflectTest, GetPushConstantBlock_Errors) {
  uint32_t block_count = 0;
  SpvReflectResult result;
  result =
      spvReflectEnumeratePushConstantBlocks(&module_, &block_count, nullptr);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  // NULL module
  EXPECT_EQ(spvReflectGetPushConstantBlock(nullptr, 0, nullptr), nullptr);
  EXPECT_EQ(spvReflectGetPushConstantBlock(nullptr, 0, &result), nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // invalid block index
  EXPECT_EQ(spvReflectGetPushConstantBlock(&module_, block_count, nullptr),
            nullptr);
  EXPECT_EQ(spvReflectGetPushConstantBlock(&module_, block_count, &result),
            nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
}

TEST_P(SpirvReflectTest, ChangeDescriptorBindingNumber) {
  uint32_t binding_count = 0;
  SpvReflectResult result;
  result =
      spvReflectEnumerateDescriptorBindings(&module_, &binding_count, nullptr);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  if (binding_count == 0) {
    return;  // can't change binding numbers if there are no bindings!
  }
  std::vector<SpvReflectDescriptorBinding*> bindings(binding_count);
  result = spvReflectEnumerateDescriptorBindings(&module_, &binding_count,
                                                 bindings.data());
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);

  uint32_t set_count = 0;
  result = spvReflectEnumerateDescriptorSets(&module_, &set_count, nullptr);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  EXPECT_GT(set_count, 0U);
  std::vector<SpvReflectDescriptorSet*> sets(set_count);
  result = spvReflectEnumerateDescriptorSets(&module_, &set_count, sets.data());
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);

  SpvReflectDescriptorBinding* b = bindings[0];
  const uint32_t new_binding_number = 1000;
  const uint32_t set_number = b->set;
  // Make sure no binding exists at the binding number we're about to change to.
  ASSERT_EQ(spvReflectGetDescriptorBinding(&module_, new_binding_number,
                                           set_number, &result),
            nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
  // Modify the binding number (leaving the set number unchanged)
  result = spvReflectChangeDescriptorBindingNumbers(
      &module_, b, new_binding_number, SPV_REFLECT_SET_NUMBER_DONT_CHANGE);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  // We should now be able to retrieve the binding at the new number
  const SpvReflectDescriptorBinding* new_binding =
      spvReflectGetDescriptorBinding(&module_, new_binding_number, set_number,
                                     &result);
  ASSERT_NE(new_binding, nullptr);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  EXPECT_EQ(new_binding->binding, new_binding_number);
  EXPECT_EQ(new_binding->set, set_number);
  // The set count & sets contents should not have changed, since we didn't
  // change the set number.
  result = spvReflectEnumerateDescriptorSets(&module_, &set_count, nullptr);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  EXPECT_EQ(set_count, sets.size());
  std::vector<SpvReflectDescriptorSet*> new_sets(set_count);
  result =
      spvReflectEnumerateDescriptorSets(&module_, &set_count, new_sets.data());
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  EXPECT_EQ(sets, new_sets);

  // TODO: confirm that the modified SPIR-V code is still valid, either by
  // running spirv-val or calling vkCreateShaderModule().
}
TEST_P(SpirvReflectTest, ChangeDescriptorBindingNumbers_Errors) {
  // NULL module
  EXPECT_EQ(spvReflectChangeDescriptorBindingNumbers(nullptr, nullptr, 0, 0),
            SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // NULL descriptor binding
  EXPECT_EQ(spvReflectChangeDescriptorBindingNumbers(&module_, nullptr, 0, 0),
            SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
}

TEST_P(SpirvReflectTest, ChangeDescriptorSetNumber) {
  uint32_t binding_count = 0;
  SpvReflectResult result;
  result =
      spvReflectEnumerateDescriptorBindings(&module_, &binding_count, nullptr);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  if (binding_count == 0) {
    return;  // can't change set numbers if there are no bindings!
  }
  std::vector<SpvReflectDescriptorBinding*> bindings(binding_count);
  result = spvReflectEnumerateDescriptorBindings(&module_, &binding_count,
                                                 bindings.data());
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);

  uint32_t set_count = 0;
  result = spvReflectEnumerateDescriptorSets(&module_, &set_count, nullptr);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  EXPECT_GT(set_count, 0U);
  std::vector<SpvReflectDescriptorSet*> sets(set_count);
  result = spvReflectEnumerateDescriptorSets(&module_, &set_count, sets.data());
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);

  SpvReflectDescriptorSet* s = sets[0];

  const uint32_t new_set_number = 13;
  const uint32_t set_binding_count = s->binding_count;
  // Make sure no set exists at the binding number we're about to change to.
  ASSERT_EQ(spvReflectGetDescriptorSet(&module_, new_set_number, &result),
            nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
  // Modify the set number
  result = spvReflectChangeDescriptorSetNumber(&module_, s, new_set_number);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  // We should now be able to retrieve the set at the new number
  const SpvReflectDescriptorSet* new_set =
      spvReflectGetDescriptorSet(&module_, new_set_number, &result);
  ASSERT_NE(new_set, nullptr);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  EXPECT_EQ(new_set->set, new_set_number);
  EXPECT_EQ(new_set->binding_count, set_binding_count);
  // The set count should not have changed, since we didn't
  // change the set number.
  result = spvReflectEnumerateDescriptorSets(&module_, &set_count, nullptr);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  EXPECT_EQ(set_count, sets.size());

  // TODO: confirm that the modified SPIR-V code is still valid, either by
  // running spirv-val or calling vkCreateShaderModule().
}
TEST_P(SpirvReflectTest, ChangeDescriptorSetNumber_Errors) {
  // NULL module
  EXPECT_EQ(spvReflectChangeDescriptorSetNumber(nullptr, nullptr, 0),
            SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // NULL descriptor set
  EXPECT_EQ(spvReflectChangeDescriptorSetNumber(&module_, nullptr, 0),
            SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
}

TEST_P(SpirvReflectTest, ChangeInputVariableLocation) {
  uint32_t iv_count = 0;
  SpvReflectResult result;
  result = spvReflectEnumerateInputVariables(&module_, &iv_count, nullptr);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  if (iv_count == 0) {
    return;  // can't change variable locations if there are no variables!
  }
  std::vector<SpvReflectInterfaceVariable*> ivars(iv_count);
  result = spvReflectEnumerateInputVariables(&module_, &iv_count, ivars.data());
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);

  SpvReflectInterfaceVariable* iv = ivars[0];

  const uint32_t new_location = 37;
  // Make sure no var exists at the location we're about to change to.
  ASSERT_EQ(
      spvReflectGetInputVariableByLocation(&module_, new_location, &result),
      nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
  // Modify the location
  result = spvReflectChangeInputVariableLocation(&module_, iv, new_location);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  // We should now be able to retrieve the variable at its new location
  const SpvReflectInterfaceVariable* new_iv =
      spvReflectGetInputVariableByLocation(&module_, new_location, &result);
  ASSERT_NE(new_iv, nullptr);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  EXPECT_EQ(new_iv->location, new_location);
  // The variable count should not have changed
  result = spvReflectEnumerateInputVariables(&module_, &iv_count, nullptr);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  EXPECT_EQ(iv_count, ivars.size());

  // TODO: confirm that the modified SPIR-V code is still valid, either by
  // running spirv-val or calling vkCreateShaderModule().
}
TEST_P(SpirvReflectTest, ChangeInputVariableLocation_Errors) {
  // NULL module
  EXPECT_EQ(spvReflectChangeInputVariableLocation(nullptr, nullptr, 0),
            SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // NULL variable
  EXPECT_EQ(spvReflectChangeInputVariableLocation(&module_, nullptr, 0),
            SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
}

TEST_P(SpirvReflectTest, ChangeOutputVariableLocation) {
  uint32_t ov_count = 0;
  SpvReflectResult result;
  result = spvReflectEnumerateOutputVariables(&module_, &ov_count, nullptr);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  if (ov_count == 0) {
    return;  // can't change variable locations if there are no variables!
  }
  std::vector<SpvReflectInterfaceVariable*> ovars(ov_count);
  result =
      spvReflectEnumerateOutputVariables(&module_, &ov_count, ovars.data());
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);

  SpvReflectInterfaceVariable* ov = ovars[0];

  const uint32_t new_location = 37;
  // Make sure no var exists at the location we're about to change to.
  ASSERT_EQ(
      spvReflectGetOutputVariableByLocation(&module_, new_location, &result),
      nullptr);
  EXPECT_EQ(result, SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
  // Modify the location
  result = spvReflectChangeOutputVariableLocation(&module_, ov, new_location);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  // We should now be able to retrieve the variable at its new location
  const SpvReflectInterfaceVariable* new_ov =
      spvReflectGetOutputVariableByLocation(&module_, new_location, &result);
  ASSERT_NE(new_ov, nullptr);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  EXPECT_EQ(new_ov->location, new_location);
  // The variable count should not have changed
  result = spvReflectEnumerateOutputVariables(&module_, &ov_count, nullptr);
  ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
  EXPECT_EQ(ov_count, ovars.size());

  // TODO: confirm that the modified SPIR-V code is still valid, either by
  // running spirv-val or calling vkCreateShaderModule().
}
TEST_P(SpirvReflectTest, ChangeOutputVariableLocation_Errors) {
  // NULL module
  EXPECT_EQ(spvReflectChangeOutputVariableLocation(nullptr, nullptr, 0),
            SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
  // NULL variable
  EXPECT_EQ(spvReflectChangeOutputVariableLocation(&module_, nullptr, 0),
            SPV_REFLECT_RESULT_ERROR_NULL_POINTER);
}

TEST_P(SpirvReflectTest, CheckYamlOutput) {
  const uint32_t yaml_verbosity = 1;
  SpvReflectToYaml yamlizer(module_, yaml_verbosity);
  std::stringstream test_yaml;
  test_yaml << yamlizer;
  std::string test_yaml_str = test_yaml.str();

  std::string golden_yaml_path = spirv_path_ + ".yaml";
  std::ifstream golden_yaml_file(golden_yaml_path);
  ASSERT_TRUE(golden_yaml_file.good());
  std::stringstream golden_yaml;
  golden_yaml << golden_yaml_file.rdbuf();
  golden_yaml_file.close();
  std::string golden_yaml_str = golden_yaml.str();

  // Quick workaround for potential line ending differences, perf be damned!
  test_yaml_str = std::regex_replace(test_yaml_str, std::regex("\r"), "");
  golden_yaml_str = std::regex_replace(golden_yaml_str, std::regex("\r"), "");

  ASSERT_EQ(test_yaml_str.size(), golden_yaml_str.size());
  // TODO: I wish there were a better way to show what changed, but the
  // differences (if any) tend to be pretty large, even for small changes.
  bool yaml_contents_match = (test_yaml_str == golden_yaml_str);
  EXPECT_TRUE(yaml_contents_match)
      << "YAML output mismatch; try regenerating the golden YAML with "
         "\"tests/build_golden_yaml.py\" and see what changed.";
}

namespace {
// TODO - have this glob search all .spv files
const std::vector<const char*> all_spirv_paths = {
    // clang-format off
    "../tests/16bit/vert_in_out_16.spv",
    "../tests/access_chains/array_length_from_access_chain.spv",
    "../tests/cbuffer_unused/cbuffer_unused_001.spv",
    "../tests/glsl/built_in_format.spv",
    "../tests/entry_exec_mode/comp_local_size.spv",
    "../tests/entry_exec_mode/geom_inv_out_vert.spv",
    "../tests/execution_mode/local_size_id_spec.spv",
    "../tests/execution_mode/local_size_id.spv",
    "../tests/glsl/buffer_handle_0.spv",
    "../tests/glsl/buffer_handle_1.spv",
    "../tests/glsl/buffer_handle_2.spv",
    "../tests/glsl/buffer_handle_3.spv",
    "../tests/glsl/buffer_handle_4.spv",
    "../tests/glsl/buffer_handle_5.spv",
    "../tests/glsl/buffer_handle_6.spv",
    "../tests/glsl/buffer_handle_7.spv",
    "../tests/glsl/buffer_handle_8.spv",
    "../tests/glsl/buffer_handle_9.spv",
    "../tests/glsl/buffer_handle_uvec2_pc.spv",
    "../tests/glsl/buffer_handle_uvec2_ssbo.spv",
    "../tests/glsl/buffer_pointer.spv",
    "../tests/glsl/built_in_format.spv",
    "../tests/glsl/fn_struct_param.spv",
    "../tests/glsl/frag_array_input.spv",
    "../tests/glsl/frag_barycentric.spv",
    "../tests/glsl/input_attachment.spv",
    "../tests/glsl/io_vars_vs.spv",
    "../tests/glsl/matrix_major_order_glsl.spv",
    "../tests/glsl/non_writable_image.spv",
    "../tests/glsl/readonly_writeonly.spv",
    "../tests/glsl/runtime_array_of_array_of_struct.spv",
    "../tests/glsl/storage_buffer.spv",
    "../tests/glsl/texel_buffer.spv",
    "../tests/hlsl/append_consume.spv",
    "../tests/hlsl/array_of_structured_buffer.spv",
    "../tests/hlsl/binding_array.spv",
    "../tests/hlsl/binding_types.spv",
    "../tests/hlsl/cbuffer.spv",
    "../tests/hlsl/constantbuffer.spv",
    "../tests/hlsl/constantbuffer_nested_structs.spv",
    "../tests/hlsl/counter_buffers.spv",
    "../tests/hlsl/localsize.spv",
    "../tests/hlsl/matrix_major_order_hlsl.spv",
    "../tests/hlsl/pushconstant.spv",
    "../tests/hlsl/semantics.spv",
    "../tests/hlsl/structuredbuffer.spv",
    "../tests/hlsl/user_type.spv",
    "../tests/interface/geom_input_builtin_array.spv",
    "../tests/interface/vertex_input_builtin_block.spv",
    "../tests/interface/vertex_input_builtin_non_block.spv",
    "../tests/issues/77/hlsl/array_from_ubo.spv",
    "../tests/issues/77/hlsl/array_from_ubo_with_O0.spv",
    "../tests/issues/77/hlsl/rocketz.spv",
    "../tests/issues/102/function_parameter_access.spv",
    "../tests/issues/178/vertex_input_struct.spv",
    "../tests/issues/178/vertex_input_struct2.spv",
    "../tests/issues/227/null_node.spv",
    "../tests/mesh_shader_ext/mesh_shader_ext.task.hlsl.spv",
    "../tests/mesh_shader_ext/mesh_shader_ext.mesh.hlsl.spv",
    "../tests/multi_entrypoint/multi_entrypoint.spv",
    "../tests/push_constants/non_zero_block_offset.spv",
    "../tests/raytrace/rayquery_equal.cs.spv",
    "../tests/raytrace/rayquery_init_ds.spv",
    "../tests/raytrace/rayquery_init_gs.spv",
    "../tests/raytrace/rayquery_init_hs.spv",
    "../tests/raytrace/rayquery_init_ps.spv",
    "../tests/raytrace/rayquery_init_rahit.spv",
    "../tests/raytrace/rayquery_init_rcall.spv",
    "../tests/raytrace/rayquery_init_rchit.spv",
    "../tests/raytrace/rayquery_init_rgen.spv",
    "../tests/raytrace/rayquery_init_rmiss.spv",
    "../tests/raytrace/raytracing.acceleration-structure.spv",
    "../tests/raytrace/raytracing.khr.closesthit.spv",
    "../tests/raytrace/raytracing.nv.acceleration-structure.spv",
    "../tests/raytrace/raytracing.nv.anyhit.spv",
    "../tests/raytrace/raytracing.nv.callable.spv",
    "../tests/raytrace/raytracing.nv.closesthit.spv",
    "../tests/raytrace/raytracing.nv.enum.spv",
    "../tests/raytrace/raytracing.nv.intersection.spv",
    "../tests/raytrace/raytracing.nv.library.spv",
    "../tests/raytrace/raytracing.nv.miss.spv",
    "../tests/raytrace/raytracing.nv.raygen.spv",
    "../tests/spec_constants/basic.spv",
    "../tests/spec_constants/convert.spv",
    "../tests/spec_constants/local_size_id_10.spv",
    "../tests/spec_constants/local_size_id_13.spv",
    "../tests/spec_constants/ssbo_array.spv",
    "../tests/spec_constants/test_32bit.spv",
    "../tests/spec_constants/test_64bit.spv",
    "../tests/spirv15/VertexShader.spv",
    "../tests/user_type/byte_address_buffer_0.spv",
    "../tests/user_type/byte_address_buffer_1.spv",
    "../tests/user_type/byte_address_buffer_2.spv",
    "../tests/user_type/byte_address_buffer_3.spv",
    "../tests/user_type/rw_byte_address_buffer.spv",
    "../tests/variable_access/copy_memory.spv",
    // clang-format on
};
}  // namespace
INSTANTIATE_TEST_CASE_P(ForAllShaders, SpirvReflectTest,
                        ::testing::ValuesIn(all_spirv_paths));

TEST(SpirvReflectTestCase, TestComputeLocalSize) {
  std::vector<uint8_t> spirv_;
  SpvReflectShaderModule module_;
  const std::string spirv_path = "../tests/entry_exec_mode/comp_local_size.spv";
  std::ifstream spirv_file(spirv_path, std::ios::binary | std::ios::ate);
  std::streampos spirv_file_nbytes = spirv_file.tellg();
  spirv_file.seekg(0);
  spirv_.resize(spirv_file_nbytes);
  spirv_file.read(reinterpret_cast<char*>(spirv_.data()), spirv_.size());

  SpvReflectResult result =
      spvReflectCreateShaderModule(spirv_.size(), spirv_.data(), &module_);
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS, result)
      << "spvReflectCreateShaderModule() failed";

  ASSERT_EQ(module_.entry_point_count, 1);
  ASSERT_EQ(module_.entry_points[0].shader_stage,
            SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT);
  ASSERT_EQ(module_.entry_points[0].local_size.x, 1);
  ASSERT_EQ(module_.entry_points[0].local_size.y, 1);
  ASSERT_EQ(module_.entry_points[0].local_size.z, 1);

  spvReflectDestroyShaderModule(&module_);
}

TEST(SpirvReflectTestCase, TestTaskShaderEXT) {
  std::vector<uint8_t> spirv_;
  SpvReflectShaderModule module_;
  const std::string spirv_path =
      "../tests/mesh_shader_ext/mesh_shader_ext.task.hlsl.spv";
  std::ifstream spirv_file(spirv_path, std::ios::binary | std::ios::ate);
  std::streampos spirv_file_nbytes = spirv_file.tellg();
  spirv_file.seekg(0);
  spirv_.resize(spirv_file_nbytes);
  spirv_file.read(reinterpret_cast<char*>(spirv_.data()), spirv_.size());

  SpvReflectResult result =
      spvReflectCreateShaderModule(spirv_.size(), spirv_.data(), &module_);
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS, result)
      << "spvReflectCreateShaderModule() failed";

  ASSERT_EQ(module_.entry_point_count, 1);
  ASSERT_EQ(module_.entry_points[0].shader_stage,
            SPV_REFLECT_SHADER_STAGE_TASK_BIT_EXT);

  spvReflectDestroyShaderModule(&module_);
}

TEST(SpirvReflectTestCase, TestMeshShaderEXT) {
  std::vector<uint8_t> spirv_;
  SpvReflectShaderModule module_;
  const std::string spirv_path =
      "../tests/mesh_shader_ext/mesh_shader_ext.mesh.hlsl.spv";
  std::ifstream spirv_file(spirv_path, std::ios::binary | std::ios::ate);
  std::streampos spirv_file_nbytes = spirv_file.tellg();
  spirv_file.seekg(0);
  spirv_.resize(spirv_file_nbytes);
  spirv_file.read(reinterpret_cast<char*>(spirv_.data()), spirv_.size());

  SpvReflectResult result =
      spvReflectCreateShaderModule(spirv_.size(), spirv_.data(), &module_);
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS, result)
      << "spvReflectCreateShaderModule() failed";

  ASSERT_EQ(module_.entry_point_count, 1);
  ASSERT_EQ(module_.entry_points[0].shader_stage,
            SPV_REFLECT_SHADER_STAGE_MESH_BIT_EXT);

  spvReflectDestroyShaderModule(&module_);
}

TEST(SpirvReflectTestCase, TestGeometryInvocationsOutputVertices) {
  std::vector<uint8_t> spirv_;
  SpvReflectShaderModule module_;
  const std::string spirv_path =
      "../tests/entry_exec_mode/geom_inv_out_vert.spv";
  std::ifstream spirv_file(spirv_path, std::ios::binary | std::ios::ate);
  std::streampos spirv_file_nbytes = spirv_file.tellg();
  spirv_file.seekg(0);
  spirv_.resize(spirv_file_nbytes);
  spirv_file.read(reinterpret_cast<char*>(spirv_.data()), spirv_.size());

  SpvReflectResult result =
      spvReflectCreateShaderModule(spirv_.size(), spirv_.data(), &module_);
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS, result)
      << "spvReflectCreateShaderModule() failed";

  ASSERT_EQ(module_.entry_point_count, 1);
  ASSERT_EQ(module_.entry_points[0].shader_stage,
            SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT);
  ASSERT_EQ(module_.entry_points[0].invocations, 2);
  ASSERT_EQ(module_.entry_points[0].output_vertices, 2);

  spvReflectDestroyShaderModule(&module_);
}

class SpirvReflectMultiEntryPointTest : public ::testing::TestWithParam<int> {
 protected:
  void SetUp() override {
    // called after constructor before each test
    const std::string spirv_path =
        "../tests/multi_entrypoint/multi_entrypoint.spv";
    std::ifstream spirv_file(spirv_path, std::ios::binary | std::ios::ate);
    std::streampos spirv_file_nbytes = spirv_file.tellg();
    spirv_file.seekg(0);
    spirv_.resize(spirv_file_nbytes);
    spirv_file.read(reinterpret_cast<char*>(spirv_.data()), spirv_.size());

    SpvReflectResult result =
        spvReflectCreateShaderModule(spirv_.size(), spirv_.data(), &module_);
    ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS, result)
        << "spvReflectCreateShaderModule() failed";
  }

  // optional:
  void TearDown() override {
    // called before destructor after all tests
    spvReflectDestroyShaderModule(&module_);
  }

  const char* eps_[2] = {"entry_vert", "entry_frag"};
  std::vector<uint8_t> spirv_;
  SpvReflectShaderModule module_;
};

TEST_F(SpirvReflectMultiEntryPointTest, GetEntryPoint) {
  ASSERT_EQ(&module_.entry_points[0],
            spvReflectGetEntryPoint(&module_, eps_[0]));
  ASSERT_EQ(&module_.entry_points[1],
            spvReflectGetEntryPoint(&module_, eps_[1]));
  ASSERT_EQ(NULL, spvReflectGetEntryPoint(&module_, "entry_tess"));
}

TEST_F(SpirvReflectMultiEntryPointTest, GetDescriptorBindings0) {
  uint32_t binding_count = 0;
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS,
            spvReflectEnumerateEntryPointDescriptorBindings(
                &module_, eps_[0], &binding_count, NULL));
  ASSERT_EQ(binding_count, 1);
  SpvReflectDescriptorBinding* binding;
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS,
            spvReflectEnumerateEntryPointDescriptorBindings(
                &module_, eps_[0], &binding_count, &binding));
  ASSERT_EQ(binding->set, 0);
  ASSERT_EQ(binding->binding, 1);
  ASSERT_EQ(strcmp(binding->name, "ubo"), 0);
  ASSERT_EQ(binding->descriptor_type,
            SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

  SpvReflectResult result;
  ASSERT_EQ(binding,
            spvReflectGetEntryPointDescriptorBinding(
                &module_, eps_[0], binding->binding, binding->set, &result));
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS, result);
}

TEST_F(SpirvReflectMultiEntryPointTest, GetDescriptorBindings1) {
  uint32_t binding_count = 0;
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS,
            spvReflectEnumerateEntryPointDescriptorBindings(
                &module_, eps_[1], &binding_count, NULL));
  ASSERT_EQ(binding_count, 2);
  SpvReflectDescriptorBinding* bindings[2];
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS,
            spvReflectEnumerateEntryPointDescriptorBindings(
                &module_, eps_[1], &binding_count, bindings));
  ASSERT_EQ(bindings[0]->set, 0);
  ASSERT_EQ(bindings[0]->binding, 0);
  ASSERT_EQ(strcmp(bindings[0]->name, "tex"), 0);
  ASSERT_EQ(bindings[0]->descriptor_type,
            SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

  ASSERT_EQ(bindings[1]->set, 0);
  ASSERT_EQ(bindings[1]->binding, 1);
  ASSERT_EQ(strcmp(bindings[1]->name, "ubo"), 0);
  ASSERT_EQ(bindings[1]->descriptor_type,
            SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

  for (size_t i = 0; i < 2; ++i) {
    SpvReflectResult result;
    ASSERT_EQ(bindings[i], spvReflectGetEntryPointDescriptorBinding(
                               &module_, eps_[1], bindings[i]->binding,
                               bindings[i]->set, &result));
    ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS, result);
  }
}

TEST_F(SpirvReflectMultiEntryPointTest, GetDescriptorBindingsShared) {
  uint32_t vert_count = 1;
  SpvReflectDescriptorBinding* vert_binding;

  uint32_t frag_count = 2;
  SpvReflectDescriptorBinding* frag_binding[2];
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS,
            spvReflectEnumerateEntryPointDescriptorBindings(
                &module_, eps_[0], &vert_count, &vert_binding));
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS,
            spvReflectEnumerateEntryPointDescriptorBindings(
                &module_, eps_[1], &frag_count, frag_binding));
  ASSERT_EQ(vert_binding, frag_binding[1]);
}

TEST_F(SpirvReflectMultiEntryPointTest, GetDescriptorSets0) {
  uint32_t set_count;
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS,
            spvReflectEnumerateEntryPointDescriptorSets(&module_, eps_[0],
                                                        &set_count, NULL));
  ASSERT_EQ(set_count, 1);
  SpvReflectDescriptorSet* set;
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS,
            spvReflectEnumerateEntryPointDescriptorSets(&module_, eps_[0],
                                                        &set_count, &set));
  ASSERT_EQ(set->set, 0);
  ASSERT_EQ(set->binding_count, 1);
  ASSERT_EQ(set, &module_.entry_points[0].descriptor_sets[0]);

  SpvReflectResult result;
  ASSERT_EQ(set, spvReflectGetEntryPointDescriptorSet(&module_, eps_[0],
                                                      set->set, &result));
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS, result);
}

TEST_F(SpirvReflectMultiEntryPointTest, GetDescriptorSets1) {
  uint32_t set_count;
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS,
            spvReflectEnumerateEntryPointDescriptorSets(&module_, eps_[1],
                                                        &set_count, NULL));
  ASSERT_EQ(set_count, 1);
  SpvReflectDescriptorSet* set;
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS,
            spvReflectEnumerateEntryPointDescriptorSets(&module_, eps_[1],
                                                        &set_count, &set));
  ASSERT_EQ(set->set, 0);
  ASSERT_EQ(set->binding_count, 2);
  ASSERT_EQ(set, &module_.entry_points[1].descriptor_sets[0]);

  SpvReflectResult result;
  ASSERT_EQ(set, spvReflectGetEntryPointDescriptorSet(&module_, eps_[1],
                                                      set->set, &result));
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS, result);
}

TEST_F(SpirvReflectMultiEntryPointTest, GetInputVariables) {
  const uint32_t counts[2] = {2, 1};
  const char* names[2][2] = {{"iUV", "pos"}, {"iUV", NULL}};
  for (size_t i = 0; i < 2; ++i) {
    uint32_t count = 0;
    ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS,
              spvReflectEnumerateEntryPointInputVariables(&module_, eps_[i],
                                                          &count, NULL));
    ASSERT_EQ(count, counts[i]);

    // 2 is the max count
    SpvReflectInterfaceVariable* vars[2];
    ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS,
              spvReflectEnumerateEntryPointInputVariables(&module_, eps_[i],
                                                          &count, vars));
    for (size_t j = 0; j < counts[i]; ++j) {
      if (vars[j]->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN) {
        // built-ins don't have reasonable locations
        continue;
      }
      SpvReflectResult result;
      const SpvReflectInterfaceVariable* var =
          spvReflectGetEntryPointInputVariableByLocation(
              &module_, eps_[i], vars[j]->location, &result);
      ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
      ASSERT_EQ(var, vars[j]);
      ASSERT_EQ(strcmp(var->name, names[i][var->location]), 0);
    }
  }
}

TEST_F(SpirvReflectMultiEntryPointTest, GetOutputVariables) {
  const uint32_t counts[2] = {2, 1};
  // One of the outputs from the first entry point is a builtin so it has no
  // position.
  const char* names[2][1] = {{"oUV"}, {"colour"}};
  for (size_t i = 0; i < 2; ++i) {
    uint32_t count = 0;
    ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS,
              spvReflectEnumerateEntryPointOutputVariables(&module_, eps_[i],
                                                           &count, NULL));
    ASSERT_EQ(count, counts[i]);

    // 2 is the max count
    SpvReflectInterfaceVariable* vars[2];
    ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS,
              spvReflectEnumerateEntryPointOutputVariables(&module_, eps_[i],
                                                           &count, vars));
    for (size_t j = 0; j < counts[i]; ++j) {
      if (vars[j]->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN) {
        // built-ins don't have reasonable locations
        continue;
      }
      SpvReflectResult result;
      const SpvReflectInterfaceVariable* var =
          spvReflectGetEntryPointOutputVariableByLocation(
              &module_, eps_[i], vars[j]->location, &result);
      ASSERT_EQ(result, SPV_REFLECT_RESULT_SUCCESS);
      ASSERT_EQ(var, vars[j]);
      ASSERT_EQ(strcmp(var->name, names[i][var->location]), 0);
    }
  }
}

TEST_F(SpirvReflectMultiEntryPointTest, GetPushConstants) {
  for (size_t i = 0; i < 2; ++i) {
    SpvReflectBlockVariable* var;
    uint32_t count = 0;
    ASSERT_EQ(SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH,
              spvReflectEnumerateEntryPointPushConstantBlocks(&module_, eps_[i],
                                                              &count, &var));
    ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS,
              spvReflectEnumerateEntryPointPushConstantBlocks(&module_, eps_[i],
                                                              &count, NULL));
    ASSERT_EQ(count, 1);
    ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS,
              spvReflectEnumerateEntryPointPushConstantBlocks(&module_, eps_[i],
                                                              &count, &var));
    SpvReflectResult result;
    ASSERT_EQ(var, spvReflectGetEntryPointPushConstantBlock(&module_, eps_[i],
                                                            &result));
    ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS, result);
  }
}

TEST_F(SpirvReflectMultiEntryPointTest, ChangeDescriptorBindingNumber) {
  const SpvReflectDescriptorBinding* binding =
      spvReflectGetEntryPointDescriptorBinding(&module_, eps_[0], 1, 0, NULL);
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS,
            spvReflectChangeDescriptorBindingNumbers(&module_, binding, 2, 1));
  // Change descriptor binding numbers doesn't currently resort so it won't
  // invalidate binding, but if that changes this test will need to be fixed.
  ASSERT_EQ(binding->set, 1);
  ASSERT_EQ(binding->binding, 2);

  SpvReflectResult result;
  const SpvReflectDescriptorSet* set0 =
      spvReflectGetEntryPointDescriptorSet(&module_, eps_[0], 1, &result);
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS, result);
  ASSERT_EQ(set0->binding_count, 1);
  const SpvReflectDescriptorSet* set1 =
      spvReflectGetEntryPointDescriptorSet(&module_, eps_[1], 1, &result);
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS, result);
  ASSERT_EQ(set1->binding_count, 1);

  ASSERT_EQ(set0->bindings[0], set1->bindings[0]);
}

TEST_F(SpirvReflectMultiEntryPointTest, ChangeDescriptorSetNumber) {
  const SpvReflectDescriptorSet* set =
      spvReflectGetDescriptorSet(&module_, 0, NULL);
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS,
            spvReflectChangeDescriptorSetNumber(&module_, set, 1));
  // Change descriptor binding numbers doesn't currently resort so it won't
  // invalidate binding, but if that changes this test will need to be fixed.
  ASSERT_EQ(set->set, 1);

  SpvReflectResult result;
  const SpvReflectDescriptorSet* set0 =
      spvReflectGetEntryPointDescriptorSet(&module_, eps_[0], 1, &result);
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS, result);
  ASSERT_EQ(set0->binding_count, 1);
  const SpvReflectDescriptorSet* set1 =
      spvReflectGetEntryPointDescriptorSet(&module_, eps_[1], 1, &result);
  ASSERT_EQ(SPV_REFLECT_RESULT_SUCCESS, result);
  ASSERT_EQ(set1->binding_count, 2);

  ASSERT_EQ(set0->bindings[0], set1->bindings[1]);
  ASSERT_EQ(set0->bindings[0]->set, 1);
}
