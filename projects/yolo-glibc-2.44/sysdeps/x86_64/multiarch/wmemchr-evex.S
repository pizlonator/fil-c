#ifndef WMEMCHR
# define WMEMCHR	__wmemchr_evex
#endif
#define USE_AS_WMEMCHR	1
#define MEMCHR	WMEMCHR

#include "memchr-evex.S"
