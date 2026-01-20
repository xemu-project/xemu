/*
Build with DXC using -O0 to preserve unused types:

   dxc -spirv -O0 -T ps_5_0 -Fo matrix_major_order_hlsl.spv matrix_major_order_hlsl.hlsl

*/


//
// NOTE: 1xN and Nx1 matrices are aliased to floatN so they're not included.
//

// -----------------------------------------------------------------------------
// Matrices without row / column major decoration
// -----------------------------------------------------------------------------

float2x2 mat_2x2;
float2x3 mat_2x3;
float2x4 mat_2x4;

float3x2 mat_3x2;
float3x3 mat_3x3;
float3x4 mat_3x4;

float4x2 mat_4x2;
float4x3 mat_4x3;
float4x4 mat_4x4;

// -----------------------------------------------------------------------------
// Matrices with row major decoration
// -----------------------------------------------------------------------------

row_major float2x2 row_major_my_mat_2x2;
row_major float2x3 row_major_my_mat_2x3;
row_major float2x4 row_major_my_mat_2x4;

row_major float3x2 row_major_my_mat_3x2;
row_major float3x3 row_major_my_mat_3x3;
row_major float3x4 row_major_my_mat_3x4;

row_major float4x2 row_major_my_mat_4x2;
row_major float4x3 row_major_my_mat_4x3;
row_major float4x4 row_major_my_mat_4x4;

// -----------------------------------------------------------------------------
// Matrices with column major decoration
// -----------------------------------------------------------------------------

column_major float2x2 column_major_my_mat_2x2;
column_major float2x3 column_major_my_mat_2x3;
column_major float2x4 column_major_my_mat_2x4;

column_major float3x2 column_major_my_mat_3x2;
column_major float3x3 column_major_my_mat_3x3;
column_major float3x4 column_major_my_mat_3x4;

column_major float4x2 column_major_my_mat_4x2;
column_major float4x3 column_major_my_mat_4x3;
column_major float4x4 column_major_my_mat_4x4;

float4 main(float4 pos : SV_POSITION) : SV_TARGET
{
    return pos;
}