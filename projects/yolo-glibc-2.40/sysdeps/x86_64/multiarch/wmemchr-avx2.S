#ifndef WMEMCHR
# define WMEMCHR	__wmemchr_avx2
#endif
#define USE_AS_WMEMCHR	1
#define MEMCHR	WMEMCHR

#include "memchr-avx2.S"
