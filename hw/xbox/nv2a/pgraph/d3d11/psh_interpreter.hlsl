// Universal, texture-free NV2A register-combiner interpreter.
// Compile offline with psh_interpreter.ps1; runtime never calls D3DCompile.

cbuffer PshInterpreterConstants : register(b0) {
    uint4 program;
    uint4 program1;
    uint4 rgb_inputs[8];
    uint4 alpha_inputs[8];
    uint4 rgb_outputs[8];
    uint4 alpha_outputs[8];
    float4 v0;
    float4 v1;
    float4 t[4];
    float4 fog;
    float4 c0[9];
    float4 c1[9];
};

struct VSInput {
    float3 position : POSITION;
    float4 v0 : COLOR0;
    float4 v1 : COLOR1;
    float4 t0 : TEXCOORD0;
    float4 t1 : TEXCOORD1;
    float4 t2 : TEXCOORD2;
    float4 t3 : TEXCOORD3;
    float4 fog : TEXCOORD4;
};

struct VSOutput {
    float4 position : SV_Position;
    float4 v0 : COLOR0;
    float4 v1 : COLOR1;
    float4 t0 : TEXCOORD0;
    float4 t1 : TEXCOORD1;
    float4 t2 : TEXCOORD2;
    float4 t3 : TEXCOORD3;
    float4 fog : TEXCOORD4;
};

VSOutput vs_main(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0f);
    output.v0 = input.v0;
    output.v1 = input.v1;
    output.t0 = input.t0;
    output.t1 = input.t1;
    output.t2 = input.t2;
    output.t3 = input.t3;
    output.fog = input.fog;
    return output;
}

struct PshState {
    float4 v0, v1, t[4], fog, r0, r1;
    float4 e, f;
    bool final;
    uint stage;
};

float4 source(PshState state, uint encoded)
{
    uint reg = encoded & 0xf;
    float4 value = 0;
    if (reg == 1) value = state.final ? c0[8] : c0[((program.x & 0x1000) != 0) ? state.stage : 0];
    else if (reg == 2) value = state.final ? c1[8] : c1[((program.x & 0x10000) != 0) ? state.stage : 0];
    else if (reg == 3) value = state.fog;
    else if (reg == 4) value = state.v0;
    else if (reg == 5) value = state.v1;
    else if (reg == 8) value = state.t[0];
    else if (reg == 9) value = state.t[1];
    else if (reg == 10) value = state.t[2];
    else if (reg == 11) value = state.t[3];
    else if (reg == 12) value = state.r0;
    else if (reg == 13) value = state.r1;
    else if (reg == 14) {
        float4 a = state.v1;
        float4 b = state.r0;
        if ((program1.x & 0x40) != 0) a.rgb = 1.0 - a.rgb;
        if ((program1.x & 0x20) != 0) b.rgb = 1.0 - b.rgb;
        value = float4(a.rgb + b.rgb, 0.0);
        if ((program1.x & 0x80) != 0) value.rgb = saturate(value.rgb);
    } else if (reg == 15) {
        value = float4(state.e.rgb * state.f.rgb, 0.0);
    }
    return value;
}

float4 mapped(PshState state, uint encoded, bool alpha)
{
    float4 value = source(state, encoded);
    uint channel = encoded & 0x10;
    if (alpha) {
        float x = (channel == 0) ? value.b : value.a;
        value = x.xxxx;
    } else if (channel != 0) {
        value = value.a.xxxx;
    }
    uint mapping = encoded & 0xe0;
    if (mapping == 0x00) value = max(value, 0.0);
    else if (mapping == 0x20) value = 1.0 - saturate(value);
    else if (mapping == 0x40) value = 2.0 * max(value, 0.0) - 1.0;
    else if (mapping == 0x60) value = 1.0 - 2.0 * max(value, 0.0);
    else if (mapping == 0x80) value = max(value, 0.0) - 0.5;
    else if (mapping == 0xa0) value = 0.5 - max(value, 0.0);
    else if (mapping == 0xe0) value = -value;
    if (alpha) value = value.xxxx;
    return value;
}

float4 output_map(float4 value, uint flags)
{
    uint mapping = flags & 0x38;
    if (mapping == 0x08) value -= 0.5;
    else if (mapping == 0x10) value *= 2.0;
    else if (mapping == 0x18) value = (value - 0.5) * 2.0;
    else if (mapping == 0x20) value *= 4.0;
    else if (mapping == 0x30) value *= 0.5;
    return value;
}

PshState run_stage(PshState state, uint4 input, uint4 output, bool alpha)
{
    uint a_code = input.x, b_code = input.y, c_code = input.z, d_code = input.w;
    float4 a = mapped(state, a_code, alpha), b = mapped(state, b_code, alpha);
    float4 c = mapped(state, c_code, alpha), d = mapped(state, d_code, alpha);
    uint flags = output.x >> 12;
    float4 ab = a * b;
    float4 cd = c * d;
    if ((flags & 2) != 0) {
        float value = dot(a.rgb, b.rgb);
        ab = alpha ? float4(0, 0, 0, 0) : float4(value, value, value, value);
    }
    if ((flags & 1) != 0) {
        float value = dot(c.rgb, d.rgb);
        cd = alpha ? float4(0, 0, 0, 0) : float4(value, value, value, value);
    }
    uint mux_lsb = 0;
    float mux_alpha = min(max(state.r0.a, 0.0), 1.0);
    if (mux_alpha > 0.0) {
        mux_lsb = (uint)(mux_alpha * 255.0);
    }
    float4 mux = ((flags & 4) != 0) ?
        (((program.x & 0x100) != 0) ? ((state.r0.a >= 0.5) ? cd : ab) :
         (((mux_lsb & 1) != 0) ? cd : ab)) : ab + cd;
    float4 ab_m = saturate(output_map(ab, flags) * 0.5 + 0.5) * 2.0 - 1.0;
    float4 cd_m = saturate(output_map(cd, flags) * 0.5 + 0.5) * 2.0 - 1.0;
    float4 mux_m = saturate(output_map(mux, flags) * 0.5 + 0.5) * 2.0 - 1.0;
    uint ab_reg = (output.x >> 4) & 0xf, cd_reg = output.x & 0xf, mux_reg = (output.x >> 8) & 0xf;
    PshState updated = state;
    if (alpha) {
        if (ab_reg == 4) updated.v0.a = ab_m.a; else if (ab_reg == 5) updated.v1.a = ab_m.a; else if (ab_reg == 8) updated.t[0].a = ab_m.a; else if (ab_reg == 9) updated.t[1].a = ab_m.a; else if (ab_reg == 10) updated.t[2].a = ab_m.a; else if (ab_reg == 11) updated.t[3].a = ab_m.a; else if (ab_reg == 12) updated.r0.a = ab_m.a; else if (ab_reg == 13) updated.r1.a = ab_m.a;
        if (cd_reg == 4) updated.v0.a = cd_m.a; else if (cd_reg == 5) updated.v1.a = cd_m.a; else if (cd_reg == 8) updated.t[0].a = cd_m.a; else if (cd_reg == 9) updated.t[1].a = cd_m.a; else if (cd_reg == 10) updated.t[2].a = cd_m.a; else if (cd_reg == 11) updated.t[3].a = cd_m.a; else if (cd_reg == 12) updated.r0.a = cd_m.a; else if (cd_reg == 13) updated.r1.a = cd_m.a;
        if (mux_reg == 4) updated.v0.a = mux_m.a; else if (mux_reg == 5) updated.v1.a = mux_m.a; else if (mux_reg == 8) updated.t[0].a = mux_m.a; else if (mux_reg == 9) updated.t[1].a = mux_m.a; else if (mux_reg == 10) updated.t[2].a = mux_m.a; else if (mux_reg == 11) updated.t[3].a = mux_m.a; else if (mux_reg == 12) updated.r0.a = mux_m.a; else if (mux_reg == 13) updated.r1.a = mux_m.a;
    } else {
        if (ab_reg == 4) updated.v0.rgb = ab_m.rgb; else if (ab_reg == 5) updated.v1.rgb = ab_m.rgb; else if (ab_reg == 8) updated.t[0].rgb = ab_m.rgb; else if (ab_reg == 9) updated.t[1].rgb = ab_m.rgb; else if (ab_reg == 10) updated.t[2].rgb = ab_m.rgb; else if (ab_reg == 11) updated.t[3].rgb = ab_m.rgb; else if (ab_reg == 12) updated.r0.rgb = ab_m.rgb; else if (ab_reg == 13) updated.r1.rgb = ab_m.rgb;
        if (cd_reg == 4) updated.v0.rgb = cd_m.rgb; else if (cd_reg == 5) updated.v1.rgb = cd_m.rgb; else if (cd_reg == 8) updated.t[0].rgb = cd_m.rgb; else if (cd_reg == 9) updated.t[1].rgb = cd_m.rgb; else if (cd_reg == 10) updated.t[2].rgb = cd_m.rgb; else if (cd_reg == 11) updated.t[3].rgb = cd_m.rgb; else if (cd_reg == 12) updated.r0.rgb = cd_m.rgb; else if (cd_reg == 13) updated.r1.rgb = cd_m.rgb;
        if (mux_reg == 4) updated.v0.rgb = mux_m.rgb; else if (mux_reg == 5) updated.v1.rgb = mux_m.rgb; else if (mux_reg == 8) updated.t[0].rgb = mux_m.rgb; else if (mux_reg == 9) updated.t[1].rgb = mux_m.rgb; else if (mux_reg == 10) updated.t[2].rgb = mux_m.rgb; else if (mux_reg == 11) updated.t[3].rgb = mux_m.rgb; else if (mux_reg == 12) updated.r0.rgb = mux_m.rgb; else if (mux_reg == 13) updated.r1.rgb = mux_m.rgb;
        if ((flags & 0x80) != 0) { if (ab_reg == 4) updated.v0.a = ab.b; else if (ab_reg == 5) updated.v1.a = ab.b; else if (ab_reg == 8) updated.t[0].a = ab.b; else if (ab_reg == 9) updated.t[1].a = ab.b; else if (ab_reg == 10) updated.t[2].a = ab.b; else if (ab_reg == 11) updated.t[3].a = ab.b; else if (ab_reg == 12) updated.r0.a = ab.b; else if (ab_reg == 13) updated.r1.a = ab.b; }
        if ((flags & 0x40) != 0) { if (cd_reg == 4) updated.v0.a = cd.b; else if (cd_reg == 5) updated.v1.a = cd.b; else if (cd_reg == 8) updated.t[0].a = cd.b; else if (cd_reg == 9) updated.t[1].a = cd.b; else if (cd_reg == 10) updated.t[2].a = cd.b; else if (cd_reg == 11) updated.t[3].a = cd.b; else if (cd_reg == 12) updated.r0.a = cd.b; else if (cd_reg == 13) updated.r1.a = cd.b; }
    }
    return updated;
}

float4 ps_main(VSOutput input) : SV_Target
{
    PshState state;
    state.v0 = input.v0; state.v1 = input.v1;
    state.t[0] = ((program.y & 0x1f) == 0) ? float4(0, 0, 0, 1) : input.t0;
    state.t[1] = (((program.y >> 5) & 0x1f) == 0) ? float4(0, 0, 0, 1) : input.t1;
    state.t[2] = (((program.y >> 10) & 0x1f) == 0) ? float4(0, 0, 0, 1) : input.t2;
    state.t[3] = (((program.y >> 15) & 0x1f) == 0) ? float4(0, 0, 0, 1) : input.t3;
    state.fog = float4(input.fog.rgb, saturate(input.fog.a));
    state.r0 = float4(0, 0, 0, ((program.y & 0x1f) == 0) ? 1.0 : state.t[0].a);
    state.r1 = 0;
    state.e = 0;
    state.f = 0;
    state.final = false;
    state.stage = 0;
    uint stages = min(program.x & 0xff, 8u);
    if (stages > 0) { state.stage = 0; state = run_stage(state, rgb_inputs[0], rgb_outputs[0], false); state = run_stage(state, alpha_inputs[0], alpha_outputs[0], true); }
    if (stages > 1) { state.stage = 1; state = run_stage(state, rgb_inputs[1], rgb_outputs[1], false); state = run_stage(state, alpha_inputs[1], alpha_outputs[1], true); }
    if (stages > 2) { state.stage = 2; state = run_stage(state, rgb_inputs[2], rgb_outputs[2], false); state = run_stage(state, alpha_inputs[2], alpha_outputs[2], true); }
    if (stages > 3) { state.stage = 3; state = run_stage(state, rgb_inputs[3], rgb_outputs[3], false); state = run_stage(state, alpha_inputs[3], alpha_outputs[3], true); }
    if (stages > 4) { state.stage = 4; state = run_stage(state, rgb_inputs[4], rgb_outputs[4], false); state = run_stage(state, alpha_inputs[4], alpha_outputs[4], true); }
    if (stages > 5) { state.stage = 5; state = run_stage(state, rgb_inputs[5], rgb_outputs[5], false); state = run_stage(state, alpha_inputs[5], alpha_outputs[5], true); }
    if (stages > 6) { state.stage = 6; state = run_stage(state, rgb_inputs[6], rgb_outputs[6], false); state = run_stage(state, alpha_inputs[6], alpha_outputs[6], true); }
    if (stages > 7) { state.stage = 7; state = run_stage(state, rgb_inputs[7], rgb_outputs[7], false); state = run_stage(state, alpha_inputs[7], alpha_outputs[7], true); }
    if (program.w != 0 || program1.x != 0) {
        state.final = true; state.stage = 8;
        state.e = mapped(state, (program1.x >> 24) & 0xff, false);
        state.f = mapped(state, (program1.x >> 16) & 0xff, false);
        float4 a = mapped(state, (program.w >> 24) & 0xff, false);
        float4 b = mapped(state, (program.w >> 16) & 0xff, false);
        float4 c = mapped(state, (program.w >> 8) & 0xff, false);
        float4 d = mapped(state, program.w & 0xff, false);
        float4 final_color = d + c * (1.0 - a) + b * a;
        final_color.a = mapped(state, (program1.x >> 8) & 0xff, true).x;
        return final_color;
    }
    return state.v0;
}
