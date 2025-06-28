/*
Compile:
  glslangValidator.exe -V -S comp -o readonly_writeonly.spv readonly_writeonly.glsl
*/
#version 450
#pragma shader_stage(compute)

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

const int kArraySize = 64;

layout(set = 0, binding = 0) buffer storage_buffer {
    readonly float a[kArraySize];
    writeonly float b[kArraySize];
    writeonly readonly float bar[kArraySize];
} foo;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    foo.b[idx] = foo.a[idx];
}
