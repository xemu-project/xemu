#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput MyInputAttachment0;
layout (input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput MyInputAttachment1;
layout (input_attachment_index = 4, set = 0, binding = 2) uniform subpassInput MyInputAttachment4;

layout (location = 0) out vec4 oColor;

void main() {
  oColor = subpassLoad(MyInputAttachment0) + 
           subpassLoad(MyInputAttachment1) + 
           subpassLoad(MyInputAttachment4);           
}