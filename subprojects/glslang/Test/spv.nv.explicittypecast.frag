#version 460
#extension GL_NV_explicit_typecast : enable
#extension GL_NV_gpu_shader5 : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_buffer_reference_uvec2 : enable

layout(buffer_reference) buffer BufRef
{
	vec4 inp;
	vec4 op;
};

float func(float a, vec2 b)
{
	return dot(b, vec2(a));
}
void main()
{
	float f_0;
	uint u_0;
	int i_0;
	float16_t f16_0; 
	uint8_t u8_0;
	int8_t i8_0;
	uint64_t u64_0;

	u8vec2 u8v2_0;
	u8vec4 u8v4_0;
	vec4 v4_0;
	f16vec3 f16v3_0;
	u64vec2 u64v2_0;
	vec3 v3_0;
	vec2 v2_0;

	mat2 m2_0, m2_1;
	mat3x2 m32_0;
	mat3 m3_0;
	mat4x4 m4_0;
	dmat2 d2_0;

	
	// Type cast scalars
	f_0 = (float) f_0;
	f_0 = (float) u_0;
	u_0 = (uint) i_0;
	i_0 = (int) f16_0;
	f16_0 = (float16_t) u64_0;
	u8_0 = (uint8_t) f16_0;
	i8_0 = (int8_t) f16_0;
	u64_0 = (uint64_t) f_0;

	// Type cast vectors
	v4_0 = (vec4) v4_0;
	v4_0 = (vec4) u8v4_0;
	f16v3_0 = (f16vec3) v3_0;
	u64v2_0 = (u64vec2) u8v2_0;

	// Narrowing vector cast

	v3_0 = (vec3)u8v4_0;
	u64v2_0 = (u64vec2)(v4_0);

	// Widening vector cast

	v4_0 = (vec4) u8_0;
	u64v2_0  = (u64vec2) f16_0;

	// Type cast matrix

	m2_0 = (mat2)m2_1;
	d2_0 = (dmat2)m2_0;
	m2_0 = (mat2)f16_0;

	// Type cast matrix slices
	m4_0 = (mat4) m3_0;
	m3_0 = (mat3) m4_0;

	// Type cast buffer reference
	BufRef B = (BufRef) u64_0;
	BufRef B2 = (BufRef) (uvec2) v2_0;
	// Type cast right to left
	u8_0 = (uint8_t)(float)(int)f16_0;

	// Type cast unary expression
	v3_0 = (vec3)(1.0f, 2.0f, 3.0f);
	f16v3_0 = (f16vec3)(u8_0, f16_0, i8_0);
	func((float)u8_0, (vec2)u64v2_0);


}
