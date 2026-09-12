#define NO_MATH_REDIRECT
#define f64xfmaf128 __hide_f64xfmaf128
#include <float128_private.h>
#undef f64xfmaf128
#include "../ldbl-128/s_fmal.c"
