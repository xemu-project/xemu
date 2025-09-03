#version 450 core

in gl_PerVertex {
    vec4 gl_Position;
    float gl_CullDistance[3];
} gl_in[];

out gl_PerVertex {
    float gl_CullDistance[3];
};

layout(triangles) in;
layout(invocations = 4) in;
layout(line_strip) out;
layout(max_vertices = 127) out;

void main()
{
    vec4 pos = gl_in[2].gl_Position;
    gl_CullDistance[2] = gl_in[1].gl_CullDistance[2] * pos.x;
}
