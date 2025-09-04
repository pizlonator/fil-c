#ifndef WCSCPY
# define WCSCPY	__wcscpy_avx2
#endif

#define USE_AS_WCSCPY
#define STRCPY	WCSCPY
#include "strcpy-avx2.S"
