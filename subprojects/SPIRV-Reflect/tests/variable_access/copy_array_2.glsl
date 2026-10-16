#version 450
layout(set = 0, binding = 0, std430) buffer foo1 {
    uvec4 a;
    uint b[4];
    uint c;
};

layout(set = 0, binding = 1, std430) buffer foo2 {
    uint d;
    uint e[4];
    uvec2 f;
};

void main() {
    b = e;
}