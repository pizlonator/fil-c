/* Generic implementations for float and double always use the builtin.  */
#define USE_COPYSIGNF_BUILTIN 1
#define USE_COPYSIGN_BUILTIN 1
#define USE_COPYSIGNL_BUILTIN 1
#if __GNUC_PREREQ (7, 0)
# define USE_COPYSIGNF128_BUILTIN 1
#else
# define USE_COPYSIGNF128_BUILTIN 0
#endif
