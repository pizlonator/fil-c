#ifndef WCPCPY
# define WCPCPY	__wcpcpy_evex
#endif

#define USE_AS_STPCPY
#define USE_AS_WCSCPY
#define STRCPY	WCPCPY
#include "strcpy-evex.S"
