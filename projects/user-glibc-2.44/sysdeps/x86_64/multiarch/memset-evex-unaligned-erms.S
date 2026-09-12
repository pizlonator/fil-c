#include <isa-level.h>

#if ISA_SHOULD_BUILD (4)

# define USE_WITH_EVEX	1

# include "x86-evex256-vecs.h"

# define MEMSET_SET_VEC0_AND_SET_RETURN(d, r) \
  vpbroadcastb d, %VMM(0); \
  movq r, %rax

# define WMEMSET_SET_VEC0_AND_SET_RETURN(d, r) \
  vpbroadcastd d, %VMM(0); \
  movq r, %rax

# define MEMSET_VDUP_TO_VEC0_HIGH()
# define MEMSET_VDUP_TO_VEC0_LOW()

# define WMEMSET_VDUP_TO_VEC0_HIGH()
# define WMEMSET_VDUP_TO_VEC0_LOW()

#ifndef MEMSET_SYMBOL
# define MEMSET_SYMBOL(p,s)	p##_evex_##s
#endif
#ifndef WMEMSET_SYMBOL
# define WMEMSET_SYMBOL(p,s)	p##_evex_##s
#endif


# define USE_LESS_VEC_MASK_STORE	1
# include "memset-vec-unaligned-erms.S"
#endif
