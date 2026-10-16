#version 450

#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require

// A 16-bit counterpart to spv.matrix.frag, covering the same matrix operations
// at half precision. Two deliberate departures from that shader: the version is
// 450 because the explicit arithmetic types extension is not available at 420,
// and the operands are converted from a 32-bit input rather than declared as
// f16 fragment inputs, which would additionally need GL_EXT_shader_16bit_storage.

layout(location = 0) in vec4 src;

layout(location = 0) out vec4 color;

void main()
{
    f16mat3x4 m1 = f16mat3x4(float16_t(src.x));
    f16mat3x4 m2 = f16mat3x4(float16_t(src.y));
    float16_t f  = float16_t(src.z);
    f16vec3   v3 = f16vec3(float16_t(src.w));
    f16vec4   v4 = f16vec4(float16_t(src.x));

    f16mat3x4 sum34;
    dmat3x4   dm;
    f16vec3   sum3;
    f16vec4   sum4;

    sum34 = m1 - m2;
    sum34 += m1 * f;
    sum34 += f * m1;
    sum34 /= matrixCompMult(m1, m2);
    sum34 += m1 / f;
    sum34 += f / m1;
    sum34 += f;
    sum34 -= f;
    dm = dmat3x4(sum34);
    sum34 = f16mat3x4(dm);

    sum3 = v4 * m2;
    sum4 = m2 * v3;

    f16mat4x3 m43 = transpose(sum34);
    f16mat4   m4  = m1 * m43;

    sum4 = v4 * m4;

    f16vec4 acc = sum4;

    ++sum34;
    --sum34;

    sum34 += f16mat3x4(f);
    sum34 += f16mat3x4(v3, f, v3, f, v3, f);

    acc += sum3 * m43 + sum4;

    acc += f16vec4(m43);
    acc += f16vec4(f16vec3(f16mat2(f)), 7.2hf);

    // Beyond what spv.matrix.frag covers. Growing a matrix from a smaller one
    // leaves the trailing diagonal at the identity, and is the only construction
    // that uses the one filler rather than just the zero filler.
    f16mat4 grown = f16mat4(f16mat2(f));
    acc += f16vec4(grown[3][3]);

    color = vec4(acc);
}
