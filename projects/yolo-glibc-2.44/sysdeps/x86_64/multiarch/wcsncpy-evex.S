#ifndef WCSNCPY
# define WCSNCPY	__wcsncpy_evex
#endif

#define USE_AS_WCSCPY
#define STRNCPY	WCSNCPY
#include "strncpy-evex.S"
