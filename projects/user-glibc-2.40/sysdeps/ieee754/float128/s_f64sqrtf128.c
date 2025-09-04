#define f32xsqrtf64x __hide_f32xsqrtf64x
#define f32xsqrtf128 __hide_f32xsqrtf128
#define f64sqrtf64x __hide_f64sqrtf64x
#define f64sqrtf128 __hide_f64sqrtf128
#include <float128_private.h>
#undef f32xsqrtf64x
#undef f32xsqrtf128
#undef f64sqrtf64x
#undef f64sqrtf128
#include "../ldbl-128/s_dsqrtl.c"
