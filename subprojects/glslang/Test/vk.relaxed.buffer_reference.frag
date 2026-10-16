#version 460

#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_samplerless_texture_functions : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

struct Material
{
    uint textures[4];
};

layout(buffer_reference, std430, buffer_reference_align = 16)
buffer MaterialBuffer
{
    Material materials[];
};

struct GlobalUniformBufferData
{
    uint64_t materials;
};

layout(std430, set = 1, binding = 0) uniform GlobalUniformBufferDesc
{
    GlobalUniformBufferData global_uniform;
};

layout(set = 0, binding = 0) uniform texture2D global_textures_2d[];
layout(set = 0, binding = 2) uniform sampler global_samplers[];

layout(location = 0) out vec4 out_color;

vec4 SampleMaterial(uint material_index, uint texture_index, vec2 uv)
{
    return texture(
        sampler2D(
            global_textures_2d[MaterialBuffer(global_uniform.materials).materials[material_index].textures[texture_index]],
            global_samplers[0]
        ),
        uv
    );
}

void main()
{
    out_color = SampleMaterial(0, 0, vec2(0.5));
}