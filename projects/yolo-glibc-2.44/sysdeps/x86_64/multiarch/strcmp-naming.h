#ifndef _STRCMP_NAMING_H_
#define _STRCMP_NAMING_H_

/* Utility macros.  */
#define STRCMP_SUFFIX(x, y) x##y
#define STRCMP_NAME(x, y) STRCMP_SUFFIX (x, y)

/* Setup base of all definitions.  */
#define STRNCASECMP_BASE __strncasecmp
#define STRCASECMP_BASE __strcasecmp
#define WCSCMP_BASE __wcscmp

#if defined USE_MULTIARCH && IS_IN (libc)
# define WCSNCMP_BASE __wcsncmp
# define STRNCMP_BASE __strncmp
# define STRCMP_BASE __strcmp

#else
/* Covers IS_IN (rtld) or non-multiarch build.  */
# define WCSNCMP_BASE wcsncmp
# define STRNCMP_BASE strncmp
# define STRCMP_BASE strcmp

# undef STRCMP_ISA
# define STRCMP_ISA
#endif

#if IS_IN (rtld) || defined USE_MULTIARCH
# define ISA_HIDDEN_JUMPTARGET(...) __VA_ARGS__
#else
# define ISA_HIDDEN_JUMPTARGET(...) HIDDEN_JUMPTARGET (__VA_ARGS__)
#endif

/* Get correct symbol for OVERFLOW_STRCMP, STRCMP, and
   STRCASECMP.  */
#if defined USE_AS_STRNCMP || defined USE_AS_STRNCASECMP_L

# if defined USE_AS_WCSCMP || defined USE_AS_WCSNCMP
#  define OVERFLOW_STRCMP_SYM WCSCMP_BASE
#  define STRCMP_SYM WCSNCMP_BASE
# elif defined USE_AS_STRCASECMP_L || defined USE_AS_STRNCASECMP_L
#  define OVERFLOW_STRCMP_SYM STRCMP_NAME (STRCASECMP_BASE, _l)
#  define STRCMP_SYM STRCMP_NAME (STRNCASECMP_BASE, _l)
# else
#  define OVERFLOW_STRCMP_SYM STRCMP_BASE
#  define STRCMP_SYM STRNCMP_BASE
# endif

# define STRCASECMP_SYM STRNCASECMP_BASE
# define OVERFLOW_STRCMP \
    ISA_HIDDEN_JUMPTARGET (STRCMP_NAME (OVERFLOW_STRCMP_SYM, STRCMP_ISA))
#else
# ifdef USE_AS_WCSCMP
#  define STRCMP_SYM WCSCMP_BASE
# elif defined USE_AS_STRCASECMP_L
#  define STRCMP_SYM STRCMP_NAME (STRCASECMP_BASE, _l)
# else
#  define STRCMP_SYM STRCMP_BASE
# endif

# define STRCASECMP_SYM STRCASECMP_BASE
#endif

#define STRCASECMP_L_NONASCII STRCMP_NAME (STRCASECMP_SYM, _l_nonascii)
#define STRCASECMP STRCMP_NAME (STRCASECMP_SYM, STRCMP_ISA)
#define STRCMP STRCMP_NAME (STRCMP_SYM, STRCMP_ISA)

#endif
