#ifndef STRNLEN
# define STRNLEN __strnlen_avx2
#endif

#define USE_AS_STRNLEN 1
#define STRLEN	STRNLEN

#include "strlen-avx2.S"
