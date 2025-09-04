#ifndef WCSNLEN
# define WCSNLEN	__wcsnlen_avx2
#endif

#define STRLEN	WCSNLEN
#define USE_AS_WCSLEN 1
#define USE_AS_STRNLEN 1

#include "strlen-avx2.S"
