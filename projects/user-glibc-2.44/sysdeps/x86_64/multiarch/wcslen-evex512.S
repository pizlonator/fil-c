#ifndef WCSLEN
# define WCSLEN	__wcslen_evex512
#endif

#define STRLEN WCSLEN
#define USE_AS_WCSLEN 1

#include "strlen-evex512.S"
