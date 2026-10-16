#version 460 core

#extension GL_EXT_buffer_reference: require
#extension GL_EXT_scalar_block_layout: require

layout (buffer_reference, buffer_reference_align = 4, scalar) readonly buffer DataBuffer {
	vec4 data;
};

layout (buffer_reference, buffer_reference_align = 8, scalar) buffer ParametersBuffer {
	DataBuffer src;
	DataBuffer dst;
};

layout(push_constant) uniform constants {
	ParametersBuffer u_param;
};

void main() {
	u_param.dst = u_param.src;
}