#version 460 core

#extension GL_EXT_buffer_reference : require

layout(buffer_reference, buffer_reference_align = 16) buffer Data {
  float g0;
  float g1;
  float g2;
};

layout(push_constant) uniform PushData {
  Data data_ptr;
} push;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
  float f1 = push.data_ptr.g1;
  Color = vec4(f1, 0.0, 0.0, 0.0);
}

