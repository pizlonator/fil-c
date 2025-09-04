/* printf implementation for the dynamic loader.
   Copyright (C) 1997-2024 Free Software Foundation, Inc.
   Copyright The GNU Toolchain Authors.
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

#include <string.h>
#if BUILD_PIE_DEFAULT
# pragma GCC visibility push(hidden)
#endif
#include <_itoa.h>
#include <assert.h>
#include <dl-writev.h>
#include <ldsodefs.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <unistd.h>
#include <intprops.h>

/* The function might be called before the process is self-relocated.  */
static size_t
_dl_debug_strlen (const char *s)
{
  const char *p = s;
  for (; *s != '\0'; s++);
  return s - p;
}

/* Bare-bones printf implementation.  This function only knows about
   the formats and flags needed and can handle only up to 64 stripes in
   the output.  */
static void
_dl_debug_vdprintf (int fd, int tag_p, const char *fmt, va_list arg)
{
# define NIOVMAX 64
  struct iovec iov[NIOVMAX];
  /* Maximum size for 'd', 'u', and 'x' including padding.  */
  enum { IFMTSIZE = INT_STRLEN_BOUND(void *) };
  char ifmtbuf[NIOVMAX][IFMTSIZE];
  int niov = 0;
  pid_t pid = 0;
  char pidbuf[12];

  while (*fmt != '\0')
    {
      const char *startp = fmt;

      if (tag_p > 0)
	{
	  /* Generate the tag line once.  It consists of the PID and a
	     colon followed by a tab.  */
	  if (pid == 0)
	    {
	      char *p;
	      pid = __getpid ();
	      assert (pid >= 0 && sizeof (pid_t) <= 4);
	      p = _itoa (pid, &pidbuf[10], 10, 0);
	      while (p > pidbuf)
		*--p = ' ';
	      pidbuf[10] = ':';
	      pidbuf[11] = '\t';
	    }

	  /* Append to the output.  */
	  assert (niov < NIOVMAX);
	  iov[niov].iov_len = 12;
	  iov[niov++].iov_base = pidbuf;

	  /* No more tags until we see the next newline.  */
	  tag_p = -1;
	}

      /* Skip everything except % and \n (if tags are needed).  */
      while (*fmt != '\0' && *fmt != '%' && (! tag_p || *fmt != '\n'))
	++fmt;

      /* Append constant string.  */
      assert (niov < NIOVMAX);
      if ((iov[niov].iov_len = fmt - startp) != 0)
	iov[niov++].iov_base = (char *) startp;

      if (*fmt == '%')
	{
	  /* It is a format specifier.  */
	  char fill = ' ';
	  int width = -1;
	  int prec = -1;
#if LONG_MAX != INT_MAX
	  int long_mod = 0;
#endif

	  /* Recognize zero-digit fill flag.  */
	  if (*++fmt == '0')
	    {
	      fill = '0';
	      ++fmt;
	    }

	  /* See whether with comes from a parameter.  Note that no other
	     way to specify the width is implemented.  */
	  if (*fmt == '*')
	    {
	      width = va_arg (arg, int);
	      /* The maximum padding accepted is up to pointer size.  */
	      assert (width < IFMTSIZE);
	      ++fmt;
	    }

	  /* Handle precision.  */
	  if (*fmt == '.' && fmt[1] == '*')
	    {
	      prec = va_arg (arg, int);
	      fmt += 2;
	    }

	  /* Recognize the l modifier.  It is only important on some
	     platforms where long and int have a different size.  We
	     can use the same code for size_t.  */
	  if (*fmt == 'l' || *fmt == 'z')
	    {
#if LONG_MAX != INT_MAX
	      long_mod = 1;
#endif
	      ++fmt;
	    }

	  switch (*fmt)
	    {
	      /* Integer formatting.  */
	    case 'd':
	    case 'u':
	    case 'x':
	      {
		/* We have to make a difference if long and int have a
		   different size.  */
#if LONG_MAX != INT_MAX
		unsigned long int num = (long_mod
					 ? va_arg (arg, unsigned long int)
					 : va_arg (arg, unsigned int));
#else
		unsigned long int num = va_arg (arg, unsigned int);
#endif
		bool negative = false;
		if (*fmt == 'd')
		  {
#if LONG_MAX != INT_MAX
		    if (long_mod)
		      {
			if ((long int) num < 0)
			  {
			    num = -num;
			    negative = true;
			  }
		      }
		    else
		      {
			if ((int) num < 0)
			  {
			    num = -(unsigned int) num;
			    negative = true;
			  }
		      }
#else
		    if ((int) num < 0)
		      {
			num = -num;
			negative = true;
		      }
#endif
		  }

		char *endp = &ifmtbuf[niov][IFMTSIZE];
		char *cp = _itoa (num, endp, *fmt == 'x' ? 16 : 10, 0);

		/* Pad to the width the user specified.  */
		if (width != -1)
		  while (endp - cp < width)
		    *--cp = fill;

		if (negative)
		  *--cp = '-';

		iov[niov].iov_base = cp;
		iov[niov].iov_len = endp - cp;
		++niov;
	      }
	      break;

	    case 's':
	      /* Get the string argument.  */
	      iov[niov].iov_base = va_arg (arg, char *);
	      iov[niov].iov_len = _dl_debug_strlen (iov[niov].iov_base);
	      if (prec != -1)
		iov[niov].iov_len = MIN ((size_t) prec, iov[niov].iov_len);
	      ++niov;
	      break;

	    case '%':
	      iov[niov].iov_base = (void *) fmt;
	      iov[niov].iov_len = 1;
	      ++niov;
	      break;

	    default:
	      assert (! "invalid format specifier");
	    }
	  ++fmt;
	}
      else if (*fmt == '\n')
	{
	  /* See whether we have to print a single newline character.  */
	  if (fmt == startp)
	    {
	      iov[niov].iov_base = (char *) startp;
	      iov[niov++].iov_len = 1;
	    }
	  else
	    /* No, just add it to the rest of the string.  */
	    ++iov[niov - 1].iov_len;

	  /* Next line, print a tag again.  */
	  tag_p = 1;
	  ++fmt;
	}
    }

  /* Finally write the result.  */
  _dl_writev (fd, iov, niov);
}


/* Write to debug file.  */
void
_dl_debug_printf (const char *fmt, ...)
{
  va_list arg;

  va_start (arg, fmt);
  _dl_debug_vdprintf (GLRO(dl_debug_fd), 1, fmt, arg);
  va_end (arg);
}


/* Write to debug file but don't start with a tag.  */
void
_dl_debug_printf_c (const char *fmt, ...)
{
  va_list arg;

  va_start (arg, fmt);
  _dl_debug_vdprintf (GLRO(dl_debug_fd), -1, fmt, arg);
  va_end (arg);
}


/* Write the given file descriptor.  */
void
_dl_dprintf (int fd, const char *fmt, ...)
{
  va_list arg;

  va_start (arg, fmt);
  _dl_debug_vdprintf (fd, 0, fmt, arg);
  va_end (arg);
}

void
_dl_printf (const char *fmt, ...)
{
  va_list arg;

  va_start (arg, fmt);
  _dl_debug_vdprintf (STDOUT_FILENO, 0, fmt, arg);
  va_end (arg);
}

void
_dl_error_printf (const char *fmt, ...)
{
  va_list arg;

  va_start (arg, fmt);
  _dl_debug_vdprintf (STDERR_FILENO, 0, fmt, arg);
  va_end (arg);
}

void
_dl_fatal_printf (const char *fmt, ...)
{
  va_list arg;

  va_start (arg, fmt);
  _dl_debug_vdprintf (STDERR_FILENO, 0, fmt, arg);
  va_end (arg);
  _exit (127);
}
rtld_hidden_def (_dl_fatal_printf)
