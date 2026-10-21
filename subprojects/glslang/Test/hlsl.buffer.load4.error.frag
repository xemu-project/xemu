
struct S {
  float4 a;
};

RWByteAddressBuffer buf;

float4 main() : SV_TARGET
{
  S s;
  uint4 v = buf.Load4(s);
  return float4(v);
}
