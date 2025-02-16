#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_ARB_enhanced_layouts : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_KHR_shader_subgroup_arithmetic : require
layout(row_major) uniform;
layout(row_major) buffer;

struct BDAGlobals_t_0 {
    vec4 g_vTest;
    vec4 g_vTest2;
};

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer BufferPointer_BDAGlobals_t_0_1 {
    BDAGlobals_t_0 _data;
};

struct GlobalsBDAPushConstant_t_0 {
    BufferPointer_BDAGlobals_t_0_1  g_GlobalsBDAPerStage_0[6];
};

layout(push_constant)
layout(std140) uniform _S2 {
    BufferPointer_BDAGlobals_t_0_1  g_GlobalsBDAPerStage_0[6];
} g_GlobalsBDAPushConstant_0;

struct PS_OUTPUT_0 {
    vec4 vColor_1;
};

layout(location = 0) out vec4 _S149;

void main() {
    _S149 = g_GlobalsBDAPushConstant_0.g_GlobalsBDAPerStage_0[0]._data.g_vTest;
    return;
}