#version 460

layout(binding = 0, offset = 0) uniform atomic_uint counter;

layout(location = 0) out vec4 Output;

void main()
{
    Output = vec4(float(atomicCounter(counter)));
}
