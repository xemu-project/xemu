// Compile: slangc multi_type_heap.slang -target spirv -capability spvDescriptorHeapEXT -I C:/code/source/nvpro-samples/nvpro_core2 -I C:/code/source/nvpro-samples/vk_mini_samples/common -entry computeMain -stage compute -o multi_type_heap.comp.spv
//
// Test fixture for SPV_EXT_descriptor_heap reflection. Exercises multiple
// descriptor types reachable from a single entry point's static call graph.
// Each distinct (heap variable, OpTypeRuntimeArray) pair should surface as its
// own entry on the entry point's resource_heap_accesses or sampler_heap_accesses.
//
// Coverage:
//   resource_heap_accesses:
//     - VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE        (Texture2D)
//     - VK_DESCRIPTOR_TYPE_STORAGE_IMAGE        (RWTexture2D)
//     - VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER (Buffer<float4>)
//     - VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER (RWBuffer<float4>)
//     - VK_DESCRIPTOR_TYPE_STORAGE_BUFFER       (StructuredBuffer<T>)      [OpTypeBufferEXT]
//     - VK_DESCRIPTOR_TYPE_STORAGE_BUFFER       (RWStructuredBuffer<T>)    [OpTypeBufferEXT]
//   sampler_heap_accesses:
//     - VK_DESCRIPTOR_TYPE_SAMPLER              (SamplerState)
//
// ConstantBuffer<T> via DescriptorHandle hits an internal slang compiler
// error ("Constant buffer type remaining in spirv emit") as of slang 2026.8.1
// and is omitted here.

struct InputElement
{
  float4 value;
};

struct OutputElement
{
  float4 sum;
};

struct PushBlock
{
  uint texIdx;        // Texture2D slot
  uint storeImgIdx;   // RWTexture2D slot
  uint texelBufIdx;   // Buffer<float4> slot
  uint rwTexelBufIdx; // RWBuffer<float4> slot
  uint sbufIdx;       // StructuredBuffer<InputElement> slot
  uint rwSbufIdx;     // RWStructuredBuffer<OutputElement> slot
  uint sampIdx;       // SamplerState slot
};
[[vk::push_constant]] ConstantBuffer<PushBlock> push;

[require(spvDescriptorHeapEXT)]
[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 tid : SV_DispatchThreadID)
{
  SamplerState                       samp   = SamplerState.Handle(uint2(push.sampIdx, 0));
  Texture2D<float4>                  tex    = Texture2D<float4>.Handle(uint2(push.texIdx, 0));
  RWTexture2D<float4>                rwImg  = RWTexture2D<float4>.Handle(uint2(push.storeImgIdx, 0));
  Buffer<float4>                     tbuf   = Buffer<float4>.Handle(uint2(push.texelBufIdx, 0));
  RWBuffer<float4>                   rwTbuf = RWBuffer<float4>.Handle(uint2(push.rwTexelBufIdx, 0));
  StructuredBuffer<InputElement>     sbuf   = StructuredBuffer<InputElement>.Handle(uint2(push.sbufIdx, 0));
  RWStructuredBuffer<OutputElement>  rwSbuf = RWStructuredBuffer<OutputElement>.Handle(uint2(push.rwSbufIdx, 0));

  // Force a use of each binding so the compiler can't fold them away.
  float2 uv = float2(tid.xy) / 64.0;
  float4 sampled  = tex.SampleLevel(samp, uv, 0.0);
  float4 loaded   = rwImg[tid.xy];
  float4 fetched  = tbuf[tid.x];
  float4 sbufVal  = sbuf[tid.x].value;

  float4 result = sampled + loaded + fetched + sbufVal;
  rwImg[tid.xy] = result;
  rwTbuf[tid.x] = result;
  rwSbuf[tid.x].sum = result;
}
