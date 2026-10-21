float4  main(int a, int b, float4 col0, uint4 col1) : SV_TARGET
{
    return a < b ? col0: col1;
}
