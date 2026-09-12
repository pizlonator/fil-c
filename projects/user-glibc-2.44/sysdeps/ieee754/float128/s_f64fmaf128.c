#define f32xfmaf64x __hide_f32xfmaf64x
#define f32xfmaf128 __hide_f32xfmaf128
#define f64fmaf64x __hide_f64fmaf64x
#define f64fmaf128 __hide_f64fmaf128
#include <float128_private.h>
#undef f32xfmaf64x
#undef f32xfmaf128
#undef f64fmaf64x
#undef f64fmaf128
#include "../ldbl-128/s_dfmal.c"
