#version 450
#pragma shader_stage(compute)

layout (local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

const int kArraySize = 64;

layout(set = 0, binding = 0) buffer InputBuffer {
    float input_values[kArraySize];
};

layout(set = 0, binding = 1) buffer OutputBuffer {
    float output_values[kArraySize];
};

void main() {
    uint index = gl_GlobalInvocationID.x;
    output_values[index] = input_values[index];
}
