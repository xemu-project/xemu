#version 450
#extension GL_GOOGLE_include_directive : require
#include "spv.debuginfo.macro.include.glsl.h"

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = INCLUDED_COLOR;
}
