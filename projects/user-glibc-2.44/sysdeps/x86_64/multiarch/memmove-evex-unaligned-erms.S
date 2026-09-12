#include <isa-level.h>

#if ISA_SHOULD_BUILD (4)

# include "x86-evex256-vecs.h"

# ifndef MEMMOVE_SYMBOL
#  define MEMMOVE_SYMBOL(p,s)	p##_evex_##s
# endif

# include "memmove-vec-unaligned-erms.S"


# if MINIMUM_X86_ISA_LEVEL == 4
#  include "memmove-shlib-compat.h"
# endif
#endif
