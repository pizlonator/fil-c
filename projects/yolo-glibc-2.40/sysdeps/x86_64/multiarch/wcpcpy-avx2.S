#ifndef WCPCPY
# define WCPCPY	__wcpcpy_avx2
#endif

#define USE_AS_STPCPY
#define USE_AS_WCSCPY
#define STRCPY	WCPCPY
#include "strcpy-avx2.S"
