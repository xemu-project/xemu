// Run: %dxc -spirv -fspv-target-env=vulkan1.3 -E main -T as_6_7 -fspv-extension=SPV_EXT_mesh_shader

struct PayLoad
{
    uint data[8];
};

groupshared PayLoad s_payload;

[numthreads(8, 1, 1)]
void main(
    in uint3 dispatchThreadId : SV_DispatchThreadID,
    in uint3 groupThreadId : SV_GroupThreadID,
    in uint threadIndex : SV_GroupIndex,
    in uint3 groupId : SV_GroupID)
{
    s_payload.data[groupId.x] = 1;
    
    DispatchMesh(1, 1, 1, s_payload);
}
