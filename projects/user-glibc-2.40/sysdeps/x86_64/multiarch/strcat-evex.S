#ifndef STRCAT
# define STRCAT	__strcat_evex
#endif

#define USE_AS_STRCAT
#define STRCPY	STRCAT
#include "strcpy-evex.S"
