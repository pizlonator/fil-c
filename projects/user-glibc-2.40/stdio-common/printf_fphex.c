/* Print floating point number in hexadecimal notation according to ISO C99.
   Copyright (C) 1997-2024 Free Software Foundation, Inc.
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

#include <array_length.h>
#include <assert.h>
#include <ctype.h>
#include <ieee754.h>
#include <math.h>
#include <printf.h>
#include <libioP.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <_itoa.h>
#include <_itowa.h>
#include <locale/localeinfo.h>
#include <stdbool.h>
#include <rounding-mode.h>
#include <sys/param.h>
#include <printf_buffer.h>
#include <errno.h>

#if __HAVE_DISTINCT_FLOAT128
# include "ieee754_float128.h"
# include <ldbl-128/printf_fphex_macros.h>
# define PRINT_FPHEX_FLOAT128 \
   PRINT_FPHEX (_Float128, fpnum.flt128, ieee854_float128, \
		IEEE854_FLOAT128_BIAS)
#endif

static void
__printf_fphex_buffer (struct __printf_buffer *buf,
		       const char *decimal,
		       const struct printf_info *info,
		       const void *const *args)
{
  /* The floating-point value to output.  */
  union
    {
      union ieee754_double dbl;
      long double ldbl;
#if __HAVE_DISTINCT_FLOAT128
      _Float128 flt128;
#endif
    }
  fpnum;

  /* This function always uses LC_NUMERIC.  */
  assert (info->extra == 0);

  /* "NaN" or "Inf" for the special cases.  */
  const char *special = NULL;

  /* Buffer for the generated number string for the mantissa.  The
     maximal size for the mantissa is 128 bits.  */
  char numbuf[32];
  char *numstr;
  char *numend;
  int negative;

  /* The maximal exponent of two in decimal notation has 5 digits.  */
  char expbuf[5];
  char *expstr;
  int expnegative;
  int exponent;

  /* Non-zero is mantissa is zero.  */
  int zero_mantissa;

  /* The leading digit before the decimal point.  */
  char leading;

  /* Precision.  */
  int precision = info->prec;

  /* Width.  */
  int width = info->width;

#define PRINTF_FPHEX_FETCH(FLOAT, VAR)					\
  {									\
    (VAR) = *(const FLOAT *) args[0];					\
									\
    /* Check for special values: not a number or infinity.  */		\
    if (isnan (VAR))							\
      {									\
	if (isupper (info->spec))					\
	  special = "NAN";						\
	else								\
	  special = "nan";						\
      }									\
    else								\
      {									\
	if (isinf (VAR))						\
	  {								\
	    if (isupper (info->spec))					\
	      special = "INF";						\
	    else							\
	      special = "inf";						\
	  }								\
      }									\
    negative = signbit (VAR);						\
  }

  /* Fetch the argument value.	*/
#if __HAVE_DISTINCT_FLOAT128
  if (info->is_binary128)
    PRINTF_FPHEX_FETCH (_Float128, fpnum.flt128)
  else
#endif
#ifndef __NO_LONG_DOUBLE_MATH
  if (info->is_long_double && sizeof (long double) > sizeof (double))
    PRINTF_FPHEX_FETCH (long double, fpnum.ldbl)
  else
#endif
    PRINTF_FPHEX_FETCH (double, fpnum.dbl.d)

#undef PRINTF_FPHEX_FETCH

  if (special)
    {
      int width = info->width;

      if (negative || info->showsign || info->space)
	--width;
      width -= 3;

      if (!info->left)
	__printf_buffer_pad (buf, ' ', width);

      if (negative)
	__printf_buffer_putc (buf, '-');
      else if (info->showsign)
	__printf_buffer_putc (buf, '+');
      else if (info->space)
	__printf_buffer_putc (buf, ' ');

      __printf_buffer_puts (buf, special);

      if (info->left)
	__printf_buffer_pad (buf, ' ', width);

      return;
    }

#if __HAVE_DISTINCT_FLOAT128
  if (info->is_binary128)
    PRINT_FPHEX_FLOAT128;
  else
#endif
  if (info->is_long_double == 0 || sizeof (double) == sizeof (long double))
    {
      /* We have 52 bits of mantissa plus one implicit digit.  Since
	 52 bits are representable without rest using hexadecimal
	 digits we use only the implicit digits for the number before
	 the decimal point.  */
      unsigned long long int num;

      num = (((unsigned long long int) fpnum.dbl.ieee.mantissa0) << 32
	     | fpnum.dbl.ieee.mantissa1);

      zero_mantissa = num == 0;

      if (sizeof (unsigned long int) > 6)
	  numstr = _itoa_word (num, numbuf + sizeof numbuf, 16,
			       info->spec == 'A');
      else
	  numstr = _itoa (num, numbuf + sizeof numbuf, 16,
			  info->spec == 'A');

      /* Fill with zeroes.  */
      while (numstr > numbuf + (sizeof numbuf - 13))
	*--numstr = '0';

      leading = fpnum.dbl.ieee.exponent == 0 ? '0' : '1';

      exponent = fpnum.dbl.ieee.exponent;

      if (exponent == 0)
	{
	  if (zero_mantissa)
	    expnegative = 0;
	  else
	    {
	      /* This is a denormalized number.  */
	      expnegative = 1;
	      exponent = IEEE754_DOUBLE_BIAS - 1;
	    }
	}
      else if (exponent >= IEEE754_DOUBLE_BIAS)
	{
	  expnegative = 0;
	  exponent -= IEEE754_DOUBLE_BIAS;
	}
      else
	{
	  expnegative = 1;
	  exponent = -(exponent - IEEE754_DOUBLE_BIAS);
	}
    }
#ifdef PRINT_FPHEX_LONG_DOUBLE
  else
    PRINT_FPHEX_LONG_DOUBLE;
#endif

  /* Look for trailing zeroes.  */
  if (! zero_mantissa)
    {
      numend = array_end (numbuf);
      while (numend[-1] == '0')
	  --numend;

      bool do_round_away = false;

      if (precision != -1 && precision < numend - numstr)
	{
	  char last_digit = precision > 0 ? numstr[precision - 1] : leading;
	  char next_digit = numstr[precision];
	  int last_digit_value = (last_digit >= 'A' && last_digit <= 'F'
				  ? last_digit - 'A' + 10
				  : (last_digit >= 'a' && last_digit <= 'f'
				     ? last_digit - 'a' + 10
				     : last_digit - '0'));
	  int next_digit_value = (next_digit >= 'A' && next_digit <= 'F'
				  ? next_digit - 'A' + 10
				  : (next_digit >= 'a' && next_digit <= 'f'
				     ? next_digit - 'a' + 10
				     : next_digit - '0'));
	  bool more_bits = ((next_digit_value & 7) != 0
			    || precision + 1 < numend - numstr);
	  int rounding_mode = get_rounding_mode ();
	  do_round_away = round_away (negative, last_digit_value & 1,
				      next_digit_value >= 8, more_bits,
				      rounding_mode);
	}

      if (precision == -1)
	precision = numend - numstr;
      else if (do_round_away)
	{
	  /* Round up.  */
	  int cnt = precision;
	  while (--cnt >= 0)
	    {
	      char ch = numstr[cnt];
	      /* We assume that the digits and the letters are ordered
		 like in ASCII.  This is true for the rest of GNU, too.  */
	      if (ch == '9')
		{
		  numstr[cnt] = info->spec;	/* This is tricky,
						   think about it!  */
		  break;
		}
	      else if (tolower (ch) < 'f')
		{
		  ++numstr[cnt];
		  break;
		}
	      else
		numstr[cnt] = '0';
	    }
	  if (cnt < 0)
	    {
	      /* The mantissa so far was fff...f  Now increment the
		 leading digit.  Here it is again possible that we
		 get an overflow.  */
	      if (leading == '9')
		leading = info->spec;
	      else if (tolower (leading) < 'f')
		++leading;
	      else
		{
		  leading = '1';
		  if (expnegative)
		    {
		      exponent -= 4;
		      if (exponent <= 0)
			{
			  exponent = -exponent;
			  expnegative = 0;
			}
		    }
		  else
		    exponent += 4;
		}
	    }
	}
    }
  else
    {
      if (precision == -1)
	precision = 0;
      numend = numstr;
    }

  /* Now we can compute the exponent string.  */
  expstr = _itoa_word (exponent, expbuf + sizeof expbuf, 10, 0);

  /* Now we have all information to compute the size.  */
  width -= ((negative || info->showsign || info->space)
	    /* Sign.  */
	    + 2    + 1 + 0 + precision + 1 + 1
	    /* 0x    h   .   hhh         P   ExpoSign.  */
	    + ((expbuf + sizeof expbuf) - expstr));
	    /* Exponent.  */

  /* Count the decimal point.
     A special case when the mantissa or the precision is zero and the `#'
     is not given.  In this case we must not print the decimal point.  */
  if (precision > 0 || info->alt)
    --width;

  if (!info->left && info->pad != '0')
    __printf_buffer_pad (buf, ' ', width);

  if (negative)
    __printf_buffer_putc (buf, '-');
  else if (info->showsign)
    __printf_buffer_putc (buf, '+');
  else if (info->space)
    __printf_buffer_putc (buf, ' ');

  __printf_buffer_putc (buf, '0');
  if ('X' - 'A' == 'x' - 'a')
    __printf_buffer_putc (buf, info->spec + ('x' - 'a'));
  else
    __printf_buffer_putc (buf, info->spec == 'A' ? 'X' : 'x');

  if (!info->left && info->pad == '0')
    __printf_buffer_pad (buf, '0', width);

  __printf_buffer_putc (buf, leading);

  if (precision > 0 || info->alt)
    __printf_buffer_puts (buf, decimal);

  if (precision > 0)
    {
      ssize_t tofill = precision - (numend - numstr);
      __printf_buffer_write (buf, numstr, MIN (numend - numstr, precision));
      __printf_buffer_pad (buf, '0', tofill);
    }

  if ('P' - 'A' == 'p' - 'a')
    __printf_buffer_putc (buf, info->spec + ('p' - 'a'));
  else
    __printf_buffer_putc (buf, info->spec == 'A' ? 'P' : 'p');

  __printf_buffer_putc (buf, expnegative ? '-' : '+');

  __printf_buffer_write (buf, expstr, (expbuf + sizeof expbuf) - expstr);

  if (info->left && info->pad != '0')
    __printf_buffer_pad (buf, info->pad, width);
}

void
__printf_fphex_l_buffer (struct __printf_buffer *buf, locale_t loc,
			 const struct printf_info *info,
			 const void *const *args)
{
  __printf_fphex_buffer (buf, _nl_lookup (loc, LC_NUMERIC, DECIMAL_POINT),
			 info, args);
}


/* The wide buffer version is implemented by translating the output of
   the multibyte version.  */

struct __printf_buffer_fphex_to_wide
{
  struct __printf_buffer base;
  wchar_t decimalwc;
  struct __wprintf_buffer *next;
  char untranslated[PRINTF_BUFFER_SIZE_DIGITS];
};

/* Translate to wide characters, rewriting "." to the actual decimal
   point.  */
void
__printf_buffer_flush_fphex_to_wide (struct __printf_buffer_fphex_to_wide *buf)
{
  /* No need to adjust buf->base.written, only buf->next->written matters.  */
  for (char *p = buf->untranslated; p < buf->base.write_ptr; ++p)
    {
      /* wchar_t overlaps with char in the ASCII range.  */
      wchar_t ch = *p;
      if (ch == L'.')
	ch = buf->decimalwc;
      __wprintf_buffer_putc (buf->next, ch);
    }

  if (!__wprintf_buffer_has_failed (buf->next))
    buf->base.write_ptr = buf->untranslated;
  else
    __printf_buffer_mark_failed (&buf->base);
}

void
__wprintf_fphex_l_buffer (struct __wprintf_buffer *next, locale_t loc,
			  const struct printf_info *info,
			  const void *const *args)
{
  struct __printf_buffer_fphex_to_wide buf;
  __printf_buffer_init (&buf.base, buf.untranslated, sizeof (buf.untranslated),
			__printf_buffer_mode_fphex_to_wide);
  buf.decimalwc = _nl_lookup_word (loc, LC_NUMERIC,
				   _NL_NUMERIC_DECIMAL_POINT_WC);
  buf.next = next;
  __printf_fphex_buffer (&buf.base, ".", info, args);
  if (__printf_buffer_has_failed (&buf.base))
    {
      __wprintf_buffer_mark_failed (buf.next);
      return;
    }
  __printf_buffer_flush_fphex_to_wide (&buf);
}
