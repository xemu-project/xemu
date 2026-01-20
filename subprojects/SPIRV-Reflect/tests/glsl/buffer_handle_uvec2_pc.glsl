#version 460
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_buffer_reference_uvec2 : require

layout(buffer_reference) buffer VertexBuffer;
layout(buffer_reference, buffer_reference_align = 16, std430) buffer VertexBuffer {
    int x;
};

layout(push_constant, std430) uniform PerFrameData {
    uvec2 bufferId;
} pc;

void main() {
    VertexBuffer(pc.bufferId).x = 0;
}