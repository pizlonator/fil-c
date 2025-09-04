/* Configuration for math tests: sNaN payloads.  SPARC version.
   Copyright (C) 2016-2024 Free Software Foundation, Inc.
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

#ifndef SPARC_MATH_TESTS_SNAN_PAYLOAD_H
#define SPARC_MATH_TESTS_SNAN_PAYLOAD_H 1

/* LEON floating-point instructions do not preserve sNaN
   payloads.  */
#if defined (__leon__)
# define SNAN_TESTS_PRESERVE_PAYLOAD	0
#else
# define SNAN_TESTS_PRESERVE_PAYLOAD	1
#endif

#endif /* math-tests-snan-payload.h.  */
