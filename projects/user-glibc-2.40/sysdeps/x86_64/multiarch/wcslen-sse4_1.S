#ifndef WCSLEN
# define WCSLEN	__wcslen_sse4_1
#endif

#define AS_WCSLEN	1
#define STRLEN	WCSLEN
#define SECTION(p)		p##.sse4.1

#include "strlen-sse2.S"
