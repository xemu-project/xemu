#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

struct B {
	float a;
};
layout (binding = 0) uniform Test {
	B b;
} test;
float dothing(const in B b) {
	return b.a;
}
layout(location = 0) out vec4 outColor;
void main() {
	outColor = vec4(0);
	// this is fine
	outColor.z = test.b.a;
	// this line causes SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_BLOCK_MEMBER_REFERENCE
	outColor.z = dothing(test.b);
}
