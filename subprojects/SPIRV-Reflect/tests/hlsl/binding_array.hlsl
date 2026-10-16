
//SamplerState          MySampler[4][3][2][1] : register(s0);
SamplerState          MySampler[6] : register(s0);
Texture2D             MyTexture[2] : register(t8);

float4 main(float4 sv_pos : SV_POSITION) : SV_TARGET {
  float4 ret0 = MyTexture[0].Sample(MySampler[0], float2(0, 0));
  float4 ret1 = MyTexture[1].Sample(MySampler[3], float2(0, 0));
  return ret0 + ret1;
}