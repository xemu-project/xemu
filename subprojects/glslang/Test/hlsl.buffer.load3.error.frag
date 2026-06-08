
struct S {
  float4 a;
};

RWByteAddressBuffer buf;

float4 main() : SV_TARGET
{
  S s;
  uint3 v = buf.Load3(s);
  return float4(v, 0);
}
