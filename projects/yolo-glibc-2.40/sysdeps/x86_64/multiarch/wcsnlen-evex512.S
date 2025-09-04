#ifndef WCSNLEN
# define WCSNLEN	__wcsnlen_evex512
#endif

#define STRNLEN	WCSNLEN
#define USE_AS_WCSLEN 1

#include "strnlen-evex512.S"
