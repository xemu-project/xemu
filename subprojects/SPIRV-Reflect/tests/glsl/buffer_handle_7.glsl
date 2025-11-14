#version 460 core
#extension GL_EXT_buffer_reference : require

layout(buffer_reference, buffer_reference_align = 4) readonly buffer t2 {
    int values[];
};

layout(buffer_reference, buffer_reference_align = 4) readonly buffer t1 {
    t2 c;
};

layout(buffer_reference, buffer_reference_align = 4) readonly buffer t0 {
    t1 a;
    t2 b;
};

layout(push_constant, std430) uniform Parameters {
    t0 x;
} params;

void main() {
    params.x.a.c.values[0] = params.x.b.values[0] + 1;
}
