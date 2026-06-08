// Run: %dxc -T lib_6_3 -fspv-extension=SPV_NV_ray_tracing
// CHECK:  OpCapability RayTracingNV
// CHECK:  OpExtension "SPV_NV_ray_tracing"
// CHECK:  OpDecorate [[a:%\d+]] BuiltIn LaunchIdNV
// CHECK:  OpDecorate [[b:%\d+]] BuiltIn LaunchSizeNV
// CHECK:  OpDecorate [[c:%\d+]] BuiltIn WorldRayOriginNV
// CHECK:  OpDecorate [[d:%\d+]] BuiltIn WorldRayDirectionNV
// CHECK:  OpDecorate [[e:%\d+]] BuiltIn RayTminNV
// CHECK:  OpDecorate [[f:%\d+]] BuiltIn IncomingRayFlagsNV
// CHECK:  OpDecorate %gl_InstanceID BuiltIn InstanceId
// CHECK:  OpDecorate [[g:%\d+]] BuiltIn InstanceCustomIndexNV
// CHECK:  OpDecorate %gl_PrimitiveID BuiltIn PrimitiveId
// CHECK:  OpDecorate [[h:%\d+]] BuiltIn ObjectRayOriginNV
// CHECK:  OpDecorate [[i:%\d+]] BuiltIn ObjectRayDirectionNV
// CHECK:  OpDecorate [[j:%\d+]] BuiltIn ObjectToWorldNV
// CHECK:  OpDecorate [[k:%\d+]] BuiltIn WorldToObjectNV
// CHECK:  OpDecorate [[l:%\d+]] BuiltIn HitKindNV

// CHECK:  OpTypePointer IncomingRayPayloadNV %Payload
struct Payload
{
  float4 color;
};
// CHECK:  OpTypePointer HitAttributeNV %Attribute
struct Attribute
{
  float2 bary;
};

// CHECK-COUNT-1: [[rstype:%\d+]] = OpTypeAccelerationStructureNV
RaytracingAccelerationStructure rs;

[shader("anyhit")]
void main(inout Payload MyPayload, in Attribute MyAttr) {

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
// CHECK:  OpLoad %uint %gl_InstanceID
  uint _7 = InstanceIndex();
// CHECK:  OpLoad %uint [[g]]
  uint _8 = InstanceID();
// CHECK:  OpLoad %uint %gl_PrimitiveID
  uint _9 = PrimitiveIndex();
// CHECK:  OpLoad %v3float [[h]]
  float3 _10 = ObjectRayOrigin();
// CHECK:  OpLoad %v3float [[i]]
  float3 _11 = ObjectRayDirection();
// CHECK: [[matotw:%\d+]] = OpLoad %mat4v3float [[j]]
// CHECK-NEXT: OpTranspose %mat3v4float [[matotw]]
  float3x4 _12 = ObjectToWorld3x4();
// CHECK:  OpLoad %mat4v3float [[j]]
  float4x3 _13 = ObjectToWorld4x3();
// CHECK: [[matwto:%\d+]] = OpLoad %mat4v3float [[k]]
// CHECK-NEXT: OpTranspose %mat3v4float [[matwto]]
  float3x4 _14 = WorldToObject3x4();
// CHECK:  OpLoad %mat4v3float [[k]]
  float4x3 _15 = WorldToObject4x3();
// CHECK:  OpLoad %uint [[l]]
  uint _16 = HitKind();

  if (_16 == 1U) {
// CHECK:  [[payloadread0:%\d+]] = OpLoad %Payload %MyPayload_0
// CHECK-NEXT : OpStore %MyPayload [[payloadread0]]
// CHECK-NEXT : OpIgnoreIntersectionNV
    IgnoreHit();
  } else {
// CHECK:  [[payloadread1:%\d+]] = OpLoad %Payload %MyPayload_0
// CHECK-NEXT : OpStore %MyPayload [[payloadread1]]
// CHECK-NEXT : OpTerminateRayNV
    AcceptHitAndEndSearch();
  }
}
