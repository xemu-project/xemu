
struct OmniNormalStength {
  float   Front;
  float   Back;
  float   Top;
  float   Bottom;
  float   Left;
  float   Right;
};

struct ClothProperties {
  float3            NormalAdjust;
  OmniNormalStength Strengths;
  uint              Type;
};

struct AuxData {
  ClothProperties ClothProperties[8];
  float3          ClothColors[8];
  float           Scales[8];
  int             EnableBitMask;
};

struct MaterialData {
  float3  Color;
  float   Specular;
  float   Diffuse;
  AuxData AuxDatArray[10];
};

struct Constants {
  MaterialData  Material[2][2][3];  
  float4x4      ModelMatrix;
  float4x4      ProjectionMatrix;
  float         Time;
  float3        Scale;  
  float2        UvOffsets[12];
  //MaterialData  Material[12];
  bool          EnableTarget;
};

ConstantBuffer<Constants> MyConstants : register(b0);

float4 main(float4 pos : POSITION) : SV_Position
{
  return float4(MyConstants.Time, 0, 0, 0);
}