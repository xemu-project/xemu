// Run: %dxc -E main -T cs_6_4 -fspv-target-env=vulkan1.2

// CHECK: OpCapability RayTracingKHR

// CHECK: %accelerationStructureNV = OpTypeAccelerationStructureKHR

RaytracingAccelerationStructure test_bvh;

[numthreads(1, 1, 1)]
void main() {}
