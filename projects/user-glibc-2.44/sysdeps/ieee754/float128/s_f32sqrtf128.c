#define f32sqrtf64x __hide_f32sqrtf64x
#define f32sqrtf128 __hide_f32sqrtf128
#include <float128_private.h>
#undef f32sqrtf64x
#undef f32sqrtf128
#include "../ldbl-128/s_fsqrtl.c"
