// Run: %dxc -T lib_6_3 -fspv-extension=SPV_NV_ray_tracing
// CHECK:  OpCapability RayTracingNV
// CHECK:  OpExtension "SPV_NV_ray_tracing"
// CHECK:  OpDecorate [[a:%\d+]] BuiltIn LaunchIdNV
// CHECK:  OpDecorate [[b:%\d+]] BuiltIn LaunchSizeNV

// CHECK:  OpTypePointer IncomingCallableDataNV %CallData
struct CallData
{
  float4 data;
};

[shader("callable")]
void main(inout CallData myCallData) {

// CHECK:  OpLoad %v3uint [[a]]
  uint3 a = DispatchRaysIndex();
// CHECK:  OpLoad %v3uint [[b]]
  uint3 b = DispatchRaysDimensions();
}
