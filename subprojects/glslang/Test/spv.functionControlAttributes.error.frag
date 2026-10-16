#version 450

#extension GL_EXT_function_control_attributes : enable

layout(location = 0) out vec4 color;

[[inline, noinline]] int sameDeclaration(int i)
{
    return i;
}

[[inline]] int prototypeConflict(int i);

[[noinline]] int prototypeConflict(int i)
{
    return i;
}

void main()
{
    color = vec4(0.0);
}
