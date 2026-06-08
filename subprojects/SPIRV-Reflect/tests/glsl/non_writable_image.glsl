/*
Compile:
  glslangValidator.exe -V -S comp -o non_writable_image.spv non_writable_image.glsl
*/
#version 450
#pragma shader_stage(compute)

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(set = 0, binding = 0, rgba32f) uniform readonly image2D input_image;
layout(set = 0, binding = 1, rgba32f) uniform writeonly image2D output_image;

void main() {
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    imageStore(output_image, p, imageLoad(input_image, p));
}