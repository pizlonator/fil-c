#ifndef WCSNCPY
# define WCSNCPY	__wcsncpy_avx2
#endif

#define USE_AS_WCSCPY
#define STRNCPY	WCSNCPY
#include "strncpy-avx2.S"
