#ifndef WCPNCPY
# define WCPNCPY	__wcpncpy_avx2
#endif

#define USE_AS_WCSCPY
#define USE_AS_STPCPY
#define STRNCPY	WCPNCPY
#include "strncpy-avx2.S"
