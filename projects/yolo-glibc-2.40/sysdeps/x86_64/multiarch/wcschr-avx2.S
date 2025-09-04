#ifndef WCSCHR
# define WCSCHR	__wcschr_avx2
#endif

#define STRCHR	WCSCHR
#define USE_AS_WCSCHR 1

#include "strchr-avx2.S"
