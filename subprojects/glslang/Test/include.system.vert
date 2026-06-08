#version 450

#extension GL_GOOGLE_include_directive : enable

#define float4 vec4

// For -Iinc2
// Should include "./bar.h"
#include "bar.h"
// Should include "./inc2/bar.h" and "./inc2/foo.h"
#include <bar.h>

out vec4 color;

void main()
{
    color = i1 + i5 + i6;
}
