#ifndef WMEMCMP
# define WMEMCMP	__wmemcmp_avx2_movbe
#endif

#define MEMCMP	WMEMCMP
#define USE_AS_WMEMCMP 1

#include "memcmp-avx2-movbe.S"
