#ifndef WCSNCAT
# define WCSNCAT	__wcsncat_avx2
#endif

#define USE_AS_WCSCPY
#define USE_AS_STRCAT

#define STRNCAT	WCSNCAT
#include "strncat-avx2.S"
