
struct S {
  float4 a;
};

RWByteAddressBuffer buf;

float4 main() : SV_TARGET
{
  S s;
  buf.Store4(s, uint4(42,43,44,45));
  return float4(0);
}
