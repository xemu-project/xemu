// dxc -spirv -fspv-reflect -T lib_6_4 -fspv-target-env=vulkan1.2
struct MaterialData_t
{
	float4 g_vTest;
	float2 g_vTest2; // accessed
	float3 g_vTest3;
	uint g_tTexture1; // accessed
	uint g_tTexture2;
	bool g_bTest1; // accessed
	bool g_bTest2;
};

static MaterialData_t _g_MaterialData;

RWByteAddressBuffer g_MaterialData : register ( u4 , space1 );

struct Payload_t
{
	float2 vTest;
	bool bTest;
	uint tTest;
};

struct PayloadShadow_t
{
	float m_flVisibility;
};

[ shader ( "closesthit" ) ]
void ClosestHit0 ( inout Payload_t payload , in BuiltInTriangleIntersectionAttributes attrs )
{
	_g_MaterialData = g_MaterialData.Load<MaterialData_t>( InstanceIndex() );

    payload.vTest = _g_MaterialData.g_vTest2;
	payload.bTest = _g_MaterialData.g_bTest1;
	payload.tTest = _g_MaterialData.g_tTexture1;
}

[ shader ( "anyhit" ) ]
void AnyHit1 ( inout PayloadShadow_t payload , in BuiltInTriangleIntersectionAttributes attrs )
{
	_g_MaterialData = g_MaterialData.Load<MaterialData_t>( InstanceIndex() );

    {
        payload . m_flVisibility = 0.0 ;
        AcceptHitAndEndSearch ( ) ;
    }
}