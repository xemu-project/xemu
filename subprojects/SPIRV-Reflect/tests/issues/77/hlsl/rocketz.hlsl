/* 

Compile command:
dxc.exe -T vs_6_0 -spirv -E Test_VS -Fo rocketz.spv rocketz.hlsl

*/

struct S_cbPerObjectBones {
    float4 v4Bones[4095];
};

shared cbuffer _cbPerObjectBones : register(b4)
{
    S_cbPerObjectBones cbPerObjectBones;
};

struct v2f {
    float4 HPosition : SV_POSITION;
    float4 Position : POSITION;
};

struct a2v_StandardWeighted {
    float4 Position : POSITION0;
    int4 Indices : BLENDINDICES;
};

float3x4 FailFunc(int4 i, float4 v4Bones[4095])
{
    float4 mRow1 = v4Bones[i.x + 0];
    return float3x4(mRow1, mRow1, mRow1);
}

v2f Test_VS(a2v_StandardWeighted input)
{
    v2f OUT = (v2f)0;
    float4 inputPosition = float4(input.Position.xyz, 1);
    int4 i = input.Indices;
    float3x4 mMatrix = FailFunc(i, cbPerObjectBones.v4Bones);
    float4 v4Position = float4(mul(mMatrix, inputPosition), 1);
    OUT.Position = v4Position;
    return OUT;
}