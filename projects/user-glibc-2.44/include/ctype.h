#ifndef _CTYPE_H

#include <ctype/ctype.h>

#ifndef _ISOMAC
/* Initialize ctype locale data.  */
extern void __ctype_init (void);
libc_hidden_proto (__ctype_init)

/* ctype/ctype.h defined this as a macro and we don't want to #undef it.
   So defeat macro expansion with parens for this declaration.  */
extern int (__isctype) (int __c, int __mask);

libc_hidden_proto (tolower)
libc_hidden_proto (toupper)

# if IS_IN (libc)

/* These accessors are used by the optimized macros to find the
   thread-local cache of ctype information from the current thread's
   locale.  For inside libc, define them as inlines using the _NL_CURRENT
   accessors.  We don't use _NL_CURRENT_LOCALE->__ctype_b here because we
   want to cause a link-time ref to _nl_current_LC_CTYPE under
   NL_CURRENT_INDIRECT.  */

#  include "../locale/localeinfo.h"

#  ifndef CTYPE_EXTERN_INLINE	/* Used by ctype/ctype-info.c, which see.  */
#   define CTYPE_EXTERN_INLINE extern inline
#  endif

extern __thread const uint16_t * __libc_tsd_CTYPE_B
  attribute_hidden attribute_tls_model_ie;
extern __thread const int32_t * __libc_tsd_CTYPE_TOUPPER
  attribute_hidden attribute_tls_model_ie;
extern __thread const int32_t * __libc_tsd_CTYPE_TOLOWER
  attribute_hidden attribute_tls_model_ie;


CTYPE_EXTERN_INLINE const uint16_t ** __attribute__ ((const))
__ctype_b_loc (void)
{
  return &__libc_tsd_CTYPE_B;
}

CTYPE_EXTERN_INLINE const int32_t ** __attribute__ ((const))
__ctype_toupper_loc (void)
{
  return &__libc_tsd_CTYPE_TOUPPER;
}

CTYPE_EXTERN_INLINE const int32_t ** __attribute__ ((const))
__ctype_tolower_loc (void)
{
  return &__libc_tsd_CTYPE_TOLOWER;
}

#  ifndef __NO_CTYPE
/* The spec says that isdigit must only match the decimal digits.  We
   can check this without a memory access.  */
#   undef isdigit
#   define isdigit(c) ({ int __c = (c); __c >= '0' && __c <= '9'; })
#   undef isdigit_l
#   define isdigit_l(c, l) ({ int __c = (c); __c >= '0' && __c <= '9'; })
#   undef __isdigit_l
#   define __isdigit_l(c, l) ({ int __c = (c); __c >= '0' && __c <= '9'; })
#  endif  /* Not __NO_CTYPE.  */

/* For use in initializers.  */
extern const char _nl_C_LC_CTYPE_class[] attribute_hidden;
extern const uint32_t _nl_C_LC_CTYPE_toupper[] attribute_hidden;
extern const uint32_t _nl_C_LC_CTYPE_tolower[] attribute_hidden;

# endif	/* IS_IN (libc).  */
#endif  /* Not _ISOMAC.  */

#endif /* ctype.h */
