// Run: %dxc -E main -T cs_6_4 -fspv-target-env=vulkan1.2 -fspv-extension=SPV_NV_ray_tracing

// CHECK: OpCapability RayTracingNV
// CHECK: OpExtension "SPV_NV_ray_tracing"

// CHECK: %accelerationStructureNV = OpTypeAccelerationStructureKHR

RaytracingAccelerationStructure test_bvh;

[numthreads(1, 1, 1)]
void main() {}
