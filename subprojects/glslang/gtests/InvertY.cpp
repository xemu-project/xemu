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
#include "glslang/Public/ResourceLimits.h"
#include "glslang/Public/ShaderLang.h"
#include "SPIRV/GlslangToSpv.h"
#include "SPIRV/disassemble.h"
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <vector>

namespace glslangtest {

class InvertYTest : public ::testing::Test {
protected:
    struct CompileResult {
        bool success;
        std::string spirvText;
        std::string infoLog;
    };

    CompileResult compileGlsl(const std::string& source, EShLanguage stage, bool invertY)
    {
        glslang::TShader shader(stage);
        glslang::TProgram program;

        const char* src = source.c_str();
        shader.setStrings(&src, 1);
        shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
        shader.setInvertY(invertY);

        EShMessages msgs = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
        if (!shader.parse(GetDefaultResources(), 450, false, msgs)) {
            return {false, "", shader.getInfoLog()};
        }

        program.addShader(&shader);
        if (!program.link(msgs)) {
            return {false, "", program.getInfoLog()};
        }

        std::vector<uint32_t> spirv;
        glslang::GlslangToSpv(*program.getIntermediate(stage), spirv);

        std::ostringstream dis;
        spv::Disassemble(dis, spirv);
        return {true, dis.str(), ""};
    }

    // Helper function to check if the given SPIR-V string contains a specific pattern.
    bool containsPattern(const std::string& spirvText, const std::string& pattern)
    {
        return spirvText.find(pattern) != std::string::npos;
    }

    // Helper function to check if the given SPIR-V string contains a FNegate instruction.
    bool containsFNegate(const std::string& spirvText) { return containsPattern(spirvText, "FNegate"); }
};

// A minimal vertex shader that writes gl_Position.
static const char* kSimpleVert = R"glsl(
#version 450
layout(location = 0) in vec3 pos;
void main() {
    gl_Position = vec4(pos, 1.0);
}
)glsl";

// A vertex shader that writes gl_Position multiple times to exercise all
// assignment sites (braces ensure each branch is a sequence node).
static const char* kMultiAssignVert = R"glsl(
#version 450
layout(location = 0) in vec3 pos;
layout(location = 1) in float w;
void main() {
    if (w > 0.0) {
        gl_Position = vec4(pos, w);
    } else {
        gl_Position = vec4(-pos, -w);
    }
}
)glsl";

// A geometry shader (takes triangles, emits a single vertex).
static const char* kSimpleGeom = R"glsl(
#version 450
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;
void main() {
    gl_Position = gl_in[0].gl_Position;
    EmitVertex();
}
)glsl";

// A tessellation evaluation shader.
static const char* kSimpleTese = R"glsl(
#version 450
layout(triangles, equal_spacing, ccw) in;
void main() {
    gl_Position = gl_in[0].gl_Position;
}
)glsl";

// --- Vertex stage ---

TEST_F(InvertYTest, VertexWithInvertYContainsFNegate)
{
    auto r = compileGlsl(kSimpleVert, EShLangVertex, true);
    ASSERT_TRUE(r.success) << r.infoLog;
    EXPECT_TRUE(containsFNegate(r.spirvText))
        << "Expected OpFNegate in SPIR-V when setInvertY(true) is used.\n"
        << r.spirvText;
}

TEST_F(InvertYTest, VertexWithoutInvertYNoFNegate)
{
    auto r = compileGlsl(kSimpleVert, EShLangVertex, false);
    ASSERT_TRUE(r.success) << r.infoLog;
    EXPECT_FALSE(containsFNegate(r.spirvText))
        << "Expected no OpFNegate in SPIR-V when setInvertY(false) is used.\n"
        << r.spirvText;
}

TEST_F(InvertYTest, VertexMultipleAssignmentsAllInverted)
{
    auto r = compileGlsl(kMultiAssignVert, EShLangVertex, true);
    ASSERT_TRUE(r.success) << r.infoLog;
    EXPECT_TRUE(containsFNegate(r.spirvText))
        << "Expected OpFNegate in SPIR-V for shader with multiple gl_Position assignments.\n"
        << r.spirvText;
}

// --- Geometry stage ---

TEST_F(InvertYTest, GeometryWithInvertYContainsFNegate)
{
    auto r = compileGlsl(kSimpleGeom, EShLangGeometry, true);
    ASSERT_TRUE(r.success) << r.infoLog;
    EXPECT_TRUE(containsFNegate(r.spirvText))
        << "Expected OpFNegate in geometry shader SPIR-V when setInvertY(true).\n"
        << r.spirvText;
}

TEST_F(InvertYTest, GeometryWithoutInvertYNoFNegate)
{
    auto r = compileGlsl(kSimpleGeom, EShLangGeometry, false);
    ASSERT_TRUE(r.success) << r.infoLog;
    EXPECT_FALSE(containsFNegate(r.spirvText))
        << "Expected no OpFNegate in geometry shader SPIR-V when setInvertY(false).\n"
        << r.spirvText;
}

// --- Tessellation evaluation stage ---

TEST_F(InvertYTest, TessEvalWithInvertYContainsFNegate)
{
    auto r = compileGlsl(kSimpleTese, EShLangTessEvaluation, true);
    ASSERT_TRUE(r.success) << r.infoLog;
    EXPECT_TRUE(containsFNegate(r.spirvText))
        << "Expected OpFNegate in tessellation evaluation shader SPIR-V when setInvertY(true).\n"
        << r.spirvText;
}

TEST_F(InvertYTest, TessEvalWithoutInvertYNoFNegate)
{
    auto r = compileGlsl(kSimpleTese, EShLangTessEvaluation, false);
    ASSERT_TRUE(r.success) << r.infoLog;
    EXPECT_FALSE(containsFNegate(r.spirvText))
        << "Expected no OpFNegate in tessellation evaluation shader SPIR-V when setInvertY(false).\n"
        << r.spirvText;
}

// --- Compute stage should not be affected ---

TEST_F(InvertYTest, ComputeWithInvertYFlagNoFNegate)
{
    // Compute shaders have no gl_Position; the flag must be a no-op.
    const char* src = R"glsl(
#version 450
layout(local_size_x = 1) in;
layout(binding = 0) buffer Out { float val; };
void main() { val = 1.0; }
)glsl";
    auto r = compileGlsl(src, EShLangCompute, true);
    ASSERT_TRUE(r.success) << r.infoLog;
    EXPECT_FALSE(containsFNegate(r.spirvText))
        << "OpFNegate should not appear in a compute shader even with setInvertY(true).\n"
        << r.spirvText;
}

} // namespace glslangtest
