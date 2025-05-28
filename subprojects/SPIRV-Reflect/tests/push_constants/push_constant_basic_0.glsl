#version 460
#extension GL_EXT_buffer_reference : enable

layout(buffer_reference) buffer Node { uint x; };

struct Bar {
    uint t;
    uint s;
};

layout(push_constant, std430) uniform foo {
    uint a;
    uint b[4];
    Bar bar;
    Node node;
};

layout(set=0, binding=0) buffer SSBO {
    uint out_value;
};

// Everything should be marked as UNUSED
void main() {}