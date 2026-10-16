#version 460
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_buffer_reference_uvec2 : require

layout(buffer_reference) buffer VertexBuffer;
layout(buffer_reference, buffer_reference_align = 16, std430) buffer VertexBuffer {
    int x;
};

layout(set = 0, binding = 0) buffer T1 {
    uvec2 bufferId;
} ssbo;

void main() {
    VertexBuffer(ssbo.bufferId).x = 0;
}