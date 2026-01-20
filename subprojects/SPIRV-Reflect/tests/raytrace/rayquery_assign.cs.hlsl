// Run: %dxc -T cs_6_5 -E CS -fspv-target-env=vulkan1.2
// CHECK:  error: store value of type 'RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH>' is unsupported

[numThreads(1,1,1)]
void CS()
{
    RayQuery<RAY_FLAG_FORCE_OPAQUE|RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;
    RayQuery<RAY_FLAG_FORCE_OPAQUE|RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> b = q;
}