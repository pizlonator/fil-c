#if defined __mc68020__ || defined __mc68030__ || defined __mc68040__ \
    || defined __mc68060__
# define USE_FFS_BUILTIN  1
#else
# define USE_FFS_BUILTIN  0
#endif
#define USE_FFSLL_BUILTIN 0
