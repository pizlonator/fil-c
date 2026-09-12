#ifndef STRNCPY
# define STRNCPY	__strncpy_sse2_unaligned
#endif

#define USE_AS_STRNCPY
#define STRCPY	STRNCPY
#include "strcpy-sse2-unaligned.S"
