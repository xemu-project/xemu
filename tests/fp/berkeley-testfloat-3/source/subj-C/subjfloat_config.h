
/*----------------------------------------------------------------------------
| The following macros are defined to indicate all the subject functions that
| exist.
*----------------------------------------------------------------------------*/

#define SUBJ_UI32_TO_F32
#define SUBJ_UI64_TO_F32
#define SUBJ_I32_TO_F32
#define SUBJ_I64_TO_F32

#define SUBJ_F32_TO_UI32_RX_MINMAG
#define SUBJ_F32_TO_UI64_RX_MINMAG
#define SUBJ_F32_TO_I32_RX_MINMAG
#define SUBJ_F32_TO_I64_RX_MINMAG
#define SUBJ_F32_ADD
#define SUBJ_F32_SUB
#define SUBJ_F32_MUL
#ifdef __STDC_VERSION__
#if 199901L <= __STDC_VERSION__
#define SUBJ_F32_MULADD
#endif
#endif
#define SUBJ_F32_DIV
#ifdef __STDC_VERSION__
#if 199901L <= __STDC_VERSION__
#define SUBJ_F32_SQRT
#endif
#endif
#define SUBJ_F32_EQ
#define SUBJ_F32_LE
#define SUBJ_F32_LT

#ifdef FLOAT64

#define SUBJ_UI32_TO_F64
#define SUBJ_UI64_TO_F64
#define SUBJ_I32_TO_F64
#define SUBJ_I64_TO_F64

#define SUBJ_F32_TO_F64

#define SUBJ_F64_TO_UI32_RX_MINMAG
#define SUBJ_F64_TO_UI64_RX_MINMAG
#define SUBJ_F64_TO_I32_RX_MINMAG
#define SUBJ_F64_TO_I64_RX_MINMAG
#define SUBJ_F64_TO_F32
#define SUBJ_F64_ADD
#define SUBJ_F64_SUB
#define SUBJ_F64_MUL
#ifdef __STDC_VERSION__
#if 199901L <= __STDC_VERSION__
#define SUBJ_F64_MULADD
#endif
#endif
#define SUBJ_F64_DIV
#define SUBJ_F64_SQRT
#define SUBJ_F64_EQ
#define SUBJ_F64_LE
#define SUBJ_F64_LT

#endif

#if defined EXTFLOAT80 && defined LONG_DOUBLE_IS_EXTFLOAT80

#define SUBJ_UI32_TO_EXTF80
#define SUBJ_UI64_TO_EXTF80
#define SUBJ_I32_TO_EXTF80
#define SUBJ_I64_TO_EXTF80

#define SUBJ_F32_TO_EXTF80
#ifdef FLOAT64
#define SUBJ_F64_TO_EXTF80
#endif

#define SUBJ_EXTF80_TO_UI32_RX_MINMAG
#define SUBJ_EXTF80_TO_UI64_RX_MINMAG
#define SUBJ_EXTF80_TO_I32_RX_MINMAG
#define SUBJ_EXTF80_TO_I64_RX_MINMAG
#define SUBJ_EXTF80_TO_F32
#ifdef FLOAT64
#define SUBJ_EXTF80_TO_F64
#endif
#define SUBJ_EXTF80_ADD
#define SUBJ_EXTF80_SUB
#define SUBJ_EXTF80_MUL
#define SUBJ_EXTF80_DIV
#define SUBJ_EXTF80_EQ
#define SUBJ_EXTF80_LE
#define SUBJ_EXTF80_LT

#endif

#if defined FLOAT128 && defined LONG_DOUBLE_IS_FLOAT128

#define SUBJ_UI32_TO_F128
#define SUBJ_UI64_TO_F128
#define SUBJ_I32_TO_F128
#define SUBJ_I64_TO_F128

#define SUBJ_F32_TO_F128
#ifdef FLOAT64
#define SUBJ_F64_TO_F128
#endif

#define SUBJ_F128_TO_UI32_RX_MINMAG
#define SUBJ_F128_TO_UI64_RX_MINMAG
#define SUBJ_F128_TO_I32_RX_MINMAG
#define SUBJ_F128_TO_I64_RX_MINMAG
#define SUBJ_F128_TO_F32
#ifdef FLOAT64
#define SUBJ_F128_TO_F64
#endif
#define SUBJ_F128_ADD
#define SUBJ_F128_SUB
#define SUBJ_F128_MUL
#ifdef __STDC_VERSION__
#if 199901L <= __STDC_VERSION__
#define SUBJ_F128_MULADD
#endif
#endif
#define SUBJ_F128_DIV
#ifdef __STDC_VERSION__
#if 199901L <= __STDC_VERSION__
#define SUBJ_F128_SQRT
#endif
#endif
#define SUBJ_F128_EQ
#define SUBJ_F128_LE
#define SUBJ_F128_LT

#endif

