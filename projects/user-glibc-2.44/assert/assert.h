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

/*
 *	ISO C99 Standard: 7.2 Diagnostics	<assert.h>
 */

#ifdef	_ASSERT_H

# undef	_ASSERT_H
# undef	assert
# undef __ASSERT_VOID_CAST

# ifdef	__USE_GNU
#  undef assert_perror
# endif

#endif /* assert.h	*/

#define	_ASSERT_H	1
#include <features.h>

#if __GLIBC_USE (ISOC23)
# ifndef __STDC_VERSION_ASSERT_H__
#  define __STDC_VERSION_ASSERT_H__ 202311L
# endif
#endif

#if defined __cplusplus && __GNUC_PREREQ (2,95)
# define __ASSERT_VOID_CAST static_cast<void>
#else
# define __ASSERT_VOID_CAST (void)
#endif

/* C23 makes assert a variadic macro so that expressions with a comma
   not between parentheses, but that would still be valid as a single
   function argument, such as those involving compound literals with a
   comma in the initializer list, can be passed to assert.  This
   depends on support for variadic macros (added in C99 and GCC 2.95),
   and on support for _Bool (added in C99 and GCC 3.0) in order to
   validate that only a single expression is passed as an argument.  */
#if ((__GLIBC_USE (ISOC23)						\
      && (defined __GNUC__						\
	  ? __GNUC_PREREQ (3, 0)					\
	  : defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L))	\
     || (defined __cplusplus && __cplusplus > 202302L))
# define __ASSERT_VARIADIC 1
#else
# define __ASSERT_VARIADIC 0
#endif

/* void assert (int expression);

   If NDEBUG is defined, do nothing.
   If not, and EXPRESSION is zero, print an error message and abort.  */

#ifdef	NDEBUG

# if __ASSERT_VARIADIC
#  define assert(...)		(__ASSERT_VOID_CAST (0))
# else
#  define assert(expr)		(__ASSERT_VOID_CAST (0))
# endif

/* void assert_perror (int errnum);

   If NDEBUG is defined, do nothing.  If not, and ERRNUM is not zero, print an
   error message with the error text for ERRNUM and abort.
   (This is a GNU extension.) */

# ifdef	__USE_GNU
#  define assert_perror(errnum)	(__ASSERT_VOID_CAST (0))
# endif

#else /* Not NDEBUG.  */

__BEGIN_DECLS

/* This prints an "Assertion failed" message and aborts.  */
extern void __assert_fail (const char *__assertion, const char *__file,
			   unsigned int __line, const char *__function)
     __THROW __attribute__ ((__noreturn__)) __COLD;

/* Likewise, but prints the error text for ERRNUM.  */
extern void __assert_perror_fail (int __errnum, const char *__file,
				  unsigned int __line, const char *__function)
     __THROW __attribute__ ((__noreturn__)) __COLD;


/* The following is not at all used here but needed for standard
   compliance.  */
extern void __assert (const char *__assertion, const char *__file, int __line)
     __THROW __attribute__ ((__noreturn__)) __COLD;


# if __ASSERT_VARIADIC && !defined __cplusplus
/* This function is not defined and is not called outside of an
   unevaluated sizeof, but serves to verify that the argument to
   assert is a single expression.  */
extern _Bool __assert_single_arg (_Bool);
# endif

__END_DECLS

/* When possible, define assert so that it does not add extra
   parentheses around EXPR.  Otherwise, those added parentheses would
   suppress warnings we'd expect to be detected by gcc's -Wparentheses.  */
# if defined __cplusplus
#  if defined __has_builtin
#   if __has_builtin (__builtin_FILE)
#    define __ASSERT_FILE __builtin_FILE ()
#    define __ASSERT_LINE __builtin_LINE ()
#   endif
#  endif
#  if !defined __ASSERT_FILE
#   define __ASSERT_FILE __FILE__
#   define __ASSERT_LINE __LINE__
#  endif
#  if __ASSERT_VARIADIC
/* The first test of __VA_ARGS__ evaluates it without converting scoped
   enumeration values to bool, and the second test checks that it is a
   single expression without evaluating it.  */
#    define assert(...)							\
     ((__VA_ARGS__)							\
      ? void (1 ? 1 : bool (__VA_ARGS__))				\
      : __assert_fail (#__VA_ARGS__, __ASSERT_FILE, __ASSERT_LINE,	\
                       __ASSERT_FUNCTION))
#  else
#    define assert(expr)						\
     (static_cast <bool> (expr)						\
      ? void (0)							\
      : __assert_fail (#expr, __ASSERT_FILE, __ASSERT_LINE,             \
                       __ASSERT_FUNCTION))
#  endif
# elif !defined __GNUC__ || defined __STRICT_ANSI__
#  if __ASSERT_VARIADIC
#   define assert(...)							\
    (((void) sizeof (__assert_single_arg (__VA_ARGS__)), __VA_ARGS__)	\
     ? __ASSERT_VOID_CAST (0)						\
     : __assert_fail (#__VA_ARGS__, __FILE__, __LINE__, __ASSERT_FUNCTION))
#  else
#   define assert(expr)							\
    ((expr)								\
     ? __ASSERT_VOID_CAST (0)						\
     : __assert_fail (#expr, __FILE__, __LINE__, __ASSERT_FUNCTION))
#  endif
# else
#  if __ASSERT_VARIADIC
#   define assert(...)							\
    ((void) sizeof (__assert_single_arg (__VA_ARGS__)), __extension__ ({ \
      if (__VA_ARGS__)							\
        ; /* empty */							\
      else								\
        __assert_fail (#__VA_ARGS__, __FILE__, __LINE__, __ASSERT_FUNCTION); \
    }))
#  else
/* The first occurrence of EXPR is not evaluated due to the sizeof,
   but will trigger any pedantic warnings masked by the __extension__
   for the second occurrence.  The ternary operator is required to
   support function pointers and bit fields in this context, and to
   suppress the evaluation of variable length arrays.  */
#   define assert(expr)							\
  ((void) sizeof ((expr) ? 1 : 0), __extension__ ({			\
      if (expr)								\
        ; /* empty */							\
      else								\
        __assert_fail (#expr, __FILE__, __LINE__, __ASSERT_FUNCTION);	\
    }))
#  endif
# endif

# ifdef	__USE_GNU
#  define assert_perror(errnum)						\
  (!(errnum)								\
   ? __ASSERT_VOID_CAST (0)						\
   : __assert_perror_fail ((errnum), __FILE__, __LINE__, __ASSERT_FUNCTION))
# endif

/* Version 2.4 and later of GCC define a magical variable `__PRETTY_FUNCTION__'
   which contains the name of the function currently being defined.
   This is broken in G++ before version 2.6.
   C9x has a similar variable called __func__, but prefer the GCC one since
   it demangles C++ function names.  */
# if defined __cplusplus ? __GNUC_PREREQ (2, 6) : __GNUC_PREREQ (2, 4)
#   define __ASSERT_FUNCTION	__extension__ __PRETTY_FUNCTION__
# else
#  if defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L
#   define __ASSERT_FUNCTION	__func__
#  else
#   define __ASSERT_FUNCTION	((const char *) 0)
#  endif
# endif

#endif /* NDEBUG.  */


#if (defined __USE_ISOC11			\
     && (!defined __STDC_VERSION__		\
	 || __STDC_VERSION__ <= 201710L		\
	 || !__GNUC_PREREQ (13, 0))		\
     && !defined __cplusplus)
# undef static_assert
# define static_assert _Static_assert
#endif
