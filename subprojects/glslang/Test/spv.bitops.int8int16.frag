#version 450

#extension GL_EXT_shader_explicit_arithmetic_types: enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8: require
#extension GL_EXT_shader_explicit_arithmetic_types_int16: require

layout(binding = 0) uniform Uniforms
{
    uint index;
};

void main()
{
}


void bitOps8(int8_t a, int8_t b)
{
    int8_t i8;
    i8vec2 i8v2;
    i8vec3 i8v3;
    i8vec4 i8v4;

    uint8_t u8;
    u8vec2  u8v2;
    u8vec3  u8v3;
    u8vec4  u8v4;

    i8 = bitfieldReverse(i8);
    i8v2 = bitfieldReverse(i8v2);
    i8v3 = bitfieldReverse(i8v3);
    i8v4 = bitfieldReverse(i8v4);

    u8 = bitfieldReverse(u8);
    u8v2 = bitfieldReverse(u8v2);
    u8v3 = bitfieldReverse(u8v3);
    u8v4 = bitfieldReverse(u8v4);

    i8 = findMSB(i8);
    i8v2 = findMSB(i8v2);
    i8v3 = findMSB(i8v3);
    i8v4 = findMSB(i8v4);

    i8 = findMSB(u8);
    i8v2 = findMSB(u8v2);
    i8v3 = findMSB(u8v3);
    i8v4 = findMSB(u8v4);


    i8 = findLSB(i8);
    i8v2 = findLSB(i8v2);
    i8v3 = findLSB(i8v3);
    i8v4 = findLSB(i8v4);

    i8 = findLSB(u8);
    i8v2 = findLSB(u8v2);
    i8v3 = findLSB(u8v3);
    i8v4 = findLSB(u8v4);


    i8 = bitCount(i8);
    i8v2 = bitCount(i8v2);
    i8v3 = bitCount(i8v3);
    i8v4 = bitCount(i8v4);

    i8 = bitCount(u8);
    i8v2 = bitCount(u8v2);
    i8v3 = bitCount(u8v3);
    i8v4 = bitCount(u8v4);

    i8 = bitfieldExtract(i8, a, b);
    i8v2 = bitfieldExtract(i8v2, a, b);
    i8v3 = bitfieldExtract(i8v3, a, b);
    i8v4 = bitfieldExtract(i8v4, a, b);

    u8 = bitfieldExtract(u8, a, b);
    u8v2 = bitfieldExtract(u8v2, a, b);
    u8v3 = bitfieldExtract(u8v3, a, b);
    u8v4 = bitfieldExtract(u8v4, a, b);

    i8 = bitfieldInsert(i8, i8, a, b);
    i8v2 = bitfieldInsert(i8v2, i8v2, a, b);
    i8v3 = bitfieldInsert(i8v3, i8v3, a, b);
    i8v4 = bitfieldInsert(i8v4, i8v4, a, b);

    u8 = bitfieldInsert(u8, u8, a, b);
    u8v2 = bitfieldInsert(u8v2, u8v2, a, b);
    u8v3 = bitfieldInsert(u8v3, u8v3, a, b);
    u8v4 = bitfieldInsert(u8v4, u8v4, a, b);
}


void bitOps16(int16_t a, int16_t b)
{
    int16_t i16;
    i16vec2 i16v2;
    i16vec3 i16v3;
    i16vec4 i16v4;

    uint16_t u16;
    u16vec2  u16v2;
    u16vec3  u16v3;
    u16vec4  u16v4;

    i16 = bitfieldReverse(i16);
    i16v2 = bitfieldReverse(i16v2);
    i16v3 = bitfieldReverse(i16v3);
    i16v4 = bitfieldReverse(i16v4);

    u16 = bitfieldReverse(u16);
    u16v2 = bitfieldReverse(u16v2);
    u16v3 = bitfieldReverse(u16v3);
    u16v4 = bitfieldReverse(u16v4);

    i16 = findMSB(i16);
    i16v2 = findMSB(i16v2);
    i16v3 = findMSB(i16v3);
    i16v4 = findMSB(i16v4);

    u16 = findMSB(u16);
    u16v2 = findMSB(u16v2);
    u16v3 = findMSB(u16v3);
    u16v4 = findMSB(u16v4);


    i16 = findLSB(i16);
    i16v2 = findLSB(i16v2);
    i16v3 = findLSB(i16v3);
    i16v4 = findLSB(i16v4);

    i16 = findLSB(u16);
    i16v2 = findLSB(u16v2);
    i16v3 = findLSB(u16v3);
    i16v4 = findLSB(u16v4);


    i16 = bitCount(i16);
    i16v2 = bitCount(i16v2);
    i16v3 = bitCount(i16v3);
    i16v4 = bitCount(i16v4);

    i16 = bitCount(u16);
    i16v2 = bitCount(u16v2);
    i16v3 = bitCount(u16v3);
    i16v4 = bitCount(u16v4);

    i16 = bitfieldExtract(i16, a, b);
    i16v2 = bitfieldExtract(i16v2, a, b);
    i16v3 = bitfieldExtract(i16v3, a, b);
    i16v4 = bitfieldExtract(i16v4, a, b);

    u16 = bitfieldExtract(u16, a, b);
    u16v2 = bitfieldExtract(u16v2, a, b);
    u16v3 = bitfieldExtract(u16v3, a, b);
    u16v4 = bitfieldExtract(u16v4, a, b);

    i16 = bitfieldInsert(i16, i16, a, b);
    i16v2 = bitfieldInsert(i16v2, i16v2, a, b);
    i16v3 = bitfieldInsert(i16v3, i16v3, a, b);
    i16v4 = bitfieldInsert(i16v4, i16v4, a, b);

    u16 = bitfieldInsert(u16, u16, a, b);
    u16v2 = bitfieldInsert(u16v2, u16v2, a, b);
    u16v3 = bitfieldInsert(u16v3, u16v3, a, b);
    u16v4 = bitfieldInsert(u16v4, u16v4, a, b);
}
