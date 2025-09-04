#ifndef WCSCPY
# define WCSCPY	__wcscpy_evex
#endif

#define USE_AS_WCSCPY
#define STRCPY	WCSCPY
#include "strcpy-evex.S"
