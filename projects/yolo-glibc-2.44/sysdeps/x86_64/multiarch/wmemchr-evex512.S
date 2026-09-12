#ifndef WMEMCHR
# define WMEMCHR	__wmemchr_evex512
#endif

#define MEMCHR	WMEMCHR
#define USE_AS_WMEMCHR	1

#define USE_WIDE_CHAR	1
#include "memchr-evex512.S"
