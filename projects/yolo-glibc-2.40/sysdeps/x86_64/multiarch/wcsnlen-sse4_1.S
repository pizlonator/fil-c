#ifndef WCSNLEN
# define WCSNLEN	__wcsnlen_sse4_1
# define OVERFLOW_STRLEN	__wcslen_sse4_1
#endif

#define AS_WCSLEN
#define AS_STRNLEN
#define STRLEN	WCSNLEN
#define SECTION(p)	p##.sse4.1

#include "strlen-sse2.S"
