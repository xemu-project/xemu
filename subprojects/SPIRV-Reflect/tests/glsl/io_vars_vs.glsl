#version 450
#pragma shader_stage(vertex)

layout (location = 0) out vec3 texcoord;
layout (location = 1) out vec4 colors[4];
layout (location = 6) out vec3 normals[4][3][2];

void main() {
  gl_Position = vec4(gl_VertexIndex, 0, 0, 1);
  texcoord = vec3(0,0,0);
  colors[1] = vec4(0);
  normals[3][2][1] = vec3(0);
}