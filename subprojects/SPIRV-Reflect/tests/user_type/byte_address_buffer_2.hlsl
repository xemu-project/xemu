// dxc -spirv -fspv-reflect -T cs_6_0 -E csmain -fspv-target-env=vulkan1.2
struct MaterialData_t {
	uint g_tTexture0;
	uint g_tTexture1;
	uint g_tTexture2;
	uint g_tTexture3;
};

static MaterialData_t _g_MaterialData;

ByteAddressBuffer g_MaterialData : register (t4 , space1);
RWStructuredBuffer<uint2> Output : register(u1);

[numthreads(1, 1, 1)]
void csmain(uint3 tid : SV_DispatchThreadID) {
    uint2 a = g_MaterialData.Load2(0);
    uint b = g_MaterialData.Load(1);
	Output[tid.x] = a * uint2(b, b);
}