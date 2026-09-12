#ifndef WCSCHR
# define WCSCHR	__wcschr_evex
#endif

#define STRCHR	WCSCHR
#define USE_AS_WCSCHR 1

#include "strchr-evex.S"
