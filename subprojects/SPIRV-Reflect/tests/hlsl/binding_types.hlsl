/*
Build with DXC using -O0 to preserve unused types:

   dxc -spirv -O0 -T ps_5_0 -Fo binding_types.spv binding_types.hlsl

*/

cbuffer MyCBuffer {
  float x;
};

struct Data { float x; };
ConstantBuffer<Data>            MyConstantBuffer;

Texture1D                       MyTexture1D;
Texture2D                       MyTexture2D;
Texture3D                       MyTexture3D;

Texture1DArray                  MyTexture1DArray;
Texture2DArray                  MyTexture2DArray;

RWTexture1D<float4>             MyRWTexture1D;
RWTexture2D<float4>             MyRWTexture2D;
RWTexture3D<float4>             MyRWTexture3D;

RWTexture1DArray<float4>        MyRWTexture1DArray;
RWTexture2DArray<float4>        MyRWTexture2DArray;

Texture2DMS<float4>             MyTexture2DMS;
Texture2DMSArray<float4>        MyTexture2DMSArray;

TextureCube<float4>             MyTextureCube;
TextureCubeArray<float4>        MyTextureCubeArray;

tbuffer MyTBuffer {
  float q;
};

struct Data2 {
  float4 x;
};

TextureBuffer<Data2>            MyTextureBuffer;

Buffer                          MyBuffer;
RWBuffer<float4>                MyRWBuffer;

StructuredBuffer<float>         MyStructuredBuffer;
RWStructuredBuffer<float>       MyRWStructuredBuffer;

AppendStructuredBuffer<float>   MyAppendStructuredBuffer;
ConsumeStructuredBuffer<float>  MyConsumeStructuredBuffer;

ByteAddressBuffer               MyByteAddressBuffer;
RWByteAddressBuffer             MyRWByteAddressBuffer;

float4 main(float4 P : SV_POSITION) : SV_TARGET
{
  return float4(0, 0, 0, 0);
}
