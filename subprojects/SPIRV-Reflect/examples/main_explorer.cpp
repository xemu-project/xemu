/* Copyright (c) 2023 LunarG, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>

#include "spirv_reflect.h"

// =================================================================================================
// PrintUsage()
// =================================================================================================
void PrintUsage() {
  std::cout << "Usage: explorer path/to/SPIR-V/bytecode.spv\n"
            << "\tThis is used to set a breakpoint and explorer the API and "
               "how to access info needed\n";
}

// =================================================================================================
// main()
// =================================================================================================
int main(int argn, char** argv) {
  if (argn != 2) {
    PrintUsage();
    return EXIT_FAILURE;
  } else if (std::string(argv[1]) == "--help") {
    PrintUsage();
    return EXIT_SUCCESS;
  }
  std::string input_spv_path = argv[1];

  std::ifstream spv_ifstream(input_spv_path.c_str(), std::ios::binary);
  if (!spv_ifstream.is_open()) {
    std::cerr << "ERROR: could not open '" << input_spv_path << "' for reading\n";
    return EXIT_FAILURE;
  }

  spv_ifstream.seekg(0, std::ios::end);
  size_t size = static_cast<size_t>(spv_ifstream.tellg());
  spv_ifstream.seekg(0, std::ios::beg);

  std::vector<char> spv_data(size);
  spv_ifstream.read(spv_data.data(), size);

  SpvReflectShaderModule module = {};
  SpvReflectResult result = spvReflectCreateShaderModule(spv_data.size(), spv_data.data(), &module);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  // Go through each enumerate to examine it
  uint32_t count = 0;

  result = spvReflectEnumerateDescriptorSets(&module, &count, NULL);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);
  std::vector<SpvReflectDescriptorSet*> sets(count);
  result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  result = spvReflectEnumerateDescriptorBindings(&module, &count, NULL);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);
  std::vector<SpvReflectDescriptorBinding*> bindings(count);
  result = spvReflectEnumerateDescriptorBindings(&module, &count, bindings.data());
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  result = spvReflectEnumerateInterfaceVariables(&module, &count, NULL);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);
  std::vector<SpvReflectInterfaceVariable*> interface_variables(count);
  result = spvReflectEnumerateInterfaceVariables(&module, &count, interface_variables.data());
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  result = spvReflectEnumerateInputVariables(&module, &count, NULL);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);
  std::vector<SpvReflectInterfaceVariable*> input_variables(count);
  result = spvReflectEnumerateInputVariables(&module, &count, input_variables.data());
  assert(result == SPV_REFLECT_RESULT_SUCCESS);
  result = spvReflectEnumerateOutputVariables(&module, &count, NULL);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);
  std::vector<SpvReflectInterfaceVariable*> output_variables(count);
  result = spvReflectEnumerateOutputVariables(&module, &count, output_variables.data());
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  result = spvReflectEnumeratePushConstantBlocks(&module, &count, NULL);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);
  std::vector<SpvReflectBlockVariable*> push_constant(count);
  result = spvReflectEnumeratePushConstantBlocks(&module, &count, push_constant.data());
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  // Can set a breakpoint here and explorer the various variables enumerated.
  spvReflectDestroyShaderModule(&module);

  return 0;
}
