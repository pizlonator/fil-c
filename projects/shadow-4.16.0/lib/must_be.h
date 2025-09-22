/*
 * SPDX-FileCopyrightText: 2019-2023, Alejandro Colomar <alx@kernel.org>
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef SHADOW_INCLUDE_LIBMISC_MUST_BE_H_
#define SHADOW_INCLUDE_LIBMISC_MUST_BE_H_


#include <config.h>

#include <assert.h>


/*
 * SYNOPSIS
 *	int must_be(bool e);
 *
 * ARGUMENTS
 *	e	Expression to be asserted.
 *
 * DESCRIPTION
 *	This macro fails compilation if 'e' is false.  If 'e' is true,
 *	it returns (int) 0, so it doesn't affect the expression in which
 *	it is contained.
 *
 *	This macro is similar to static_assert(3).  While
 *	static_assert(3) can only be used where a statement is allowed,
 *	this must_be() macro can be used wherever an expression is
 *	allowed.
 *
 * RETURN VALUE
 *	0
 *
 * ERRORS
 *	If 'e' is false, the compilation will fail, as when using
 *	static_assert(3).
 *
 * EXAMPLES
 *	#define must_be_array(a)  must_be(is_array(a))
 *
 *	#define NITEMS(a)  (sizeof(a) / sizeof(*(a)) + must_be_array(a))
 *
 *	int foo[42];
 *	int bar[NITEMS(foo)];
 */


#define must_be(e)                                                            \
(                                                                             \
	0 * (int) sizeof(                                                     \
		struct {                                                      \
			static_assert(e, "");                                 \
			int ISO_C_forbids_a_struct_with_no_members_;          \
		}                                                             \
	)                                                                     \
)


/*
 * SYNOPSIS
 *	int must_be_array(a);
 *
 * ARGUMENTS
 *	a	Array.
 *
 * DESCRIPTION
 *	This macro fails compilation if 'a' is not an array.  It is
 *	useful in macros that accept an array as a parameter, where this
 *	macro can validate the macro argument.  It prevent passing a
 *	pointer to such macros, which would otherwise produce silent
 *	bugs.
 *
 * RETURN VALUE
 *	0
 *
 * ERRORS
 *	If 'a' is not an array, the compilation will fail.
 *
 * EXAMPLES
 *	int a[10];
 *	int *p;
 *
 *	must_be_array(a);  // Ok
 *	must_be_array(p);  // Compile-time error
 *
 * SEE ALSO
 *	must_be()
 */


#define is_same_type(a, b)    __builtin_types_compatible_p(a, b)
#define is_same_typeof(a, b)  is_same_type(typeof(a), typeof(b))
#define is_array(a)           (!is_same_typeof((a), &(a)[0]))
#define must_be_array(a)      must_be(is_array(a))


#endif  // include guard
