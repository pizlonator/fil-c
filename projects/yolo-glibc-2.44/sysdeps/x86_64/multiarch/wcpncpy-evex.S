#ifndef WCPNCPY
# define WCPNCPY	__wcpncpy_evex
#endif

#define USE_AS_WCSCPY
#define USE_AS_STPCPY
#define STRNCPY	WCPNCPY
#include "strncpy-evex.S"
