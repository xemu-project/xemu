
struct S {
  float4 a;
};

RWByteAddressBuffer buf;

float4 main() : SV_TARGET
{
  S s;
  uint2 v = buf.Load2(s);
  return float4(v, 0, 0);
}
