#include <isa-level.h>

#if ISA_SHOULD_BUILD (4)

# include "x86-evex512-vecs.h"

# ifndef MEMMOVE_SYMBOL
#  define MEMMOVE_SYMBOL(p,s)	p##_avx512_##s
# endif

# include "memmove-vec-unaligned-erms.S"
#endif
