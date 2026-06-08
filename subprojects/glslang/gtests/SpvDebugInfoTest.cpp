// Copyright (C) 2026 NVIDIA Corporation.
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
//    Neither the name of 3Dlabs Inc. Ltd. nor the names of its
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

// Tests for NonSemantic.Shader.DebugInfo (NSDI) features.

#include "TestFixture.h"
#include "glslang/Public/ResourceLimits.h"
#include <gtest/gtest.h>
#include <sstream>
#include <string>

namespace glslangtest {
namespace {

class SpvDebugInfoTest : public ::testing::Test {
protected:
    // Compile a GLSL compute shader with NonSemantic debug info enabled, targeting
    // Vulkan 1.1 / SPIR-V 1.3, and return the disassembly.
    std::string compileWithDebugInfo(const std::string& source)
    {
        glslang::TShader shader(EShLangCompute);
        const char* str = source.c_str();
        shader.setStrings(&str, 1);
        shader.setEnvInput(glslang::EShSourceGlsl, EShLangCompute,
                           glslang::EShClientVulkan, 110);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);

        EShMessages messages = static_cast<EShMessages>(EShMsgDefault | EShMsgDebugInfo);
        if (!shader.parse(GetDefaultResources(), 450, false, messages))
            return "COMPILATION_FAILED: " + std::string(shader.getInfoLog());

        glslang::TProgram program;
        program.addShader(&shader);
        if (!program.link(messages))
            return "LINKING_FAILED: " + std::string(program.getInfoLog());

        glslang::SpvOptions opts;
        opts.generateDebugInfo = true;
        opts.emitNonSemanticShaderDebugInfo = true;
        opts.disableOptimizer = true;

        std::vector<uint32_t> spirv;
        glslang::GlslangToSpv(*program.getIntermediate(EShLangCompute), spirv, &opts);

        std::ostringstream out;
        spv::Disassemble(out, spirv);
        return out.str();
    }
};

// DebugTypeVectorIdEXT (opcode 109) must be emitted for OpTypeCooperativeVectorNV
// when NonSemantic debug info is enabled.
TEST_F(SpvDebugInfoTest, CooperativeVectorNVEmitsDebugTypeVectorIdEXT)
{
    const std::string source = R"(
#version 450 core
#extension GL_KHR_memory_scope_semantics : enable
#extension GL_NV_cooperative_vector : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
layout(local_size_x = 64) in;
layout(set = 0, binding = 0) buffer B { float data[]; } buf;
void main() {
    coopvecNV<float, 8> v = coopvecNV<float, 8>(0.0);
    v = v + v;
    buf.data[gl_GlobalInvocationID.x] = v[0];
}
)";
    std::string spirv = compileWithDebugInfo(source);
    EXPECT_NE(spirv.find("DebugTypeVectorIdEXT"), std::string::npos)
        << "Expected DebugTypeVectorIdEXT in disassembly.\nSPIR-V:\n" << spirv;
    EXPECT_NE(spirv.find("TypeCooperativeVectorNV"), std::string::npos)
        << "Expected TypeCooperativeVectorNV in disassembly.\nSPIR-V:\n" << spirv;
}

// DebugTypeVectorIdEXT must also be emitted when the component count is a
// specialization constant (OpSpecConstant), since the spec allows any constant
// instruction for the ComponentCount operand.
TEST_F(SpvDebugInfoTest, CooperativeVectorNVSpecConstCountEmitsDebugTypeVectorIdEXT)
{
    const std::string source = R"(
#version 450 core
#extension GL_KHR_memory_scope_semantics : enable
#extension GL_NV_cooperative_vector : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
layout(local_size_x = 64) in;
layout(constant_id = 0) const uint N = 16;
layout(set = 0, binding = 0) buffer B { float data[]; } buf;
void main() {
    coopvecNV<float, N> v = coopvecNV<float, N>(1.0);
    buf.data[gl_GlobalInvocationID.x] = v[0];
}
)";
    std::string spirv = compileWithDebugInfo(source);
    EXPECT_NE(spirv.find("DebugTypeVectorIdEXT"), std::string::npos)
        << "Expected DebugTypeVectorIdEXT with spec-constant count.\nSPIR-V:\n" << spirv;
}

// DebugTypeCooperativeMatrixKHR (opcode 110) must replace the former
// opaque-composite workaround for OpTypeCooperativeMatrixKHR.
TEST_F(SpvDebugInfoTest, CooperativeMatrixKHREmitsDebugTypeCooperativeMatrixKHR)
{
    const std::string source = R"(
#version 450 core
#pragma use_vulkan_memory_model
#extension GL_KHR_memory_scope_semantics : enable
#extension GL_KHR_cooperative_matrix : enable
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : enable
layout(local_size_x = 32) in;
layout(set = 0, binding = 0) buffer B { float16_t data[]; } buf;
void main() {
    coopmat<float16_t, gl_ScopeSubgroup, 16, 16, gl_MatrixUseAccumulator> m =
        coopmat<float16_t, gl_ScopeSubgroup, 16, 16, gl_MatrixUseAccumulator>(float16_t(0.0));
    coopMatStore(m, buf.data, 0, 0, gl_CooperativeMatrixLayoutRowMajor);
}
)";
    std::string spirv = compileWithDebugInfo(source);
    EXPECT_NE(spirv.find("DebugTypeCooperativeMatrixKHR"), std::string::npos)
        << "Expected DebugTypeCooperativeMatrixKHR in disassembly.\nSPIR-V:\n" << spirv;
    // The old opaque-composite workaround must not appear.
    EXPECT_EQ(spirv.find("coopmat<"), std::string::npos)
        << "Opaque-composite workaround should not appear in disassembly.\nSPIR-V:\n" << spirv;
}

// DebugTypeCooperativeMatrixKHR must be emitted when Rows and Columns are
// specialization constants (OpSpecConstant), since the spec allows any constant
// instruction for those operands.
TEST_F(SpvDebugInfoTest, CooperativeMatrixKHRSpecConstDimsEmitsDebugType)
{
    const std::string source = R"(
#version 450 core
#pragma use_vulkan_memory_model
#extension GL_KHR_memory_scope_semantics : enable
#extension GL_KHR_cooperative_matrix : enable
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : enable
layout(local_size_x = 32) in;
layout(constant_id = 0) const uint M = 16;
layout(constant_id = 1) const uint N = 16;
layout(set = 0, binding = 0) buffer B { float16_t data[]; } buf;
void main() {
    coopmat<float16_t, gl_ScopeSubgroup, M, N, gl_MatrixUseAccumulator> m =
        coopmat<float16_t, gl_ScopeSubgroup, M, N, gl_MatrixUseAccumulator>(float16_t(0.0));
    coopMatStore(m, buf.data, 0, 0, gl_CooperativeMatrixLayoutRowMajor);
}
)";
    std::string spirv = compileWithDebugInfo(source);
    EXPECT_NE(spirv.find("DebugTypeCooperativeMatrixKHR"), std::string::npos)
        << "Expected DebugTypeCooperativeMatrixKHR with spec-constant dims.\nSPIR-V:\n" << spirv;
}

// DebugTypeCooperativeMatrixKHR must be emitted for each distinct (component,
// scope, rows, cols, use) tuple -- MatrixUseA, MatrixUseB, and MatrixUseAccumulator
// with different component types all produce separate debug type instructions.
TEST_F(SpvDebugInfoTest, CooperativeMatrixKHRMultipleUsesEmitDistinctDebugTypes)
{
    const std::string source = R"(
#version 450 core
#pragma use_vulkan_memory_model
#extension GL_KHR_memory_scope_semantics : enable
#extension GL_KHR_cooperative_matrix : enable
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : enable
layout(local_size_x = 32) in;
layout(set = 0, binding = 0) buffer B { float16_t data[]; } buf;
void main() {
    coopmat<float16_t, gl_ScopeSubgroup, 16, 16, gl_MatrixUseA>           matA;
    coopmat<float16_t, gl_ScopeSubgroup, 16, 16, gl_MatrixUseB>           matB;
    coopmat<float,     gl_ScopeSubgroup, 16, 16, gl_MatrixUseAccumulator>  matC =
        coopmat<float, gl_ScopeSubgroup, 16, 16, gl_MatrixUseAccumulator>(0.0);
    matC = coopMatMulAdd(matA, matB, matC);
    coopMatStore(matC, buf.data, 0, 0, gl_CooperativeMatrixLayoutRowMajor);
}
)";
    std::string spirv = compileWithDebugInfo(source);

    // Count occurrences of DebugTypeCooperativeMatrixKHR: one for each distinct
    // (component, scope, rows, cols, use) combination used above.
    size_t count = 0;
    size_t pos = 0;
    while ((pos = spirv.find("DebugTypeCooperativeMatrixKHR", pos)) != std::string::npos) {
        ++count;
        pos += 1;
    }
    EXPECT_GE(count, 3u)
        << "Expected at least 3 DebugTypeCooperativeMatrixKHR instructions.\nSPIR-V:\n" << spirv;
}

// The removed golden-file test spv.debuginfo.coopmatKHR.comp combined spec-constant
// dimensions with all three matrix use types. Verify that distinct
// DebugTypeCooperativeMatrixKHR instructions are emitted for MatrixUseA (lM x lK),
// MatrixUseB (lK x lN), and MatrixUseAccumulator (lM x lN) when all dimensions are
// specialization constants.
TEST_F(SpvDebugInfoTest, CooperativeMatrixKHRSpecConstDimsMultipleUsesEmitDistinctDebugTypes)
{
    const std::string source = R"(
#version 450 core
#pragma use_vulkan_memory_model
#extension GL_KHR_memory_scope_semantics : enable
#extension GL_KHR_cooperative_matrix : enable
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : enable
layout(local_size_x = 32) in;
layout(constant_id = 0) const uint lM = 16;
layout(constant_id = 1) const uint lN = 16;
layout(constant_id = 2) const uint lK = 16;
layout(set = 0, binding = 0) buffer B { float data[]; } buf;
void main() {
    coopmat<float16_t, gl_ScopeSubgroup, lM, lK, gl_MatrixUseA>           matA;
    coopmat<float16_t, gl_ScopeSubgroup, lK, lN, gl_MatrixUseB>           matB;
    coopmat<float,     gl_ScopeSubgroup, lM, lN, gl_MatrixUseAccumulator>  matC =
        coopmat<float, gl_ScopeSubgroup, lM, lN, gl_MatrixUseAccumulator>(0.0);
    matC = coopMatMulAdd(matA, matB, matC);
    coopMatStore(matC, buf.data, 0, 0, gl_CooperativeMatrixLayoutRowMajor);
}
)";
    std::string spirv = compileWithDebugInfo(source);

    size_t count = 0;
    size_t pos = 0;
    while ((pos = spirv.find("DebugTypeCooperativeMatrixKHR", pos)) != std::string::npos) {
        ++count;
        pos += 1;
    }
    EXPECT_GE(count, 3u)
        << "Expected at least 3 DebugTypeCooperativeMatrixKHR instructions "
           "(one per use type with spec-constant dimensions).\nSPIR-V:\n" << spirv;
}

// DebugTypeBasic for bfloat16_t must carry the debug type name "bfloat16_t",
// which is emitted only when the FPEncoding operand (BFloat16KHR = 0) is present.
TEST_F(SpvDebugInfoTest, BFloat16EmitsDebugTypeBasicWithFPEncoding)
{
    const std::string source = R"(
#version 450 core
#extension GL_EXT_bfloat16 : require
#extension GL_EXT_shader_explicit_arithmetic_types : enable
layout(local_size_x = 64) in;
layout(set = 0, binding = 0) buffer B { float data[]; } buf;
void main() {
    bfloat16_t bf = bfloat16_t(1.0);
    buf.data[gl_GlobalInvocationID.x] = float(bf);
}
)";
    std::string spirv = compileWithDebugInfo(source);
    // The DebugTypeBasic for bfloat16_t should contain the name "bfloat16_t".
    EXPECT_NE(spirv.find("\"bfloat16_t\""), std::string::npos)
        << "Expected \"bfloat16_t\" string in debug type.\nSPIR-V:\n" << spirv;
}

// DebugTypeBasic for floate4m3_t must carry the debug type name "floate4m3_t",
// which is emitted only when the FPEncoding operand (Float8E4M3EXT = 4214) is present.
TEST_F(SpvDebugInfoTest, Float8E4M3EmitsDebugTypeBasicWithFPEncoding)
{
    const std::string source = R"(
#version 450 core
#extension GL_EXT_bfloat16 : require
#extension GL_EXT_float_e4m3 : require
#extension GL_EXT_shader_explicit_arithmetic_types : enable
layout(local_size_x = 64) in;
layout(set = 0, binding = 0) buffer B { float data[]; } buf;
void main() {
    floate4m3_t f = floate4m3_t(2.0);
    buf.data[gl_GlobalInvocationID.x] = float(f);
}
)";
    std::string spirv = compileWithDebugInfo(source);
    EXPECT_NE(spirv.find("\"floate4m3_t\""), std::string::npos)
        << "Expected \"floate4m3_t\" string in debug type.\nSPIR-V:\n" << spirv;
}

// DebugTypeBasic for floate5m2_t must carry the debug type name "floate5m2_t",
// which is emitted only when the FPEncoding operand (Float8E5M2EXT = 4215) is present.
TEST_F(SpvDebugInfoTest, Float8E5M2EmitsDebugTypeBasicWithFPEncoding)
{
    const std::string source = R"(
#version 450 core
#extension GL_EXT_bfloat16 : require
#extension GL_EXT_float_e5m2 : require
#extension GL_EXT_shader_explicit_arithmetic_types : enable
layout(local_size_x = 64) in;
layout(set = 0, binding = 0) buffer B { float data[]; } buf;
void main() {
    floate5m2_t f = floate5m2_t(3.0);
    buf.data[gl_GlobalInvocationID.x] = float(f);
}
)";
    std::string spirv = compileWithDebugInfo(source);
    EXPECT_NE(spirv.find("\"floate5m2_t\""), std::string::npos)
        << "Expected \"floate5m2_t\" string in debug type.\nSPIR-V:\n" << spirv;
}

// When a version-101 opcode is emitted the import string must be promoted to
// NonSemantic.Shader.DebugInfo.101.
TEST_F(SpvDebugInfoTest, Version101OpcodePromotesImportStringTo101)
{
    // DebugTypeCooperativeMatrixKHR is a version-101 opcode; using it must
    // cause the import string to read ".101", not ".100".
    const std::string source = R"(
#version 450 core
#pragma use_vulkan_memory_model
#extension GL_KHR_memory_scope_semantics : enable
#extension GL_KHR_cooperative_matrix : enable
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : enable
layout(local_size_x = 32) in;
layout(set = 0, binding = 0) buffer B { float16_t data[]; } buf;
void main() {
    coopmat<float16_t, gl_ScopeSubgroup, 16, 16, gl_MatrixUseAccumulator> m =
        coopmat<float16_t, gl_ScopeSubgroup, 16, 16, gl_MatrixUseAccumulator>(float16_t(0.0));
    coopMatStore(m, buf.data, 0, 0, gl_CooperativeMatrixLayoutRowMajor);
}
)";
    std::string spirv = compileWithDebugInfo(source);
    EXPECT_NE(spirv.find("\"NonSemantic.Shader.DebugInfo.101\""), std::string::npos)
        << "Expected import string NonSemantic.Shader.DebugInfo.101.\nSPIR-V:\n" << spirv;
    EXPECT_EQ(spirv.find("\"NonSemantic.Shader.DebugInfo.100\""), std::string::npos)
        << "Import string must not be .100 when version-101 opcodes are used.\nSPIR-V:\n" << spirv;
}

// When only version-100 opcodes are emitted the import string must stay at
// NonSemantic.Shader.DebugInfo.100 and must not be promoted to .101.
TEST_F(SpvDebugInfoTest, Version100OnlyKeepsImportStringAt100)
{
    const std::string source = R"(
#version 450 core
layout(local_size_x = 64) in;
layout(set = 0, binding = 0) buffer B { float data[]; } buf;
void main() {
    float x = buf.data[gl_GlobalInvocationID.x];
    buf.data[gl_GlobalInvocationID.x] = x * 2.0;
}
)";
    std::string spirv = compileWithDebugInfo(source);
    EXPECT_NE(spirv.find("\"NonSemantic.Shader.DebugInfo.100\""), std::string::npos)
        << "Expected import string NonSemantic.Shader.DebugInfo.100.\nSPIR-V:\n" << spirv;
    EXPECT_EQ(spirv.find("\"NonSemantic.Shader.DebugInfo.101\""), std::string::npos)
        << "Import string must not be .101 when only version-100 opcodes are used.\nSPIR-V:\n" << spirv;
}

} // namespace
} // namespace glslangtest
