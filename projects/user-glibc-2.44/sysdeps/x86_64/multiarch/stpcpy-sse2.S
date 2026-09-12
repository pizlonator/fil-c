#ifndef STPCPY
# define STPCPY	__stpcpy_sse2
#endif

#define USE_AS_STPCPY
#define STRCPY	STPCPY
#include "strcpy-sse2.S"
