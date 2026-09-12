#if IS_IN (libc)
# define MEMRCHR  __memrchr_ia32
# include <string.h>
extern void *__memrchr_ia32 (const void *, int, size_t);
#endif

#include "string/memrchr.c"
strong_alias (__memrchr_ia32, __GI___memrchr)
