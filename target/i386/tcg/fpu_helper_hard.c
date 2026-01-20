#if defined(XBOX) && defined(__x86_64__)
#define USE_HARD_FPU 1
#include "fpu_helper.c"
#endif
