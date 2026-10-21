#version 450
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require

layout(location = 0) in float16_t a;
layout(location = 1) in f16vec3 b;
layout(location = 2) in uint16_t c;
layout(location = 3) in u16vec3 d;
layout(location = 4) in int16_t e;
layout(location = 5) in i16vec3 f;

layout(location = 0) out float16_t _a;
layout(location = 1) out f16vec3 _b;
layout(location = 2) out uint16_t _c;
layout(location = 3) out u16vec3 _d;
layout(location = 4) out int16_t _e;
layout(location = 5) out i16vec3 _f;

void main()
{
    _a = a * float16_t(2.0);
    _b = b * f16vec3(2.0);
    _c = c * 2us;
    _d = d * u16vec3(2);
    _e = e * 2s;
    _f = f * i16vec3(2);
}
