#ifndef WMEMCHR
# define WMEMCHR	__wmemchr_sse2
#endif
#define USE_AS_WMEMCHR	1
#define MEMCHR	WMEMCHR

#include "memchr-sse2.S"
