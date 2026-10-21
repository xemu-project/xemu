#version 450 core
#extension GL_ARB_gpu_shader_int64 : enable

void main()
{
    int64_t  a = doubleBitsToInt64(1.0lf);   // 4607182418800017408
    uint64_t b = doubleBitsToUint64(2.0lf);  // 4611686018427387904
    double   c = int64BitsToDouble(4607182418800017408L);    // 1.0
    double   d = uint64BitsToDouble(13830554455654793216UL); // -1.0
}
