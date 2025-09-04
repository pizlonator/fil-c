# ifndef MEMCHR
#  define MEMCHR       __memchr_evex512
# endif

#include "x86-evex512-vecs.h"
#include "reg-macros.h"

#include "memchr-evex-base.S"
