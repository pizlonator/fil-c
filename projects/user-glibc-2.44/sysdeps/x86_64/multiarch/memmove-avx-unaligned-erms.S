#include <isa-level.h>

#if ISA_SHOULD_BUILD (3)

# include "x86-avx-vecs.h"

# ifndef MEMMOVE_SYMBOL
#  define MEMMOVE_SYMBOL(p,s)	p##_avx_##s
# endif

# include "memmove-vec-unaligned-erms.S"

# if MINIMUM_X86_ISA_LEVEL == 3
#  include "memmove-shlib-compat.h"
# endif
#endif
