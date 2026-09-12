#ifndef STPCPY
# define STPCPY	__stpcpy_sse2_unaligned
#endif

#define USE_AS_STPCPY
#define STRCPY	STPCPY
#include "strcpy-sse2-unaligned.S"
