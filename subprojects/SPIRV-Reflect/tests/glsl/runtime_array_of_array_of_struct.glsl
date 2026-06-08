#version 450
#pragma shader_stage(compute)

#extension GL_EXT_nonuniform_qualifier : enable

layout (local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

struct Foo {
    int a;
    int b;
};

layout(set = 0, binding = 0) buffer InutBuffer {
    Foo input_values[][3];
};

layout(set = 0, binding = 1) buffer OutputBuffer {
    Foo output_values[][3];
};

void main() {
    uint index = gl_GlobalInvocationID.x;
    output_values[index] = input_values[index];
}
