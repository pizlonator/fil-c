#if __GNUC_PREREQ (12, 0) && defined __riscv_zbb
#  define USE_FFS_BUILTIN 1
#  define USE_FFSLL_BUILTIN 1
#elif __GNUC_PREREQ (13, 0) && defined __riscv_xtheadbb
#  define USE_FFS_BUILTIN 0
#  define USE_FFSLL_BUILTIN 1
#else
#  define USE_FFS_BUILTIN 0
#  define USE_FFSLL_BUILTIN 0
#endif
