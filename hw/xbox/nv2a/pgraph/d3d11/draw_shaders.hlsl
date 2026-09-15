// Offline-only shaders used by the D3D11 draw primitive's WARP test.
// Runtime code consumes generated DXBC; it never calls D3DCompile.

struct VSInput {
    float3 position : POSITION;
    float4 color : COLOR0;
};

struct VSOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
};

VSOutput vs_main(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0f);
    output.color = input.color;
    return output;
}

float4 ps_main(VSOutput input) : SV_Target
{
    return input.color;
}
