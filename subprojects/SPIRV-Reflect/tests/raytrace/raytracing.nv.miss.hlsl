// Run: %dxc -T lib_6_3 -fspv-extension=SPV_NV_ray_tracing
// CHECK:  OpCapability RayTracingNV
// CHECK:  OpExtension "SPV_NV_ray_tracing"
// CHECK:  OpDecorate [[a:%\d+]] BuiltIn LaunchIdNV
// CHECK:  OpDecorate [[b:%\d+]] BuiltIn LaunchSizeNV
// CHECK:  OpDecorate [[c:%\d+]] BuiltIn WorldRayOriginNV
// CHECK:  OpDecorate [[d:%\d+]] BuiltIn WorldRayDirectionNV
// CHECK:  OpDecorate [[e:%\d+]] BuiltIn RayTminNV
// CHECK:  OpDecorate [[f:%\d+]] BuiltIn IncomingRayFlagsNV

// CHECK:  OpTypePointer IncomingRayPayloadNV %Payload
struct Payload
{
  float4 color;
};

[shader("miss")]
void main(inout Payload MyPayload) {

// CHECK:  OpLoad %v3uint [[a]]
  uint3 _1 = DispatchRaysIndex();
// CHECK:  OpLoad %v3uint [[b]]
  uint3 _2 = DispatchRaysDimensions();
// CHECK:  OpLoad %v3float [[c]]
  float3 _3 = WorldRayOrigin();
// CHECK:  OpLoad %v3float [[d]]
  float3 _4 = WorldRayDirection();
// CHECK:  OpLoad %float [[e]]
  float _5 = RayTMin();
// CHECK:  OpLoad %uint [[f]]
  uint _6 = RayFlags();
}
