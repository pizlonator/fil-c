#ifndef STRNCAT
# define STRNCAT	__strncat_sse2_unaligned
#endif

#define USE_AS_STRNCAT
#define STRCAT	STRNCAT
#include "strcat-sse2-unaligned.S"
