
struct Constants_t {
  float3        Scale;  
  float         Time;
  float2        UvOffsets;
};

[[vk::push_constant]] Constants_t g_PushConstants;

float4 main(float4 pos : POSITION) : SV_Position
{
  return float4(g_PushConstants.Time, 0, 0, 0);
}
