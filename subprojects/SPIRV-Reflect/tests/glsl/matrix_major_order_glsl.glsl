/*
Build with DXC using -Od to preserve unused types:

    glslangValidator.exe -Od -V -R --aml --amb -e main -S frag -o matrix_major_order_glsl.spv matrix_major_order_glsl.glsl
    
*/
#version 450

//
// NOTE: GLSL does not have 1xN or Nx1 matrices
//

// -----------------------------------------------------------------------------
// Matrices without row / column major decoration
// -----------------------------------------------------------------------------

uniform mat2x2 mat_2x2;
uniform mat2x3 mat_2x3;
uniform mat2x4 mat_2x4;

uniform mat3x2 mat_3x2;
uniform mat3x3 mat_3x3;
uniform mat3x4 mat_3x4;

uniform mat4x2 mat_4x2;
uniform mat4x3 mat_4x3;
uniform mat4x4 mat_4x4;

// -----------------------------------------------------------------------------
// Matrices with row major decoration
// -----------------------------------------------------------------------------

layout(row_major) uniform mat2x2 row_major_my_mat_2x2;
layout(row_major) uniform mat2x3 row_major_my_mat_2x3;
layout(row_major) uniform mat2x4 row_major_my_mat_2x4;

layout(row_major) uniform mat3x2 row_major_my_mat_3x2;
layout(row_major) uniform mat3x3 row_major_my_mat_3x3;
layout(row_major) uniform mat3x4 row_major_my_mat_3x4;

layout(row_major) uniform mat4x2 row_major_my_mat_4x2;
layout(row_major) uniform mat4x3 row_major_my_mat_4x3;
layout(row_major) uniform mat4x4 row_major_my_mat_4x4;

// -----------------------------------------------------------------------------
// Matrices with column major decoration
// -----------------------------------------------------------------------------

layout(column_major) uniform mat2x2 column_major_my_mat_2x2;
layout(column_major) uniform mat2x3 column_major_my_mat_2x3;
layout(column_major) uniform mat2x4 column_major_my_mat_2x4;

layout(column_major) uniform mat3x2 column_major_my_mat_3x2;
layout(column_major) uniform mat3x3 column_major_my_mat_3x3;
layout(column_major) uniform mat3x4 column_major_my_mat_3x4;

layout(column_major) uniform mat4x2 column_major_my_mat_4x2;
layout(column_major) uniform mat4x3 column_major_my_mat_4x3;
layout(column_major) uniform mat4x4 column_major_my_mat_4x4;

layout(location = 1) in  vec4 my_input;
layout(location = 0) out vec4 my_output;

void main()
{
    my_output = my_input;
}