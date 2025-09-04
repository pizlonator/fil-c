#ifndef WCSCHR
# define WCSCHR	__wcschr_evex512
#endif

#define STRCHR	WCSCHR
#define USE_AS_WCSCHR 1

#define USE_WIDE_CHAR 1
#include "strchr-evex512.S"
