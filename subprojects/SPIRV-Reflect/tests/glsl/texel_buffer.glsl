#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#pragma shader_stage(vertex)
layout (set = 0, binding = 1) uniform isamplerBuffer texel_buffer_i;
layout (set = 0, binding = 2) uniform samplerBuffer texel_buffer_f;

void main() {
  int i = texelFetch(texel_buffer_i, gl_InstanceIndex).x;
  float f = texelFetch(texel_buffer_f, gl_InstanceIndex).x;
  gl_Position = vec4(i, f, 0, 1);
}