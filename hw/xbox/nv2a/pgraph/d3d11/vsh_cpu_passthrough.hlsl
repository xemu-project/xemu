// Offline-only vertex shader for CPU-transformed programmable NV2A VSH data.
// Runtime code consumes generated DXBC and never invokes D3DCompile.

struct VSInput {
    float4 position : POSITION;
    float4 d0 : COLOR0;
    float4 d1 : COLOR1;
    float4 b0 : COLOR2;
    float4 b1 : COLOR3;
    float4 fog : FOG0;
    float4 pts : PSIZE0;
    float4 t0 : TEXCOORD0;
    float4 t1 : TEXCOORD1;
    float4 t2 : TEXCOORD2;
    float4 t3 : TEXCOORD3;
};

struct VSOutput {
    float4 position : SV_Position;
    float4 d0 : COLOR0;
    float4 d1 : COLOR1;
    float4 b0 : COLOR2;
    float4 b1 : COLOR3;
    float4 fog : FOG0;
    float4 pts : PSIZE0;
    float4 t0 : TEXCOORD0;
    float4 t1 : TEXCOORD1;
    float4 t2 : TEXCOORD2;
    float4 t3 : TEXCOORD3;
};

VSOutput vs_main(VSInput input)
{
    VSOutput output;
    output.position = input.position;
    output.d0 = input.d0;
    output.d1 = input.d1;
    output.b0 = input.b0;
    output.b1 = input.b1;
    output.fog = input.fog;
    output.pts = input.pts;
    output.t0 = input.t0;
    output.t1 = input.t1;
    output.t2 = input.t2;
    output.t3 = input.t3;
    return output;
}
