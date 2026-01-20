#version 460 core
#extension GL_ARB_separate_shader_objects : enable

layout(location=0) in dvec2 position;
layout(location=1) in double in_thing;

layout(location=0) out VertexData {
double out_thing;
} VertexOut;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    VertexOut.out_thing = in_thing;
}
