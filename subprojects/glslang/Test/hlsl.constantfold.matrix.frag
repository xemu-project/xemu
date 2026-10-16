// Constant folding of component-wise intrinsics whose operands are matrices.
// getVectorSize() is 0 for a matrix, so these used to fold with a -1 smear
// index (min/max/clamp/pow/lerp) or a zero-length result (step/smoothstep).
float4 main() : SV_Target
{
    float2x2 mn = min(float2x2(1, 2, 3, 4), float2x2(4, 3, 2, 1));
    float2x2 mx = max(float2x2(1, 2, 3, 4), float2x2(4, 3, 2, 1));
    float2x2 cl = clamp(float2x2(0, 5, 10, -2), float2x2(1, 1, 1, 1), float2x2(8, 8, 8, 8));
    float2x2 lp = lerp(float2x2(0, 0, 0, 0), float2x2(4, 4, 4, 4), float2x2(0.25, 0.5, 0.75, 1.0));
    float2x2 st = step(float2x2(1, 2, 3, 4), float2x2(4, 3, 2, 1));
    float2x2 ss = smoothstep(float2x2(0, 0, 0, 0), float2x2(2, 2, 2, 2), float2x2(1, 1, 1, 1));

    return float4(mn[0] + mx[0] + cl[0] + lp[0], st[0] + ss[0]);
}
