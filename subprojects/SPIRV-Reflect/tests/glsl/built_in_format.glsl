#version 450
#pragma shader_stage(vertex)

layout (location = 0) out vec3 texcoord;

void main() {
  gl_Position = vec4(gl_VertexIndex, 0, 0, 1);
  texcoord = vec3(0,0,0);
}