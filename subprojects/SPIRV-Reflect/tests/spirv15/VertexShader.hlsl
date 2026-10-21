Texture2D Tex0;

float4 vsmain(float4 P : POSITION, uint instanceId :SV_InstanceID) : SV_Position
{
    return P + Tex0[uint2(instanceId % 16, instanceId / 16)];
}