/* Formatting a monetary value according to the given locale.
   Copyright (C) 1996-2024 Free Software Foundation, Inc.
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

#include <ctype.h>
#include <errno.h>
#include <langinfo.h>
#include <locale.h>
#include <monetary.h>
#include "../libio/libioP.h"
#include "../libio/strfile.h"
#include <printf.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../locale/localeinfo.h"
#include <bits/floatn.h>
#include <stdio-common/grouping_iterator.h>
#include <printf_buffer.h>

#define to_digit(Ch) ((Ch) - '0')


/* We use this code also for the extended locale handling where the
   function gets as an additional argument the locale which has to be
   used.  To access the values we have to redefine the _NL_CURRENT
   macro.  */
#undef _NL_CURRENT
#define _NL_CURRENT(category, item) \
  (current->values[_NL_ITEM_INDEX (item)].string)


/* We have to overcome some problems with this implementation.  On the
   one hand the strfmon() function is specified in XPG4 and of course
   it has to follow this.  But on the other hand POSIX.2 specifies
   some information in the LC_MONETARY category which should be used,
   too.  Some of the information contradicts the information which can
   be specified in format string.  */
static void
__vstrfmon_l_buffer (struct __printf_buffer *buf, locale_t loc,
		     const char *fmt, va_list ap, unsigned int flags)
{
  struct __locale_data *current = loc->__locales[LC_MONETARY];
  struct printf_info info;

  /* Loop through the format-string.  */
  while (*fmt != '\0' && !__printf_buffer_has_failed (buf))
    {
      /* The floating-point value to output.  */
      union
      {
	double dbl;
	long double ldbl;
#if __HAVE_DISTINCT_FLOAT128
	_Float128 f128;
#endif
      }
      fpnum;
      int int_format;
      int print_curr_symbol;
      int left_prec;
      int left_pad;
      int right_prec;
      int group;
      char pad;
      int is_long_double;
      int is_binary128;
      int p_sign_posn;
      int n_sign_posn;
      int sign_posn;
      int other_sign_posn;
      int left;
      int is_negative;
      int sep_by_space;
      int other_sep_by_space;
      int cs_precedes;
      int other_cs_precedes;
      const char *sign_string;
      const char *other_sign_string;
      const char *currency_symbol;
      size_t currency_symbol_len;
      long int width;
      const void *ptr;
      char space_char;

      /* Process all character which do not introduce a format
	 specification.  */
      if (*fmt != '%')
	{
	  __printf_buffer_putc (buf, *fmt++);
	  continue;
	}

      /* "%%" means a single '%' character.  */
      if (fmt[1] == '%')
	{
	  __printf_buffer_putc (buf, *++fmt);
	  ++fmt;
	  continue;
	}

      /* Defaults for formatting.  */
      int_format = 0;			/* Use international curr. symbol */
      print_curr_symbol = 1;		/* Print the currency symbol.  */
      left_prec = -1;			/* No left precision specified.  */
      right_prec = -1;			/* No right precision specified.  */
      group = 1;			/* Print digits grouped.  */
      pad = ' ';			/* Fill character is <SP>.  */
      is_long_double = 0;		/* Double argument by default.  */
      is_binary128 = 0;			/* Long double argument by default.  */
      p_sign_posn = -2;			/* This indicates whether the */
      n_sign_posn = -2;			/* '(' flag is given.  */
      width = -1;			/* No width specified so far.  */
      left = 0;				/* Right justified by default.  */

      /* Parse group characters.  */
      while (1)
	{
	  switch (*++fmt)
	    {
	    case '=':			/* Set fill character.  */
	      pad = *++fmt;
	      if (pad == '\0')
		{
		  /* Premature EOS.  */
		  __set_errno (EINVAL);
		  __printf_buffer_mark_failed (buf);
		  return;
		}
	      continue;
	    case '^':			/* Don't group digits.  */
	      group = 0;
	      continue;
	    case '+':			/* Use +/- for sign of number.  */
	      if (n_sign_posn != -2)
		{
		  __set_errno (EINVAL);
		  __printf_buffer_mark_failed (buf);
		  return;
		}
	      p_sign_posn = *_NL_CURRENT (LC_MONETARY, P_SIGN_POSN);
	      n_sign_posn = *_NL_CURRENT (LC_MONETARY, N_SIGN_POSN);
	      continue;
	    case '(':			/* Use ( ) for negative sign.  */
	      if (n_sign_posn != -2)
		{
		  __set_errno (EINVAL);
		  __printf_buffer_mark_failed (buf);
		  return;
		}
	      p_sign_posn = 0;
	      n_sign_posn = 0;
	      continue;
	    case '!':			/* Don't print the currency symbol.  */
	      print_curr_symbol = 0;
	      continue;
	    case '-':			/* Print left justified.  */
	      left = 1;
	      continue;
	    default:
	      /* Will stop the loop.  */;
	    }
	  break;
	}

      if (isdigit (*fmt))
	{
	  /* Parse field width.  */
	  width = to_digit (*fmt);

	  while (isdigit (*++fmt))
	    {
	      int val = to_digit (*fmt);

	      if (width > LONG_MAX / 10
		  || (width == LONG_MAX && val > LONG_MAX % 10))
		{
		  __set_errno (E2BIG);
		  __printf_buffer_mark_failed (buf);
		  return;
		}

	      width = width * 10 + val;
	    }
	}

      /* Recognize left precision.  */
      if (*fmt == '#')
	{
	  if (!isdigit (*++fmt))
	    {
	      __set_errno (EINVAL);
	      __printf_buffer_mark_failed (buf);
	      return;
	    }
	  left_prec = to_digit (*fmt);

	  while (isdigit (*++fmt))
	    {
	      left_prec *= 10;
	      left_prec += to_digit (*fmt);
	    }
	}

      /* Recognize right precision.  */
      if (*fmt == '.')
	{
	  if (!isdigit (*++fmt))
	    {
	      __set_errno (EINVAL);
	      __printf_buffer_mark_failed (buf);
	      return;
	    }
	  right_prec = to_digit (*fmt);

	  while (isdigit (*++fmt))
	    {
	      right_prec *= 10;
	      right_prec += to_digit (*fmt);
	    }
	}

      /* Handle modifier.  This is an extension.  */
      if (*fmt == 'L')
	{
	  ++fmt;
	  if (__glibc_likely ((flags & STRFMON_LDBL_IS_DBL) == 0))
	    is_long_double = 1;
#if __HAVE_DISTINCT_FLOAT128
	  if (__glibc_likely ((flags & STRFMON_LDBL_USES_FLOAT128) != 0))
	    is_binary128 = is_long_double;
#endif
	}

      /* Handle format specifier.  */
      char int_symbol[4];
      switch (*fmt++)
	{
	case 'i': {		/* Use international currency symbol.  */
	  const char *int_curr_symbol;

	  int_curr_symbol = _NL_CURRENT (LC_MONETARY, INT_CURR_SYMBOL);
	  strncpy(int_symbol, int_curr_symbol, 3);
	  int_symbol[3] = '\0';

	  currency_symbol_len = 3;
	  currency_symbol = &int_symbol[0];
	  space_char = int_curr_symbol[3];
	  int_format = 1;
	  break;
	}
	case 'n':		/* Use national currency symbol.  */
	  currency_symbol = _NL_CURRENT (LC_MONETARY, CURRENCY_SYMBOL);
	  currency_symbol_len = strlen (currency_symbol);
	  space_char = ' ';
	  int_format = 0;
	  break;
	default:		/* Any unrecognized format is an error.  */
	  __set_errno (EINVAL);
	  __printf_buffer_mark_failed (buf);
	  return;
	}

      /* If not specified by the format string now find the values for
	 the format specification.  */
      if (p_sign_posn == -2)
	p_sign_posn = *_NL_CURRENT (LC_MONETARY, int_format ? INT_P_SIGN_POSN : P_SIGN_POSN);
      if (n_sign_posn == -2)
	n_sign_posn = *_NL_CURRENT (LC_MONETARY, int_format ? INT_N_SIGN_POSN : N_SIGN_POSN);

      if (right_prec == -1)
	{
	  right_prec = *_NL_CURRENT (LC_MONETARY, int_format ? INT_FRAC_DIGITS : FRAC_DIGITS);

	  if (right_prec == '\377')
	    right_prec = 2;
	}

      /* If we have to print the digits grouped determine how many
	 extra characters this means.  */
      if (group && left_prec != -1)
	{
	  struct grouping_iterator it;
	  __grouping_iterator_init (&it, LC_MONETARY, loc, left_prec);
	  left_prec += it.separators;
	}

      /* Now it's time to get the value.  */
      if (is_long_double == 1)
	{
#if __HAVE_DISTINCT_FLOAT128
	  if (is_binary128 == 1)
	    {
	      fpnum.f128 = va_arg (ap, _Float128);
	      is_negative = fpnum.f128 < 0;
	      if (is_negative)
	        fpnum.f128 = -fpnum.f128;
	    }
	  else
#endif
	  {
	    fpnum.ldbl = va_arg (ap, long double);
	    is_negative = fpnum.ldbl < 0;
	    if (is_negative)
	      fpnum.ldbl = -fpnum.ldbl;
	  }
	}
      else
	{
	  fpnum.dbl = va_arg (ap, double);
	  is_negative = fpnum.dbl < 0;
	  if (is_negative)
	    fpnum.dbl = -fpnum.dbl;
	}

      /* We now know the sign of the value and can determine the format.  */
      if (is_negative)
	{
	  sign_string = _NL_CURRENT (LC_MONETARY, NEGATIVE_SIGN);
	  /* If the locale does not specify a character for the
	     negative sign we use a '-'.  */
	  if (*sign_string == '\0')
	    sign_string = (const char *) "-";
	  cs_precedes = *_NL_CURRENT (LC_MONETARY, int_format ? INT_N_CS_PRECEDES : N_CS_PRECEDES);
	  sep_by_space = *_NL_CURRENT (LC_MONETARY, int_format ? INT_N_SEP_BY_SPACE : N_SEP_BY_SPACE);
	  sign_posn = n_sign_posn;

	  other_sign_string = _NL_CURRENT (LC_MONETARY, POSITIVE_SIGN);
	  other_cs_precedes = *_NL_CURRENT (LC_MONETARY, int_format ? INT_P_CS_PRECEDES : P_CS_PRECEDES);
	  other_sep_by_space = *_NL_CURRENT (LC_MONETARY, int_format ? INT_P_SEP_BY_SPACE : P_SEP_BY_SPACE);
	  other_sign_posn = p_sign_posn;
	}
      else
	{
	  sign_string = _NL_CURRENT (LC_MONETARY, POSITIVE_SIGN);
	  cs_precedes = *_NL_CURRENT (LC_MONETARY, int_format ? INT_P_CS_PRECEDES : P_CS_PRECEDES);
	  sep_by_space = *_NL_CURRENT (LC_MONETARY, int_format ? INT_P_SEP_BY_SPACE : P_SEP_BY_SPACE);
	  sign_posn = p_sign_posn;

	  other_sign_string = _NL_CURRENT (LC_MONETARY, NEGATIVE_SIGN);
	  if (*other_sign_string == '\0')
	    other_sign_string = (const char *) "-";
	  other_cs_precedes = *_NL_CURRENT (LC_MONETARY, int_format ? INT_N_CS_PRECEDES : N_CS_PRECEDES);
	  other_sep_by_space = *_NL_CURRENT (LC_MONETARY, int_format ? INT_N_SEP_BY_SPACE : N_SEP_BY_SPACE);
	  other_sign_posn = n_sign_posn;
	}

      /* Set default values for unspecified information.  */
      if (cs_precedes != 0)
	cs_precedes = 1;
      if (other_cs_precedes != 0)
	other_cs_precedes = 1;
      if (sep_by_space == '\377')
	sep_by_space = 0;
      if (other_sep_by_space == '\377')
	other_sep_by_space = 0;
      if (sign_posn == '\377')
	sign_posn = 1;
      if (other_sign_posn == '\377')
	other_sign_posn = 1;

      /* Check for degenerate cases */
      if (sep_by_space == 2)
	{
	  if (sign_posn == 0
	      || (sign_posn == 1 && !cs_precedes)
	      || (sign_posn == 2 && cs_precedes))
	    /* sign and symbol are not adjacent, so no separator */
	    sep_by_space = 0;
	}
      if (other_sep_by_space == 2)
	{
	  if (other_sign_posn == 0
	      || (other_sign_posn == 1 && !other_cs_precedes)
	      || (other_sign_posn == 2 && other_cs_precedes))
	    /* sign and symbol are not adjacent, so no separator */
	    other_sep_by_space = 0;
	}

      /* Set the left precision and padding needed for alignment */
      if (left_prec == -1)
	{
	  left_prec = 0;
	  left_pad = 0;
	}
      else
	{
	  /* Set left_pad to number of spaces needed to align positive
	     and negative formats */

	  int left_bytes = 0;
	  int other_left_bytes = 0;

	  /* Work out number of bytes for currency string and separator
	     preceding the value */
	  if (cs_precedes)
	    {
	      left_bytes += currency_symbol_len;
	      if (sep_by_space != 0)
		++left_bytes;
	    }

	  if (other_cs_precedes)
	    {
	      other_left_bytes += currency_symbol_len;
	      if (other_sep_by_space != 0)
		++other_left_bytes;
	    }

	  /* Work out number of bytes for the sign (or left parenthesis)
	     preceding the value */
	  if (sign_posn == 0 && is_negative)
	    ++left_bytes;
	  else if (sign_posn == 1)
	    left_bytes += strlen (sign_string);
	  else if (cs_precedes && (sign_posn == 3 || sign_posn == 4))
	    left_bytes += strlen (sign_string);

	  if (other_sign_posn == 0 && !is_negative)
	    ++other_left_bytes;
	  else if (other_sign_posn == 1)
	    other_left_bytes += strlen (other_sign_string);
	  else if (other_cs_precedes
		   && (other_sign_posn == 3 || other_sign_posn == 4))
	    other_left_bytes += strlen (other_sign_string);

	  /* Compare the number of bytes preceding the value for
	     each format, and set the padding accordingly */
	  if (other_left_bytes > left_bytes)
	    left_pad = other_left_bytes - left_bytes;
	  else
	    left_pad = 0;
	}

      /* Perhaps we'll someday make these things configurable so
	 better start using symbolic names now.  */
#define left_paren '('
#define right_paren ')'

      char *startp = buf->write_ptr;

      __printf_buffer_pad (buf, ' ', left_pad);

      if (sign_posn == 0 && is_negative)
	__printf_buffer_putc (buf, left_paren);

      if (cs_precedes)
	{
	  if (sign_posn != 0 && sign_posn != 2 && sign_posn != 4
	      && sign_posn != 5)
	    {
	      __printf_buffer_puts (buf, sign_string);
	      if (sep_by_space == 2)
		__printf_buffer_putc (buf, ' ');
	    }

	  if (print_curr_symbol)
	    __printf_buffer_puts (buf, currency_symbol);

	  if (sign_posn == 4)
	    {
	      if (print_curr_symbol && sep_by_space == 2)
		__printf_buffer_putc (buf, space_char);
	      __printf_buffer_puts (buf, sign_string);
	      if (sep_by_space == 1)
		/* POSIX.2 and SUS are not clear on this case, but C99
		   says a space follows the adjacent-symbol-and-sign */
		__printf_buffer_putc (buf, ' ');
	    }
	  else
	    if (print_curr_symbol && sep_by_space == 1)
	      __printf_buffer_putc (buf, space_char);
	}
      else
	if (sign_posn != 0 && sign_posn != 2 && sign_posn != 3
	    && sign_posn != 4 && sign_posn != 5)
	  __printf_buffer_puts (buf, sign_string);

      /* Print the number.  */
      memset (&info, '\0', sizeof (info));
      info.prec = right_prec;
      info.width = left_prec + (right_prec ? (right_prec + 1) : 0);
      info.spec = 'f';
      info.is_long_double = is_long_double;
      info.is_binary128 = is_binary128;
      info.group = group;
      info.pad = pad;
      info.extra = 1;		/* This means use values from LC_MONETARY.  */

      ptr = &fpnum;
      __printf_fp_l_buffer (buf, loc, &info, &ptr);
      if (__printf_buffer_has_failed (buf))
	return;

      if (!cs_precedes)
	{
	  if (sign_posn == 3)
	    {
	      if (sep_by_space == 1)
		__printf_buffer_putc (buf, ' ');
	      __printf_buffer_puts (buf, sign_string);
	    }

	  if (print_curr_symbol)
	    {
	      if ((sign_posn == 3 && sep_by_space == 2)
		  || (sign_posn == 4 && sep_by_space == 1)
		  || (sign_posn == 2 && sep_by_space == 1)
		  || (sign_posn == 1 && sep_by_space == 1)
		  || (sign_posn == 0 && sep_by_space == 1))
		__printf_buffer_putc (buf, space_char);
	      __printf_buffer_write (buf, currency_symbol,
				       __strnlen (currency_symbol,
						  currency_symbol_len));
	    }

	  if (sign_posn == 4)
	    {
	      if (sep_by_space == 2)
		__printf_buffer_putc (buf, ' ');
	      __printf_buffer_puts (buf, sign_string);
	    }
	}

      if (sign_posn == 2)
	{
	  if (sep_by_space == 2)
	    __printf_buffer_putc (buf, ' ');
	  __printf_buffer_puts (buf, sign_string);
	}

      if (sign_posn == 0 && is_negative)
	__printf_buffer_putc (buf, right_paren);

      /* Now test whether the output width is filled.  */
      if (buf->write_ptr - startp < width)
	{
	  size_t pad_width = width - (buf->write_ptr - startp);
	  __printf_buffer_pad (buf, ' ', pad_width);
	  if (__printf_buffer_has_failed (buf))
	    /* Implies length check.  */
	    return;
	  /* Left padding is already in the correct position.
	     Otherwise move the field contents in place.  */
	  if (!left)
	    {
	      memmove (startp + pad_width, startp, buf->write_ptr - startp);
	      memset (startp, ' ', pad_width);
	    }
	}
    }
}

ssize_t
__vstrfmon_l_internal (char *s, size_t maxsize, locale_t loc,
		       const char *format, va_list ap, unsigned int flags)
{
  struct __printf_buffer buf;
  __printf_buffer_init (&buf, s, maxsize, __printf_buffer_mode_strfmon);
  __vstrfmon_l_buffer (&buf, loc, format, ap, flags);
  __printf_buffer_putc (&buf, '\0'); /* Terminate the string.  */
  if (__printf_buffer_has_failed (&buf))
    return -1;
  else
    return buf.write_ptr - buf.write_base - 1; /* Exclude NUL byte.  */
}

ssize_t
___strfmon_l (char *s, size_t maxsize, locale_t loc, const char *format, ...)
{
  va_list ap;

  va_start (ap, format);

  ssize_t res = __vstrfmon_l_internal (s, maxsize, loc, format, ap, 0);

  va_end (ap);

  return res;
}
ldbl_strong_alias (___strfmon_l, __strfmon_l)
ldbl_weak_alias (___strfmon_l, strfmon_l)
