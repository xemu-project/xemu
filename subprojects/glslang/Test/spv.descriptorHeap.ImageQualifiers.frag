#version 460
#extension GL_EXT_descriptor_heap : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_samplerless_texture_functions : require

layout(descriptor_heap, r32f) writeonly uniform image2D Writes[];
layout(descriptor_heap, r32f) readonly uniform image2D Reads[];

void main()
{
    imageStore(Writes[10], ivec2(0), imageLoad(Reads[20], ivec2(0)));
}
