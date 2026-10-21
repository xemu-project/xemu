
struct S {
  float4 a;
};

RWByteAddressBuffer buf;

float4 main() : SV_TARGET
{
  S s;
  uint v = buf.Load(s);
  return float4(v, 0, 0, 0);
}
