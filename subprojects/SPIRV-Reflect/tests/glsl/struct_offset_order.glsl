#version 460

layout(binding = 0, std430) uniform UniformBufferObject {
	layout(offset = 0) float a;
	layout(offset = 4) float b;
	layout(offset = 8) float c;
	layout(offset = 16) float d;
} ubo;

layout(binding = 0, std430) uniform UniformBufferObject2 {
	layout(offset = 4) float b;
	layout(offset = 0) float a;
	layout(offset = 16) float c;
	layout(offset = 8) float d;
} ubo2;

void main() {}