
struct SepNormal {
  float x;
  float y;
  float z;
};

struct Rgb {
  float r[5];
  float g[5];
  float b[5];
};

struct Uv {
  float u;
  float v;
};

struct Data {
  float3      Position;
  SepNormal   Normal;
  Rgb         Colors[3];
  Uv          TexCoords;
  float       Scales[3];
  uint        Id;
};

StructuredBuffer<Data> MyData : register(t0);

float4 main() : SV_Target
{
  return float4(MyData[0].Position, 1);
}