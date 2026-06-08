#version 450
#extension GL_EXT_buffer_reference : enable

layout(buffer_reference) buffer T1;
layout(set = 3, binding = 1, buffer_reference) buffer T1 {
   layout(offset = 48) T1  c[2]; // stride = 8 for std430, 16 for std140
} x;

void main() {}