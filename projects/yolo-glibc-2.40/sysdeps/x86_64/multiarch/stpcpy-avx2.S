#ifndef STPCPY
# define STPCPY	__stpcpy_avx2
#endif

#define USE_AS_STPCPY
#define STRCPY	STPCPY
#include "strcpy-avx2.S"
