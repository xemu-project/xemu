/* Texture-free NV2A register-combiner reference evaluator. */

#include "psh_reference.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct Input {
    unsigned reg;
    unsigned channel;
    unsigned mapping;
} Input;

typedef struct Output {
    unsigned ab;
    unsigned cd;
    unsigned mux;
    unsigned flags;
} Output;

typedef struct Evaluator {
    const PshReferenceProgram *program;
    const PshReferenceInputs *inputs;
    PshReferenceVec4 v0, v1, t[4], fog, r0, r1;
    PshReferenceVec4 color;
    PshReferenceVec4 e, f;
    unsigned stage;
    bool final;
} Evaluator;

static bool fail(char *error, size_t error_size, const char *format, ...)
{
    if (error != NULL && error_size != 0) {
        va_list ap;
        va_start(ap, format);
        vsnprintf(error, error_size, format, ap);
        va_end(ap);
        error[error_size - 1] = '\0';
    }
    return false;
}

static PshReferenceVec4 v4(float x, float y, float z, float w)
{
    PshReferenceVec4 result = { x, y, z, w };
    return result;
}

static PshReferenceVec4 add(PshReferenceVec4 a, PshReferenceVec4 b)
{
    return v4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

static PshReferenceVec4 mul(PshReferenceVec4 a, PshReferenceVec4 b)
{
    return v4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}

static PshReferenceVec4 clamp_v4(PshReferenceVec4 a, float lo, float hi)
{
    a.x = fminf(fmaxf(a.x, lo), hi);
    a.y = fminf(fmaxf(a.y, lo), hi);
    a.z = fminf(fmaxf(a.z, lo), hi);
    a.w = fminf(fmaxf(a.w, lo), hi);
    return a;
}

static float channel(PshReferenceVec4 a, unsigned channel_id)
{
    return channel_id == PS_CHANNEL_ALPHA ? a.w : a.z;
}

static bool valid_mapping(unsigned mapping)
{
    switch (mapping) {
    case PS_INPUTMAPPING_UNSIGNED_IDENTITY:
    case PS_INPUTMAPPING_UNSIGNED_INVERT:
    case PS_INPUTMAPPING_EXPAND_NORMAL:
    case PS_INPUTMAPPING_EXPAND_NEGATE:
    case PS_INPUTMAPPING_HALFBIAS_NORMAL:
    case PS_INPUTMAPPING_HALFBIAS_NEGATE:
    case PS_INPUTMAPPING_SIGNED_IDENTITY:
    case PS_INPUTMAPPING_SIGNED_NEGATE:
        return true;
    default:
        return false;
    }
}

static bool valid_register(unsigned reg)
{
    switch (reg) {
    case PS_REGISTER_ZERO:
    case PS_REGISTER_C0:
    case PS_REGISTER_C1:
    case PS_REGISTER_FOG:
    case PS_REGISTER_V0:
    case PS_REGISTER_V1:
    case PS_REGISTER_T0:
    case PS_REGISTER_T1:
    case PS_REGISTER_T2:
    case PS_REGISTER_T3:
    case PS_REGISTER_R0:
    case PS_REGISTER_R1:
    case PS_REGISTER_V1R0_SUM:
    case PS_REGISTER_EF_PROD:
        return true;
    default:
        return false;
    }
}

static bool parse_input(uint32_t encoded, Input *input, bool alpha, bool final,
                        char *error, size_t error_size, const char *what)
{
    input->reg = encoded & 0xf;
    input->channel = encoded & PS_CHANNEL_ALPHA;
    input->mapping = encoded & 0xe0;
    if ((encoded & ~0xffu) != 0 || !valid_register(input->reg)) {
        return fail(error, error_size,
                    "%s: unsupported register encoding 0x%02x", what,
                    (unsigned)encoded);
    }
    if (alpha) {
        if (input->channel != PS_CHANNEL_BLUE &&
            input->channel != PS_CHANNEL_ALPHA) {
            return fail(error, error_size, "%s: invalid alpha channel 0x%x",
                        what, input->channel);
        }
    } else if (input->channel != PS_CHANNEL_RGB &&
               input->channel != PS_CHANNEL_ALPHA) {
        return fail(error, error_size, "%s: invalid RGB channel 0x%x", what,
                    input->channel);
    }
    if (!valid_mapping(input->mapping)) {
        return fail(error, error_size, "%s: unsupported input mapping 0x%x",
                    what, input->mapping);
    }
    if (!final && (input->reg == PS_REGISTER_V1R0_SUM ||
                   input->reg == PS_REGISTER_EF_PROD)) {
        return fail(error, error_size,
                    "%s: final-only register used by ordinary stage", what);
    }
    if (final && input->mapping != PS_INPUTMAPPING_UNSIGNED_IDENTITY &&
        input->mapping != PS_INPUTMAPPING_UNSIGNED_INVERT) {
        return fail(error, error_size,
                    "%s: mapping 0x%x is invalid for final combiner", what,
                    input->mapping);
    }
    return true;
}

static bool parse_inputs(uint32_t encoded, Input in[4], bool alpha, bool final,
                         char *error, size_t error_size, const char *what)
{
    /* NV2A packs A/B/C/D in descending bytes. */
    const unsigned bytes[4] = {
        encoded >> 24,
        encoded >> 16,
        encoded >> 8,
        encoded,
    };
    const char names[4] = { 'a', 'b', 'c', 'd' };
    char label[64];
    for (unsigned i = 0; i < 4; ++i) {
        snprintf(label, sizeof(label), "%s.%c", what, names[i]);
        if (!parse_input(bytes[i] & 0xff, &in[i], alpha, final, error,
                         error_size, label)) {
            return false;
        }
    }
    return true;
}

static bool valid_destination(unsigned reg)
{
    /* Zero is the discard destination.  Texture registers are writable by
     * NV2A register combiners too, and retaining them here makes the
     * evaluator useful for the texture-free PASSTHRU sequence. */
    switch (reg) {
    case PS_REGISTER_ZERO:
    case PS_REGISTER_V0:
    case PS_REGISTER_V1:
    case PS_REGISTER_T0:
    case PS_REGISTER_T1:
    case PS_REGISTER_T2:
    case PS_REGISTER_T3:
    case PS_REGISTER_R0:
    case PS_REGISTER_R1:
        return true;
    default:
        return false;
    }
}

static bool valid_output(uint32_t encoded, bool alpha, Output *output,
                         char *error, size_t error_size, const char *what)
{
    /* Output encodings use bits 12..19 for the eight documented flags. */
    if ((encoded & ~0xfffffu) != 0) {
        return fail(error, error_size, "%s: reserved output bits 0x%08x", what,
                    encoded);
    }
    output->cd = encoded & 0xf;
    output->ab = (encoded >> 4) & 0xf;
    output->mux = (encoded >> 8) & 0xf;
    output->flags = encoded >> 12;
    if (output->flags & ~0xffu) {
        return fail(error, error_size, "%s: unsupported output flags 0x%x",
                    what, output->flags);
    }
    if (!valid_destination(output->ab) || !valid_destination(output->cd) ||
        !valid_destination(output->mux)) {
        return fail(error, error_size, "%s: unsupported output destination",
                    what);
    }
    if ((output->flags & 0x38) != PS_COMBINEROUTPUT_IDENTITY &&
        (output->flags & 0x38) != PS_COMBINEROUTPUT_BIAS &&
        (output->flags & 0x38) != PS_COMBINEROUTPUT_SHIFTLEFT_1 &&
        (output->flags & 0x38) != PS_COMBINEROUTPUT_SHIFTLEFT_1_BIAS &&
        (output->flags & 0x38) != PS_COMBINEROUTPUT_SHIFTLEFT_2 &&
        (output->flags & 0x38) != PS_COMBINEROUTPUT_SHIFTRIGHT_1) {
        return fail(error, error_size, "%s: unsupported output mapping 0x%x",
                    what, output->flags & 0x38);
    }
    if (alpha && (output->flags & (PS_COMBINEROUTPUT_AB_DOT_PRODUCT |
                                   PS_COMBINEROUTPUT_CD_DOT_PRODUCT |
                                   PS_COMBINEROUTPUT_AB_BLUE_TO_ALPHA |
                                   PS_COMBINEROUTPUT_CD_BLUE_TO_ALPHA))) {
        return fail(error, error_size,
                    "%s: RGB-only output flag used by alpha combiner", what);
    }
    return true;
}

bool psh_reference_validate(const PshReferenceProgram *program, char *error,
                            size_t error_size)
{
    if (program == NULL) {
        return fail(error, error_size, "program is null");
    }
    const unsigned stages = program->combiner_control & 0xff;
    const unsigned flags = program->combiner_control >> 8;
    if (stages > 8) {
        return fail(error, error_size, "combiner stage count %u exceeds 8",
                    stages);
    }
    if (flags & ~(PS_COMBINERCOUNT_MUX_MSB | PS_COMBINERCOUNT_UNIQUE_C0 |
                  PS_COMBINERCOUNT_UNIQUE_C1)) {
        return fail(error, error_size, "unsupported combiner flags 0x%x",
                    flags);
    }
    if (program->shader_stage_program & ~0x000fffffu) {
        return fail(error, error_size,
                    "shader stage program has reserved bits");
    }
    for (unsigned i = 0; i < 4; ++i) {
        unsigned mode = (program->shader_stage_program >> (i * 5)) & 0x1f;
        if (mode != PS_TEXTUREMODES_NONE && mode != PS_TEXTUREMODES_PASSTHRU) {
            return fail(error, error_size,
                        "texture stage %u mode 0x%x is unsupported (only "
                        "NONE/PASSTHRU)",
                        i, mode);
        }
    }
    /* NONE and PASSTHRU do not consume OTHERSTAGEINPUT.  Keep it opaque so
     * programs emitted by the hardware can retain that semantically-unused
     * register without becoming invalid. */

    Input in[4];
    Output out;
    for (unsigned i = 0; i < stages; ++i) {
        char label[48];
        snprintf(label, sizeof(label), "stage %u RGB inputs", i);
        if (!parse_inputs(program->rgb_inputs[i], in, false, false, error,
                          error_size, label)) {
            return false;
        }
        snprintf(label, sizeof(label), "stage %u alpha inputs", i);
        if (!parse_inputs(program->alpha_inputs[i], in, true, false, error,
                          error_size, label)) {
            return false;
        }
        snprintf(label, sizeof(label), "stage %u RGB output", i);
        if (!valid_output(program->rgb_outputs[i], false, &out, error,
                          error_size, label)) {
            return false;
        }
        snprintf(label, sizeof(label), "stage %u alpha output", i);
        if (!valid_output(program->alpha_outputs[i], true, &out, error,
                          error_size, label)) {
            return false;
        }
    }

    const bool has_final =
        program->final_inputs_0 != 0 || program->final_inputs_1 != 0;
    if (has_final) {
        const unsigned final_flags = program->final_inputs_1 & 0xff;
        if (final_flags & ~(PS_FINALCOMBINERSETTING_CLAMP_SUM |
                            PS_FINALCOMBINERSETTING_COMPLEMENT_V1 |
                            PS_FINALCOMBINERSETTING_COMPLEMENT_R0)) {
            return fail(error, error_size,
                        "unsupported final combiner flags 0x%x", final_flags);
        }
        Input final_e, final_f;
        if (!parse_inputs(program->final_inputs_0, in, false, true, error,
                          error_size, "final combiner A-D") ||
            !parse_input((program->final_inputs_1 >> 24) & 0xff, &final_e,
                         false, true, error, error_size, "final combiner E") ||
            !parse_input((program->final_inputs_1 >> 16) & 0xff, &final_f,
                         false, true, error, error_size, "final combiner F")) {
            return false;
        }
        if (final_e.reg == PS_REGISTER_EF_PROD ||
            final_f.reg == PS_REGISTER_EF_PROD) {
            return fail(error, error_size,
                        "final combiner E/F cannot use EF_PROD");
        }
        /* G is the third byte in final_inputs_1 and is an alpha input. */
        if (!parse_input((program->final_inputs_1 >> 8) & 0xff, &in[0], true,
                         true, error, error_size, "final combiner G")) {
            return false;
        }
    }
    return true;
}

static PshReferenceVec4 source(Evaluator *ev, Input input)
{
    PshReferenceVec4 value;
    switch (input.reg) {
    case PS_REGISTER_ZERO:
        value = v4(0, 0, 0, 0);
        break;
    case PS_REGISTER_C0:
        value = ev->inputs->c0[ev->final ? 8 :
                                           ((ev->program->combiner_control >>
                                             8) & PS_COMBINERCOUNT_UNIQUE_C0 ?
                                                ev->stage :
                                                0)];
        break;
    case PS_REGISTER_C1:
        value = ev->inputs->c1[ev->final ? 8 :
                                           ((ev->program->combiner_control >>
                                             8) & PS_COMBINERCOUNT_UNIQUE_C1 ?
                                                ev->stage :
                                                0)];
        break;
    case PS_REGISTER_FOG:
        value = ev->fog;
        break;
    case PS_REGISTER_V0:
        value = ev->v0;
        break;
    case PS_REGISTER_V1:
        value = ev->v1;
        break;
    case PS_REGISTER_T0:
    case PS_REGISTER_T1:
    case PS_REGISTER_T2:
    case PS_REGISTER_T3:
        value = ev->t[input.reg - PS_REGISTER_T0];
        break;
    case PS_REGISTER_R0:
        value = ev->r0;
        break;
    case PS_REGISTER_R1:
        value = ev->r1;
        break;
    case PS_REGISTER_V1R0_SUM: {
        PshReferenceVec4 v1 = ev->v1;
        PshReferenceVec4 r0 = ev->r0;
        const unsigned flags = ev->program->final_inputs_1 & 0xff;
        if (flags & PS_FINALCOMBINERSETTING_COMPLEMENT_V1) {
            v1.x = 1 - v1.x;
            v1.y = 1 - v1.y;
            v1.z = 1 - v1.z;
        }
        if (flags & PS_FINALCOMBINERSETTING_COMPLEMENT_R0) {
            r0.x = 1 - r0.x;
            r0.y = 1 - r0.y;
            r0.z = 1 - r0.z;
        }
        value = add(v1, r0);
        value.w = 0;
        if (flags & PS_FINALCOMBINERSETTING_CLAMP_SUM) {
            value.x = fminf(fmaxf(value.x, 0), 1);
            value.y = fminf(fmaxf(value.y, 0), 1);
            value.z = fminf(fmaxf(value.z, 0), 1);
        }
        break;
    }
    case PS_REGISTER_EF_PROD:
        /* GLSL authority expands this as vec4(varE * varF, 0.0) in
         * psh.c:get_var(), lines 358-361; retain the intended E*F product. */
        value = mul(ev->e, ev->f);
        value.w = 0;
        break;
    default:
        value = v4(0, 0, 0, 0);
        break;
    }
    return value;
}

static PshReferenceVec4 mapped(Evaluator *ev, Input input, bool alpha)
{
    PshReferenceVec4 raw;
    switch (input.reg) {
    case PS_REGISTER_ZERO:
        raw = v4(0, 0, 0, 0);
        break;
    case PS_REGISTER_C0:
        raw = ev->inputs->c0[ev->final ? 8 :
                                         ((ev->program->combiner_control >> 8) &
                                                  PS_COMBINERCOUNT_UNIQUE_C0 ?
                                              ev->stage :
                                              0)];
        break;
    case PS_REGISTER_C1:
        raw = ev->inputs->c1[ev->final ? 8 :
                                         ((ev->program->combiner_control >> 8) &
                                                  PS_COMBINERCOUNT_UNIQUE_C1 ?
                                              ev->stage :
                                              0)];
        break;
    case PS_REGISTER_FOG:
        raw = ev->fog;
        break;
    case PS_REGISTER_V0:
        raw = ev->v0;
        break;
    case PS_REGISTER_V1:
        raw = ev->v1;
        break;
    case PS_REGISTER_T0:
    case PS_REGISTER_T1:
    case PS_REGISTER_T2:
    case PS_REGISTER_T3:
        raw = ev->t[input.reg - PS_REGISTER_T0];
        break;
    case PS_REGISTER_R0:
        raw = ev->r0;
        break;
    case PS_REGISTER_R1:
        raw = ev->r1;
        break;
    case PS_REGISTER_V1R0_SUM:
        raw = source(ev, input);
        break;
    case PS_REGISTER_EF_PROD:
        raw = source(ev, input);
        break;
    default:
        raw = v4(0, 0, 0, 0);
        break;
    }
    if (alpha) {
        float a = channel(raw, input.channel);
        raw = v4(a, a, a, a);
    } else if (input.channel == PS_CHANNEL_ALPHA) {
        raw = v4(raw.w, raw.w, raw.w, raw.w);
    }
    switch (input.mapping) {
    case PS_INPUTMAPPING_UNSIGNED_IDENTITY:
        raw.x = fmaxf(raw.x, 0);
        raw.y = fmaxf(raw.y, 0);
        raw.z = fmaxf(raw.z, 0);
        raw.w = fmaxf(raw.w, 0);
        break;
    case PS_INPUTMAPPING_UNSIGNED_INVERT:
        raw.x = 1 - fminf(fmaxf(raw.x, 0), 1);
        raw.y = 1 - fminf(fmaxf(raw.y, 0), 1);
        raw.z = 1 - fminf(fmaxf(raw.z, 0), 1);
        raw.w = 1 - fminf(fmaxf(raw.w, 0), 1);
        break;
    case PS_INPUTMAPPING_EXPAND_NORMAL:
        raw.x = 2 * fmaxf(raw.x, 0) - 1;
        raw.y = 2 * fmaxf(raw.y, 0) - 1;
        raw.z = 2 * fmaxf(raw.z, 0) - 1;
        raw.w = 2 * fmaxf(raw.w, 0) - 1;
        break;
    case PS_INPUTMAPPING_EXPAND_NEGATE:
        raw.x = 1 - 2 * fmaxf(raw.x, 0);
        raw.y = 1 - 2 * fmaxf(raw.y, 0);
        raw.z = 1 - 2 * fmaxf(raw.z, 0);
        raw.w = 1 - 2 * fmaxf(raw.w, 0);
        break;
    case PS_INPUTMAPPING_HALFBIAS_NORMAL:
        raw.x = fmaxf(raw.x, 0) - .5f;
        raw.y = fmaxf(raw.y, 0) - .5f;
        raw.z = fmaxf(raw.z, 0) - .5f;
        raw.w = fmaxf(raw.w, 0) - .5f;
        break;
    case PS_INPUTMAPPING_HALFBIAS_NEGATE:
        raw.x = .5f - fmaxf(raw.x, 0);
        raw.y = .5f - fmaxf(raw.y, 0);
        raw.z = .5f - fmaxf(raw.z, 0);
        raw.w = .5f - fmaxf(raw.w, 0);
        break;
    case PS_INPUTMAPPING_SIGNED_NEGATE:
        raw.x = -raw.x;
        raw.y = -raw.y;
        raw.z = -raw.z;
        raw.w = -raw.w;
        break;
    case PS_INPUTMAPPING_SIGNED_IDENTITY:
        break;
    }
    if (alpha) {
        raw.x = raw.y = raw.z = raw.w = raw.x;
    }
    return raw;
}

static PshReferenceVec4 output_map(PshReferenceVec4 a, unsigned mapping)
{
    switch (mapping) {
    case PS_COMBINEROUTPUT_BIAS:
        a.x -= .5f;
        a.y -= .5f;
        a.z -= .5f;
        a.w -= .5f;
        break;
    case PS_COMBINEROUTPUT_SHIFTLEFT_1:
        a = mul(a, v4(2, 2, 2, 2));
        break;
    case PS_COMBINEROUTPUT_SHIFTLEFT_1_BIAS:
        a.x = (a.x - .5f) * 2;
        a.y = (a.y - .5f) * 2;
        a.z = (a.z - .5f) * 2;
        a.w = (a.w - .5f) * 2;
        break;
    case PS_COMBINEROUTPUT_SHIFTLEFT_2:
        a = mul(a, v4(4, 4, 4, 4));
        break;
    case PS_COMBINEROUTPUT_SHIFTRIGHT_1:
        a = mul(a, v4(.5f, .5f, .5f, .5f));
        break;
    case PS_COMBINEROUTPUT_IDENTITY:
        break;
    }
    return a;
}

static void write(Evaluator *ev, unsigned reg, bool alpha,
                  PshReferenceVec4 value, float blue_to_alpha)
{
    PshReferenceVec4 *dst = NULL;
    switch (reg) {
    case PS_REGISTER_V0:
        dst = &ev->v0;
        break;
    case PS_REGISTER_V1:
        dst = &ev->v1;
        break;
    case PS_REGISTER_T0:
        dst = &ev->t[0];
        break;
    case PS_REGISTER_T1:
        dst = &ev->t[1];
        break;
    case PS_REGISTER_T2:
        dst = &ev->t[2];
        break;
    case PS_REGISTER_T3:
        dst = &ev->t[3];
        break;
    case PS_REGISTER_R0:
        dst = &ev->r0;
        break;
    case PS_REGISTER_R1:
        dst = &ev->r1;
        break;
    default:
        return;
    }
    if (alpha)
        dst->w = value.w;
    else {
        dst->x = value.x;
        dst->y = value.y;
        dst->z = value.z;
        if (!isnan(blue_to_alpha))
            dst->w = blue_to_alpha;
    }
}

static bool stage(Evaluator *ev, unsigned index, uint32_t in_raw,
                  uint32_t out_raw, bool alpha, char *error, size_t error_size)
{
    Input in[4];
    Output out;
    if (!parse_inputs(in_raw, in, alpha, false, error, error_size,
                      "stage input") ||
        !valid_output(out_raw, alpha, &out, error, error_size, "stage output"))
        return false;
    PshReferenceVec4 a = mapped(ev, in[0], alpha), b = mapped(ev, in[1], alpha),
                     c = mapped(ev, in[2], alpha), d = mapped(ev, in[3], alpha);
    PshReferenceVec4 ab = (out.flags & PS_COMBINEROUTPUT_AB_DOT_PRODUCT) ?
                              v4(a.x * b.x + a.y * b.y + a.z * b.z,
                                 a.x * b.x + a.y * b.y + a.z * b.z,
                                 a.x * b.x + a.y * b.y + a.z * b.z,
                                 a.x * b.x + a.y * b.y + a.z * b.z) :
                              mul(a, b);
    PshReferenceVec4 cd = (out.flags & PS_COMBINEROUTPUT_CD_DOT_PRODUCT) ?
                              v4(c.x * d.x + c.y * d.y + c.z * d.z,
                                 c.x * d.x + c.y * d.y + c.z * d.z,
                                 c.x * d.x + c.y * d.y + c.z * d.z,
                                 c.x * d.x + c.y * d.y + c.z * d.z) :
                              mul(c, d);
    PshReferenceVec4 mux;
    if (out.flags & PS_COMBINEROUTPUT_AB_CD_MUX) {
        bool select_cd;
        if ((ev->program->combiner_control >> 8) & PS_COMBINERCOUNT_MUX_MSB) {
            select_cd = ev->r0.w >= .5f;
        } else {
            /* The register is normally stage-clamped, but blue-to-alpha can
             * supply arbitrary caller data.  Keep the conversion defined for
             * NaN, infinity, and values outside the byte range. */
            const float mux_alpha =
                isfinite(ev->r0.w) ? fminf(fmaxf(ev->r0.w, 0.0f), 1.0f) : 0;
            select_cd =
                mux_alpha > 0.0f && ((uint32_t)(mux_alpha * 255.0f) & 1u);
        }
        mux = select_cd ? cd : ab;
    } else {
        mux = add(ab, cd);
    }
    const unsigned mapping = out.flags & 0x38;
    write(ev, out.ab, alpha, clamp_v4(output_map(ab, mapping), -1, 1),
          (!alpha && (out.flags & PS_COMBINEROUTPUT_AB_BLUE_TO_ALPHA)) ? ab.z :
                                                                         NAN);
    write(ev, out.cd, alpha, clamp_v4(output_map(cd, mapping), -1, 1),
          (!alpha && (out.flags & PS_COMBINEROUTPUT_CD_BLUE_TO_ALPHA)) ? cd.z :
                                                                         NAN);
    write(ev, out.mux, alpha, clamp_v4(output_map(mux, mapping), -1, 1), NAN);
    (void)index;
    return true;
}

bool psh_reference_evaluate(const PshReferenceProgram *program,
                            const PshReferenceInputs *inputs,
                            PshReferenceResult *result, char *error,
                            size_t error_size)
{
    if (inputs == NULL || result == NULL)
        return fail(error, error_size, "inputs/result is null");
    if (!psh_reference_validate(program, error, error_size))
        return false;
    Evaluator ev = { .program = program,
                     .inputs = inputs,
                     .v0 = inputs->v0,
                     .v1 = inputs->v1,
                     .fog = inputs->fog };
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned mode = (program->shader_stage_program >> (i * 5)) & 0x1f;
        ev.t[i] = mode == PS_TEXTUREMODES_NONE ? v4(0, 0, 0, 1) : inputs->t[i];
    }
    ev.fog.w = fminf(fmaxf(ev.fog.w, 0), 1);
    ev.r0 =
        v4(0, 0, 0,
           (((program->shader_stage_program & 0x1f) == PS_TEXTUREMODES_NONE) ?
                1 :
                ev.t[0].w));
    ev.r1 = v4(0, 0, 0, 0);
    const unsigned stages = program->combiner_control & 0xff;
    for (unsigned i = 0; i < stages; i++) {
        ev.stage = i;
        if (!stage(&ev, i, program->rgb_inputs[i], program->rgb_outputs[i],
                   false, error, error_size) ||
            !stage(&ev, i, program->alpha_inputs[i], program->alpha_outputs[i],
                   true, error, error_size))
            return false;
    }
    if (program->final_inputs_0 || program->final_inputs_1) {
        ev.final = true;
        ev.stage = 8;
        Input q[4];
        if (!parse_inputs(program->final_inputs_0, q, false, true, error,
                          error_size, "final A-D"))
            return false;
        Input ef[2];
        if (!parse_input((program->final_inputs_1 >> 24) & 0xff, &ef[0], false,
                         true, error, error_size, "final E") ||
            !parse_input((program->final_inputs_1 >> 16) & 0xff, &ef[1], false,
                         true, error, error_size, "final F"))
            return false;
        ev.e = mapped(&ev, ef[0], false);
        ev.f = mapped(&ev, ef[1], false);
        PshReferenceVec4 a = mapped(&ev, q[0], false),
                         b = mapped(&ev, q[1], false),
                         c = mapped(&ev, q[2], false),
                         d = mapped(&ev, q[3], false);
        Input g;
        if (!parse_input((program->final_inputs_1 >> 8) & 0xff, &g, true, true,
                         error, error_size, "final G"))
            return false;
        ev.color =
            add(d, add(mul(c, v4(1 - a.x, 1 - a.y, 1 - a.z, 1)), mul(b, a)));
        ev.color.w = mapped(&ev, g, true).x;
    } else
        ev.color = ev.v0;
    result->color = ev.color;
    result->r0 = ev.r0;
    result->r1 = ev.r1;
    return true;
}
