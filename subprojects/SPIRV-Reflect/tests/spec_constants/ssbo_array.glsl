#version 450

layout(constant_id = 8) const int SIZE_A = 4;
layout(constant_id = 5) const int SIZE_B = 3;
layout(set = 0, binding = 0, std430) buffer SSBO {
    float a[SIZE_A];
    float b[SIZE_B];
} ssbo;

void main() {
    ssbo.a[2] = ssbo.b[1];
}