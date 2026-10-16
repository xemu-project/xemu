#version 450

#extension GL_EXT_shader_explicit_arithmetic_types_int64: enable

layout(local_size_x = 1) in;

layout(constant_id = 0) const int64_t SONE = 1;
layout(constant_id = 1) const uint64_t UONE = 1;
layout(constant_id = 2) const float F_ONE = 1.0;
layout(constant_id = 3) const double D_ONE = 1.0;

const uint U_SONE = uint(SONE);
const uint U_UONE = uint(UONE);

const int S_SONE = int(SONE);
const int S_UONE = int(UONE);

const float F_ONE2 = float(D_ONE);
const double D_ONE2 = double(F_ONE);

#define DUMMY_SSBO(name, bind, size) layout(std430, set = 0, binding = bind) buffer SSBO_##name { float val[size]; float dummy; } name

DUMMY_SSBO(U_Sone, 0, U_SONE);
DUMMY_SSBO(U_Uone, 1, U_UONE);
DUMMY_SSBO(S_Sone, 2, S_SONE);
DUMMY_SSBO(S_Uone, 3, S_UONE);

void main()
{
	U_Sone.val[0] = F_ONE;
	U_Uone.val[0] = F_ONE;
	S_Sone.val[0] = F_ONE2;
	S_Uone.val[0] = F_ONE2;
}