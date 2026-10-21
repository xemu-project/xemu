#version 460
#extension GL_EXT_optional_input_attachment_index : enable
layout (set=0, binding=0) uniform subpassInput ia[];
layout (location=0) out vec4 outColor;
void main() {
	outColor = subpassLoad(ia[0]);
}
