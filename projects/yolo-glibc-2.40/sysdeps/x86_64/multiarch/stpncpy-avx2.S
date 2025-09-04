#ifndef STPNCPY
# define STPNCPY	__stpncpy_avx2
#endif

#define USE_AS_STPCPY
#define STRNCPY	STPNCPY
#include "strncpy-avx2.S"
