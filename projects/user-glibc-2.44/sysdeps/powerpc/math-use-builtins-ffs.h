#define USE_FFS_BUILTIN    1
#ifdef __powerpc64__
# define USE_FFSLL_BUILTIN 1
#else
# define USE_FFSLL_BUILTIN 0
#endif
