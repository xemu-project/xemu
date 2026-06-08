#ifndef SAMPLE_SPV_H
#define SAMPLE_SPV_H

#include <stdint.h>

/* Source from sample.hlsl

Texture2D     MyTexture : register(t0, space0);
SamplerState  MySampler : register(s1, space1);

struct RGB {
  float r;
  float g;
  float b;
};

struct UBO {
  float4x4  XformMatrix;
  float3    Scale;
  RGB       Rgb;
  float     t;
  float2    uv; 
};

ConstantBuffer<UBO> MyConstants : register(b2, space2);

struct Data {
  float4  Element;
};

ConsumeStructuredBuffer<Data> MyBufferIn : register(u3, space2);
AppendStructuredBuffer<Data> MyBufferOut : register(u4, space2);

struct PSInput {
  float4  Position  : SV_POSITION;
  float3  Normal    : NORMAL;
  float3  Color     : COLOR;
  float   Alpha     : OPACITY;
  float4  Scaling   : SCALE;
  float2  TexCoord0 : TEXCOORD0;
  float2  TexCoord1 : TEXCOORD1;
  float2  TexCoord2 : TEXCOORD2;
};

struct PSOutput {
  float4  oColor0 : SV_TARGET0;
  float4  oColor1 : SV_TARGET1;
  float4  oColor2 : SV_TARGET2;
  float4  oColor3 : SV_TARGET3;
  float4  oColor4 : SV_TARGET4;
  float4  oColor5 : SV_TARGET5;
  float4  oColor6 : SV_TARGET6;
  float4  oColor7 : SV_TARGET7;
};

PSOutput main(PSInput input)
{
  Data val = MyBufferIn[0];
  MyBufferOut[0] = val;

  PSOutput ret;
  ret.oColor0 = mul(MyConstants.XformMatrix, input.Position);
  ret.oColor1 = float4(input.Normal, 1) + float4(MyConstants.Scale, 0);
  ret.oColor2 = float4(input.Color, 1);
  ret.oColor3 = float4(MyTexture.Sample(MySampler, input.TexCoord0).xyz, input.Alpha);
  ret.oColor4 = input.Scaling;
  ret.oColor5 = float4(input.TexCoord0, 0, 0);
  ret.oColor6 = float4(input.TexCoord1, 0, 0);
  ret.oColor7 = float4(input.TexCoord2, 0, 0);
  return ret;
}

*/

// Imported from file 'sample.spv'
const uint32_t k_sample_spv[] = {
0x07230203,0x00010000,0x000d0004,0x00000106,
0x00000000,0x00020011,0x00000001,0x0006000b,
0x00000001,0x4c534c47,0x6474732e,0x3035342e,
0x00000000,0x0003000e,0x00000000,0x00000001,
0x0015000f,0x00000004,0x00000004,0x6e69616d,
0x00000000,0x00000086,0x0000008a,0x0000008d,
0x00000091,0x00000094,0x00000098,0x0000009b,
0x0000009e,0x000000a6,0x000000a9,0x000000ac,
0x000000af,0x000000b2,0x000000b5,0x000000b8,
0x000000bb,0x00030010,0x00000004,0x00000007,
0x00030003,0x00000005,0x000001f4,0x000a0004,
0x475f4c47,0x4c474f4f,0x70635f45,0x74735f70,
0x5f656c79,0x656e696c,0x7269645f,0x69746365,
0x00006576,0x00080004,0x475f4c47,0x4c474f4f,
0x6e695f45,0x64756c63,0x69645f65,0x74636572,
0x00657669,0x00040005,0x00000004,0x6e69616d,
0x00000000,0x00040005,0x00000014,0x61746144,
0x00000000,0x00050006,0x00000014,0x00000000,
0x6d656c45,0x00746e65,0x00050005,0x00000016,
0x7542794d,0x72656666,0x00006e49,0x00050006,
0x00000016,0x00000000,0x74616440,0x00000061,
0x00050005,0x00000018,0x7542794d,0x72656666,
0x00006e49,0x00050005,0x00000021,0x7542794d,
0x72656666,0x0074754f,0x00030005,0x0000002c,
0x00424752,0x00040006,0x0000002c,0x00000000,
0x00000072,0x00040006,0x0000002c,0x00000001,
0x00000067,0x00040006,0x0000002c,0x00000002,
0x00000062,0x00050005,0x0000002d,0x6f43794d,
0x6174736e,0x0073746e,0x00060006,0x0000002d,
0x00000000,0x726f6658,0x74614d6d,0x00786972,
0x00050006,0x0000002d,0x00000001,0x6c616353,
0x00000065,0x00040006,0x0000002d,0x00000002,
0x00626752,0x00040006,0x0000002d,0x00000003,
0x00000074,0x00040006,0x0000002d,0x00000004,
0x00007675,0x00050005,0x0000002f,0x6f43794d,
0x6174736e,0x0073746e,0x00050005,0x00000053,
0x6554794d,0x72757478,0x00000065,0x00050005,
0x00000057,0x6153794d,0x656c706d,0x00000072,
0x00060005,0x00000086,0x75706e69,0x6f502e74,
0x69746973,0x00006e6f,0x00060005,0x0000008a,
0x75706e69,0x6f4e2e74,0x6c616d72,0x00000000,
0x00050005,0x0000008d,0x75706e69,0x6f432e74,
0x00726f6c,0x00050005,0x00000091,0x75706e69,
0x6c412e74,0x00616870,0x00060005,0x00000094,
0x75706e69,0x63532e74,0x6e696c61,0x00000067,
0x00060005,0x00000098,0x75706e69,0x65542e74,
0x6f6f4378,0x00306472,0x00060005,0x0000009b,
0x75706e69,0x65542e74,0x6f6f4378,0x00316472,
0x00060005,0x0000009e,0x75706e69,0x65542e74,
0x6f6f4378,0x00326472,0x00090005,0x000000a6,
0x746e6540,0x6f507972,0x4f746e69,0x75707475,
0x436f2e74,0x726f6c6f,0x00000030,0x00090005,
0x000000a9,0x746e6540,0x6f507972,0x4f746e69,
0x75707475,0x436f2e74,0x726f6c6f,0x00000031,
0x00090005,0x000000ac,0x746e6540,0x6f507972,
0x4f746e69,0x75707475,0x436f2e74,0x726f6c6f,
0x00000032,0x00090005,0x000000af,0x746e6540,
0x6f507972,0x4f746e69,0x75707475,0x436f2e74,
0x726f6c6f,0x00000033,0x00090005,0x000000b2,
0x746e6540,0x6f507972,0x4f746e69,0x75707475,
0x436f2e74,0x726f6c6f,0x00000034,0x00090005,
0x000000b5,0x746e6540,0x6f507972,0x4f746e69,
0x75707475,0x436f2e74,0x726f6c6f,0x00000035,
0x00090005,0x000000b8,0x746e6540,0x6f507972,
0x4f746e69,0x75707475,0x436f2e74,0x726f6c6f,
0x00000036,0x00090005,0x000000bb,0x746e6540,
0x6f507972,0x4f746e69,0x75707475,0x436f2e74,
0x726f6c6f,0x00000037,0x00050048,0x00000014,
0x00000000,0x00000023,0x00000000,0x00040047,
0x00000015,0x00000006,0x00000010,0x00050048,
0x00000016,0x00000000,0x00000023,0x00000000,
0x00030047,0x00000016,0x00000003,0x00040047,
0x00000018,0x00000022,0x00000002,0x00040047,
0x00000018,0x00000021,0x00000003,0x00040047,
0x00000021,0x00000022,0x00000002,0x00040047,
0x00000021,0x00000021,0x00000004,0x00050048,
0x0000002c,0x00000000,0x00000023,0x00000000,
0x00050048,0x0000002c,0x00000001,0x00000023,
0x00000004,0x00050048,0x0000002c,0x00000002,
0x00000023,0x00000008,0x00040048,0x0000002d,
0x00000000,0x00000004,0x00050048,0x0000002d,
0x00000000,0x00000023,0x00000000,0x00050048,
0x0000002d,0x00000000,0x00000007,0x00000010,
0x00050048,0x0000002d,0x00000001,0x00000023,
0x00000040,0x00050048,0x0000002d,0x00000002,
0x00000023,0x00000050,0x00050048,0x0000002d,
0x00000003,0x00000023,0x00000060,0x00050048,
0x0000002d,0x00000004,0x00000023,0x00000064,
0x00030047,0x0000002d,0x00000002,0x00040047,
0x0000002f,0x00000022,0x00000002,0x00040047,
0x0000002f,0x00000021,0x00000002,0x00040047,
0x00000053,0x00000022,0x00000000,0x00040047,
0x00000053,0x00000021,0x00000000,0x00040047,
0x00000057,0x00000022,0x00000001,0x00040047,
0x00000057,0x00000021,0x00000001,0x00040047,
0x00000086,0x0000000b,0x0000000f,0x00040047,
0x0000008a,0x0000001e,0x00000000,0x00040047,
0x0000008d,0x0000001e,0x00000001,0x00040047,
0x00000091,0x0000001e,0x00000002,0x00040047,
0x00000094,0x0000001e,0x00000003,0x00040047,
0x00000098,0x0000001e,0x00000004,0x00040047,
0x0000009b,0x0000001e,0x00000005,0x00040047,
0x0000009e,0x0000001e,0x00000006,0x00040047,
0x000000a6,0x0000001e,0x00000000,0x00040047,
0x000000a9,0x0000001e,0x00000001,0x00040047,
0x000000ac,0x0000001e,0x00000002,0x00040047,
0x000000af,0x0000001e,0x00000003,0x00040047,
0x000000b2,0x0000001e,0x00000004,0x00040047,
0x000000b5,0x0000001e,0x00000005,0x00040047,
0x000000b8,0x0000001e,0x00000006,0x00040047,
0x000000bb,0x0000001e,0x00000007,0x00020013,
0x00000002,0x00030021,0x00000003,0x00000002,
0x00030016,0x00000006,0x00000020,0x00040017,
0x00000007,0x00000006,0x00000004,0x00040017,
0x00000008,0x00000006,0x00000003,0x00040017,
0x00000009,0x00000006,0x00000002,0x0003001e,
0x00000014,0x00000007,0x0003001d,0x00000015,
0x00000014,0x0003001e,0x00000016,0x00000015,
0x00040020,0x00000017,0x00000002,0x00000016,
0x0004003b,0x00000017,0x00000018,0x00000002,
0x00040015,0x00000019,0x00000020,0x00000001,
0x0004002b,0x00000019,0x0000001a,0x00000000,
0x00040020,0x0000001b,0x00000002,0x00000014,
0x0004003b,0x00000017,0x00000021,0x00000002,
0x00040020,0x00000025,0x00000002,0x00000007,
0x00040018,0x0000002b,0x00000007,0x00000004,
0x0005001e,0x0000002c,0x00000006,0x00000006,
0x00000006,0x0007001e,0x0000002d,0x0000002b,
0x00000008,0x0000002c,0x00000006,0x00000009,
0x00040020,0x0000002e,0x00000002,0x0000002d,
0x0004003b,0x0000002e,0x0000002f,0x00000002,
0x00040020,0x00000030,0x00000002,0x0000002b,
0x0004002b,0x00000019,0x00000035,0x00000001,
0x0004002b,0x00000006,0x00000039,0x3f800000,
0x00040020,0x0000003e,0x00000002,0x00000008,
0x0004002b,0x00000006,0x00000041,0x00000000,
0x00090019,0x00000051,0x00000006,0x00000001,
0x00000000,0x00000000,0x00000000,0x00000001,
0x00000000,0x00040020,0x00000052,0x00000000,
0x00000051,0x0004003b,0x00000052,0x00000053,
0x00000000,0x0002001a,0x00000055,0x00040020,
0x00000056,0x00000000,0x00000055,0x0004003b,
0x00000056,0x00000057,0x00000000,0x0003001b,
0x00000059,0x00000051,0x00040020,0x00000085,
0x00000001,0x00000007,0x0004003b,0x00000085,
0x00000086,0x00000001,0x00040020,0x00000089,
0x00000001,0x00000008,0x0004003b,0x00000089,
0x0000008a,0x00000001,0x0004003b,0x00000089,
0x0000008d,0x00000001,0x00040020,0x00000090,
0x00000001,0x00000006,0x0004003b,0x00000090,
0x00000091,0x00000001,0x0004003b,0x00000085,
0x00000094,0x00000001,0x00040020,0x00000097,
0x00000001,0x00000009,0x0004003b,0x00000097,
0x00000098,0x00000001,0x0004003b,0x00000097,
0x0000009b,0x00000001,0x0004003b,0x00000097,
0x0000009e,0x00000001,0x00040020,0x000000a5,
0x00000003,0x00000007,0x0004003b,0x000000a5,
0x000000a6,0x00000003,0x0004003b,0x000000a5,
0x000000a9,0x00000003,0x0004003b,0x000000a5,
0x000000ac,0x00000003,0x0004003b,0x000000a5,
0x000000af,0x00000003,0x0004003b,0x000000a5,
0x000000b2,0x00000003,0x0004003b,0x000000a5,
0x000000b5,0x00000003,0x0004003b,0x000000a5,
0x000000b8,0x00000003,0x0004003b,0x000000a5,
0x000000bb,0x00000003,0x00050036,0x00000002,
0x00000004,0x00000000,0x00000003,0x000200f8,
0x00000005,0x0004003d,0x00000007,0x00000087,
0x00000086,0x0004003d,0x00000008,0x0000008b,
0x0000008a,0x0004003d,0x00000008,0x0000008e,
0x0000008d,0x0004003d,0x00000006,0x00000092,
0x00000091,0x0004003d,0x00000007,0x00000095,
0x00000094,0x0004003d,0x00000009,0x00000099,
0x00000098,0x0004003d,0x00000009,0x0000009c,
0x0000009b,0x0004003d,0x00000009,0x0000009f,
0x0000009e,0x00060041,0x0000001b,0x000000c1,
0x00000018,0x0000001a,0x0000001a,0x0004003d,
0x00000014,0x000000c2,0x000000c1,0x00050051,
0x00000007,0x000000c3,0x000000c2,0x00000000,
0x00060041,0x0000001b,0x000000c6,0x00000021,
0x0000001a,0x0000001a,0x00050041,0x00000025,
0x000000c8,0x000000c6,0x0000001a,0x0003003e,
0x000000c8,0x000000c3,0x00050041,0x00000030,
0x000000cb,0x0000002f,0x0000001a,0x0004003d,
0x0000002b,0x000000cc,0x000000cb,0x00050090,
0x00000007,0x000000cd,0x00000087,0x000000cc,
0x00050051,0x00000006,0x000000d1,0x0000008b,
0x00000000,0x00050051,0x00000006,0x000000d2,
0x0000008b,0x00000001,0x00050051,0x00000006,
0x000000d3,0x0000008b,0x00000002,0x00070050,
0x00000007,0x000000d4,0x000000d1,0x000000d2,
0x000000d3,0x00000039,0x00050041,0x0000003e,
0x000000d5,0x0000002f,0x00000035,0x0004003d,
0x00000008,0x000000d6,0x000000d5,0x00050051,
0x00000006,0x000000d7,0x000000d6,0x00000000,
0x00050051,0x00000006,0x000000d8,0x000000d6,
0x00000001,0x00050051,0x00000006,0x000000d9,
0x000000d6,0x00000002,0x00070050,0x00000007,
0x000000da,0x000000d7,0x000000d8,0x000000d9,
0x00000041,0x00050081,0x00000007,0x000000db,
0x000000d4,0x000000da,0x00050051,0x00000006,
0x000000df,0x0000008e,0x00000000,0x00050051,
0x00000006,0x000000e0,0x0000008e,0x00000001,
0x00050051,0x00000006,0x000000e1,0x0000008e,
0x00000002,0x00070050,0x00000007,0x000000e2,
0x000000df,0x000000e0,0x000000e1,0x00000039,
0x0004003d,0x00000051,0x000000e4,0x00000053,
0x0004003d,0x00000055,0x000000e5,0x00000057,
0x00050056,0x00000059,0x000000e6,0x000000e4,
0x000000e5,0x00050057,0x00000007,0x000000e9,
0x000000e6,0x00000099,0x0008004f,0x00000008,
0x000000ea,0x000000e9,0x000000e9,0x00000000,
0x00000001,0x00000002,0x00050051,0x00000006,
0x000000ed,0x000000ea,0x00000000,0x00050051,
0x00000006,0x000000ee,0x000000ea,0x00000001,
0x00050051,0x00000006,0x000000ef,0x000000ea,
0x00000002,0x00070050,0x00000007,0x000000f0,
0x000000ed,0x000000ee,0x000000ef,0x00000092,
0x00050051,0x00000006,0x000000f7,0x00000099,
0x00000000,0x00050051,0x00000006,0x000000f8,
0x00000099,0x00000001,0x00070050,0x00000007,
0x000000f9,0x000000f7,0x000000f8,0x00000041,
0x00000041,0x00050051,0x00000006,0x000000fd,
0x0000009c,0x00000000,0x00050051,0x00000006,
0x000000fe,0x0000009c,0x00000001,0x00070050,
0x00000007,0x000000ff,0x000000fd,0x000000fe,
0x00000041,0x00000041,0x00050051,0x00000006,
0x00000103,0x0000009f,0x00000000,0x00050051,
0x00000006,0x00000104,0x0000009f,0x00000001,
0x00070050,0x00000007,0x00000105,0x00000103,
0x00000104,0x00000041,0x00000041,0x0003003e,
0x000000a6,0x000000cd,0x0003003e,0x000000a9,
0x000000db,0x0003003e,0x000000ac,0x000000e2,
0x0003003e,0x000000af,0x000000f0,0x0003003e,
0x000000b2,0x00000095,0x0003003e,0x000000b5,
0x000000f9,0x0003003e,0x000000b8,0x000000ff,
0x0003003e,0x000000bb,0x00000105,0x000100fd,
0x00010038
};

/* SPIRV Disassembly

; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 2
; Bound: 241
; Schema: 0
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %4 "main" %117 %121 %124 %128 %131 %135 %138 %141 %149 %152 %155 %158 %161 %164 %167 %170
               OpExecutionMode %4 OriginUpperLeft
               OpSource HLSL 500
               OpName %4 "main"
               OpName %10 "PSInput"
               OpMemberName %10 0 "Position"
               OpMemberName %10 1 "Normal"
               OpMemberName %10 2 "Color"
               OpMemberName %10 3 "Alpha"
               OpMemberName %10 4 "Scaling"
               OpMemberName %10 5 "TexCoord0"
               OpMemberName %10 6 "TexCoord1"
               OpMemberName %10 7 "TexCoord2"
               OpName %12 "PSOutput"
               OpMemberName %12 0 "oColor0"
               OpMemberName %12 1 "oColor1"
               OpMemberName %12 2 "oColor2"
               OpMemberName %12 3 "oColor3"
               OpMemberName %12 4 "oColor4"
               OpMemberName %12 5 "oColor5"
               OpMemberName %12 6 "oColor6"
               OpMemberName %12 7 "oColor7"
               OpName %17 "Data"
               OpMemberName %17 0 "Element"
               OpName %20 "Data"
               OpMemberName %20 0 "Element"
               OpName %22 "MyBufferIn"
               OpMemberName %22 0 "@data"
               OpName %24 "MyBufferIn"
               OpName %33 "MyBufferOut"
               OpName %44 "RGB"
               OpMemberName %44 0 "r"
               OpMemberName %44 1 "g"
               OpMemberName %44 2 "b"
               OpName %45 "MyConstants"
               OpMemberName %45 0 "XformMatrix"
               OpMemberName %45 1 "Scale"
               OpMemberName %45 2 "Rgb"
               OpMemberName %45 3 "t"
               OpMemberName %45 4 "uv"
               OpName %47 "MyConstants"
               OpName %117 "input.Position"
               OpName %121 "input.Normal"
               OpName %124 "input.Color"
               OpName %128 "input.Alpha"
               OpName %131 "input.Scaling"
               OpName %135 "input.TexCoord0"
               OpName %138 "input.TexCoord1"
               OpName %141 "input.TexCoord2"
               OpName %149 "@entryPointOutput.oColor0"
               OpName %152 "@entryPointOutput.oColor1"
               OpName %155 "@entryPointOutput.oColor2"
               OpName %158 "@entryPointOutput.oColor3"
               OpName %161 "@entryPointOutput.oColor4"
               OpName %164 "@entryPointOutput.oColor5"
               OpName %167 "@entryPointOutput.oColor6"
               OpName %170 "@entryPointOutput.oColor7"
               OpMemberDecorate %20 0 Offset 0
               OpDecorate %21 ArrayStride 16
               OpMemberDecorate %22 0 Offset 0
               OpDecorate %22 BufferBlock
               OpDecorate %24 DescriptorSet 2
               OpDecorate %24 Binding 3
               OpDecorate %33 DescriptorSet 2
               OpDecorate %33 Binding 4
               OpMemberDecorate %44 0 Offset 0
               OpMemberDecorate %44 1 Offset 4
               OpMemberDecorate %44 2 Offset 8
               OpMemberDecorate %45 0 RowMajor
               OpMemberDecorate %45 0 Offset 0
               OpMemberDecorate %45 0 MatrixStride 16
               OpMemberDecorate %45 1 Offset 64
               OpMemberDecorate %45 2 Offset 80
               OpMemberDecorate %45 3 Offset 96
               OpMemberDecorate %45 4 Offset 100
               OpDecorate %45 Block
               OpDecorate %47 DescriptorSet 2
               OpDecorate %47 Binding 2
               OpDecorate %117 BuiltIn FragCoord
               OpDecorate %121 Location 0
               OpDecorate %124 Location 1
               OpDecorate %128 Location 2
               OpDecorate %131 Location 3
               OpDecorate %135 Location 4
               OpDecorate %138 Location 5
               OpDecorate %141 Location 6
               OpDecorate %149 Location 0
               OpDecorate %152 Location 1
               OpDecorate %155 Location 2
               OpDecorate %158 Location 3
               OpDecorate %161 Location 4
               OpDecorate %164 Location 5
               OpDecorate %167 Location 6
               OpDecorate %170 Location 7
          %2 = OpTypeVoid
          %3 = OpTypeFunction %2
          %6 = OpTypeFloat 32
          %7 = OpTypeVector %6 4
          %8 = OpTypeVector %6 3
          %9 = OpTypeVector %6 2
         %10 = OpTypeStruct %7 %8 %8 %6 %7 %9 %9 %9
         %12 = OpTypeStruct %7 %7 %7 %7 %7 %7 %7 %7
         %17 = OpTypeStruct %7
         %20 = OpTypeStruct %7
         %21 = OpTypeRuntimeArray %20
         %22 = OpTypeStruct %21
         %23 = OpTypePointer Uniform %22
         %24 = OpVariable %23 Uniform
         %25 = OpTypeInt 32 1
         %26 = OpConstant %25 0
         %27 = OpTypePointer Uniform %20
         %33 = OpVariable %23 Uniform
         %37 = OpTypePointer Uniform %7
         %43 = OpTypeMatrix %7 4
         %44 = OpTypeStruct %6 %6 %6
         %45 = OpTypeStruct %43 %8 %44 %6 %9
         %46 = OpTypePointer Uniform %45
         %47 = OpVariable %46 Uniform
         %48 = OpTypePointer Uniform %43
         %53 = OpConstant %25 1
         %57 = OpConstant %6 1
         %62 = OpTypePointer Uniform %8
         %65 = OpConstant %6 0
        %116 = OpTypePointer Input %7
        %117 = OpVariable %116 Input
        %120 = OpTypePointer Input %8
        %121 = OpVariable %120 Input
        %124 = OpVariable %120 Input
        %127 = OpTypePointer Input %6
        %128 = OpVariable %127 Input
        %131 = OpVariable %116 Input
        %134 = OpTypePointer Input %9
        %135 = OpVariable %134 Input
        %138 = OpVariable %134 Input
        %141 = OpVariable %134 Input
        %148 = OpTypePointer Output %7
        %149 = OpVariable %148 Output
        %152 = OpVariable %148 Output
        %155 = OpVariable %148 Output
        %158 = OpVariable %148 Output
        %161 = OpVariable %148 Output
        %164 = OpVariable %148 Output
        %167 = OpVariable %148 Output
        %170 = OpVariable %148 Output
          %4 = OpFunction %2 None %3
          %5 = OpLabel
        %118 = OpLoad %7 %117
        %122 = OpLoad %8 %121
        %125 = OpLoad %8 %124
        %129 = OpLoad %6 %128
        %132 = OpLoad %7 %131
        %136 = OpLoad %9 %135
        %139 = OpLoad %9 %138
        %142 = OpLoad %9 %141
        %182 = OpAccessChain %27 %24 %26 %26
        %183 = OpLoad %20 %182
        %184 = OpCompositeExtract %7 %183 0
        %187 = OpAccessChain %27 %33 %26 %26
        %189 = OpAccessChain %37 %187 %26
               OpStore %189 %184
        %192 = OpAccessChain %48 %47 %26
        %193 = OpLoad %43 %192
        %194 = OpVectorTimesMatrix %7 %118 %193
        %198 = OpCompositeExtract %6 %122 0
        %199 = OpCompositeExtract %6 %122 1
        %200 = OpCompositeExtract %6 %122 2
        %201 = OpCompositeConstruct %7 %198 %199 %200 %57
        %202 = OpAccessChain %62 %47 %53
        %203 = OpLoad %8 %202
        %204 = OpCompositeExtract %6 %203 0
        %205 = OpCompositeExtract %6 %203 1
        %206 = OpCompositeExtract %6 %203 2
        %207 = OpCompositeConstruct %7 %204 %205 %206 %65
        %208 = OpFAdd %7 %201 %207
        %212 = OpCompositeExtract %6 %125 0
        %213 = OpCompositeExtract %6 %125 1
        %214 = OpCompositeExtract %6 %125 2
        %215 = OpCompositeConstruct %7 %212 %213 %214 %57
        %219 = OpCompositeConstruct %7 %65 %65 %65 %129
        %226 = OpCompositeExtract %6 %136 0
        %227 = OpCompositeExtract %6 %136 1
        %228 = OpCompositeConstruct %7 %226 %227 %65 %65
        %232 = OpCompositeExtract %6 %139 0
        %233 = OpCompositeExtract %6 %139 1
        %234 = OpCompositeConstruct %7 %232 %233 %65 %65
        %238 = OpCompositeExtract %6 %142 0
        %239 = OpCompositeExtract %6 %142 1
        %240 = OpCompositeConstruct %7 %238 %239 %65 %65
               OpStore %149 %194
               OpStore %152 %208
               OpStore %155 %215
               OpStore %158 %219
               OpStore %161 %132
               OpStore %164 %228
               OpStore %167 %234
               OpStore %170 %240
               OpReturn
               OpFunctionEnd

*/

#endif // SAMPLE_SPV_H
