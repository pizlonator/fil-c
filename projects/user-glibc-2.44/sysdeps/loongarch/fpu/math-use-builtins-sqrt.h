#if __loongarch_grlen == 64
  #define USE_SQRT_BUILTIN 1
  #define USE_SQRTF_BUILTIN 1
#else
  #define USE_SQRT_BUILTIN 0
  #define USE_SQRTF_BUILTIN 0
#endif

#define USE_SQRTL_BUILTIN 0
#define USE_SQRTF128_BUILTIN 0
