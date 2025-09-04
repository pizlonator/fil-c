/* Copyright (C) 2023-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published
   by the Free Software Foundation; version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <https://www.gnu.org/licenses/>.  */

#ifndef _ENDSWITH_H
#define _ENDSWITH_H

#include <string.h>

/* Return true if the N bytes at NAME end with with the characters in
   the string SUFFIX.  (NAME[N + 1] does not have to be a null byte.)
   Expected to be called with a string literal for SUFFIX.  */
static inline bool
endswithn (const char *name, size_t n, const char *suffix)
{
  return (n >= strlen (suffix)
	  && memcmp (name + n - strlen (suffix), suffix,
		     strlen (suffix)) == 0);
}

#endif /* _ENDSWITH_H */
