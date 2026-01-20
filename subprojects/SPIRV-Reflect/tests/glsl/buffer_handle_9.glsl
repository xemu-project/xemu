#version 460
#extension GL_EXT_buffer_reference2: require
#extension GL_EXT_scalar_block_layout : enable

layout(buffer_reference, scalar) readonly buffer VertexBufferPtr {
    vec4 v[];
};

// Has a OpTypeRuntimeArray
layout(binding = 1, set = 0, scalar) readonly buffer Vertices {
    VertexBufferPtr vertex_buffers[];
};

void main() {
    gl_Position = vertex_buffers[gl_VertexIndex].v[gl_VertexIndex];
}