#ifndef WCSCAT
# define WCSCAT	__wcscat_avx2
#endif

#define USE_AS_WCSCPY
#define USE_AS_STRCAT

#define STRCPY	WCSCAT

#include "strcpy-avx2.S"
