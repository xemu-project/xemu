// Compile: slangc as_heap.slang -target spirv -capability spvDescriptorHeapEXT -capability SPV_KHR_ray_query -capability rayquery -entry computeMain -stage compute -o as_heap.comp.spv
//
// Test fixture for SPV_EXT_descriptor_heap + acceleration-structure reflection.
// Exercises the AS classification path through the resource heap, plus a
// storage-image heap slot for the output so the access chain has somewhere
// to write.
//
// Coverage:
//   resource_heap_accesses:
//     - VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR (RaytracingAccelerationStructure)
//     - VK_DESCRIPTOR_TYPE_STORAGE_IMAGE              (RWTexture2D)

struct PushBlock
{
  uint asIdx;    // RaytracingAccelerationStructure slot
  uint imgIdx;   // RWTexture2D slot
};
[[vk::push_constant]] ConstantBuffer<PushBlock> push;

[require(spvDescriptorHeapEXT, spvRayQueryKHR)]
[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 tid : SV_DispatchThreadID)
{
  RaytracingAccelerationStructure as = RaytracingAccelerationStructure.Handle(uint2(push.asIdx, 0));
  RWTexture2D<float4>             img = RWTexture2D<float4>.Handle(uint2(push.imgIdx, 0));

  RayDesc ray;
  ray.Origin    = float3(float2(tid.xy), 0.0);
  ray.Direction = float3(0.0, 0.0, 1.0);
  ray.TMin      = 0.0;
  ray.TMax      = 1000.0;

  RayQuery<RAY_FLAG_NONE> q;
  q.TraceRayInline(as, RAY_FLAG_NONE, 0xFFu, ray);
  q.Proceed();

  float4 color = (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
                   ? float4(1.0, 0.0, 0.0, 1.0)
                   : float4(0.0, 0.0, 0.0, 1.0);
  img[tid.xy] = color;
}
