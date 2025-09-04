#ifndef WMEMCMP
# define WMEMCMP	__wmemcmp_evex_movbe
#endif

#define MEMCMP	WMEMCMP
#define USE_AS_WMEMCMP 1

#include "memcmp-evex-movbe.S"
