#version 450

uniform sampler samp;
uniform texture1D tex;

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = texture(sampler1D(tex, samp), 0.5);
}
