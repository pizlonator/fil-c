/* Copyright (C) 1991-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#define CTYPE_EXTERN_INLINE /* Define real functions for accessors.  */
#include <ctype.h>
#include <locale/localeinfo.h>

/* Fallback initialization using relocations.  See the _nl_C_locobj
   initializers in locale/xlocale.c.  Usually, this is overwritten by
   __ctype_init before user code runs, but this does not happen for
   threads in secondary namespaces.  With the initializers, secondary
   namespaces at least get locale data from the C locale.  */
__thread const uint16_t * __libc_tsd_CTYPE_B attribute_tls_model_ie
  = (const uint16_t *) _nl_C_LC_CTYPE_class + 128;
__thread const int32_t * __libc_tsd_CTYPE_TOLOWER attribute_tls_model_ie
  = (const int32_t *) _nl_C_LC_CTYPE_tolower + 128;
__thread const int32_t * __libc_tsd_CTYPE_TOUPPER attribute_tls_model_ie
  = (const int32_t *) _nl_C_LC_CTYPE_toupper + 128;


void
__ctype_init (void)
{
  __libc_tsd_CTYPE_B
    = ((const uint16_t *) _NL_CURRENT (LC_CTYPE, _NL_CTYPE_CLASS)) + 128;
  __libc_tsd_CTYPE_TOUPPER
    = ((const int32_t *) _NL_CURRENT (LC_CTYPE, _NL_CTYPE_TOUPPER)) + 128;
  __libc_tsd_CTYPE_TOLOWER =
    ((const int32_t *) _NL_CURRENT (LC_CTYPE, _NL_CTYPE_TOLOWER)) + 128;
}
libc_hidden_def (__ctype_init)


#include <shlib-compat.h>
#if SHLIB_COMPAT (libc, GLIBC_2_0, GLIBC_2_3)

/* Defined in locale/C-ctype.c.  */
extern const char _nl_C_LC_CTYPE_class32[] attribute_hidden;
extern const char _nl_C_LC_CTYPE_class_upper[] attribute_hidden;
extern const char _nl_C_LC_CTYPE_class_lower[] attribute_hidden;
extern const char _nl_C_LC_CTYPE_class_alpha[] attribute_hidden;
extern const char _nl_C_LC_CTYPE_class_digit[] attribute_hidden;
extern const char _nl_C_LC_CTYPE_class_xdigit[] attribute_hidden;
extern const char _nl_C_LC_CTYPE_class_space[] attribute_hidden;
extern const char _nl_C_LC_CTYPE_class_print[] attribute_hidden;
extern const char _nl_C_LC_CTYPE_class_graph[] attribute_hidden;
extern const char _nl_C_LC_CTYPE_class_blank[] attribute_hidden;
extern const char _nl_C_LC_CTYPE_class_cntrl[] attribute_hidden;
extern const char _nl_C_LC_CTYPE_class_punct[] attribute_hidden;
extern const char _nl_C_LC_CTYPE_class_alnum[] attribute_hidden;

#define b(t,x,o) (((const t *) _nl_C_LC_CTYPE_##x) + o)

const unsigned short int *__ctype_b = b (unsigned short int, class, 128);
const __uint32_t *__ctype32_b = b (__uint32_t, class32, 0);
const __int32_t *__ctype_tolower = b (__int32_t, tolower, 128);
const __int32_t *__ctype_toupper = b (__int32_t, toupper, 128);
const __uint32_t *__ctype32_tolower = b (__uint32_t, tolower, 128);
const __uint32_t *__ctype32_toupper = b (__uint32_t, toupper, 128);

compat_symbol (libc, __ctype_b, __ctype_b, GLIBC_2_0);
compat_symbol (libc, __ctype_tolower, __ctype_tolower, GLIBC_2_0);
compat_symbol (libc, __ctype_toupper, __ctype_toupper, GLIBC_2_0);
compat_symbol (libc, __ctype32_b, __ctype32_b, GLIBC_2_0);
compat_symbol (libc, __ctype32_tolower, __ctype32_tolower, GLIBC_2_2);
compat_symbol (libc, __ctype32_toupper, __ctype32_toupper, GLIBC_2_2);

#endif
