#if defined __ARC_FPU_DP_FMA__
# define USE_FMA_BUILTIN 1
#else
# define USE_FMA_BUILTIN 0
#endif

#if defined __ARC_FPU_SP_FMA__
# define USE_FMAF_BUILTIN 1
#else
# define USE_FMAF_BUILTIN 0
#endif

#define USE_FMAL_BUILTIN 0
#define USE_FMAF128_BUILTIN 0
