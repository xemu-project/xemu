
struct S {
  float4 a;
};

RWByteAddressBuffer buf;

float4 main() : SV_TARGET
{
  S s;
  buf.InterlockedAdd(s, 42);
  return float4(0);
}
