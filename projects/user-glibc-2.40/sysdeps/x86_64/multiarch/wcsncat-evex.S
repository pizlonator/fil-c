#ifndef WCSNCAT
# define WCSNCAT	__wcsncat_evex
#endif

#define USE_AS_WCSCPY
#define USE_AS_STRCAT

#define STRNCAT	WCSNCAT
#include "strncat-evex.S"
