#if defined __ARM_FEATURE_FMA && __ARM_FP & 0x4
# define USE_FMA_BUILTIN 1
#endif
#if defined __ARM_FEATURE_FMA && __ARM_FP & 0x2
# define USE_FMAF_BUILTIN 1
#else
# define USE_FMA_BUILTIN 0
# define USE_FMAF_BUILTIN 0
#endif
#define USE_FMAL_BUILTIN 0
#define USE_FMAF128_BUILTIN 0
