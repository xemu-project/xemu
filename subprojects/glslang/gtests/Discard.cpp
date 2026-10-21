// Copyright (C) 2026 The Khronos Group Inc.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
//    Neither the name of The Khronos Group Inc. nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include "TestFixture.h"
#include "glslang/Public/ShaderLang.h"
#include "SPIRV/GlslangToSpv.h"
#include "SPIRV/disassemble.h"
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <vector>

namespace glslangtest {

class DiscardTest : public ::testing::Test {
protected:
    struct CompileResult {
        bool success;
        std::string spirvText;
        std::string infoLog;
    };

    CompileResult compileGlsl(const std::string& source, glslang::EShTargetLanguageVersion spvVersion,
                              bool discardIsTerminate)
    {
        const char* const src = source.c_str();

        glslang::TShader shader(EShLangFragment);
        shader.setStrings(&src, 1);
        shader.setEnvInput(glslang::EShSourceGlsl, EShLangFragment, glslang::EShClientVulkan, 100);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_4);
        shader.setEnvTarget(glslang::EShTargetSpv, spvVersion);
        shader.setAutoMapBindings(true);
        shader.setAutoMapLocations(true);
        shader.setDiscardIsTerminate(discardIsTerminate);

        const EShMessages msgs = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
        if (!shader.parse(GetDefaultResources(), 450, false, msgs)) {
            return {false, "", shader.getInfoLog()};
        }

        glslang::TProgram program;
        program.addShader(&shader);
        if (!program.link(msgs)) {
            return {false, "", program.getInfoLog()};
        }

        std::vector<uint32_t> spirv;
        glslang::GlslangToSpv(*program.getIntermediate(EShLangFragment), spirv);

        std::ostringstream dis;
        spv::Disassemble(dis, spirv);
        return {true, dis.str(), ""};
    }

    // Helper function to check if the given SPIR-V string contains a specific pattern.
    static bool containsPattern(const std::string& spirvText, const std::string& pattern)
    {
        return spirvText.find(pattern) != std::string::npos;
    }
};

static const char kSimpleFrag[] = R"glsl(
#version 450
uniform sampler2D tex;
in vec2 texCoord;
out vec4 outColor;
void main() {
    outColor = texture(tex, texCoord);
    if (outColor.a == 0.0)
        discard;
}
)glsl";

TEST_F(DiscardTest, DiscardMappedToOpKill)
{
    const bool discardIsTerminate = false;
    auto const r = compileGlsl(kSimpleFrag, glslang::EShTargetSpv_1_4, discardIsTerminate);
    ASSERT_TRUE(r.success) << r.infoLog;
    EXPECT_TRUE(containsPattern(r.spirvText, "Kill"))
        << "Expected OpKill in SPIR-V when SPIR-V version < 1.6 is used.\n"
        << r.spirvText;
    EXPECT_FALSE(containsPattern(r.spirvText, "DemoteToHelperInvocation"))
        << "Did not expect OpDemoteToHelperInvocation in SPIR-V when SPIR-V version < 1.6 is used.\n"
        << r.spirvText;
    EXPECT_FALSE(containsPattern(r.spirvText, "TerminateInvocation"))
        << "Did not expect OpTerminateInvocation in SPIR-V when SPIR-V version < 1.6 is used.\n"
        << r.spirvText;
}

TEST_F(DiscardTest, DiscardMappedToOpDemoteToHelperInvocation)
{
    const bool discardIsTerminate = false;
    auto const r = compileGlsl(kSimpleFrag, glslang::EShTargetSpv_1_6, discardIsTerminate);
    ASSERT_TRUE(r.success) << r.infoLog;
    EXPECT_FALSE(containsPattern(r.spirvText, "Kill"))
        << "Did not expect OpKill in SPIR-V when SPIR-V version >= 1.6 is used.\n"
        << r.spirvText;
    EXPECT_TRUE(containsPattern(r.spirvText, "DemoteToHelperInvocation"))
        << "Expected OpDemoteToHelperInvocation in SPIR-V when setDiscardIsTerminate(false) is used.\n"
        << r.spirvText;
    EXPECT_FALSE(containsPattern(r.spirvText, "TerminateInvocation"))
        << "Did not expect OpTerminateInvocation in SPIR-V when setDiscardIsTerminate(false) is used.\n"
        << r.spirvText;
}

TEST_F(DiscardTest, DiscardMappedToOpTerminateInvocation)
{
    const bool discardIsTerminate = true;
    auto const r = compileGlsl(kSimpleFrag, glslang::EShTargetSpv_1_6, discardIsTerminate);
    ASSERT_TRUE(r.success) << r.infoLog;
    EXPECT_FALSE(containsPattern(r.spirvText, "Kill"))
        << "Did not expect OpKill in SPIR-V when SPIR-V version >= 1.6 is used.\n"
        << r.spirvText;
    EXPECT_FALSE(containsPattern(r.spirvText, "DemoteToHelperInvocation"))
        << "Did not expect OpDemoteToHelperInvocation in SPIR-V when setDiscardIsTerminate(true) is used.\n"
        << r.spirvText;
    EXPECT_TRUE(containsPattern(r.spirvText, "TerminateInvocation"))
        << "Expected OpTerminateInvocation in SPIR-V when setDiscardIsTerminate(true) is used.\n"
        << r.spirvText;
}

} // namespace glslangtest
