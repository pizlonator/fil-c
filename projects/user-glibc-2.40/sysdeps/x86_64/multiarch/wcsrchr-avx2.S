#ifndef WCSRCHR
# define WCSRCHR	__wcsrchr_avx2
#endif

#define STRRCHR	WCSRCHR
#define USE_AS_WCSRCHR 1

#include "strrchr-avx2.S"
