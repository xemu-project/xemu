/*
 * x87 FPU support
 *
 * Copyright (c) 2021 Matt Borgerson
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#define PRECf glue(PREC, f)
#define fPREC glue(f, PREC)
#define PREC_SUFFIX glue(_, fPREC)
#define PREC_TYPE glue(TCGv_, fPREC)
#define tcg_temp_new_fp glue(tcg_temp_new_, fPREC)
#define tcg_temp_free_fp glue(tcg_temp_free_, fPREC)
#define tcg_gen_st80f_fp glue(tcg_gen_st80f, PREC_SUFFIX)
#define tcg_gen_ld80f_fp glue(tcg_gen_ld80f, PREC_SUFFIX)
#define get_ft0 glue(get_ft0, PREC_SUFFIX)
#define get_stn glue(get_stn, PREC_SUFFIX)
#define get_st0 glue(get_st0, PREC_SUFFIX)

static PREC_TYPE get_ft0(DisasContext *s)
{
    gen_flcr(s);

    PREC_TYPE *v = (PREC_TYPE *)&s->ft0;

    if (*v == NULL) {
        *v = tcg_temp_new_fp();
        TCGv_ptr p = gen_ft0_ptr();
        tcg_gen_ld80f_fp(*v, p);
        tcg_temp_free_ptr(p);
    }

    return *v;
}

static PREC_TYPE get_stn(DisasContext *s, int opreg)
{
    assert(!(opreg & ~7));
    gen_flcr(s);

    PREC_TYPE *t = (PREC_TYPE *)&s->fpregs[(s->fpstt_delta + opreg) & 7];

    if (*t == NULL) {
        *t = tcg_temp_new_fp();
        TCGv_ptr p = gen_stn_ptr(opreg);
        tcg_gen_ld80f_fp(*t, p);
        tcg_temp_free_ptr(p);
    }

    return *t;
}

static PREC_TYPE get_st0(DisasContext *s)
{
    return get_stn(s, 0);
}

static void glue(flush_fp_regs, PREC_SUFFIX)(DisasContext *s)
{
    for (int i = 0; i < 8; i++) {
        PREC_TYPE *t = (PREC_TYPE *)&s->fpregs[(s->fpstt_delta + i) & 7];
        if (*t) {
            TCGv_ptr ptr = gen_stn_ptr(i);
            tcg_gen_st80f_fp(*t, ptr);
            tcg_temp_free_fp(*t);
            tcg_temp_free_ptr(ptr);
            *t = NULL;
        }
   }

    if (s->ft0) {
        TCGv_ptr ptr = gen_ft0_ptr();
        tcg_gen_st80f_fp((PREC_TYPE)s->ft0, ptr);
        tcg_temp_free_ptr(ptr);
        s->ft0 = NULL;
    }
}

static void glue(gen_fpop, PREC_SUFFIX)(DisasContext *s)
{
    PREC_TYPE *t = (PREC_TYPE *)&s->fpregs[s->fpstt_delta & 7];
    if (*t) {
        tcg_temp_free_fp(*t);
        *t = NULL;
    }
}

static void glue(gen_fcom, PREC_SUFFIX)(DisasContext *s, PREC_TYPE arg1,
                                        PREC_TYPE arg2)
{
    TCGv_i64 res = tcg_temp_new_i64();

    glue(tcg_gen_com, PREC_SUFFIX)(res, arg1, arg2);

    /*
     * Result is EFLAGS register format as follows
     *
     *                C3 C2 C0
     * arg1 > arg2    0  0  0
     * arg1 < arg2    0  0  1
     * arg1 = arg2    1  0  0
     * unordered      1  1  1
     *
     * C3,C2,C0 = ZF,PF,CF = Bit 6,2,0
     *
     * fpus =  {0x0100, 0x4000, 0x0000, 0x4500};
     *          <       =       >       UO
     */

    tcg_gen_andi_i64(res, res, 0x45);
    tcg_gen_shli_i64(res, res, 8);

    TCGv_i64 fpus = tcg_temp_new_i64();
    tcg_gen_ld16u_i64(fpus, cpu_env, offsetof(CPUX86State, fpus));
    tcg_gen_andi_i64(fpus, fpus, ~0x4500);
    tcg_gen_or_i64(fpus, fpus, res);
    tcg_gen_st16_i64(fpus, cpu_env, offsetof(CPUX86State, fpus));

    tcg_temp_free_i64(fpus);
    tcg_temp_free_i64(res);

    /* FIXME: Exceptions */
}

/* FIXME: This decode logic should be shared with helper variant */

static void glue(gen_helper_fp_arith_ST0_FT0, PREC_SUFFIX)(DisasContext *s,
                                                           int op)
{
    PREC_TYPE st0 = get_st0(s);
    PREC_TYPE ft0 = get_ft0(s);

    switch (op) {
    case 0:
        glue(tcg_gen_add, PREC_SUFFIX)(st0, st0, ft0);
        break;
    case 1:
        glue(tcg_gen_mul, PREC_SUFFIX)(st0, st0, ft0);
        break;
    case 2:
    case 3:
        glue(gen_fcom, PREC_SUFFIX)(s, st0, ft0);
        break;
    case 4:
        glue(tcg_gen_sub, PREC_SUFFIX)(st0, st0, ft0);
        break;
    case 5:
        glue(tcg_gen_sub, PREC_SUFFIX)(st0, ft0, st0);
        break;
    case 6:
        glue(tcg_gen_div, PREC_SUFFIX)(st0, st0, ft0);
        break;
    case 7:
        glue(tcg_gen_div, PREC_SUFFIX)(st0, ft0, st0);
        break;
    default:
        g_assert_not_reached();
    }
}

static void glue(gen_helper_fp_arith_STN_ST0, PREC_SUFFIX)(DisasContext *s,
                                                           int op,
                                                           int opreg)
{
    PREC_TYPE stn = get_stn(s, opreg);
    PREC_TYPE st0 = get_st0(s);

    switch (op) {
    case 0:
        glue(tcg_gen_add, PREC_SUFFIX)(stn, stn, st0);
        break;
    case 1:
        glue(tcg_gen_mul, PREC_SUFFIX)(stn, stn, st0);
        break;
    case 4:
        glue(tcg_gen_sub, PREC_SUFFIX)(stn, st0, stn);
        break;
    case 5:
        glue(tcg_gen_sub, PREC_SUFFIX)(stn, stn, st0);
        break;
    case 6:
        glue(tcg_gen_div, PREC_SUFFIX)(stn, st0, stn);
        break;
    case 7:
        glue(tcg_gen_div, PREC_SUFFIX)(stn, stn, st0);
        break;
    default:
        g_assert_not_reached();
    }
}

static void glue(gen_fmov_FT0_STN, PREC_SUFFIX)(DisasContext *s, int st_index)
{
    glue(tcg_gen_mov, PREC_SUFFIX)(get_ft0(s), get_stn(s, st_index));
}

static void glue(gen_fmov_ST0_STN, PREC_SUFFIX)(DisasContext *s, int st_index)
{
    glue(tcg_gen_mov, PREC_SUFFIX)(get_st0(s), get_stn(s, st_index));
}

static void glue(gen_fmov_STN_ST0, PREC_SUFFIX)(DisasContext *s, int st_index)
{
    glue(tcg_gen_mov, PREC_SUFFIX)(get_stn(s, st_index), get_st0(s));
}

static void glue(gen_flds_FT0, PREC_SUFFIX)(DisasContext *s, TCGv_i32 arg)
{
    glue(gen_mov32i, PREC_SUFFIX)(get_ft0(s), arg);
}

static void glue(gen_flds_ST0, PREC_SUFFIX)(DisasContext *s, TCGv_i32 arg)
{
    glue(gen_mov32i, PREC_SUFFIX)(get_st0(s), arg);
}

static void glue(gen_fldl_FT0, PREC_SUFFIX)(DisasContext *s, TCGv_i64 arg)
{
    glue(gen_mov64i, PREC_SUFFIX)(get_ft0(s), arg);
}

static void glue(gen_fldl_ST0, PREC_SUFFIX)(DisasContext *s, TCGv_i64 arg)
{
    glue(gen_mov64i, PREC_SUFFIX)(get_st0(s), arg);
}

static void glue(gen_fildl_FT0, PREC_SUFFIX)(DisasContext *s, TCGv_i32 arg)
{
    glue(tcg_gen_cvt32i, PREC_SUFFIX)(get_ft0(s), arg);
}

static void glue(gen_fildl_ST0, PREC_SUFFIX)(DisasContext *s, TCGv_i32 arg)
{
    glue(tcg_gen_cvt32i, PREC_SUFFIX)(get_st0(s), arg);
}

static void glue(gen_fildll_ST0, PREC_SUFFIX)(DisasContext *s, TCGv_i64 arg)
{
    glue(tcg_gen_cvt64i, PREC_SUFFIX)(get_st0(s), arg);
}

static void glue(gen_fistl_ST0, PREC_SUFFIX)(DisasContext *s, TCGv_i32 arg)
{
    glue(glue(tcg_gen_cvt, PRECf), _i32)(arg, get_st0(s));
}

static void glue(gen_fistll_ST0, PREC_SUFFIX)(DisasContext *s, TCGv_i64 arg)
{
    glue(glue(tcg_gen_cvt, PRECf), _i64)(arg, get_st0(s));
}

static void glue(gen_fsts_ST0, PREC_SUFFIX)(DisasContext *s, TCGv_i32 arg)
{
    glue(glue(gen_mov, PRECf), _i32)(arg, get_st0(s));
}

static void glue(gen_fstl_ST0, PREC_SUFFIX)(DisasContext *s, TCGv_i64 arg)
{
    glue(glue(gen_mov, PRECf), _i64)(arg, get_st0(s));
}

static void glue(gen_fchs_ST0, PREC_SUFFIX)(DisasContext *s)
{
    PREC_TYPE st0 = get_st0(s);
    glue(tcg_gen_chs, PREC_SUFFIX)(st0, st0);
}

static void glue(gen_fabs_ST0, PREC_SUFFIX)(DisasContext *s)
{
    PREC_TYPE st0 = get_st0(s);
    glue(tcg_gen_abs, PREC_SUFFIX)(st0, st0);
}

static void glue(gen_fsqrt, PREC_SUFFIX)(DisasContext *s)
{
    PREC_TYPE st0 = get_st0(s);
    glue(tcg_gen_sqrt, PREC_SUFFIX)(st0, st0);
}

static void glue(gen_fsin, PREC_SUFFIX)(DisasContext *s)
{
    PREC_TYPE st0 = get_st0(s);
    glue(tcg_gen_sin, PREC_SUFFIX)(st0, st0);
}

static void glue(gen_fcos, PREC_SUFFIX)(DisasContext *s)
{
    PREC_TYPE st0 = get_st0(s);
    glue(tcg_gen_cos, PREC_SUFFIX)(st0, st0);
}

static void glue(gen_fld1_ST0, PREC_SUFFIX)(DisasContext *s)
{
    glue(gen_movi, PREC_SUFFIX)(s, get_st0(s), 1.0);
}

static void glue(gen_fldz_ST0, PREC_SUFFIX)(DisasContext *s)
{
    glue(gen_movi, PREC_SUFFIX)(s, get_st0(s), 0.0);
}

static void glue(gen_fldz_FT0, PREC_SUFFIX)(DisasContext *s)
{
    glue(gen_movi, PREC_SUFFIX)(s, get_ft0(s), 0.0);
}
