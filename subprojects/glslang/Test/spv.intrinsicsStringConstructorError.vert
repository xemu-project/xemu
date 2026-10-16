#version 460

#extension GL_EXT_spirv_intrinsics : enable

void main()
{
    // A string literal is not a valid numeric constructor argument.
    uint u = uint("foo");
    int i = int("bar");
    float f = float("baz");
    uvec2 v = uvec2(1u, "qux");
}
