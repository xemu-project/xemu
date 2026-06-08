#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_float32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(buffer_reference, buffer_reference_align = 4) readonly buffer ReadVecf
{
    float32_t values[];
};

layout(buffer_reference, buffer_reference_align = 4) writeonly buffer WriteVecf
{
    float32_t values[];
};

layout(push_constant, std430) uniform Parameters
{
    ReadVecf a;
    ReadVecf b;
    WriteVecf c;
    uint64_t n;
} params;

void main()
{
    uint32_t idx = gl_GlobalInvocationID.x;
    uint32_t stride = gl_NumWorkGroups.x * gl_WorkGroupSize.x;
    for (; idx < params.n; idx += stride) {
        params.c.values[idx] = params.a.values[idx] + params.b.values[idx];
    }
}