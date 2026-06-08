#version 450
layout(set = 0, binding = 0) buffer foo {
    uvec4 a; // not used
    uint b[4]; // used
    uint c; // not used
};

void main() {
    uint d[4] = {4, 5, 6, 7};
    b = d;
}