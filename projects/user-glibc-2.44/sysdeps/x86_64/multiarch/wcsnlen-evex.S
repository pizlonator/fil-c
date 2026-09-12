#ifndef WCSNLEN
# define WCSNLEN	__wcsnlen_evex
#endif

#define STRNLEN	WCSNLEN
#define USE_AS_WCSLEN 1

#include "strnlen-evex.S"
