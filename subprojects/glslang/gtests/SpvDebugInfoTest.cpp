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
#include "SPIRV/NonSemanticShaderDebugInfo.h"
#include "glslang/Public/ResourceLimits.h"
#include <gtest/gtest.h>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace glslangtest {
namespace {

std::string decodeLiteralString(const uint32_t* words, size_t wordCount)
{
    std::string result;
    for (size_t wordIndex = 0; wordIndex < wordCount; ++wordIndex) {
        for (unsigned int byteIndex = 0; byteIndex < 4; ++byteIndex) {
            const char character = static_cast<char>((words[wordIndex] >> (8 * byteIndex)) & 0xff);
            if (character == '\0')
                return result;
            result.push_back(character);
        }
    }
    return result;
}

bool containsDebugTypeBasic(const std::vector<uint32_t>& spirv, const char* expectedName,
                            uint32_t expectedWidth, spv::FPEncoding expectedFPEncoding)
{
    if (spirv.size() < 5 || spirv[0] != spv::MagicNumber)
        return false;

    const uint32_t idBound = spirv[3];
    std::vector<std::string> strings(idBound);
    std::vector<uint32_t> constants(idBound);
    std::vector<bool> isConstant(idBound);
    uint32_t debugInfoImportId = 0;

    size_t end = spirv.size();

    // Resolve the IDs referenced by DebugTypeBasic before inspecting the instruction itself.
    // OpExtInstImport gives the result ID used to select the .101 debug-info instruction set,
    // OpString gives the source-level type name, and OpConstant gives the values of the width,
    // base encoding, flags, and optional FPEncoding operands.
    //
    // For example, the relevant SPIR-V for floate2m1_t has this shape:
    //   %debug = OpExtInstImport "NonSemantic.Shader.DebugInfo.101"
    //   %name = OpString "floate2m1_t"
    //   %width = OpConstant %uint 4
    //   %encoding = OpConstant %uint 3       ; NonSemanticShaderDebugInfoFloat
    //   %flags = OpConstant %uint 0          ; NonSemanticShaderDebugInfoNone
    //   %fpEncoding = OpConstant %uint 4225  ; Float4E2M1EXT
    //   %type = OpExtInst %void %debug DebugTypeBasic
    //               %name %width %encoding %flags %fpEncoding
    for (size_t offset = 5; offset < spirv.size();) {
        const uint32_t wordCount = spirv[offset] >> spv::WordCountShift;
        const spv::Op opcode = static_cast<spv::Op>(spirv[offset] & spv::OpCodeMask);
        if (wordCount == 0 || offset + wordCount > spirv.size())
            return false;

        if (opcode == spv::Op::OpFunction) {
            // SPIR-V's logical layout places imports, debug strings, constants, and global
            // debug-type instructions before the first OpFunction. Record that boundary while
            // resolving IDs so this pass can exit early and the subsequent DebugTypeBasic scan
            // can stop there as well.
            end = offset;
            break;
        }

        if (opcode == spv::Op::OpExtInstImport && wordCount >= 3) {
            const uint32_t resultId = spirv[offset + 1];
            if (resultId < idBound &&
                decodeLiteralString(&spirv[offset + 2], wordCount - 2) ==
                    "NonSemantic.Shader.DebugInfo.101") {
                debugInfoImportId = resultId;
            }
        } else if (opcode == spv::Op::OpString && wordCount >= 3) {
            const uint32_t resultId = spirv[offset + 1];
            if (resultId < idBound)
                strings[resultId] = decodeLiteralString(&spirv[offset + 2], wordCount - 2);
        } else if (opcode == spv::Op::OpConstant && wordCount == 4) {
            const uint32_t resultId = spirv[offset + 2];
            if (resultId < idBound) {
                constants[resultId] = spirv[offset + 3];
                isConstant[resultId] = true;
            }
        }

        offset += wordCount;
    }

    if (debugInfoImportId == 0)
        return false;

    const auto hasConstantValue = [&](uint32_t id, uint32_t value) {
        return id < idBound && isConstant[id] && constants[id] == value;
    };

    // A DebugTypeBasic with an FPEncoding has five operands after its instruction number:
    // Name, Size, Encoding, Flags, and FPEncoding. Including the five-word OpExtInst prefix,
    // that gives a word count of 10. Match the imported .101 instruction set and resolve each
    // operand ID through the OpString and OpConstant tables populated above.
    for (size_t offset = 5; offset < end;) {
        const uint32_t wordCount = spirv[offset] >> spv::WordCountShift;
        const spv::Op opcode = static_cast<spv::Op>(spirv[offset] & spv::OpCodeMask);
        if (wordCount == 0 || offset + wordCount > spirv.size())
            return false;

        if (opcode == spv::Op::OpExtInst && wordCount == 10 &&
            spirv[offset + 3] == debugInfoImportId &&
            spirv[offset + 4] == NonSemanticShaderDebugInfoDebugTypeBasic) {
            const uint32_t nameId = spirv[offset + 5];
            const uint32_t widthId = spirv[offset + 6];
            const uint32_t encodingId = spirv[offset + 7];
            const uint32_t flagsId = spirv[offset + 8];
            const uint32_t fpEncodingId = spirv[offset + 9];
            if (nameId < idBound && strings[nameId] == expectedName &&
                hasConstantValue(widthId, expectedWidth) &&
                hasConstantValue(encodingId, NonSemanticShaderDebugInfoFloat) &&
                hasConstantValue(flagsId, NonSemanticShaderDebugInfoNone) &&
                hasConstantValue(fpEncodingId, static_cast<uint32_t>(expectedFPEncoding))) {
                return true;
            }
        }

        offset += wordCount;
    }

    return false;
}

bool containsDebugEntryPoint(const std::vector<uint32_t>& spirv, const char* expectedCompilerSignature,
                             const char* expectedCommandLineArguments, const char* expectedCurrentWorkingDirectory)
{
    if (spirv.size() < 5 || spirv[0] != spv::MagicNumber)
        return false;

    const uint32_t idBound = spirv[3];
    std::vector<std::string> strings(idBound);
    uint32_t debugInfoImportId = 0;
    uint32_t compilerSignatureId = 0;
    uint32_t commandLineArgumentsId = 0;
    uint32_t currentWorkingDirectoryId = 0;

    for (size_t offset = 5; offset < spirv.size();) {
        const uint32_t wordCount = spirv[offset] >> spv::WordCountShift;
        const spv::Op opcode = static_cast<spv::Op>(spirv[offset] & spv::OpCodeMask);
        if (wordCount == 0 || offset + wordCount > spirv.size())
            return false;

        if (opcode == spv::Op::OpExtInstImport && wordCount >= 3) {
            if (decodeLiteralString(&spirv[offset + 2], wordCount - 2) ==
                "NonSemantic.Shader.DebugInfo.102") {
                debugInfoImportId = spirv[offset + 1];
            }
        } else if (opcode == spv::Op::OpString && wordCount >= 3) {
            const uint32_t resultId = spirv[offset + 1];
            if (resultId < idBound)
                strings[resultId] = decodeLiteralString(&spirv[offset + 2], wordCount - 2);
        } else if (opcode == spv::Op::OpExtInst && wordCount == 10 &&
                   spirv[offset + 3] == debugInfoImportId &&
                   spirv[offset + 4] == NonSemanticShaderDebugInfoDebugEntryPoint) {
            compilerSignatureId = spirv[offset + 7];
            commandLineArgumentsId = spirv[offset + 8];
            currentWorkingDirectoryId = spirv[offset + 9];
        }

        offset += wordCount;
    }

    return debugInfoImportId != 0 && compilerSignatureId < idBound &&
           strings[compilerSignatureId] == expectedCompilerSignature && commandLineArgumentsId < idBound &&
           strings[commandLineArgumentsId] == expectedCommandLineArguments && currentWorkingDirectoryId < idBound &&
           strings[currentWorkingDirectoryId] == expectedCurrentWorkingDirectory;
}

class SpvDebugInfoTest : public ::testing::Test {
protected:
    bool compileToSpirvWithDebugInfo(const std::string& source, std::vector<uint32_t>& spirv, std::string& error,
                                     const char* compilerSignature = nullptr,
                                     const char* commandLineArguments = nullptr,
                                     const char* currentWorkingDirectory = nullptr)
    {
        spirv.clear();
        error.clear();
        glslang::TShader shader(EShLangCompute);
        const char* str = source.c_str();
        shader.setStrings(&str, 1);
        shader.setEnvInput(glslang::EShSourceGlsl, EShLangCompute,
                           glslang::EShClientVulkan, 110);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);

        EShMessages messages = static_cast<EShMessages>(EShMsgDefault | EShMsgDebugInfo);
        if (!shader.parse(GetDefaultResources(), 450, false, messages)) {
            error = "COMPILATION_FAILED: " + std::string(shader.getInfoLog());
            return false;
        }

        glslang::TProgram program;
        program.addShader(&shader);
        if (!program.link(messages)) {
            error = "LINKING_FAILED: " + std::string(program.getInfoLog());
            return false;
        }

        glslang::SpvOptions opts;
        opts.generateDebugInfo = true;
        opts.emitNonSemanticShaderDebugInfo = true;
        opts.disableOptimizer = true;
        opts.compilerSignature = compilerSignature;
        opts.commandLineArguments = commandLineArguments;
        opts.currentWorkingDirectory = currentWorkingDirectory;

        glslang::GlslangToSpv(*program.getIntermediate(EShLangCompute), spirv, &opts);
        return true;
    }

    // Compile a GLSL compute shader with NonSemantic debug info enabled, targeting
    // Vulkan 1.1 / SPIR-V 1.3, and return the disassembly.
    std::string compileWithDebugInfo(const std::string& source)
    {
        std::vector<uint32_t> spirv;
        std::string error;
        if (!compileToSpirvWithDebugInfo(source, spirv, error))
            return error;

        std::ostringstream out;
        spv::Disassemble(out, spirv);
        return out.str();
    }
};

TEST_F(SpvDebugInfoTest, DebugEntryPointUsesSuppliedCompilationMetadata)
{
    const std::string source = R"(
#version 450 core
layout(local_size_x = 1) in;
void main() {}
)";
    constexpr char compilerSignature[] = "test compiler signature";
    constexpr char commandLineArguments[] = "-V -gVS shader.comp";
    constexpr char currentWorkingDirectory[] = "test/current/working/directory";
    std::vector<uint32_t> spirv;
    std::string error;
    ASSERT_TRUE(compileToSpirvWithDebugInfo(source, spirv, error, compilerSignature, commandLineArguments,
                                           currentWorkingDirectory))
        << error;

    EXPECT_TRUE(containsDebugEntryPoint(spirv, compilerSignature, commandLineArguments, currentWorkingDirectory));
}

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

// DebugTypeBasic for OCP microscaling types must carry their source-level type
// names, which are emitted only when the FPEncoding operand is present.
TEST_F(SpvDebugInfoTest, OcpMicroscalingTypesEmitDebugTypeBasicWithFPEncoding)
{
    const std::string source = R"(
#version 450 core
#extension GL_EXT_float_e2m1 : require
#extension GL_EXT_float_e3m2 : require
#extension GL_EXT_float_e2m3 : require
#extension GL_EXT_float_ue8m0 : require
#extension GL_EXT_float_mxint8 : require
#extension GL_EXT_shader_explicit_arithmetic_types : enable
layout(local_size_x = 64) in;
layout(set = 0, binding = 0) buffer B { float data[]; } buf;
void main() {
    floate2m1_t e2m1 = floate2m1_t(1.0fe2m1);
    floate3m2_t e3m2 = floate3m2_t(1.0fe3m2);
    floate2m3_t e2m3 = floate2m3_t(1.0fe2m3);
    floatue8m0_t ue8m0 = floatue8m0_t(1.0fue8m0);
    floatmxint8_t mxint8 = floatmxint8_t(1.0fmxint8);
    buf.data[gl_GlobalInvocationID.x] =
        float(e2m1) + float(e3m2) + float(e2m3) + float(ue8m0) + float(mxint8);
}
)";
    std::vector<uint32_t> spirv;
    std::string error;
    ASSERT_TRUE(compileToSpirvWithDebugInfo(source, spirv, error)) << error;

    struct ExpectedDebugType {
        const char* name;
        uint32_t width;
        spv::FPEncoding fpEncoding;
    };
    const ExpectedDebugType expectedTypes[] = {
        {"floate2m1_t", 4, spv::FPEncoding::Float4E2M1EXT},
        {"floate3m2_t", 6, spv::FPEncoding::Float6E3M2EXT},
        {"floate2m3_t", 6, spv::FPEncoding::Float6E2M3EXT},
        {"floatue8m0_t", 8, spv::FPEncoding::Float8UnsignedE8M0EXT},
        {"floatmxint8_t", 8, spv::FPEncoding::MXInt8EXT},
    };

    std::ostringstream disassembly;
    spv::Disassemble(disassembly, spirv);
    for (const auto& expected : expectedTypes) {
        EXPECT_TRUE(containsDebugTypeBasic(spirv, expected.name, expected.width, expected.fpEncoding))
            << "Expected DebugTypeBasic for \"" << expected.name << "\" with width " << expected.width
            << " and FPEncoding " << static_cast<uint32_t>(expected.fpEncoding)
            << ".\nSPIR-V:\n" << disassembly.str();
    }
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
