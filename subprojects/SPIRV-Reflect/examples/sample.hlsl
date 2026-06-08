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