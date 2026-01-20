#!/bin/sh
if [ "$#" -ne 1 ]; then
    echo -e "error: missing path to DXC"
    echo -e "\nUsage:\n  compile_shaders.sh <path to DXC>\n"
fi

DXC_EXE=$1

echo "Using DXC at $DXC_EXE"

function exec_dxc {
    echo $1
    eval $1
}

#exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T cs_6_5 -E CS -Fo rayquery_assign.cs.spv rayquery_assign.cs.hlsl"
exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T cs_6_5 -E main -Fo rayquery_equal.cs.spv rayquery_equal.cs.hlsl"
exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T ds_6_5 -E main -Fo rayquery_init_ds.spv rayquery_init_ds.hlsl"
exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T gs_6_5 -E main -Fo rayquery_init_gs.spv rayquery_init_gs.hlsl"
exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T hs_6_5 -E main -Fo rayquery_init_hs.spv rayquery_init_hs.hlsl"
exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T ps_6_5 -E main -Fo rayquery_init_ps.spv rayquery_init_ps.hlsl"
exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T lib_6_3 -Fo rayquery_init_rahit.spv rayquery_init_rahit.hlsl"
exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T lib_6_3 -Fo rayquery_init_rcall.spv rayquery_init_rcall.hlsl"
exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T lib_6_3 -Fo rayquery_init_rchit.spv rayquery_init_rchit.hlsl"
exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T lib_6_3 -Fo rayquery_init_rgen.spv rayquery_init_rgen.hlsl"
#exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T lib_6_3 -Fo rayquery_init_rint.spv rayquery_init_rint.hlsl"
exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T lib_6_3 -Fo rayquery_init_rmiss.spv rayquery_init_rmiss.hlsl"
#exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T lib_6_3 -Fo rayquery_init_vs.spv rayquery_init_vs.hlsl"
#exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T lib_6_3 -Fo rayquery_tryAllOps.cs.spv rayquery_tryAllOps.cs.hlsl"
exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T cs_6_4 -E main -Fo raytracing.acceleration-structure.spv raytracing.acceleration-structure.hlsl"
exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T lib_6_3 -Fo raytracing.khr.closesthit.spv raytracing.khr.closesthit.hlsl"
exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T cs_6_4 -E main -Fo raytracing.nv.acceleration-structure.spv raytracing.nv.acceleration-structure.hlsl"
exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T lib_6_3 -fspv-extension=SPV_NV_ray_tracing -Fo raytracing.nv.anyhit.spv raytracing.nv.anyhit.hlsl"
exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T lib_6_3 -Fo raytracing.nv.callable.spv raytracing.nv.callable.hlsl"
exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T lib_6_3 -Fo raytracing.nv.closesthit.spv raytracing.nv.closesthit.hlsl"
exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T lib_6_3 -Fo raytracing.nv.enum.spv raytracing.nv.enum.hlsl"
exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T lib_6_3 -Fo raytracing.nv.intersection.spv raytracing.nv.intersection.hlsl"
exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T lib_6_3 -fspv-extension=SPV_NV_ray_tracing -Fo raytracing.nv.library.spv raytracing.nv.library.hlsl"
exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T lib_6_3 -Fo raytracing.nv.miss.spv raytracing.nv.miss.hlsl"
exec_dxc "$DXC_EXE -spirv -fspv-target-env=vulkan1.2 -T lib_6_3 -Fo raytracing.nv.raygen.spv raytracing.nv.raygen.hlsl"
