#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_descriptor_heap : enable

layout (set = 0, binding = 0) buffer SSBO {
    uint result;
};
layout (set = 0, binding = 1) uniform UBO {
    uint data;
} ubo[];

layout(descriptor_heap) buffer StorageBuffer {
    vec4 a;
} heapStorageBufferA[];

layout (location = 0) in vec2 uvs;
layout (location = 1) flat in uint index;

layout (location = 0) out vec4 fragColor;

void main()
{
    result = ubo[nonuniformEXT(result)].data;
}