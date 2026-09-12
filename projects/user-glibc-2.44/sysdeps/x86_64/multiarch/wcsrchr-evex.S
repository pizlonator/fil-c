#ifndef WCSRCHR
# define WCSRCHR	__wcsrchr_evex
#endif

#define STRRCHR	WCSRCHR
#define USE_AS_WCSRCHR 1
#define USE_WIDE_CHAR 1
#include "strrchr-evex.S"
