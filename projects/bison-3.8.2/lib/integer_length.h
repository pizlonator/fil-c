/* integer_length - find most significant bit in an unsigned integer.
   Copyright (C) 2011-2021 Free Software Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Bruno Haible <bruno@clisp.org>, 2011.  */

#ifndef _INTEGER_LENGTH_H
#define _INTEGER_LENGTH_H

#ifdef __cplusplus
extern "C" {
#endif

/* These functions return the minimum number of bits required to represent
   the given unsigned integer.
   For non-zero values, this is the position of the most significant bit
   that is set, plus one.  For zero, it is 0.  */

/* Returns the integer length of x.
   The result is >= 0, <= sizeof (unsigned int) * CHAR_BIT.  */
extern int integer_length (unsigned int x);

/* Returns the integer length of x.
   The result is >= 0, <= sizeof (unsigned long) * CHAR_BIT.  */
extern int integer_length_l (unsigned long x);

/* Returns the integer length of x.
   The result is >= 0, <= sizeof (unsigned long long) * CHAR_BIT.  */
extern int integer_length_ll (unsigned long long x);

#ifdef __cplusplus
}
#endif

#endif /* _INTEGER_LENGTH_H */
