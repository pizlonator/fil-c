/* Argument-processing fragment for vfprintf.
   Copyright (C) 1991-2024 Free Software Foundation, Inc.
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

/* This file is included twice from vfprintf-internal.c, for standard
   and GNU-style positional (%N$) arguments.  Before that,
   process_arg_int etc. macros have to be defined to extract one
   argument of the appropriate type, in addition to the file-specific
   macros in vfprintf-internal.c.  */

{
  /* Start real work.  We know about all flags and modifiers and
     now process the wanted format specifier.  */
LABEL (form_percent):
  /* Write a literal "%".  */
  Xprintf_buffer_putc (buf, L_('%'));
  break;

LABEL (form_integer):
  /* Signed decimal integer.  */
  base = 10;

  if (is_longlong)
    {
      long long int signed_number = process_arg_long_long_int ();
      is_negative = signed_number < 0;
      number.longlong = is_negative ? (- signed_number) : signed_number;

      goto LABEL (longlong_number);
    }
  else
    {
      long int signed_number;
      if (is_long_num)
        signed_number = process_arg_long_int ();
      else if (is_char)
        signed_number = (signed char) process_arg_unsigned_int ();
      else if (!is_short)
        signed_number = process_arg_int ();
      else
        signed_number = (short int) process_arg_unsigned_int ();

      is_negative = signed_number < 0;
      number.word = is_negative ? (- signed_number) : signed_number;

      goto LABEL (number);
    }
  /* NOTREACHED */

LABEL (form_unsigned):
  /* Unsigned decimal integer.  */
  base = 10;
  goto LABEL (unsigned_number);
  /* NOTREACHED */

LABEL (form_octal):
  /* Unsigned octal integer.  */
  base = 8;
  goto LABEL (unsigned_number);
  /* NOTREACHED */

LABEL (form_hexa):
  /* Unsigned hexadecimal integer.  */
  base = 16;
  goto LABEL (unsigned_number);
  /* NOTREACHED */

LABEL (form_binary):
  /* Unsigned binary integer.  */
  base = 2;
  goto LABEL (unsigned_number);
  /* NOTREACHED */

LABEL (unsigned_number):      /* Unsigned number of base BASE.  */

  /* ISO specifies the `+' and ` ' flags only for signed
     conversions.  */
  is_negative = 0;
  showsign = 0;
  space = 0;

  if (is_longlong)
    {
      number.longlong = process_arg_unsigned_long_long_int ();

      LABEL (longlong_number):
      if (prec < 0)
        /* Supply a default precision if none was given.  */
        prec = 1;
      else
        /* We have to take care for the '0' flag.  If a precision
           is given it must be ignored.  */
        pad = L_(' ');

      /* If the precision is 0 and the number is 0 nothing has to
         be written for the number, except for the 'o' format in
         alternate form.  */
      if (prec == 0 && number.longlong == 0)
        {
          string = workend;
          if (base == 8 && alt)
            *--string = L_('0');
        }
      else
        /* Put the number in WORK.  */
        string = _itoa (number.longlong, workend, base, spec == L_('X'));
      /* Simplify further test for num != 0.  */
      number.word = number.longlong != 0;
    }
  else
    {
      if (is_long_num)
        number.word = process_arg_unsigned_long_int ();
      else if (is_char)
        number.word = (unsigned char) process_arg_unsigned_int ();
      else if (!is_short)
        number.word = process_arg_unsigned_int ();
      else
        number.word = (unsigned short int) process_arg_unsigned_int ();

      LABEL (number):
      if (prec < 0)
        /* Supply a default precision if none was given.  */
        prec = 1;
      else
        /* We have to take care for the '0' flag.  If a precision
           is given it must be ignored.  */
        pad = L_(' ');

      /* If the precision is 0 and the number is 0 nothing has to
         be written for the number, except for the 'o' format in
         alternate form.  */
      if (prec == 0 && number.word == 0)
        {
          string = workend;
          if (base == 8 && alt)
            *--string = L_('0');
        }
      else
        /* Put the number in WORK.  */
        string = _itoa_word (number.word, workend, base,
                             spec == L_('X'));
    }

  /* Grouping is also used for outdigits translation.  */
  struct grouping_iterator iter;
  bool number_slow_path = group || (use_outdigits && base == 10);
  if (group)
    __grouping_iterator_init (&iter, LC_NUMERIC, _NL_CURRENT_LOCALE,
                              workend - string);
  else if (use_outdigits && base == 10)
    __grouping_iterator_init_none (&iter, workend - string);

  int number_length;
#ifndef COMPILE_WPRINTF
  if (use_outdigits && base == 10)
    number_length = __translated_number_width (_NL_CURRENT_LOCALE,
                                               string, workend);
  else
    number_length = workend - string;
  if (group)
    number_length += iter.separators * strlen (thousands_sep);
#else
  number_length = workend - string;
  /* All wide separators have length 1.  */
  if (group && thousands_sep != L'\0')
    number_length += iter.separators;
#endif

  /* The marker comes right before the number, but is not subject
     to grouping.  */
  bool octal_marker = (prec <= number_length && number.word != 0
                       && alt && base == 8);

  /* At this point prec_inc is the additional bytes required for the
     specified precision.  It is 0 if the precision would not have
     required additional bytes i.e. the number of input digits is more
     than the precision.  It is greater than zero if the precision is
     more than the number of digits without grouping (precision only
     considers digits).  */
  unsigned int prec_inc = MAX (0, prec - (workend - string));

  if (!left)
    {
      width -= number_length + prec_inc;

      if (number.word != 0 && alt && (base == 16 || base == 2))
        /* Account for 0X, 0x, 0B or 0b hex or binary marker.  */
        width -= 2;

      if (octal_marker)
        --width;

      if (is_negative || showsign || space)
        --width;

      if (pad == L_(' '))
        {
          Xprintf_buffer_pad (buf, L_(' '), width);
          width = 0;
        }

      if (is_negative)
        Xprintf_buffer_putc (buf, L_('-'));
      else if (showsign)
        Xprintf_buffer_putc (buf, L_('+'));
      else if (space)
        Xprintf_buffer_putc (buf, L_(' '));

      if (number.word != 0 && alt && (base == 16 || base == 2))
        {
          Xprintf_buffer_putc (buf, L_('0'));
          Xprintf_buffer_putc (buf, spec);
        }

      width += prec_inc;
      Xprintf_buffer_pad (buf, L_('0'), width);

      if (octal_marker)
        Xprintf_buffer_putc (buf, L_('0'));

      if (number_slow_path)
        group_number (buf, &iter, string, workend, thousands_sep,
                      use_outdigits && base == 10);
      else
        Xprintf_buffer_write (buf, string, workend - string);

      break;
    }
  else
    {
      /* Perform left justification adjustments.  */

      if (is_negative)
        {
          Xprintf_buffer_putc (buf, L_('-'));
          --width;
        }
      else if (showsign)
        {
          Xprintf_buffer_putc (buf, L_('+'));
          --width;
        }
      else if (space)
        {
          Xprintf_buffer_putc (buf, L_(' '));
          --width;
        }

      if (number.word != 0 && alt && (base == 16 || base == 2))
        {
          Xprintf_buffer_putc (buf, L_('0'));
          Xprintf_buffer_putc (buf, spec);
          width -= 2;
        }

      if (octal_marker)
	--width;

      /* Adjust the width by subtracting the number of bytes
         required to represent the number with grouping characters
	 (NUMBER_LENGTH) and any additional bytes required for
	 precision.  */
      width -= number_length + prec_inc;

      Xprintf_buffer_pad (buf, L_('0'), prec_inc);

      if (octal_marker)
        Xprintf_buffer_putc (buf, L_('0'));

      if (number_slow_path)
        group_number (buf, &iter, string, workend, thousands_sep,
                      use_outdigits && base == 10);
      else
        Xprintf_buffer_write (buf, string, workend - string);

      Xprintf_buffer_pad (buf, L_(' '), width);
      break;
    }

LABEL (form_pointer):
  /* Generic pointer.  */
  {
    const void *ptr = process_arg_pointer ();
    if (ptr != NULL)
      {
        /* If the pointer is not NULL, write it as a %#x spec.  */
        base = 16;
        number.word = (unsigned long int) ptr;
        is_negative = 0;
        alt = 1;
        group = 0;
        spec = L_('x');
        goto LABEL (number);
      }
    else
      {
        /* Write "(nil)" for a nil pointer.  */
        string = (CHAR_T *) L_("(nil)");
        /* Make sure the full string "(nil)" is printed.  */
        if (prec < 5)
          prec = 5;
        /* This is a wide string iff compiling wprintf.  */
        is_long = sizeof (CHAR_T) > 1;
        goto LABEL (print_string);
      }
  }
  /* NOTREACHED */

LABEL (form_number):
  if ((mode_flags & PRINTF_FORTIFY) != 0)
    {
      if (! readonly_format)
        {
          extern int __readonly_area (const void *, size_t)
            attribute_hidden;
          readonly_format
            = __readonly_area (format, ((STR_LEN (format) + 1)
                                        * sizeof (CHAR_T)));
        }
      if (readonly_format < 0)
        __libc_fatal ("*** %n in writable segment detected ***\n");
    }
  /* Answer the count of characters written.  */
  void *ptrptr = process_arg_pointer ();
  unsigned int written = Xprintf_buffer_done (buf);
  if (is_longlong)
    *(long long int *) ptrptr = written;
  else if (is_long_num)
    *(long int *) ptrptr = written;
  else if (is_char)
    *(char *) ptrptr = written;
  else if (!is_short)
    *(int *) ptrptr = written;
  else
    *(short int *) ptrptr = written;
  break;

LABEL (form_strerror):
  /* Print description of error ERRNO.  */
  if (alt)
    string = (CHAR_T *) __get_errname (save_errno);
  else
    string = (CHAR_T *) __strerror_r (save_errno, (char *) work_buffer,
                                      WORK_BUFFER_SIZE * sizeof (CHAR_T));
  if (string == NULL)
    {
      /* Print as a decimal number. */
      base = 10;
      is_negative = save_errno < 0;
      number.word = save_errno;
      if (is_negative)
        number.word = -number.word;
      goto LABEL (number);
    }
  else
    {
      is_long = 0;  /* This is no wide-char string.  */
      goto LABEL (print_string);
    }

LABEL (form_character):
  /* Character.  */
  if (is_long)
    goto LABEL (form_wcharacter);
  --width;  /* Account for the character itself.  */
  if (!left)
    Xprintf_buffer_pad (buf, L_(' '), width);
#ifdef COMPILE_WPRINTF
  __wprintf_buffer_putc (buf, __btowc ((unsigned char) /* Promoted. */
                                       process_arg_int ()));
#else
  __printf_buffer_putc (buf, (unsigned char) /* Promoted.  */
                        process_arg_int ());
#endif
  if (left)
    Xprintf_buffer_pad (buf, L_(' '), width);
  break;

LABEL (form_string):
  {
    size_t len;

    /* The string argument could in fact be `char *' or `wchar_t *'.
       But this should not make a difference here.  */
#ifdef COMPILE_WPRINTF
    string = (CHAR_T *) process_arg_wstring ();
#else
    string = (CHAR_T *) process_arg_string ();
#endif
    /* Entry point for printing other strings.  */
    LABEL (print_string):

    if (string == NULL)
      {
        /* Write "(null)" if there's space.  */
        if (prec == -1 || prec >= (int) array_length (null) - 1)
          {
            string = (CHAR_T *) null;
            len = array_length (null) - 1;
          }
        else
          {
            string = (CHAR_T *) L"";
            len = 0;
          }
      }
    else if (!is_long && spec != L_('S'))
      {
#ifdef COMPILE_WPRINTF
        outstring_converted_wide_string (buf, (const char *) string,
                                         prec, width, left);
        /* The padding has already been written.  */
        break;
#else
        if (prec != -1)
          /* Search for the end of the string, but don't search past
             the length (in bytes) specified by the precision.  */
          len = __strnlen (string, prec);
        else
          len = strlen (string);
#endif
      }
    else
      {
#ifdef COMPILE_WPRINTF
        if (prec != -1)
          /* Search for the end of the string, but don't search past
             the length specified by the precision.  */
          len = __wcsnlen (string, prec);
        else
          len = __wcslen (string);
#else
        outstring_converted_wide_string (buf, (const wchar_t *) string,
                                         prec, width, left);
        /* The padding has already been written.  */
        break;
#endif
      }

    if ((width -= len) < 0)
      {
        Xprintf_buffer_write (buf, string, len);
        break;
      }

    if (!left)
      Xprintf_buffer_pad (buf, L_(' '), width);
    Xprintf_buffer_write (buf, string, len);
    if (left)
      Xprintf_buffer_pad (buf, L_(' '), width);
  }
  break;

#ifdef COMPILE_WPRINTF
LABEL (form_wcharacter):
  {
    /* Wide character.  */
    --width;
    if (!left)
      Xprintf_buffer_pad (buf, L_(' '), width);
    Xprintf_buffer_putc (buf, process_arg_wchar_t ());
    if (left)
      Xprintf_buffer_pad (buf, L_(' '), width);
  }
  break;

#else /* !COMPILE_WPRINTF */
LABEL (form_wcharacter):
  {
    /* Wide character.  */
    char wcbuf[MB_LEN_MAX];
    mbstate_t mbstate;
    size_t len;

    memset (&mbstate, '\0', sizeof (mbstate_t));
    len = __wcrtomb (wcbuf, process_arg_wchar_t (), &mbstate);
    if (len == (size_t) -1)
      {
        /* Something went wrong during the conversion.  Bail out.  */
        __printf_buffer_mark_failed (buf);
        goto all_done;
      }
    width -= len;
    if (!left)
      Xprintf_buffer_pad (buf, L_(' '), width);
    Xprintf_buffer_write (buf, wcbuf, len);
    if (left)
      Xprintf_buffer_pad (buf, L_(' '), width);
  }
  break;
#endif /* !COMPILE_WPRINTF */
}
