#version 420

// A 64-bit counterpart to spv.matrix.frag, covering the same matrix operations
// at double precision. As in the 16-bit counterpart, the operands are converted
// from a 32-bit input rather than declared as double fragment inputs, so that
// the three shaders differ only in the width being exercised.

layout(location = 0) in vec4 src;

layout(location = 0) out vec4 color;

void main()
{
    dmat3x4 m1 = dmat3x4(double(src.x));
    dmat3x4 m2 = dmat3x4(double(src.y));
    double  f  = double(src.z);
    dvec3   v3 = dvec3(double(src.w));
    dvec4   v4 = dvec4(double(src.x));

    dmat3x4 sum34;
    mat3x4  fm;
    dvec3   sum3;
    dvec4   sum4;

    sum34 = m1 - m2;
    sum34 += m1 * f;
    sum34 += f * m1;
    sum34 /= matrixCompMult(m1, m2);
    sum34 += m1 / f;
    sum34 += f / m1;
    sum34 += f;
    sum34 -= f;
    fm = mat3x4(sum34);
    sum34 = dmat3x4(fm);

    sum3 = v4 * m2;
    sum4 = m2 * v3;

    dmat4x3 m43 = transpose(sum34);
    dmat4   m4  = m1 * m43;

    sum4 = v4 * m4;

    dvec4 acc = sum4;

    ++sum34;
    --sum34;

    sum34 += dmat3x4(f);
    sum34 += dmat3x4(v3, f, v3, f, v3, f);

    acc += sum3 * m43 + sum4;

    acc += dvec4(m43);
    acc += dvec4(dvec3(dmat2(f)), 7.2lf);

    // Beyond what spv.matrix.frag covers. Growing a matrix from a smaller one
    // leaves the trailing diagonal at the identity, and is the only construction
    // that uses the one filler rather than just the zero filler.
    dmat4 grown = dmat4(dmat2(f));
    acc += dvec4(grown[3][3]);

    color = vec4(acc);
}
