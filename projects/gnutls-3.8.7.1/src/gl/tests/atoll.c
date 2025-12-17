/* Copyright (C) 1991, 1997-1998, 2008-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#if !_LIBC
# include <config.h>
#endif

/* Specification.  */
#include <stdlib.h>

#if _LIBC
# undef atoll
#endif


/* Convert a string to a long long int.  */
long long int
atoll (const char *nptr)
{
  return strtoll (nptr, (char **) NULL, 10);
}
