#ifndef WCSRCHR
# define WCSRCHR	__wcsrchr_evex512
#endif

#define STRRCHR	WCSRCHR
#define USE_AS_WCSRCHR 1

#define USE_WIDE_CHAR 1

#include "x86-evex512-vecs.h"
#include "reg-macros.h"

#include "strrchr-evex512.S"
