/* Parse dates for touch and date.

   Copyright (C) 1998, 2003, 2005, 2007-2024 Free Software Foundation, Inc.

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

/* Yacc-based version written by Jim Kingdon and David MacKenzie.
   Rewritten by Jim Meyering.  */

#ifndef POSIXTM_H_
# define POSIXTM_H_

# include <time.h>

/* POSIX Date Syntax flags.  */
# define PDS_TRAILING_YEAR 1
# define PDS_CENTURY 2
# define PDS_SECONDS 4
# define PDS_PRE_2000 8

/* For compatibility with older versions of this header, in which
   PDS_LEADING_YEAR had its own bit.  */
# define PDS_LEADING_YEAR 0

bool posixtime (time_t *p, const char *s, unsigned int syntax_bits);

#endif
