#ifndef RAWMEMCHR
# define RAWMEMCHR	__rawmemchr_evex512
#endif
#define USE_AS_RAWMEMCHR	1
#define MEMCHR	RAWMEMCHR

#include "memchr-evex512.S"
