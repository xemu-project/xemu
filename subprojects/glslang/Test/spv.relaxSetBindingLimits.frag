#version 450

layout(set = 300, binding = 70000) uniform sampler2D tex;
layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = texture(tex, vec2(0.5));
}
