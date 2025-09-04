#include <isa-level.h>

#if ISA_SHOULD_BUILD (3)

# define USE_WITH_AVX2	1

# ifndef VEC_SIZE
#  include "x86-avx-vecs.h"
# endif

# define MEMSET_SET_VEC0_AND_SET_RETURN(d, r) \
  vmovd d, %xmm0; \
  movq r, %rax;

# define WMEMSET_SET_VEC0_AND_SET_RETURN(d, r) \
  MEMSET_SET_VEC0_AND_SET_RETURN(d, r)

# define MEMSET_VDUP_TO_VEC0_HIGH() vpbroadcastb %xmm0, %ymm0
# define MEMSET_VDUP_TO_VEC0_LOW() vpbroadcastb %xmm0, %xmm0

# define WMEMSET_VDUP_TO_VEC0_HIGH() vpbroadcastd %xmm0, %ymm0
# define WMEMSET_VDUP_TO_VEC0_LOW() vpbroadcastd %xmm0, %xmm0

# ifndef MEMSET_SYMBOL
#  define MEMSET_SYMBOL(p,s)	p##_avx2_##s
# endif
# ifndef WMEMSET_SYMBOL
#  define WMEMSET_SYMBOL(p,s)	p##_avx2_##s
# endif

# define USE_XMM_LESS_VEC
# include "memset-vec-unaligned-erms.S"
#endif
