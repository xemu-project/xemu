#version 450 core

#extension GL_AMD_gpu_shader_half_float : require
#extension GL_EXT_shader_explicit_arithmetic_types : enable

layout(set=0, binding=0, std430) buffer SSBO
{
    f16vec2 coords2D;
    f16vec2 gradX;
    f16vec2 gradY;
};

void main() {
    gradX = dFdx(coords2D);
    gradY = dFdy(coords2D);
}
