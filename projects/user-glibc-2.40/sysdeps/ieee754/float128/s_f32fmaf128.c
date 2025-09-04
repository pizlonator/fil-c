#define f32fmaf64x __hide_f32fmaf64x
#define f32fmaf128 __hide_f32fmaf128
#include <float128_private.h>
#undef f32fmaf64x
#undef f32fmaf128
#include "../ldbl-128/s_ffmal.c"
