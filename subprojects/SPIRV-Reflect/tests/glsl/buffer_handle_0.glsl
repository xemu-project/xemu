#version 450
#extension GL_EXT_buffer_reference : enable

layout(buffer_reference) buffer t1;
layout(buffer_reference) buffer t2;
layout(buffer_reference) buffer t3;
layout(buffer_reference) buffer t4;

layout(buffer_reference, std430) buffer t1 {
    t4 i_1;
};

layout(buffer_reference, std430) buffer t2 {
    t4 i_2;
};

layout(buffer_reference, std430) buffer t3 {
    t2 i_3;
};

layout(set = 1, binding = 2, buffer_reference, std430) buffer t4 {
    layout(offset = 0)  int j;
    t1 k_1;
    t2 k_2;
    t3 k_3;
} x;

layout(set = 0, binding = 0, std430) buffer t5 {
    t4 m;
} s5;

void main() {}
