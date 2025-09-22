/* A more useful interface to strtol.

   Copyright (C) 1995-1996, 1998-1999, 2001-2004, 2006-2023 Free Software
   Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#ifndef XSTRTOL_H_
# define XSTRTOL_H_ 1

/* Get intmax_t, uintmax_t.  */
# include <stdint.h>

# ifndef _STRTOL_ERROR
enum strtol_error
  {
    LONGINT_OK = 0,

    /* These two values can be ORed together, to indicate that both
       errors occurred.  */
    LONGINT_OVERFLOW = 1,
    LONGINT_INVALID_SUFFIX_CHAR = 2,

    LONGINT_INVALID_SUFFIX_CHAR_WITH_OVERFLOW = (LONGINT_INVALID_SUFFIX_CHAR
                                                 | LONGINT_OVERFLOW),
    LONGINT_INVALID = 4
  };
typedef enum strtol_error strtol_error;
# endif

# define _DECLARE_XSTRTOL(name, type) \
  strtol_error name (const char *, char **, int, type *, const char *);
_DECLARE_XSTRTOL (xstrtol, long int)
_DECLARE_XSTRTOL (xstrtoul, unsigned long int)
_DECLARE_XSTRTOL (xstrtoll, long long int)
_DECLARE_XSTRTOL (xstrtoull, unsigned long long int)
_DECLARE_XSTRTOL (xstrtoimax, intmax_t)
_DECLARE_XSTRTOL (xstrtoumax, uintmax_t)

#endif /* not XSTRTOL_H_ */
