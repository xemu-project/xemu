#version 460
#extension GL_NV_explicit_typecast : enable
#extension GL_EXT_buffer_reference : enable
layout(buffer_reference) buffer BufRef
{
	vec4 inp;
	vec4 op;
};
struct S { uint dummy; };
int func() { return 5; };
void main()
{
	// Cannot typecast to struct
	S temp;
	uint v = 1U;
	temp = (S) v;

	// Cannot typecast to buffer reference uvec2 without extension
	uvec2 uv2 = uvec2(2,4);
	BufRef B = (BufRef) uv2;

	//// Cannot typecast to function
	uint temp2 = 2U;
	int tempf = (func)temp2;

	//// Cannot typecast to array
	int arr[2];
	float farr[2] = {1.0f, 2.0f};
	arr = (int[10]) farr;
	
}
