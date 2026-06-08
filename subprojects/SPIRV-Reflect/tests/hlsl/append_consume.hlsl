
struct Data {
  uint  rgba;
};

ConsumeStructuredBuffer<Data>  BufferIn : register(u0, space1);
AppendStructuredBuffer<Data>  BufferOut : register(u1, space2);

float4 main(float4 sv_pos : SV_Position) : SV_TARGET
{
  Data val = BufferIn.Consume();          
  BufferOut.Append(val);
  return float4(1, 0, 0, 0);
}