#version 450

#extension GL_EXT_buffer_reference : enable

layout(buffer_reference, std430) buffer blockType {
    vec4 v;
};

layout(binding = 0, std430) buffer t2 {
    blockType f;
} t;

void main()
{
    // Out-of-order full l-value swizzle store through a buffer_reference emits a
    // read-modify-write load on the PhysicalStorageBuffer pointer.
    t.f.v.wzyx = vec4(1.0);
}
