
struct S {
  float4 a;
};

RWByteAddressBuffer buf;

float4 main() : SV_TARGET
{
  S s;
  buf.Store2(s, uint2(42, 43));
  return float4(0);
}
