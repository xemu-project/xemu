
struct S {
  float4 a;
};

RWByteAddressBuffer buf;

float4 main() : SV_TARGET
{
  S s;
  buf.Store3(s, uint3(42,43,44));
  return float4(0);
}
