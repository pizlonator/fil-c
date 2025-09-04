/* Symbol rediretion for loader/static initialization code.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#ifndef _DL_IFUNC_GENERIC_H
#define _DL_IFUNC_GENERIC_H

/* The compiler might optimize loops or other constructs to library calls even
   in freestanding mode.  Although it is safe in most scenarios, on program
   loading and static startup, iFUNC resolution might require other dynamic
   relocation that are not yet done.

   Instead of adding per-file compiler flags to avoid the libcall generation,
   this header defines generic symbols that will be routed instead of the
   default symbols name.

   Each port can then make the required redirection to the expected generic
   implementation if the symbol is provided as iFUNC, with a global definition
   such as:

     asm ("memset = __memset_generic");
 */

#endif
