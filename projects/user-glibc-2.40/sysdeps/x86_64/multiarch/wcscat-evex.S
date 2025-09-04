#ifndef WCSCAT
# define WCSCAT	__wcscat_evex
#endif

#define USE_AS_WCSCPY
#define USE_AS_STRCAT

#define STRCPY	WCSCAT
#include "strcpy-evex.S"
