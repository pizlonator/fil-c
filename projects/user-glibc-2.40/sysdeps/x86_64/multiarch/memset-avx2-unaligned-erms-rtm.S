#include "x86-avx-rtm-vecs.h"

#define MEMSET_SYMBOL(p,s)	p##_avx2_##s##_rtm
#define WMEMSET_SYMBOL(p,s)	p##_avx2_##s##_rtm

# include "memset-avx2-unaligned-erms.S"
