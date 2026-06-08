/*
Build with DXC using -fspv-reflect to enable counter buffers

   dxc -spirv -fspv-reflect -T ps_5_0 -Fo counter_buffers.spv counter_buffers.hlsl

*/

struct Data {
  float4 f4;
  int    i;
};

ConsumeStructuredBuffer<Data> MyBufferIn : register(u3, space2);
AppendStructuredBuffer<Data> MyBufferOut : register(u4, space2);

float4  main(float4 PosCS : SV_Position) : SV_TARGET0
{
  Data val = MyBufferIn.Consume();
  MyBufferOut.Append(val);

  return val.f4;
}