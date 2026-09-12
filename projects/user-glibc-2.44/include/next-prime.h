/* Finding the next prime >= a given small integer.
   Copyright (C) 1995-2026 Free Software Foundation, Inc.

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

#ifndef _GL_NEXT_PRIME_H
#define _GL_NEXT_PRIME_H

/* This file uses _GL_ATTRIBUTE_CONST.  */
#if !_LIBC && !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _LIBC
# define next_prime __next_prime
#else
# define attribute_hidden
#endif

/* Round a given CANDIDATE number up to the nearest prime, and return that
   prime.  Primes lower than 10 are merely skipped.  */
extern size_t _GL_ATTRIBUTE_CONST next_prime (size_t candidate)
  attribute_hidden;;


#ifdef __cplusplus
}
#endif

#endif /* _GL_NEXT_PRIME_H */
