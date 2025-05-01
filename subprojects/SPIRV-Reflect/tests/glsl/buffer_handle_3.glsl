#version 450
#extension GL_EXT_buffer_reference : enable

layout(buffer_reference) buffer t1;

layout(buffer_reference, std430) buffer t1 {
    t1 i;
};

layout(set = 1, binding = 2, buffer_reference, std430) buffer t4 {
    layout(offset = 0)  int j;
    t1 k;
} x;

layout(set = 0, binding = 0, std430) buffer t5 {
    t4 m;
} s5;

void main() {}
