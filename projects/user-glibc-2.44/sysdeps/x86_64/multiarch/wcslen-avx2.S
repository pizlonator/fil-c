#ifndef WCSLEN
# define WCSLEN	__wcslen_avx2
#endif

#define STRLEN WCSLEN
#define USE_AS_WCSLEN 1

#include "strlen-avx2.S"
