// dxc -spirv -fspv-reflect -T cs_6_0 -E csmain -fspv-target-env=vulkan1.2
uint g_global;

struct MaterialData_t {
	float4 g_vTest;
	float2 g_vTest2;
	float3 g_vTest3;
	uint g_tTexture1;
	uint g_tTexture2;
	bool g_bTest1;
	bool g_bTest2;
};

static MaterialData_t _g_MaterialData;

ByteAddressBuffer g_MaterialData : register (t4 , space1);
RWStructuredBuffer<uint2> Output : register(u1);

[numthreads(1, 1, 1)]
void csmain(uint3 tid : SV_DispatchThreadID) {
    uint2 a = g_MaterialData.Load2( tid.x + g_global );
    uint b = g_MaterialData.Load( tid.x + g_global );
    uint2 c = g_MaterialData.Load2(4);
	Output[tid.x] = a * uint2(b, b) * c;
}