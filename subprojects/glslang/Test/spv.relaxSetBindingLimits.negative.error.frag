#version 450

layout(set = -1, binding = 0) uniform sampler2D negSet;
layout(set = 0, binding = -1) uniform sampler2D negBinding;

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(1.0);
}
