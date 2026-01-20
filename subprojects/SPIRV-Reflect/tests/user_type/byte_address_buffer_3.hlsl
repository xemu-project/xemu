// dxc -spirv -fspv-reflect -T lib_6_4 -fspv-target-env=vulkan1.2
struct MaterialData_t {
	float4 g_vTest0;
	float4 g_vTest1; // accessed
	float4 g_vTest2; // accessed twice, but only listed once
	float4 g_vTest3; // accessed
	float4 g_vTest4;
};

static MaterialData_t _g_MaterialData;

ByteAddressBuffer g_MaterialData : register ( t4 , space1 );

struct PayloadShadow_t {
	float m_flVisibility;
};

[ shader ( "anyhit" ) ]
void AnyHit1 ( inout PayloadShadow_t payload , in BuiltInTriangleIntersectionAttributes attrs ) {
	_g_MaterialData = g_MaterialData.Load<MaterialData_t>( InstanceIndex() );

    {
        payload . m_flVisibility = _g_MaterialData.g_vTest1.x + _g_MaterialData.g_vTest2.x;
        AcceptHitAndEndSearch ( ) ;
    }
}

[ shader ( "anyhit" ) ]
void AnyHit0 ( inout PayloadShadow_t payload , in BuiltInTriangleIntersectionAttributes attrs ) {
	_g_MaterialData = g_MaterialData.Load<MaterialData_t>( InstanceIndex() );

    {
        payload . m_flVisibility = _g_MaterialData.g_vTest2.x + _g_MaterialData.g_vTest3.x;
        AcceptHitAndEndSearch ( ) ;
    }
}