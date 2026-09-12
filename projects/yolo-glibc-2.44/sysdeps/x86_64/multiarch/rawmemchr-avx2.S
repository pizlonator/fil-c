#ifndef RAWMEMCHR
# define RAWMEMCHR	__rawmemchr_avx2
#endif
#define USE_AS_RAWMEMCHR	1
#define MEMCHR	RAWMEMCHR

#include "memchr-avx2.S"
