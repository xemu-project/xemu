/*
 Copyright 2017-2018 Google Inc.

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

#if defined(WIN32)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>
#endif

#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "../common/output_stream.h"
#include "examples/common.h"
#include "spirv_reflect.h"

void StreamWrite(std::ostream& os, const SpvReflectDescriptorBinding& obj, bool write_set, const char* indent = "") {
  const char* t = indent;

  os << " " << obj.name;
  os << "\n";
  os << t;
  os << ToStringDescriptorType(obj.descriptor_type);
  os << " "
     << "(" << ToStringResourceType(obj.resource_type) << ")";
  if ((obj.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE) ||
      (obj.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE) ||
      (obj.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER) ||
      (obj.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)) {
    os << "\n";
    os << t;
    os << "dim=" << obj.image.dim << ", ";
    os << "depth=" << obj.image.depth << ", ";
    os << "arrayed=" << obj.image.arrayed << ", ";
    os << "ms=" << obj.image.ms << ", ";
    os << "sampled=" << obj.image.sampled;
  }
}

void StreamWrite(std::ostream& os, const SpvReflectShaderModule& obj, const char* indent = "") {
  os << "entry point     : " << obj.entry_point_name << "\n";
  os << "source lang     : " << spvReflectSourceLanguage(obj.source_language) << "\n";
  os << "source lang ver : " << obj.source_language_version;
}

void StreamWrite(std::ostream& os, const SpvReflectDescriptorBinding& obj) { StreamWrite(os, obj, true, "  "); }

// Specialized stream-writer that only includes descriptor bindings.
void StreamWrite(std::ostream& os, const spv_reflect::ShaderModule& obj) {
  const char* t = "  ";
  const char* tt = "    ";
  const char* ttt = "      ";

  StreamWrite(os, obj.GetShaderModule(), "");

  SpvReflectResult result = SPV_REFLECT_RESULT_NOT_READY;
  uint32_t count = 0;
  std::vector<SpvReflectInterfaceVariable*> variables;
  std::vector<SpvReflectDescriptorBinding*> bindings;
  std::vector<SpvReflectDescriptorSet*> sets;

  count = 0;
  result = obj.EnumerateDescriptorBindings(&count, nullptr);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);
  bindings.resize(count);
  result = obj.EnumerateDescriptorBindings(&count, bindings.data());
  assert(result == SPV_REFLECT_RESULT_SUCCESS);
  if (count > 0) {
    os << "\n";
    os << "\n";
    os << t << "Descriptor bindings: " << count << "\n";
    for (size_t i = 0; i < bindings.size(); ++i) {
      auto p_binding = bindings[i];
      assert(result == SPV_REFLECT_RESULT_SUCCESS);
      os << tt << i << ":";
      StreamWrite(os, *p_binding, true, ttt);
      if (i < (count - 1)) {
        os << "\n\n";
      }
    }
  }
}

// =================================================================================================
// PrintUsage()
// =================================================================================================
void PrintUsage() {
  std::cout << "Usage: hlsl_resource_types [OPTIONS] path/to/SPIR-V/bytecode.spv" << std::endl
            << "Options:" << std::endl
            << " --help:               Display this message" << std::endl
            << std::endl;
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
    std::cerr << "ERROR: could not open '" << input_spv_path << "' for reading" << std::endl;
    return EXIT_FAILURE;
  }

  spv_ifstream.seekg(0, std::ios::end);
  size_t size = static_cast<size_t>(spv_ifstream.tellg());
  spv_ifstream.seekg(0, std::ios::beg);

  {
    std::vector<char> spv_data(size);
    spv_ifstream.read(spv_data.data(), size);

    spv_reflect::ShaderModule reflection(spv_data.size(), spv_data.data());
    if (reflection.GetResult() != SPV_REFLECT_RESULT_SUCCESS) {
      std::cerr << "ERROR: could not process '" << input_spv_path << "' (is it a valid SPIR-V bytecode?)" << std::endl;
      return EXIT_FAILURE;
    }

    StreamWrite(std::cout, reflection);
    std::cout << std::endl << std::endl;
  }

  return 0;
}
