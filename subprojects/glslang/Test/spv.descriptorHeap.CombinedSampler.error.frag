#version 450
#extension GL_EXT_descriptor_heap : require

layout(descriptor_heap) uniform sampler2D u_image[];

layout(location = 0) flat in vec2 vtx_out_texCoord;
layout(location = 0) out vec4 o_result;

void main()
{
    vec2 texCoord = vtx_out_texCoord;
    vec4 result = texture(u_image[0], texCoord);
    o_result = result;
}
