#include <float128_private.h>
#if !defined __USE_EXTERN_INLINES && !defined(__OPTIMIZE_SIZE__)
# undef libm_alias_float128_r
# define libm_alias_float128_r(from, to, r)			\
  static_weak_alias (from ## f128 ## r, to ## f128 ## r);	\
  libm_alias_float128_other_r (from, to, r)
#endif
#include "../ldbl-128/s_fabsl.c"
#ifndef __USE_EXTERN_INLINES
libm_hidden_def (fabsf128)
#endif
