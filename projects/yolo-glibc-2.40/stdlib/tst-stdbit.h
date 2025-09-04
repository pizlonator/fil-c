/* Common test support for <stdbit.h> tests.
   Copyright (C) 2024 Free Software Foundation, Inc.
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

#ifndef _TST_STDBIT_H
#define _TST_STDBIT_H

#include <stdbit.h>
#include <stdbool.h>

#include <array_length.h>
#include <support/check.h>

struct stdbit_test
{
  /* The test input.  */
  uint64_t x;
  /* Expected results if that test input is converted to 8, 16, 32 or
     64 bits and then passed to the function under test for that
     width.  */
  uint64_t res_8, res_16, res_32, res_64;
};

#define TEST_TYPE(EXPR, TYPE)						\
  _Static_assert (_Generic ((EXPR), TYPE: 1, default: 0), "bad type")

/* Test a <stdbit.h> function / macro.  For each function family, and
   each input type, we test both with and without macros from the
   header being used, both with a possibly wider argument being passed
   (that must be truncated by the prototype) and with the argument
   truncated in the caller, as well as testing the type-generic macro
   (with the argument truncated in the caller).  Also test that the
   results have the correct type; also test truncation from
   floating-point arguments (valid for functions, including with macro
   expansion, because the prototype must implicitly convert to integer
   type; not valid for the type-generic macros).  Also test that the
   argument is evaluated exactly once.  Also test the macros are
   usable (e.g. in typeof) at top level (GCC doesn't allow ({})
   outside functions: bug 93239).  */

#define TEST_STDBIT_T(FUNC, X, RES, TTYPE, TYPE, SUFFIX)		\
  do									\
    {									\
      TEST_COMPARE (FUNC ## SUFFIX (X), (RES));				\
      TEST_TYPE (FUNC ## SUFFIX (X), TTYPE);				\
      TEST_COMPARE ((FUNC ## SUFFIX) (X), (RES));			\
      TEST_TYPE ((FUNC ## SUFFIX) (X), TTYPE);				\
      TEST_COMPARE (FUNC ## SUFFIX ((TYPE) (X)), (RES));		\
      TEST_TYPE (FUNC ## SUFFIX ((TYPE) (X)), TTYPE);			\
      TEST_COMPARE ((FUNC ## SUFFIX) ((TYPE) (X)), (RES));		\
      TEST_TYPE ((FUNC ## SUFFIX) ((TYPE) (X)), TTYPE);			\
      TEST_COMPARE (FUNC ((TYPE) (X)), (RES));				\
      TEST_TYPE (FUNC ((TYPE) (X)), TTYPE);				\
      if (sizeof (TYPE) <= 2)						\
	{								\
	  TEST_COMPARE (FUNC ## SUFFIX ((float) (TYPE) (X)), (RES));	\
	  TEST_TYPE (FUNC ## SUFFIX ((float) (TYPE) (X)), TTYPE);	\
	  TEST_COMPARE ((FUNC ## SUFFIX) ((float) (TYPE) (X)), (RES));	\
	  TEST_TYPE ((FUNC ## SUFFIX) ((float) (TYPE) (X)), TTYPE);	\
	}								\
      if (sizeof (TYPE) <= 4)						\
	{								\
	  TEST_COMPARE (FUNC ## SUFFIX ((double) (TYPE) (X)), (RES));	\
	  TEST_TYPE (FUNC ## SUFFIX ((double) (TYPE) (X)), TTYPE);	\
	  TEST_COMPARE ((FUNC ## SUFFIX) ((double) (TYPE) (X)), (RES));	\
	  TEST_TYPE ((FUNC ## SUFFIX) ((double) (TYPE) (X)), TTYPE);	\
	  TEST_COMPARE (FUNC ## SUFFIX ((long double) (TYPE) (X)), (RES)); \
	  TEST_TYPE (FUNC ## SUFFIX ((long double) (TYPE) (X)), TTYPE); \
	  TEST_COMPARE ((FUNC ## SUFFIX) ((long double) (TYPE) (X)), (RES)); \
	  TEST_TYPE ((FUNC ## SUFFIX) ((long double) (TYPE) (X)), TTYPE); \
	}								\
      TYPE xt = (X);							\
      TEST_COMPARE (FUNC ## SUFFIX (xt++), (RES));			\
      TEST_COMPARE (xt, (TYPE) ((X) + 1));				\
      xt = (X);								\
      TEST_COMPARE (FUNC (xt++), (RES));				\
      TEST_COMPARE (xt, (TYPE) ((X) + 1));				\
    }									\
  while (0)

#define TEST_STDBIT_UI(FUNC, INPUTS)			\
  do							\
    for (int i = 0; i < array_length (INPUTS); i++)	\
      {							\
	uint64_t x = (INPUTS)[i].x;			\
	unsigned int res_8 = (INPUTS)[i].res_8;		\
	unsigned int res_16 = (INPUTS)[i].res_16;	\
	unsigned int res_32 = (INPUTS)[i].res_32;	\
	unsigned int res_64 = (INPUTS)[i].res_64;	\
	unsigned int res_l = (sizeof (long int) == 4	\
			      ? res_32 : res_64);	\
	TEST_STDBIT_T (FUNC, x, res_8, unsigned int,	\
		       unsigned char, _uc);		\
	TEST_STDBIT_T (FUNC, x, res_16, unsigned int,	\
		       unsigned short, _us);		\
	TEST_STDBIT_T (FUNC, x, res_32, unsigned int,	\
		       unsigned int, _ui);		\
	TEST_STDBIT_T (FUNC, x, res_l, unsigned int,	\
		       unsigned long int, _ul);		\
	TEST_STDBIT_T (FUNC, x, res_64, unsigned int,	\
		       unsigned long long int, _ull);	\
      }							\
  while (0)

#define TEST_STDBIT_BOOL(FUNC, INPUTS)					\
  do									\
    for (int i = 0; i < array_length (INPUTS); i++)			\
      {									\
	uint64_t x = (INPUTS)[i].x;					\
	bool res_8 = (INPUTS)[i].res_8;					\
	bool res_16 = (INPUTS)[i].res_16;				\
	bool res_32 = (INPUTS)[i].res_32;				\
	bool res_64 = (INPUTS)[i].res_64;				\
	bool res_l = (sizeof (long int) == 4 ? res_32 : res_64);	\
	TEST_STDBIT_T (FUNC, x, res_8, _Bool, unsigned char, _uc);	\
	TEST_STDBIT_T (FUNC, x, res_16, _Bool, unsigned short, _us);	\
	TEST_STDBIT_T (FUNC, x, res_32, _Bool, unsigned int, _ui);	\
	TEST_STDBIT_T (FUNC, x, res_l, _Bool, unsigned long int, _ul);	\
	TEST_STDBIT_T (FUNC, x, res_64, _Bool,				\
		       unsigned long long int, _ull);			\
      }									\
  while (0)

#define TEST_STDBIT_SAME(FUNC, INPUTS)				\
  do								\
    for (int i = 0; i < array_length (INPUTS); i++)		\
      {								\
	uint64_t x = (INPUTS)[i].x;				\
	unsigned char res_8 = (INPUTS)[i].res_8;		\
	unsigned short res_16 = (INPUTS)[i].res_16;		\
	unsigned int res_32 = (INPUTS)[i].res_32;		\
	unsigned long long int res_64 = (INPUTS)[i].res_64;	\
	unsigned long int res_l = (sizeof (long int) == 4	\
				   ? res_32 : res_64);		\
	TEST_STDBIT_T (FUNC, x, res_8, unsigned char,		\
		       unsigned char, _uc);			\
	TEST_STDBIT_T (FUNC, x, res_16, unsigned short,		\
		       unsigned short, _us);			\
	TEST_STDBIT_T (FUNC, x, res_32, unsigned int,		\
		       unsigned int, _ui);			\
	TEST_STDBIT_T (FUNC, x, res_l, unsigned long int,	\
		       unsigned long int, _ul);			\
	TEST_STDBIT_T (FUNC, x, res_64, unsigned long long int,	\
		       unsigned long long int, _ull);		\
      }								\
  while (0)

#define TEST_STDBIT_UI_TOPLEVEL(FUNC)				\
  TEST_TYPE (FUNC ## _uc ((unsigned char) 0), unsigned int);	\
  TEST_TYPE (FUNC ((unsigned char) 0), unsigned int);		\
  TEST_TYPE (FUNC ## _us ((unsigned short) 0), unsigned int);	\
  TEST_TYPE (FUNC ((unsigned short) 0), unsigned int);		\
  TEST_TYPE (FUNC ## _ui (0U), unsigned int);			\
  TEST_TYPE (FUNC (0U), unsigned int);				\
  TEST_TYPE (FUNC ## _ul (0UL), unsigned int);			\
  TEST_TYPE (FUNC (0UL), unsigned int);				\
  TEST_TYPE (FUNC ## _ull (0ULL), unsigned int);		\
  TEST_TYPE (FUNC (0ULL), unsigned int)

#define TEST_STDBIT_BOOL_TOPLEVEL(FUNC)			\
  TEST_TYPE (FUNC ## _uc ((unsigned char) 0), _Bool);	\
  TEST_TYPE (FUNC ((unsigned char) 0), _Bool);		\
  TEST_TYPE (FUNC ## _us ((unsigned short) 0), _Bool);	\
  TEST_TYPE (FUNC ((unsigned short) 0), _Bool);		\
  TEST_TYPE (FUNC ## _ui (0U), _Bool);			\
  TEST_TYPE (FUNC (0U), _Bool);				\
  TEST_TYPE (FUNC ## _ul (0UL), _Bool);			\
  TEST_TYPE (FUNC (0UL), _Bool);			\
  TEST_TYPE (FUNC ## _ull (0ULL), _Bool);		\
  TEST_TYPE (FUNC (0ULL), _Bool)

#define TEST_STDBIT_SAME_TOPLEVEL(FUNC)				\
  TEST_TYPE (FUNC ## _uc ((unsigned char) 0), unsigned char);	\
  TEST_TYPE (FUNC ((unsigned char) 0), unsigned char);		\
  TEST_TYPE (FUNC ## _us ((unsigned short) 0), unsigned short);	\
  TEST_TYPE (FUNC ((unsigned short) 0), unsigned short);	\
  TEST_TYPE (FUNC ## _ui (0U), unsigned int);			\
  TEST_TYPE (FUNC (0U), unsigned int);				\
  TEST_TYPE (FUNC ## _ul (0UL), unsigned long int);		\
  TEST_TYPE (FUNC (0UL), unsigned long int);			\
  TEST_TYPE (FUNC ## _ull (0ULL), unsigned long long int);	\
  TEST_TYPE (FUNC (0ULL), unsigned long long int)

#endif
