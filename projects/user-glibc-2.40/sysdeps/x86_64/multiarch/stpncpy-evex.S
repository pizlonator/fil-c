#ifndef STPNCPY
# define STPNCPY	__stpncpy_evex
#endif

#define USE_AS_STPCPY
#define STRNCPY	STPNCPY
#include "strncpy-evex.S"
