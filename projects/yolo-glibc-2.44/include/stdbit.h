
#ifndef _STDBIT_H
#include <stdlib/stdbit.h>

#ifndef _ISOMAC
# include <stdint.h>

#  if __glibc_has_builtin (__builtin_stdc_rotate_left)
#   define stdc_rotate_left(__x, __n) (__builtin_stdc_rotate_left (__x, __n))
#   define stdc_rotate_left_uc(__x, __n) (stdc_rotate_left (__x, __n))
#   define stdc_rotate_left_us(__x, __n) (stdc_rotate_left (__x, __n))
#   define stdc_rotate_left_ui(__x, __n) (stdc_rotate_left (__x, __n))
#   define stdc_rotate_left_ul(__x, __n) (stdc_rotate_left (__x, __n))
#   define stdc_rotate_left_ull(__x, __n) (stdc_rotate_left (__x, __n))
#  else
#   if __WORDSIZE == 64
#    define __ROL_UL_GENERIC __rol64_inline
#   else
#    define __ROL_UL_GENERIC __rol32_inline
#   endif
#   define stdc_rotate_left(__x, __n)		\
   _Generic((__x),				\
      unsigned char: __rol8_inline,		\
      unsigned short: __rol16_inline,		\
      unsigned int: __rol32_inline,		\
      unsigned long: __ROL_UL_GENERIC,		\
      unsigned long long: __rol64_inline	\
   )(__x, __n)

#   define __rol_generic(__v, __n)		\
 ((__v << (__n & (sizeof __v * 8 - 1)))		\
  | (__v >> ((-__n) & (sizeof __v * 8 - 1))))

static __always_inline uint8_t
__rol8_inline (uint8_t __x, unsigned int __n)
{
  return __rol_generic (__x, __n);
}

static __always_inline uint16_t
__rol16_inline (uint16_t __x, unsigned int __n)
{
  return __rol_generic (__x, __n);
}

static __always_inline uint32_t
__rol32_inline (uint32_t __x, unsigned int __n)
{
  return __rol_generic (__x, __n);
}

static __always_inline uint64_t
__rol64_inline (uint64_t __x, unsigned int __n)
{
  return __rol_generic (__x, __n);
}
#   define stdc_rotate_left_uc(__x, __n) (__rol8_inline (__x, __n))
#   define stdc_rotate_left_us(__x, __n) (__rol16_inline (__x, __n))
#   define stdc_rotate_left_ui(__x, __n) (__rol32_inline (__x, __n))
#   if __WORDSIZE == 64
#    define stdc_rotate_left_ul(__x, __n) (__rol64_inline (__x, __n))
#   else
#    define stdc_rotate_left_ul(__x, __n) (__rol32_inline (__x, __n))
#   endif
#   define stdc_rotate_left_ull(__x, __n) (__rol64_inline (__x, __n))
#  endif /* __glibc_has_builtin (__builtin_stdc_rotate_left) */

#  if __glibc_has_builtin (__builtin_stdc_rotate_right)
#   define stdc_rotate_right(__x, __n) (__builtin_stdc_rotate_right (__x, __n))
#   define stdc_rotate_right_uc(__x, __n) (stdc_rotate_right (__x, __n))
#   define stdc_rotate_right_us(__x, __n) (stdc_rotate_right (__x, __n))
#   define stdc_rotate_right_ui(__x, __n) (stdc_rotate_right (__x, __n))
#   define stdc_rotate_right_ul(__x, __n) (stdc_rotate_right (__x, __n))
#   define stdc_rotate_right_ull(__x, __n) (stdc_rotate_right (__x, __n))
#  else
#   if __WORDSIZE == 64
#    define __ROR_UL_GENERIC __ror64_inline
#   else
#    define __ROR_UL_GENERIC __ror32_inline
#   endif
#   define stdc_rotate_right(__x, __n)		\
   _Generic((__x),				\
      unsigned char: __ror8_inline,		\
      unsigned short: __ror16_inline,		\
      unsigned int: __ror32_inline,		\
      unsigned long: __ROR_UL_GENERIC,		\
      unsigned long long: __ror64_inline	\
   )(__x, __n)

#   define __ror_generic(__v, __n)		\
 ((__v >> (__n & (sizeof __v * 8 - 1)))		\
  | (__v << ((-__n) & (sizeof __v * 8 - 1))))

static __always_inline uint8_t
__ror8_inline (uint8_t __x, unsigned int __n)
{
  return __ror_generic (__x, __n);
}

static __always_inline uint16_t
__ror16_inline (uint16_t __x, unsigned int __n)
{
  return __ror_generic (__x, __n);
}

static __always_inline uint32_t
__ror32_inline (uint32_t __x, unsigned int __n)
{
  return __ror_generic (__x, __n);
}

static __always_inline uint64_t
__ror64_inline (uint64_t __x, unsigned int __n)
{
  return __ror_generic (__x, __n);
}
#   define stdc_rotate_right_uc(__x, __n) (__ror8_inline (__x, __n))
#   define stdc_rotate_right_us(__x, __n) (__ror16_inline (__x, __n))
#   define stdc_rotate_right_ui(__x, __n) (__ror32_inline (__x, __n))
#   if __WORDSIZE == 64
#    define stdc_rotate_right_ul(__x, __n) (__ror64_inline (__x, __n))
#   else
#    define stdc_rotate_right_ul(__x, __n) (__ror32_inline (__x, __n))
#   endif
#   define stdc_rotate_right_ull(__x, __n) (__ror64_inline (__x, __n))
#  endif /* __glibc_has_builtin (__builtin_stdc_rotate_right) */

# endif /* _ISOMAC */

#endif  /* _STDBIT_H */
