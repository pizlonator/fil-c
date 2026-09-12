#if IS_IN (libc)

# include "x86-avx-rtm-vecs.h"

# define MEMMOVE_SYMBOL(p,s)	p##_avx_##s##_rtm

# include "memmove-vec-unaligned-erms.S"
#endif
