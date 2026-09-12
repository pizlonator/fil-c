#ifndef	_PRINTF_H

/* Workaround PR90731 with GCC 9 when using ldbl redirects in C++.  */
#include <bits/floatn.h>
#if defined __cplusplus && __LDOUBLE_REDIRECTS_TO_FLOAT128_ABI == 1
# if __GNUC_PREREQ (9, 0) && !__GNUC_PREREQ (9, 3)
#   pragma GCC system_header
# endif
#endif

#include <stdio-common/printf.h>

# ifndef _ISOMAC

/* Internal interfaces for registered specifiers.  */
extern printf_arginfo_size_function **__printf_arginfo_table attribute_hidden;
extern printf_function **__printf_function_table attribute_hidden;
extern printf_va_arg_function **__printf_va_arg_table attribute_hidden;
int __register_printf_specifier (int, printf_function,
				 printf_arginfo_size_function);
libc_hidden_proto (__register_printf_specifier)

/* The various kinds of arguments that can be passed to printf.  */
union printf_arg
  {
    wchar_t pa_wchar;
    int pa_int;
    long int pa_long_int;
    long long int pa_long_long_int;
    unsigned int pa_u_int;
    unsigned long int pa_u_long_int;
    unsigned long long int pa_u_long_long_int;
    double pa_double;
    long double pa_long_double;
#if __HAVE_FLOAT128_UNLIKE_LDBL
    _Float128 pa_float128;
#endif
    const char *pa_string;
    const wchar_t *pa_wstring;
    void *pa_pointer;
    void *pa_user;
};

/* Invoke a registered printf callback.  Called from vfprintf and vfwprintf.  */
int __printf_function_invoke (void *, printf_function callback,
			      union printf_arg *args_value,
			      size_t ndata_args,
			      struct printf_info *info) attribute_hidden;
int __wprintf_function_invoke (void *, printf_function callback,
			       union printf_arg *args_value,
			       size_t ndata_args,
			       struct printf_info *info) attribute_hidden;

#include <bits/types/locale_t.h>

/* Returns the width (as for printf, in bytes) of the converted ASCII
   number in the characters in the range [FIRST, LAST).  The range
   must only contain ASCII digits.  The caller is responsible for
   avoiding overflow.

   This function is used during non-wide digit translation.  Wide
   digit translate produces one wide character per ASCII digit,
   so the width is simply LAST - FIRST.  */
int __translated_number_width (locale_t loc,
			       const char *first, const char *last)
  attribute_hidden;


struct __printf_buffer;
void __printf_buffer (struct __printf_buffer *buf, const char *format,
		      va_list ap, unsigned int mode_flags);
struct __wprintf_buffer;
void __wprintf_buffer (struct __wprintf_buffer *buf, const wchar_t *format,
		       va_list ap, unsigned int mode_flags);

extern int __printf_fp (FILE *, const struct printf_info *,
			const void *const *);
libc_hidden_proto (__printf_fp)

void __printf_fphex_l_buffer (struct __printf_buffer *, locale_t,
			      const struct printf_info *,
			      const void *const *) attribute_hidden;
void __printf_fp_l_buffer (struct __printf_buffer *, locale_t,
			   const struct printf_info *,
			   const void *const *) attribute_hidden;
struct __wprintf_buffer;
void __wprintf_fphex_l_buffer (struct __wprintf_buffer *, locale_t,
			       const struct printf_info *,
			       const void *const *) attribute_hidden;
void __wprintf_fp_l_buffer (struct __wprintf_buffer *, locale_t,
			    const struct printf_info *,
			    const void *const *) attribute_hidden;

# endif /* !_ISOMAC */
#endif
