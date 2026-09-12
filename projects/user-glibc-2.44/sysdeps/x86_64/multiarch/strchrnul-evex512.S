#ifndef STRCHRNUL
# define STRCHRNUL	__strchrnul_evex512
#endif

#define STRCHR	STRCHRNUL
#define USE_AS_STRCHRNUL 1

#include "strchr-evex512.S"
