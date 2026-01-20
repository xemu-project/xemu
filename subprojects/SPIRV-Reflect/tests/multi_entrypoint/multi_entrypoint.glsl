#version 450

layout (set = 0, binding = 1) uniform Ubo {
  vec4 data;
} ubo;

layout (location = 1) in vec3 pos;
layout (location = 0) in vec2 iUV;
layout (location = 0) out vec2 oUV;

layout (push_constant) uniform PushConstantVert {
  float val;
} push_constant_vert;

out gl_PerVertex {
  vec4 gl_Position;
};

vec4 getData() {
  return ubo.data;
}

void entry_vert() {
  oUV = iUV;
  gl_Position = vec4(pos, 1.0) * push_constant_vert.val * getData();
}

layout (set = 0, binding = 0) uniform sampler2D tex;
layout (location = 1) out vec4 colour;

/*
layout (push_constant) uniform PushConstantFrag {
  float val;
} push_constant_frag;
*/

void entry_frag() {
  colour = texture(tex, iUV) * push_constant_vert.val * getData();
}

void main() {
  entry_vert();
  entry_frag();
}
