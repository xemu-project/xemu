StructuredBuffer<float> a;
RWStructuredBuffer<float> b;
AppendStructuredBuffer<float> c;
ConsumeStructuredBuffer<float> d;
Texture1D<float> e;
Texture2D<float> f;
Texture3D<float> g;
TextureCube<float> h;
Texture1DArray<float> i;
Texture2DArray<float> j;
Texture2DMS<float> k;
Texture2DMSArray<float> l;
TextureCubeArray<float> m;
RWTexture1D<float> n;
RWTexture2D<float> o;
RWTexture3D<float> p;
RWTexture1DArray<float> q;
RWTexture2DArray<float> r;
Buffer<float> s;
RWBuffer<float> t;
Texture2DMSArray<float4, 64> u;
const Texture2DMSArray<float4, 64> v;
Texture1D t1;
Texture2D t2;

Texture1D<float> eArr[5];
Texture2D<float> fArr[5];
Texture3D<float> gArr[5];
TextureCube<float> hArr[5];
Texture1DArray<float> iArr[5];
Texture2DArray<float> jArr[5];
Texture2DMS<float> kArr[5];
Texture2DMSArray<float> lArr[5];
TextureCubeArray<float> mArr[5];
RWTexture1D<float> nArr[5];
RWTexture2D<float> oArr[5];
RWTexture3D<float> pArr[5];
RWTexture1DArray<float> qArr[5];
RWTexture2DArray<float> rArr[5];
Buffer<float> sArr[5];
RWBuffer<float> tArr[5];

cbuffer MyCBuffer { float x; };

tbuffer MyTBuffer { float y; };

ByteAddressBuffer bab;

RWByteAddressBuffer rwbab;

RaytracingAccelerationStructure rs;

struct S {
    float  f1;
    float3 f2;
};

ConstantBuffer<S> cb;

TextureBuffer<S> tb;

void main(){
}
