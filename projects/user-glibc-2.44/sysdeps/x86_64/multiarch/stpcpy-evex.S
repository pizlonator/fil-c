#ifndef STPCPY
# define STPCPY	__stpcpy_evex
#endif

#define USE_AS_STPCPY
#define STRCPY	STPCPY
#include "strcpy-evex.S"
