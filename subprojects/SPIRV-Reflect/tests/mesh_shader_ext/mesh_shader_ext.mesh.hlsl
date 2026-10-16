// Run: %dxc -spirv -fspv-target-env=vulkan1.3 -E main -T ms_6_7 -fspv-extension=SPV_EXT_mesh_shader

struct PayLoad
{
    uint data[8];
};

struct Vertex
{
    float4 pos : SV_Position;
};

struct Primitive
{
    uint data : COLOR0;
};

[outputtopology("triangle")]
[numthreads(32, 1, 1)]
void main(in uint3 dispatchThreadId : SV_DispatchThreadID,
    in uint3 groupThreadId : SV_GroupThreadID,
    in uint threadIndex : SV_GroupIndex,
    in uint3 groupId : SV_GroupID,
    out vertices Vertex verts[64],
    out indices uint3 tris[124],
    out primitives Primitive prims[124])
{
    SetMeshOutputCounts(3, 1);
    
    verts[0].pos = float4(1.f, 0.f, 0.f, 1.f);
    verts[1].pos = float4(1.f, 1.f, 0.f, 1.f);
    verts[2].pos = float4(1.f, 0.f, 1.f, 1.f);
    
    prims[0].data = 1;
    tris[0] = uint3(0, 1, 2);
}