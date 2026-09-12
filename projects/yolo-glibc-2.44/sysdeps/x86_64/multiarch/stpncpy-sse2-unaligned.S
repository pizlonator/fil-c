#ifndef STPNCPY
# define STPNCPY	__stpncpy_sse2_unaligned
#endif

#define USE_AS_STPCPY
#define USE_AS_STRNCPY
#define STRCPY	STPNCPY
#include "strcpy-sse2-unaligned.S"
