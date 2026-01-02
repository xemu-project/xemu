/*
glslangValidator -V -S frag -g0 --ku -o function_parameter_access.spv function_parameter_access.glsl
*/

#version 450

layout(binding = 0) uniform sampler Sampler;
layout(binding = 1) uniform texture2D Texture;
layout(binding = 2) uniform sampler2D Sampler2D;

layout(binding = 3) uniform sampler NeverAccessedSampler;
layout(binding = 4) uniform texture2D NeverAccessedTexture;
layout(binding = 5) uniform sampler2D NeverAccessedSampler2D;

layout(location = 0) out vec4 color;

vec4 access_sampler_and_texture(texture2D t, sampler s, vec2 coord)
{
    vec4 ret = texture(sampler2D(t, s), coord);
    return vec4(5.0) * ret;
}

vec4 access_combined_sampler(sampler2D s)
{
    vec2 coord = vec2(0.5, 0.5);
    vec4 ret = texture(s, coord);
    return vec4(1.0, 2.0, 3.0, 1.0) * ret;
}

vec4 call_access_functions(texture2D t, sampler s)
{
    return access_combined_sampler(Sampler2D) + access_sampler_and_texture(t, s, vec2(0.25, 0.75));
}

vec4 never_called(texture2D t, sampler s, float u, float v)
{
    vec4 ret = texture(sampler2D(t, s), vec2(u, v));
    return vec4(-3.0) * ret;
}

vec4 never_called_2(vec2 coord)
{
    vec4 ret = texture(sampler2D(NeverAccessedTexture, NeverAccessedSampler), coord);
    ret *= texture(NeverAccessedSampler2D, coord);
    return ret;
}

void main()
{
    color = vec4(-1.0) * call_access_functions(Texture, Sampler);
}
