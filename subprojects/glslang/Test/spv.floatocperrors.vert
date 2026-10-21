#version 450 core

#extension GL_EXT_float_e2m1 : require
#extension GL_EXT_float_ue8m0 : require

struct NestedSubByte { floate2m1_t value; };
struct NestedByte { floatue8m0_t value; };

layout(location = 0) out NestedSubByte subByteOutput;
layout(location = 1) out NestedByte byteOutput;

void main()
{
    gl_Position = vec4(0.0);
}
