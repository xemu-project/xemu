#version 450

struct Bar {
   uint x;
   uint y;
   uint z[2];
};

layout(set = 0, binding = 0, std430) buffer foo1 {
    uvec4 a;
    Bar b;
    uint c;
};

layout(set = 0, binding = 1, std430) buffer foo2 {
    uvec4 d;
    Bar e;
    uint f;
};

void main() {
    b = e;
}