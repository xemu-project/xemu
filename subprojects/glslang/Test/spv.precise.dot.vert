#version 450 core

out gl_PerVertex
{
    precise vec4 gl_Position;
};

float u;

void main()
{
    vec4 v = vec4(u);
    float f = dot(v, v);
    gl_Position = vec4(f);
}
