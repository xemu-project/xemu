#version 460
layout (set=0, binding=0) uniform subpassInput ia;
layout (location=0) out vec4 outColor;
void main() {
	outColor = subpassLoad(ia);
}
