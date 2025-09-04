#ifndef WCSLEN
# define WCSLEN	__wcslen_evex
#endif

#define STRLEN WCSLEN
#define USE_AS_WCSLEN 1

#include "strlen-evex.S"
